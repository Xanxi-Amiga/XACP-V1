# ZZRastan for ZZ9000

**ZZRastan** is a standalone recreation of the original **Rastan** arcade
hardware for classic Amiga systems equipped with an **MNT ZZ9000**.

It is not a normal 68k game port. The arcade CPUs and hardware are emulated on
the ZZ9000 ARM Cortex-A9 Core1, while AmigaOS handles the host-side integration:
program launch, Picasso96/RTG display, keyboard/joystick input and AHI audio.

**ZZRastan is freeware for non-commercial use. It is not open-source software
as a whole.**

No commercial game ROMs are included.

---

## Features

- Original Rastan arcade 68000 program executed through Cyclone on ZZ9000 Core1
- CZ80 Z80 sound CPU emulation
- Taito tilemap and sprite hardware recreation
- YM2151 FM sound
- MSM5205 ADPCM sound
- Faithful mono audio path
- Approximately 60 Hz game logic with smooth RTG output
- Rastan World and Rastan Saga Japan ROM sets supported
- Keyboard input
- Two-button Amiga DB9 joystick support
- USB game-controller support through AmigaOS
- Keyboard, DB9 and USB controls can be used simultaneously
- Pause
- Optional infinite-lives trainer
- Clean Core1 stop and relaunch

Hardware testing includes a **68030/25** Amiga, where the game is smooth.
Slower 68030 systems are not currently guaranteed.

Save states are not included in ZZRastan 1.0.

---

## Architecture

```text
Amiga 68k side
  |
  |-- proprietary launcher
  |-- proprietary GUI
  |-- Picasso96 / RTG display
  |-- keyboard + DB9 + USB input
  |-- AHI audio playback
  |-- ROM loading
  |
  +---- shared ZZ9000 DDR / XACP ----+
                                     |
ZZ9000 ARM Core1                     |
  |                                  |
  |-- Cyclone 68000 core             |
  |-- CZ80 Z80 core                  |
  |-- Taito video hardware model     |
  |-- YM2151                         |
  |-- MSM5205                        |
  |-- game scheduler                 |
  |-- framebuffer renderer           |
  +----------------------------------+
```

The Amiga CPU does not emulate the Rastan arcade machine itself. The
compute-heavy emulation runs on the ZZ9000 ARM Core1.

---

## Requirements

- Classic Amiga running AmigaOS 3.x
- MNT ZZ9000
- XACP-compatible ZZ9000 firmware and matching `zz9000.card`
- Picasso96 / RTG
- AHI
- A legally obtained supported Rastan arcade ROM set

Minimum hardware actually validated so far: **68030 at 25 MHz**.

---

## Supported ROM sets

Validated sets:

```text
Rastan      - World
Rastan Saga - Japan
```

The GUI can auto-detect the set or force World/Japan.

### World 68000 program ROMs

```text
b04-38.19
b04-37.7
b04-40.20
b04-39.8
b04-42.21
b04-43-1.9
```

### Japan 68000 program ROMs

```text
b04-32.19
b04-31.7
b04-34-1.20
b04-33-1.8
b04-36.21
b04-35.9
```

### Shared graphics and sound ROMs

```text
b04-01.40
b04-02.67
b04-03.39
b04-04.66
b04-05.15
b04-06.28
b04-07.14
b04-08.27
b04-19.49
b04-20.76
```

**ROM files are not distributed with ZZRastan.** Users must provide their own
legally obtained game data.

---

## Installation

Download `ZZRastan_1.0.lha` from the GitHub Releases page and extract it.

Recommended layout:

```text
ZZRastan/
  ZZRastan
  ZZRastan.info
  ZZRastanGUI
  ZZRastanGUI.info
  zzrastan.bin
  README
  SOURCE.txt
  licenses/
  roms/
```

Place the supported ROM files in `roms/`.

The GUI finds `ZZRastan` and `zzrastan.bin` in its own program drawer.

---

## Usage

### Workbench

Launch `ZZRastanGUI`, select the ROM directory if necessary, choose
Auto-detect, World or Japan, then press **Launch**.

### Shell

```text
ZZRastan roms zzrastan.bin
```

The GUI is the recommended launcher.

---

## Controls

| Control | Action |
|---|---|
| Cursor keys | Move |
| A | Attack |
| S | Jump |
| 1 | Start |
| 5 | Insert coin |
| P | Pause |
| T | Infinite lives |
| Esc | Quit |

Joystick/gamepad:

```text
Main button     Attack
Second button   Jump
```

Keyboard, DB9 joystick and supported USB controllers can be used
simultaneously.

---

## Licensing and distribution

ZZRastan 1.0 is distributed as **freeware for non-commercial use**.

### Amiga launcher and GUI

The Amiga 68k launcher and `ZZRastanGUI` are proprietary closed-source
software.

```text
Copyright (c) 2026 Xanxi.
All rights reserved.
```

They may be redistributed unmodified as part of the complete ZZRastan
freeware package. No permission is granted to modify, relicense or
incorporate these proprietary components into another project.

### ARM Core1 blob

`zzrastan.bin` contains original ZZRastan code together with third-party
emulation components under several different licenses. The blob therefore
does **not** have a single blanket license.

The corresponding source code for the ARM Core1 blob is published in this
repository and includes the source required by the applicable third-party
licenses.

Third-party components retain their own copyright and license terms,
including:

- Cyclone 68000
- CZ80
- Jarek Burczynski YM2151 implementation / applicable FBNeo-MAME terms
- ymfm

See [`licenses/`](licenses/) for the authoritative notices.

The availability of the Core1 source does not make ZZRastan as a whole
open-source software and does not place original ZZRastan-specific code in
the public domain.

No Rastan ROMs, graphics, music, samples or other commercial game data are
distributed.

---

## Source

The source corresponding to the ZZRastan 1.0 ARM Core1 blob is in:

```text
source/
ymfm/
```

Release tag:

```text
ZZRastan_V1.0
```

The Amiga 68k launcher and GUI are separate proprietary programs and their
source code is not included.

---

## Credits

Project direction, development, release and real-hardware validation:
**Xanxi**

Third-party emulation components and their authors are credited in
[`licenses/`](licenses/).

Rastan and the original arcade game software and hardware-related
intellectual property remain property of their respective rights holders.
