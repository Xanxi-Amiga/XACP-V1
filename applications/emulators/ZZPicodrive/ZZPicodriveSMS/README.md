# ZZPicoDriveSMS

**ZZPicoDriveSMS** is a Sega Master System emulator for Amiga classic systems
equipped with an **MNT ZZ9000**.

The Amiga 68k side handles the launcher, file access, RTG display, input,
audio output, SRAM and savestates. The emulation core runs on the ZZ9000
ARM Core1 and renders directly into ZZ9000 RTG memory.

This is the **Master System Edition** of ZZPicoDrive.

## Features

- Sega Master System emulation using PicoDrive.
- ARM Core1 execution on the ZZ9000.
- Native AmigaOS launcher with ROM selection and options.
- Direct RTG rendering through Picasso96.
- Low 68k CPU usage.
- PAL and NTSC support.
- AHI stereo audio backend.
- Direct Paula DMA audio backend.
- Keyboard controls.
- lowlevel.library / CD32-style controller support.
- DB9 joystick support.
- Two-player support.
- SRAM support for games that use battery backup.
- Savestates.
- Embedded ARM blob in the executable.

## Requirements

- Amiga with MNT ZZ9000.
- Firmware XX19 from Xanxi's branch, or later XX firmware, or any firmware
  explicitly compatible with ZZPicoDriveSMS.
- Matching `zz9000.card` file.
- Picasso96 RTG setup.
- Picasso96 mode supporting 320x240 in 32-bit colour.
- AHI installed if using the AHI audio backend.

ZZPicoDriveSMS is not compatible with MNT firmwares 1.0 to 1.13.  
ZZPicoDriveSMS is not compatible with MiDWaN / BlitterStudio firmwares.

This is not a criticism of other ZZ9000 firmware branches. ZZPicoDriveSMS uses
specific low-level Core1, DDR memory mapping and RTG/PAN mechanisms required
by this application. Other firmware branches may work only if they explicitly
provide a compatible interface.

The firmware is not included in this repository or in the emulator archive.

Download the XX19 firmware and matching `zz9000.card` from:

https://github.com/Xanxi-Amiga/XACP-V1/releases/tag/XX19_firmware

Do not mix `BOOT.BIN`, `zz9000.card` and ZZPicoDriveSMS executables from
different packages. A mismatched firmware/card/executable set may boot but
fail at runtime.

## ROM format

Supported:

- `.sms`
- `.bin`

Not supported:

- `.zip`

Please decompress `.zip` archives before use.

No Sega Master System ROMs are included.

Only use ROM files you legally own.

## Audio

ZZPicoDriveSMS produces 16-bit stereo PCM audio at 22050 Hz.

The Sega Master System PSG audio emulation runs on the ZZ9000 ARM side.
The ARM writes the stereo PCM stream into shared memory, and the 68k side
plays it back.

**YM2413 / FM audio is not enabled in this release.** Games with optional FM
sound should still run, but this release uses the standard PSG audio path.

Audio modes:

### AHI

16-bit stereo output through AHI. This works with any AHI-supported audio
hardware, including Paula, sound cards and Prisma. AHI must be installed.

### Paula DMA

Direct playback through Paula channels AUD0/AUD1, bypassing AHI. This backend
does not require AHI and provides direct Paula output.

### None

Audio disabled.

## Controls

ZZPicoDriveSMS supports several input methods selectable from the launcher.

### Keyboard

| Key | Master System input |
| --- | --- |
| Cursor keys | Direction pad |
| Numeric keypad 1 | Button 1 |
| Numeric keypad 2 | Button 2 |
| Return | Start / Pause when supported |

### DB9 joystick

Standard Amiga joystick ports can be used through the DB9 input mode.
Direction and fire buttons are mapped to the Master System pad.

### CD32 / USB controllers through lowlevel.library

CD32-style pads and compatible USB controllers can be used through
`lowlevel.library` when supported by your Amiga setup.

### Two-player support

Two-player support is available when suitable input sources are selected for
player 1 and player 2.

## Hotkeys

| Key | Action |
| --- | --- |
| Shift + Esc | Quit emulator |
| F1-F5 | Load savestate slot 1-5 |
| Shift + F1-F5 | Save savestate slot 1-5 |

## Installation

See [INSTALL.md](INSTALL.md).

## Troubleshooting

If the emulator does not open the display, check that your ZZ9000 Picasso96
configuration has a valid 320x240x32 RTG mode.

A High Colour Workbench screen is not enough. ZZPicoDriveSMS needs a real
32-bit / True Colour ZZ9000 RTG mode.

If you get a black screen, OpenScreen failed, or immediate return to Workbench,
please report:

- Amiga model and CPU
- AmigaOS version
- Picasso96 version
- ZZ9000 firmware version
- `zz9000.card` version
- whether Workbench runs on ZZ9000 RTG
- audio backend selected
- game tested
- exact symptom

## Source status

The source code for the ZZ9000 / Amiga port will be made available after
cleanup.

This cleanup step is only meant to separate release code from temporary
debugging code and work-in-progress experiments.

## Credits

ZZ9000 / Amiga port, launcher, ARM integration, audio/video/input/save handling:

**Xanxi**

Emulation core:

**PicoDrive by notaz and contributors**

See included license files for third-party components.
