# The Pico 2 W's partition table

`ab.json` lays the 4 MB flash out for updates over the link (PICO-0011,
PICO-0012, robohub ADR-0023): the table in the first 8 kB, partitions A and B of
2036 kB each — A/B-linked, so the boot ROM boots the newer of two valid images,
prefers the one named by a flash update boot, and gives a try-before-you-buy
image a trial — then, outside any partition, a spare sector, the update state
sector (`src/platform/pico_update.c`) and the keystore's two sectors
(`src/platform/pico_keystore.c`).

`tools/flash_ab.sh` writes it with an image into partition A; Mesh does so for
every Pico 2 W it flashes (MESH-0041).
