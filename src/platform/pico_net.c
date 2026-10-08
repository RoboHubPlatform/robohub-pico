/*
 * pico_net.c — the network contract over the Pico W's wireless chip and LwIP.
 *
 * The chip and its stack belong to the Pico SDK: `cyw43_arch` owns the radio and
 * its event loop, and LwIP owns IP, TCP and UDP. This file is the join — the
 * contract robohub-common declares, answered by the SDK's calls — so middleware
 * that speaks MQTT or HTTP is written once and runs here as it does on a POSIX
 * gateway.
 *
 * The SDK's `pico_cyw43_arch_lwip_threadsafe_background` runs LwIP bare-metal
 * (`NO_SYS=1`), so only the RAW API is available: the BSD socket and netconn
 * layers block in an OS that is not there. The socket contract is therefore
 * implemented over TCP and UDP control blocks, and a receive returns whatever
 * the background context has already delivered rather than parking the core —
 * which is exactly the non-blocking poll `robohub/net.h` asks for.
 *
 * The receive path is a single-producer/single-consumer ring: the lwIP
 * callbacks run in the arch's background context and only advance `rx_head`,
 * a caller only advances `rx_tail`. Calls into LwIP from the caller's context
 * are bracketed with `cyw43_arch_lwip_begin`/`end`, which is what the arch's
 * thread-safety contract requires.
 *
 * It is compiled where the wireless headers are: the platform carries it as an
 * interface source, so it builds in the program alongside `pico_cyw43_arch`,
 * with that target's includes and definitions.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#include "robohub/net.h"

#include <string.h>

#include "pico/cyw43_arch.h"
#include "pico/time.h"

#include "lwip/ip_addr.h"
#include "lwip/netif.h"
#include "lwip/pbuf.h"
#include "lwip/tcp.h"
#include "lwip/udp.h"

/** How many sockets may be open at once. A gateway needs a handful. */
#ifndef RH_NET_MAX_SOCKETS
#define RH_NET_MAX_SOCKETS 4
#endif

/** How much one socket buffers while a caller is not reading. For TCP this is
 * at least `TCP_WND` (see lwipopts.h), so a refused receive pbuf always fits
 * once the ring drains and backpressure cannot deadlock. For UDP each datagram
 * is stored with its length, and one that does not fit is dropped whole. */
#ifndef RH_NET_RX_CAPACITY
#define RH_NET_RX_CAPACITY 3072u
#endif

typedef struct {
    rh_bool in_use;
    rh_bool is_tcp;
    rh_bool connected;
    rh_bool errored;
    struct tcp_pcb* tcp;
    struct udp_pcb* udp;
    ip_addr_t remote;
    rh_u16 remote_port;
    /* Filled by the lwIP callbacks, drained by the caller. */
    rh_u8 rx[RH_NET_RX_CAPACITY];
    volatile rh_u32 rx_head;
    volatile rh_u32 rx_tail;
    rh_u32 rx_dropped;
} rh_net_socket;

static rh_net_socket net_sockets[RH_NET_MAX_SOCKETS];
static rh_bool net_initialized;
static rh_bool net_up;

/* -------------------------------------------------------------------------- */
/*  Receive ring                                                              */
/* -------------------------------------------------------------------------- */

static rh_u32 rx_count(const rh_net_socket* socket)
{
    rh_u32 head = socket->rx_head;
    rh_u32 tail = socket->rx_tail;
    return (head >= tail) ? (head - tail) : (RH_NET_RX_CAPACITY - (tail - head));
}

/* One slot is kept empty so head == tail always means empty. */
static rh_u32 rx_free(const rh_net_socket* socket)
{
    return (RH_NET_RX_CAPACITY - 1u) - rx_count(socket);
}

static rh_u32 rx_next(rh_u32 index)
{
    index++;
    return (index >= RH_NET_RX_CAPACITY) ? 0u : index;
}

static void rx_put(rh_net_socket* socket, rh_u32* cursor, rh_u8 byte)
{
    socket->rx[*cursor] = byte;
    *cursor = rx_next(*cursor);
}

/* Producer: the lwIP receive callbacks, in the background context. Only the
 * caller's `rx_tail` is read; `rx_head` is published once per push, so a record
 * a consumer sees is always whole. */

/* TCP: a stream of bytes, no boundaries. Fails rather than dropping, so the
 * caller can refuse the pbuf and let the window throttle the sender. */
static rh_bool rx_push_bytes(rh_net_socket* socket, const void* data, rh_u32 length)
{
    if (length > rx_free(socket)) {
        return RH_FALSE;
    }
    const rh_u8* bytes = (const rh_u8*)data;
    rh_u32 head = socket->rx_head;
    for (rh_u32 i = 0; i < length; i++) {
        rx_put(socket, &head, bytes[i]);
    }
    socket->rx_head = head;
    return RH_TRUE;
}

/* UDP: each datagram is `[length:u16 little-endian][payload]`, so a receive can
 * return exactly one datagram, as a datagram socket does, and discard whatever
 * of it the caller's buffer could not hold. */
static rh_bool rx_datagram_begin(rh_net_socket* socket, rh_u32 length, rh_u32* cursor)
{
    if (length > 0xFFFFu || (length + 2u) > rx_free(socket)) {
        socket->rx_dropped++;
        return RH_FALSE;
    }
    *cursor = socket->rx_head;
    rx_put(socket, cursor, (rh_u8)(length & 0xFFu));
    rx_put(socket, cursor, (rh_u8)((length >> 8) & 0xFFu));
    return RH_TRUE;
}

static void rx_datagram_append(rh_net_socket* socket, rh_u32* cursor,
                               const void* data, rh_u32 length)
{
    const rh_u8* bytes = (const rh_u8*)data;
    for (rh_u32 i = 0; i < length; i++) {
        rx_put(socket, cursor, bytes[i]);
    }
}

static void rx_datagram_commit(rh_net_socket* socket, rh_u32 cursor)
{
    socket->rx_head = cursor;
}

/* A whole datagram is buffered, so this either sees a complete record or none. */
static rh_bool rx_peek_datagram(const rh_net_socket* socket, rh_u32* length)
{
    if (rx_count(socket) < 2u) {
        return RH_FALSE;
    }
    rh_u32 tail = socket->rx_tail;
    rh_u32 low = socket->rx[tail];
    rh_u32 high = socket->rx[rx_next(tail)];
    *length = low | (high << 8);
    return rx_count(socket) >= (*length + 2u);
}

/* Consumer: the caller's context. */

static rh_u32 rx_pop_bytes(rh_net_socket* socket, void* data, rh_u32 length)
{
    rh_u8* bytes = (rh_u8*)data;
    rh_u32 tail = socket->rx_tail;
    rh_u32 head = socket->rx_head;
    rh_u32 count = 0u;
    while (count < length && tail != head) {
        bytes[count++] = socket->rx[tail];
        tail = rx_next(tail);
    }
    socket->rx_tail = tail;
    return count;
}

static rh_u32 rx_pop_datagram(rh_net_socket* socket, void* data, rh_u32 length)
{
    rh_u32 datagram_len = 0u;
    if (!rx_peek_datagram(socket, &datagram_len)) {
        return 0u;
    }
    rh_u8* bytes = (rh_u8*)data;
    rh_u32 cursor = rx_next(rx_next(socket->rx_tail)); /* past the length header */
    rh_u32 out = (datagram_len < length) ? datagram_len : length;
    for (rh_u32 i = 0; i < out; i++) {
        bytes[i] = socket->rx[cursor];
        cursor = rx_next(cursor);
    }
    /* Discard the whole datagram, including the part the caller could not take. */
    cursor = socket->rx_tail;
    for (rh_u32 i = 0; i < datagram_len + 2u; i++) {
        cursor = rx_next(cursor);
    }
    socket->rx_tail = cursor;
    return out;
}

/* -------------------------------------------------------------------------- */
/*  lwIP callbacks                                                            */
/* -------------------------------------------------------------------------- */

static err_t tcp_on_recv(void* arg, struct tcp_pcb* pcb, struct pbuf* p, err_t err)
{
    rh_net_socket* socket = (rh_net_socket*)arg;

    if (p == 0) {
        /* The far end closed. The pcb stays until we close it. */
        socket->connected = RH_FALSE;
        socket->errored = RH_TRUE;
        return ERR_OK;
    }
    if (err != ERR_OK) {
        pbuf_free(p);
        socket->errored = RH_TRUE;
        return err;
    }

    /* Not enough room: refuse the whole pbuf. lwIP keeps it and calls again from
     * its fast timer, and the unacknowledged window throttles the sender, so
     * nothing is lost. `RH_NET_RX_CAPACITY` is at least `TCP_WND`, so a refused
     * pbuf always fits once the caller has drained the ring. */
    if (p->tot_len > rx_free(socket)) {
        return ERR_MEM;
    }

    for (struct pbuf* q = p; q != 0; q = q->next) {
        (void)rx_push_bytes(socket, q->payload, q->len);
    }
    tcp_recved(pcb, p->tot_len);
    pbuf_free(p);
    return ERR_OK;
}

static err_t tcp_on_sent(void* arg, struct tcp_pcb* pcb, u16_t len)
{
    (void)arg;
    (void)pcb;
    (void)len;
    return ERR_OK;
}

static void tcp_on_error(void* arg, err_t err)
{
    rh_net_socket* socket = (rh_net_socket*)arg;
    (void)err;
    /* lwIP has already freed the pcb; it must not be used again. */
    socket->tcp = 0;
    socket->connected = RH_FALSE;
    socket->errored = RH_TRUE;
}

static err_t tcp_on_connected(void* arg, struct tcp_pcb* pcb, err_t err)
{
    rh_net_socket* socket = (rh_net_socket*)arg;
    if (err == ERR_OK) {
        socket->connected = RH_TRUE;
        tcp_nagle_disable(pcb); /* telemetry frames are small and prompt */
    } else {
        socket->errored = RH_TRUE;
    }
    return ERR_OK;
}

static void udp_on_recv(void* arg, struct udp_pcb* pcb, struct pbuf* p,
                        const ip_addr_t* addr, u16_t port)
{
    rh_net_socket* socket = (rh_net_socket*)arg;
    (void)pcb;
    (void)addr;
    (void)port;
    if (p == 0) {
        return;
    }
    /* One record per datagram, published only once all of it is written. A
     * datagram that does not fit is dropped whole and counted, as it would be. */
    rh_u32 cursor = 0u;
    if (rx_datagram_begin(socket, p->tot_len, &cursor)) {
        for (struct pbuf* q = p; q != 0; q = q->next) {
            rx_datagram_append(socket, &cursor, q->payload, q->len);
        }
        rx_datagram_commit(socket, cursor);
    }
    pbuf_free(p);
}

/* -------------------------------------------------------------------------- */
/*  Interface                                                                 */
/* -------------------------------------------------------------------------- */

RoboHubError robohub_net_wifi_connect(const RoboHubWiFiConfig* config)
{
    if (config == 0 || config->ssid == 0) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    /* `cyw43_arch_init` brings the chip up once; a later connect only joins. */
    if (!net_initialized) {
        if (cyw43_arch_init() != 0) {
            return ROBOHUB_ERR_IO;
        }
        net_initialized = RH_TRUE;
    }
    cyw43_arch_enable_sta_mode();

    const char* password = (config->password != 0) ? config->password : "";
    rh_u32 timeout = (config->timeout_ms != 0u) ? config->timeout_ms : 15000u;
    if (cyw43_arch_wifi_connect_timeout_ms(config->ssid, password,
                                           CYW43_AUTH_WPA2_AES_PSK, timeout) != 0) {
        return ROBOHUB_ERR_IO;
    }
    net_up = RH_TRUE;
    return ROBOHUB_OK;
}

RoboHubError robohub_net_wifi_disconnect(void)
{
    if (net_up) {
        cyw43_arch_disable_sta_mode();
        net_up = RH_FALSE;
    }
    return ROBOHUB_OK;
}

RoboHubNetStatus robohub_net_status(void)
{
    if (!net_up) {
        return ROBOHUB_NET_DISCONNECTED;
    }

    cyw43_arch_lwip_begin();
    struct netif* interface = netif_default;
    RoboHubNetStatus result = ROBOHUB_NET_CONNECTING;
    if (interface != 0 && netif_is_up(interface) && netif_is_link_up(interface) &&
        !ip4_addr_isany_val(*netif_ip4_addr(interface))) {
        result = ROBOHUB_NET_CONNECTED;
    }
    cyw43_arch_lwip_end();
    return result;
}

RoboHubError robohub_net_ip(char* out_ip, rh_size max_len)
{
    if (out_ip == 0 || max_len == 0u) {
        return ROBOHUB_ERR_INVALID_ARG;
    }

    cyw43_arch_lwip_begin();
    struct netif* interface = netif_default;
    RoboHubError result = ROBOHUB_ERR_STATE;
    if (interface != 0 && netif_is_up(interface) &&
        !ip4_addr_isany_val(*netif_ip4_addr(interface))) {
        const char* text = ip4addr_ntoa(netif_ip4_addr(interface));
        if (strlen(text) + 1u > max_len) {
            result = ROBOHUB_ERR_INVALID_ARG;
        } else {
            strcpy(out_ip, text);
            result = ROBOHUB_OK;
        }
    }
    cyw43_arch_lwip_end();
    return result;
}

/* -------------------------------------------------------------------------- */
/*  Sockets                                                                   */
/* -------------------------------------------------------------------------- */

RoboHubSocket robohub_socket_create(RoboHubSocketType type)
{
    for (rh_u32 i = 0; i < RH_NET_MAX_SOCKETS; i++) {
        rh_net_socket* socket = &net_sockets[i];
        if (socket->in_use) {
            continue;
        }
        memset(socket, 0, sizeof(*socket));
        socket->is_tcp = (type == ROBOHUB_SOCKET_TCP) ? RH_TRUE : RH_FALSE;

        cyw43_arch_lwip_begin();
        if (socket->is_tcp) {
            socket->tcp = tcp_new();
            if (socket->tcp != 0) {
                tcp_arg(socket->tcp, socket);
                tcp_recv(socket->tcp, tcp_on_recv);
                tcp_sent(socket->tcp, tcp_on_sent);
                tcp_err(socket->tcp, tcp_on_error);
            }
        } else {
            socket->udp = udp_new();
            if (socket->udp != 0) {
                udp_recv(socket->udp, udp_on_recv, socket);
            }
        }
        cyw43_arch_lwip_end();

        if ((socket->is_tcp && socket->tcp == 0) ||
            (!socket->is_tcp && socket->udp == 0)) {
            continue;
        }
        socket->in_use = RH_TRUE;
        return (RoboHubSocket)socket;
    }
    return 0;
}

RoboHubError robohub_socket_connect(RoboHubSocket socket, const char* host, rh_u16 port)
{
    rh_net_socket* self = (rh_net_socket*)socket;
    if (self == 0 || host == 0) {
        return ROBOHUB_ERR_INVALID_ARG;
    }

    /* An address, not a name: `dns_gethostbyname` resolves asynchronously, and
     * the POSIX reference the contract is tested against is numeric too. */
    ip4_addr_t address;
    if (!ip4addr_aton(host, &address)) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    ip_addr_copy_from_ip4(self->remote, address);
    self->remote_port = port;

    cyw43_arch_lwip_begin();
    err_t err;
    if (self->is_tcp) {
        err = (self->tcp != 0)
                  ? tcp_connect(self->tcp, &self->remote, port, tcp_on_connected)
                  : ERR_CONN;
    } else {
        err = (self->udp != 0) ? udp_connect(self->udp, &self->remote, port) : ERR_CONN;
    }
    cyw43_arch_lwip_end();
    return (err == ERR_OK) ? ROBOHUB_OK : ROBOHUB_ERR_IO;
}

int robohub_socket_send(RoboHubSocket socket, const void* data, rh_size len)
{
    rh_net_socket* self = (rh_net_socket*)socket;
    if (self == 0 || (data == 0 && len > 0u)) {
        return ROBOHUB_SOCKET_ERROR;
    }
    if (len == 0u) {
        return 0;
    }

    u16_t wanted = (len > 0xFFFFu) ? 0xFFFFu : (u16_t)len;
    cyw43_arch_lwip_begin();
    int result = ROBOHUB_SOCKET_ERROR;

    if (self->is_tcp) {
        if (self->tcp != 0 && self->connected) {
            /* All or nothing: a TCP stream has no frame boundary to resync on,
             * so a short write would corrupt the next frame rather than lose
             * this one. The caller retries the whole buffer later. */
            if (tcp_sndbuf(self->tcp) >= wanted &&
                tcp_write(self->tcp, data, wanted, TCP_WRITE_FLAG_COPY) == ERR_OK) {
                tcp_output(self->tcp);
                result = (int)wanted;
            }
        }
    } else if (self->udp != 0) {
        struct pbuf* p = pbuf_alloc(PBUF_TRANSPORT, wanted, PBUF_RAM);
        if (p != 0) {
            if (pbuf_take(p, data, wanted) == ERR_OK && udp_send(self->udp, p) == ERR_OK) {
                result = (int)wanted;
            }
            pbuf_free(p);
        }
    }

    cyw43_arch_lwip_end();
    return result;
}

int robohub_socket_recv(RoboHubSocket socket, void* data, rh_size len, rh_u32 timeout_ms)
{
    rh_net_socket* self = (rh_net_socket*)socket;
    if (self == 0 || data == 0 || len == 0u) {
        return ROBOHUB_SOCKET_ERROR;
    }

    absolute_time_t deadline = make_timeout_time_ms(timeout_ms);
    for (;;) {
        if (self->is_tcp) {
            if (rx_count(self) > 0u) {
                return (int)rx_pop_bytes(self, data, len);
            }
        } else {
            /* One datagram per call, as a datagram socket delivers. */
            rh_u32 datagram_len = 0u;
            if (rx_peek_datagram(self, &datagram_len)) {
                return (int)rx_pop_datagram(self, data, len);
            }
        }
        if (self->errored) {
            return ROBOHUB_SOCKET_ERROR;
        }
        if (timeout_ms == 0u) {
            return ROBOHUB_SOCKET_TIMEOUT;
        }
        if (absolute_time_diff_us(get_absolute_time(), deadline) <= 0) {
            return ROBOHUB_SOCKET_TIMEOUT;
        }
        /* Sleep a tick and re-check: the arch services the stack in the
         * background, so a receive blocks the caller no longer than this. */
        sleep_ms(1);
    }
}

void robohub_socket_close(RoboHubSocket socket)
{
    rh_net_socket* self = (rh_net_socket*)socket;
    if (self == 0) {
        return;
    }

    cyw43_arch_lwip_begin();
    if (self->is_tcp) {
        if (self->tcp != 0) {
            /* Detach first: `tcp_abort` may reach the error callback, and it
             * must not touch a socket that is being torn down. */
            tcp_arg(self->tcp, 0);
            tcp_recv(self->tcp, 0);
            tcp_sent(self->tcp, 0);
            tcp_err(self->tcp, 0);
            tcp_poll(self->tcp, 0, 0);
            if (tcp_close(self->tcp) != ERR_OK) {
                tcp_abort(self->tcp);
            }
            self->tcp = 0;
        }
    } else if (self->udp != 0) {
        udp_recv(self->udp, 0, 0);
        udp_remove(self->udp);
        self->udp = 0;
    }
    cyw43_arch_lwip_end();

    self->in_use = RH_FALSE;
    self->connected = RH_FALSE;
    self->errored = RH_FALSE;
}
