# ZZPicoDriveMD 1.1 - Genesis / Mega Drive Edition

## Summary

ZZPicoDriveMD is a Sega Mega Drive / Genesis emulator for classic Amiga systems
equipped with an MNT ZZ9000.

The emulation core runs on the ZZ9000 ARM Core1. The Amiga 68k side handles the
launcher, file access, RTG display, input, audio output, SRAM and savestates.

## Highlights

- Sega Mega Drive / Genesis emulation using PicoDrive.
- ARM Core1 execution on the ZZ9000.
- PAL and NTSC support.
- AHI stereo audio backend.
- Direct Paula DMA audio backend.
- Keyboard, DB9 joystick and lowlevel.library controller support.
- Two-player support.
- SRAM support.
- Savestates.
- Embedded ARM blob in the executable.

## Requirements

Current supported XACP baseline:

**XACP v1.6 / firmware XX19a or later**, with matching `zz9000.card`,
Picasso96 and a valid 320x240x32 ZZ9000 RTG mode.

Firmware XX19a:

https://github.com/Xanxi-Amiga/XACP-ZZ9000/releases/tag/XX19a

ZZPicoDriveMD is not compatible with official MNT firmware 1.13 or with
MiDWaN / BlitterStudio firmware unless compatible XACP interfaces are provided.

## ROMs

No commercial ROM images are included.

Supported ROM formats: `.bin`, `.gen`, `.md`.

## Sources

The corresponding ZZPicoDriveMD 1.1 source is published in `source/`.

PicoDrive base revision:

`26ecb2b6358fefba24e3d68b9eb2efba7f10d5ee`

Cyclone 68000 revision used by the build:

`3ac7cf1bdeecb60e2414980e8dc72ff092f69769`
