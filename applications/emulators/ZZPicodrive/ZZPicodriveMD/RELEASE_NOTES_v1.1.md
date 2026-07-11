# ZZPicoDriveMD 1.1 - Genesis / Mega Drive Edition

## Summary

ZZPicoDriveMD is a Sega Mega Drive / Genesis emulator for Amiga classic systems
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

- MNT ZZ9000.
- Firmware XX19 from Xanxi's branch, or later XX firmware, or any firmware
  explicitly compatible with ZZPicoDriveMD.
- Matching `zz9000.card`.
- Picasso96.
- Valid 320x240x32 ZZ9000 RTG mode.
- AHI if using the AHI backend.

The firmware is not included in this release.

Download the XX19 firmware and matching `zz9000.card` from:

https://github.com/Xanxi-Amiga/XACP-V1/releases/tag/XX19_firmware

ZZPicoDriveMD is not compatible with MNT firmwares 1.0 to 1.13 or
MiDWaN / BlitterStudio firmwares.

## ROMs

No commercial ROMs are included.

Supported ROM formats:

- `.bin`
- `.gen`
- `.md`

Unsupported:

- `.zip`
- `.smd`

## Aminet

Aminet package type:

```text
misc/emu
```

## Sources

The source code for the ZZ9000 / Amiga port will be made available after
cleanup.
