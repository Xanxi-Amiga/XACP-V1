# ZZPicoDriveMD third-party notices

ZZPicoDriveMD is a ZZ9000 / AmigaOS port built around **PicoDrive**.

## PicoDrive

Upstream project: **notaz/picodrive**

Corresponding release base:

```text
PicoDrive commit: 26ecb2b6358fefba24e3d68b9eb2efba7f10d5ee
Date:             2025-04-03
```

PicoDrive at this revision is distributed under its historical non-commercial
license, reproduced in `COPYING_PICODRIVE.txt` and preserved in the vendored
upstream source tree.

That license requires modified redistributions to include the complete source
code for the components used by the binary. For this reason the public source
publication vendors the exact PicoDrive revision used by ZZPicoDriveMD 1.1.

PicoDrive authorship and contributor information is preserved in the upstream
`AUTHORS` file.

## Cyclone 68000

The Mega Drive build uses the Cyclone 68000 core referenced by the PicoDrive
revision above:

```text
Repository: irixxxx/cyclone68000
Commit:     3ac7cf1bdeecb60e2414980e8dc72ff092f69769
```

Cyclone is dual-licensed under GNU GPL v2 and the MAME-style non-commercial
license. Its original license files are preserved in the vendored source tree.

## ZZ9000 / XACP integration

ZZ9000 / AmigaOS launcher, Core1 integration, shared-memory protocol,
video/audio/input/save handling and real-hardware validation:

**Xanxi, 2026**

No Sega ROM images or proprietary game data are included.

ZZ9000 is a product of MNT Research GmbH. ZZPicoDriveMD and XACP are independent
software projects and are not official MNT Research products.
