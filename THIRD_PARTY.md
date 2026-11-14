# Third-party code

robohub-pico is MIT-licensed (see [LICENSE](LICENSE)). It carries two projects
under `vendor/` that keep their own licences, which apply to those files and to
what is built from them:

| Folder | Project | Licence |
|---|---|---|
| `vendor/pico-sdk` | Raspberry Pi Pico SDK, (c) Raspberry Pi (Trading) Ltd. | BSD-3-Clause — [vendor/pico-sdk/LICENSE.TXT](vendor/pico-sdk/LICENSE.TXT) |
| `vendor/FreeRTOS-Kernel` | FreeRTOS kernel, (c) Amazon.com, Inc. or its affiliates | MIT — [vendor/FreeRTOS-Kernel/LICENSE.md](vendor/FreeRTOS-Kernel/LICENSE.md) |

The BSD-3-Clause licence asks that its copyright notice and conditions travel
with redistributed source, and appear in the documentation of a redistributed
binary: firmware built for a Pico ships with this notice.
