# Third-party source archives

ZZPicoDriveMD 1.1 uses PicoDrive by notaz and contributors plus the Cyclone
68000 core.

For source redistribution, keep the exact upstream source archives here:

- `picodrive-26ecb2b6358fefba24e3d68b9eb2efba7f10d5ee.zip`
- `cyclone68000-3ac7cf1bdeecb60e2414980e8dc72ff092f69769.zip`

Upstream revisions used by the released MD 1.1 build:

PicoDrive:
`26ecb2b6358fefba24e3d68b9eb2efba7f10d5ee`

Cyclone 68000:
`3ac7cf1bdeecb60e2414980e8dc72ff092f69769`

The Cyclone repository intentionally does not store a generated `Cyclone.s`.
The file is produced from the Cyclone generator sources by its Makefile. The
ZZPicoDrive MD build script now performs that generation automatically when
`Cyclone.s` is absent.

No Sega ROM images are part of this source package.
