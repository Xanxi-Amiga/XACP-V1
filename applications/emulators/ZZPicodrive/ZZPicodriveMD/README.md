# ZZPicoDriveMD

**ZZPicoDriveMD** is a Sega Mega Drive / Genesis emulator for classic Amiga systems equipped with an **MNT ZZ9000**.

The Amiga 68k side handles the launcher, file access, RTG display, input, audio output, SRAM and savestates. The emulation core runs on the ZZ9000 ARM Core1 and renders directly into ZZ9000 RTG memory.

This is the **Genesis / Mega Drive Edition** of ZZPicoDrive.

## Features

* Sega Mega Drive / Genesis emulation using PicoDrive
* ARM Core1 execution on the ZZ9000
* Native AmigaOS launcher with ROM selection and options
* Direct RTG rendering through Picasso96
* Low 68k CPU usage
* PAL and NTSC support
* AHI stereo audio backend
* Direct Paula DMA audio backend
* Keyboard controls
* lowlevel.library / CD32-style controller support
* DB9 joystick support
* Two-player support
* SRAM support for games that use battery backup
* Savestates
* Embedded ARM blob in the executable

## Requirements

* Amiga with MNT ZZ9000
* **XACP v1.6 / firmware XX19a or later**, or firmware explicitly compatible with the required XACP interfaces
* Matching `zz9000.card`
* Picasso96 RTG setup
* Picasso96 mode supporting 320x240 in 32-bit colour
* AHI installed if using the AHI audio backend

Firmware XX19a:

https://github.com/Xanxi-Amiga/XACP-ZZ9000/releases/tag/XX19a

ZZPicoDriveMD is not compatible with the official MNT firmware 1.13.

It is also not compatible with other firmware branches unless they explicitly provide the XACP interfaces required by ZZPicoDriveMD.

Do not mix `BOOT.bin`, `zz9000.card` and ZZPicoDriveMD executables from unrelated firmware generations.

## ROM format

Supported:

* `.bin`
* `.gen`
* `.md`

Not supported:

* `.zip`
* `.smd`

Please decompress `.zip` archives and convert interleaved `.smd` files to raw `.bin` format before use.

No Sega / Mega Drive / Genesis ROMs are included.

Only use ROM files you are legally entitled to use.

## Audio

ZZPicoDriveMD produces 16-bit stereo PCM audio at 22050 Hz.

Mega Drive audio emulation, including YM2612 FM and PSG, runs on the ZZ9000 ARM side. The ARM writes the stereo PCM stream into shared memory and the 68k side plays it back.

### AHI

16-bit stereo output through AHI. This works with any AHI-supported audio hardware. AHI must be installed.

### Paula DMA

Direct playback through Paula channels AUD0/AUD1, bypassing AHI.

### None

Audio disabled.

## Controls

### Keyboard

| Key              | Mega Drive input |
| ---------------- | ---------------- |
| Cursor keys      | Direction pad    |
| Numeric keypad 1 | Button A         |
| Numeric keypad 2 | Button B         |
| Numeric keypad 3 | Button C         |
| Return           | Start            |

### DB9 joystick

Standard Amiga joystick ports can be used through the DB9 input mode.

### CD32 / USB controllers through lowlevel.library

CD32-style pads and compatible controllers can be used through `lowlevel.library` when supported by the Amiga setup.

### Two-player support

Two-player support is available when suitable input sources are selected for player 1 and player 2.

## Hotkeys

| Key           | Action                  |
| ------------- | ----------------------- |
| Shift + Esc   | Quit emulator           |
| F1-F5         | Load savestate slot 1-5 |
| Shift + F1-F5 | Save savestate slot 1-5 |

## Installation

See [INSTALL.md](INSTALL.md).

## Source code

The corresponding source code for **ZZPicoDriveMD 1.1** is available in:

```text
source/
```

The source distribution contains the Amiga-side launcher and integration code, the ZZ9000 ARM/Core1 code, build files, and the PicoDrive components used by the emulator.

The PicoDrive base used for this port is:

```text
PicoDrive
commit 26ecb2b6358fefba24e3d68b9eb2efba7f10d5ee
```

Cyclone 68000 is based on:

```text
commit 3ac7cf1bdeecb60e2414980e8dc72ff092f69769
```

See `LICENSE_NOTICE.md`, the included PicoDrive `COPYING` file and the original third-party license files for licensing information.

## Credits

ZZ9000 / Amiga port, launcher, ARM integration, audio/video/input/save handling:

**Xanxi**

Emulation core:

**PicoDrive by notaz and contributors**

ZZ9000 is a product of MNT Research GmbH.

ZZPicoDriveMD is an independent project and is not an official MNT Research product.