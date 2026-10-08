/*
 * lwipopts.h — LwIP as the Pico W's radio and the RoboHub net port want it.
 *
 * LwIP is configured by this header, and the SDK does not ship one: the
 * application is expected to say what it wants. This says what a RoboHub board
 * wants from the network — TCP, UDP, DHCP and DNS — and nothing it does not,
 * kept small because it is a microcontroller's heap the tables are drawn from.
 *
 * `pico_cyw43_arch_lwip_threadsafe_background` is the arch this pairs with: the
 * radio's events and the stack are serviced in a background context, and only
 * the lwIP RAW API is available to callers. That is why `LWIP_SOCKET` and
 * `LWIP_NETCONN` are off — the BSD socket and netconn layers need an OS
 * (`NO_SYS=0`) to block in, and this platform runs bare-metal. The socket
 * contract `robohub/net.h` declares is implemented over the RAW API in
 * `src/platform/pico_net.c` instead, which is also what lets a receive be a
 * non-blocking poll rather than a call that parks the core.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#ifndef _LWIPOPTS_H
#define _LWIPOPTS_H

/* No operating system of LwIP's own: the cyw43 arch services the stack from its
 * background context. This is what selects the RAW API. */
#define NO_SYS                      1

/* The BSD socket and netconn layers require `NO_SYS=0`; the RoboHub contract is
 * answered over the RAW API, so neither is built. */
#define LWIP_SOCKET                 0
#define LWIP_NETCONN                0

/* Memory: enough for a handful of connections, no more. `MEM_SIZE` must be at
 * least `TCP_SND_BUF`, because `tcp_write` copies into this heap — advertise
 * more send buffer than the heap can back and `tcp_write` reports `ERR_MEM`
 * for data `tcp_sndbuf()` said would fit. */
#define MEM_LIBC_MALLOC             0
#define MEM_ALIGNMENT               4
#define MEM_SIZE                    8000
#define PBUF_POOL_SIZE              24
#define MEMP_NUM_TCP_SEG            32
#define MEMP_NUM_ARP_QUEUE          10
#define MEM_STATS                   0
#define SYS_STATS                   0
#define MEMP_STATS                  0
#define LINK_STATS                  0
#define LWIP_STATS                  0
#define LWIP_STATS_DISPLAY          0

/* The protocols a sensor gateway uses. */
#define LWIP_ARP                    1
#define LWIP_ETHERNET               1
#define LWIP_ICMP                   1
#define LWIP_RAW                    1
#define LWIP_IPV4                   1
#define LWIP_TCP                    1
#define LWIP_UDP                    1
#define LWIP_DHCP                   1
#define LWIP_DNS                    1
#define LWIP_TCP_KEEPALIVE          1
#define LWIP_CALLBACK_API           1

/* TCP windows sized for the link and for the receive ring the net port keeps.
 * `TCP_MSS` is named before the windows expressed in it, and `TCP_WND` is kept
 * at or below `RH_NET_RX_CAPACITY` (see pico_net.c) so a refused receive pbuf
 * always fits once the ring drains — the backpressure cannot deadlock. The send
 * buffer fits the heap above. */
#define TCP_MSS                     1460
#define TCP_WND                     (2 * TCP_MSS)
#define TCP_SND_BUF                 (4 * TCP_MSS)
#define TCP_SND_QUEUELEN            ((4 * (TCP_SND_BUF) + (TCP_MSS - 1)) / (TCP_MSS))
#define TCP_LISTEN_BACKLOG          4

/* Interface callbacks so the status and address are readable while it changes. */
#define LWIP_NETIF_STATUS_CALLBACK  1
#define LWIP_NETIF_LINK_CALLBACK    1
#define LWIP_NETIF_HOSTNAME         1

/* The chip's checksum and the small things LwIP wants on a part this size. */
#define LWIP_CHKSUM_ALGORITHM       3
#define LWIP_NETIF_TX_SINGLE_PBUF   1
#define DHCP_DOES_ARP_CHECK         0
#define LWIP_DHCP_DOES_ACD_CHECK    0

#endif /* _LWIPOPTS_H */
