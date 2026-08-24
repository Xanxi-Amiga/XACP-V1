# ZZRastan for ZZ9000

**ZZRastan** is a standalone recreation of the original **Rastan** arcade hardware
for Amiga systems equipped with an **MNT ZZ9000**.

It is not a normal 68k game port. The arcade machine is emulated on the ZZ9000
ARM Core1 while AmigaOS handles the host-side integration: program launch,
Picasso96/RTG display, keyboard/joystick input and AHI audio.

No commercial game ROMs are included.

---

## Features

- Original Rastan arcade program executed through a 68000 CPU core on ZZ9000 Core1.
- Z80 sound CPU emulation.
- Taito-style tilemap and sprite hardware recreation.
- YM2151 FM sound and MSM5205 ADPCM.
- Faithful mono audio path, matching the original game's mono routing.
- Approximately 60 Hz game logic with smooth RTG output.
- Automatic support for the validated **World** and **Japan** ROM sets.
- Keyboard input.
- Two-button Amiga DB9 joystick support.
- USB game-controller support through AmigaOS.
- Keyboard, DB9 and USB controls can be used without selecting an exclusive input mode.
- Pause function.
- Optional infinite-lives trainer.
- Clean stop and relaunch of the ARM Core1 application.

Hardware testing includes a **68030/25** Amiga, where the game was reported smooth.
Slower 68030 systems are not currently guaranteed.

---

## Architecture

ZZRastan uses a split Amiga/ZZ9000 architecture:

```text
Amiga 68k side
  |
  |-- launcher / Workbench integration
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

The Amiga CPU does not emulate the Rastan arcade machine itself. The compute-heavy
emulation runs on the ZZ9000 ARM Core1.

---

## Requirements

- Amiga with MNT ZZ9000.
- Matching XACP-compatible ZZ9000 firmware (XX19 or later version) and `zz9000.card`.
- Picasso96 / RTG.
- AHI.
- A legal dump of a supported Rastan arcade ROM set.
- Minimum hardware actually validated so far: **68030 at 25 MHz**.

Do not mix unrelated firmware, `zz9000.card` and application packages unless that
combination has been explicitly tested.

---

## Supported ROM sets

Validated sets:

```text
World
Japan
```

ZZRastan automatically detects the supported set.

**ROM files are not distributed with ZZRastan.** You must provide your own legally
obtained game data.

The final release package should document the exact filenames expected by the
release build once the cleaned source/binary set is frozen.

---

## Installation

Copy the release files to one directory on the Amiga.

Expected release layout:

```text
ZZRastan/
  ZZRastan
  ZZRastan.info
  zzrastan.bin
  README.md
  LICENSES/
  roms/
```

Place your supported Rastan ROM files in the ROM directory expected by the final
release build.

The ARM Core1 blob must match the ZZRastan executable from the same release.

---

## Controls

Current validated keyboard controls:

```text
Cursor keys   Move
A             Attack
S             Jump
1             Start
5             Coin
P             Pause
T             Infinite-lives trainer
```

Two-button DB9 joystick and supported USB game controllers can also be used.

---

## Performance

The emulation runs on the ZZ9000 ARM Core1. The Amiga CPU is used mainly for host
integration.

Validated hardware includes:

```text
68060/50   smooth
68030/25   smooth
```

The optimized host-side input polling substantially reduces 68k overhead compared
with earlier development builds.

---

## Save states

Save-state support is the only feature still under final validation at the time
this README was prepared.
Save states are not included in ZZRastan 1.0 and will appear in a later version.


## Licensing

ZZRastan contains or derives from several third-party components with their own
license terms. Their original notices must be retained.

See:

```text
LICENSES/
```

No Rastan game ROMs, graphics, music or other commercial game data are distributed.

---

## Credits

Project direction, development, release and real-hardware validation: Xanxi

Third-party emulation components and their authors are credited in LICENSES/.

Rastan and the original arcade hardware/software are property of their respective
rights holders. ZZRastan is an independent compatibility/emulation project and
does not include the original commercial ROM data.
