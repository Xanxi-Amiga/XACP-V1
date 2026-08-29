# XACP Applications

This directory contains the applications, games, emulators, multimedia tools and validation software developed for the Xanxi XACP platform on the MNT ZZ9000.

XACP applications use the ARM Cortex-A9 processors and DDR memory of the ZZ9000 to offload workloads from the Amiga 68k while retaining AmigaOS integration for display, input, files, audio and user interfaces.

The current platform baseline is:

```text
Firmware:  XX19a
Protocol:  XACP v1.6
```

## Current compatibility

**All current public XACP applications in this repository are compatible with the latest firmware published in the repository.**

Historical firmware builds are retained for reference, regression testing and documentation of the platform's evolution, but they are not required for normal use of current applications.

The only exception is the set of **very early fractal / Core1 demonstration programs from early May 2026**. These predate the current XACP architecture and are preserved for historical purposes only. They should not be considered part of the current compatibility baseline.

---

## Application overview

| Directory                               | Application               | Role                                           | Status               |
| --------------------------------------- | ------------------------- | ---------------------------------------------- | -------------------- |
| `ZZMIDI/`                               | **ZZMIDI**                | SoundFont MIDI synthesis using the ZZ9000 ARM  | Current              |
| `ZZSpeech/`                             | **ZZSpeech**              | CMU Flite speech synthesis on ARM Core1        | Current              |
| `emulators/ZZPicodrive/ZZPicodriveMD/`  | **ZZPicoDriveMD 1.1**     | Sega Mega Drive / Genesis emulator             | Current              |
| `emulators/ZZPicodrive/ZZPicodriveSMS/` | **ZZPicoDriveSMS 1.1**    | Sega Master System emulator                    | Current              |
| `games/ZZDoom/`                         | **ZZDoom**                | Doom engine running on ARM Core1               | Current              |
| `games/ZZRastan/`                       | **ZZRastan 1.0**          | Rastan arcade hardware recreation on ARM Core1 | Current              |
| `mpegplayer/`                           | **ZZ-MPEG**               | MPEG-1 video / MP2 audio playback              | Advanced Beta        |
| `mp3/`                                  | **ZZMP3Play / ZZPlayGUI** | ARM-accelerated MP3 / MP2 playback             | Current / historical |
| `mp3/`                                  | **zz9000.engine**         | AmigaAMP external ARM decoding engine          | Current / historical |
| `mp3/`                                  | **mpega.library**         | XACP-backed MPEGA-compatible decoding          | Current / historical |
| `benchmarks/`                           | **ZZBench GUI**           | 68k / ARM / memory bandwidth benchmark         | Current              |
| `fractals/core1-julia-v2/`              | **JuliaV2**               | Core1 execution and clean-return validation    | Current validation   |
| `fractals/`                             | **early ZZFractal demos** | Early XACP graphical experiments               | Historical only      |

Detailed installation, usage and licensing information belongs in each application's own directory.

---

# ZZMIDI

Directory:

```text
ZZMIDI/
```

ZZMIDI provides General MIDI / SoundFont synthesis using the ZZ9000 ARM.

The Amiga side handles the user interface, files and AHI playback while the firmware-side ARM service performs MIDI parsing and SoundFont synthesis using TinySoundFont.

Main capabilities include:

```text
SoundFont MIDI synthesis
TinySoundFont ARM rendering
TinyMidiLoader MIDI parsing
SoundFont banks up to the supported XACP staging limit
AHI audio playback
GUI MIDI file playback
persistent SoundFont reuse
XACP shared MIDI service
```

ZZMIDI is an example of a persistent **Core0 firmware service**, unlike applications such as ZZPicoDrive or ZZDoom which execute complete engines on Core1.

The ZZMIDI firmware memory regions are protected by the XACP v1.6 shared DDR allocation model.

See the ZZMIDI directory for detailed release documentation and SoundFont recommendations.

---

# ZZSpeech

Directory:

```text
ZZSpeech/
```

ZZSpeech provides speech synthesis for AmigaOS using CMU Flite running on ZZ9000 ARM Core1.

The Amiga side provides:

```text
resident daemon
Intuition / GadTools GUI
ZZSay command-line client
ZZSPEECH: DOS handler
AHI playback
WAV export
external .flitevox voice support
```

Unlike ZZMIDI, ZZSpeech does not require the speech engine to be permanently built into the main firmware.

The Flite Core1 program is loaded dynamically when ZZSpeech starts.

Only one dynamically loaded Core1 application can execute at a time, so ZZSpeech should be stopped cleanly before launching another Core1 application such as ZZPicoDrive, ZZDoom or ZZRastan.

ZZSpeech 1.0 is distributed as closed-source freeware. See its own documentation and license files for redistribution and third-party licensing terms.

---

# ZZPicoDrive

Directory:

```text
emulators/ZZPicodrive/
```

ZZPicoDrive runs the PicoDrive emulation engine on ZZ9000 ARM Core1 while the Amiga side provides file access, RTG display, input, audio output, SRAM and savestate handling.

Two public editions are currently included.

## ZZPicoDriveMD 1.1

Directory:

```text
emulators/ZZPicodrive/ZZPicodriveMD/
```

Sega Mega Drive / Genesis emulator.

Main features include:

```text
Mega Drive / Genesis emulation
ARM Core1 execution
PAL and NTSC operation
Picasso96 RTG output
AHI stereo audio
direct Paula DMA audio
keyboard input
DB9 joystick input
lowlevel.library / CD32-style controllers
two-player support
SRAM
savestates
embedded ARM Core1 blob
```

No commercial ROMs are included.

---

## ZZPicoDriveSMS 1.1

Directory:

```text
emulators/ZZPicodrive/ZZPicodriveSMS/
```

Sega Master System edition of ZZPicoDrive.

It uses the same general XACP / Core1 architecture as the Mega Drive edition.

Features include:

```text
Master System emulation
ARM Core1 execution
PAL and NTSC operation
Picasso96 RTG output
AHI or Paula audio
keyboard / joystick / controller input
two-player support
SRAM
savestates
```

YM2413 / FM audio is not enabled in the 1.1 release.

No commercial ROMs are included.

---

# ZZDoom

Directory:

```text
games/ZZDoom/
```

ZZDoom is a Doom port based on `doomgeneric`.

The Doom engine runs on the ZZ9000 ARM Core1 while AmigaOS provides the host environment.

Main features include:

```text
ARM Core1 Doom engine
320x200 output
640x400 output
Picasso96 RTG display
keyboard input
AHI sound effects
MIDI integration
AmigaOS file access
save / load support
clean Core1 stop and return
```

ZZDoom was one of the projects that established the practical dynamic Core1 application model later reused by more complex XACP software.

Historical ZZDoom binaries are retained in the application's own archive directory.

---

# ZZRastan

Directory:

```text
games/ZZRastan/
```

ZZRastan 1.0 is a standalone recreation of the original Rastan arcade hardware for Amiga systems equipped with the ZZ9000.

It is not a normal 68k game port.

The arcade CPUs and hardware are emulated on the ZZ9000 ARM Cortex-A9 Core1 while AmigaOS provides:

```text
program launch
Picasso96 / RTG display
keyboard and joystick input
AHI audio output
ROM loading
```

The ARM side implements:

```text
Cyclone 68000 execution
CZ80 Z80 execution
Taito video hardware
YM2151 FM audio
MSM5205 ADPCM audio
game scheduling
framebuffer rendering
```

Rastan World and Rastan Saga Japan ROM sets are supported.

No commercial game ROMs or game assets are distributed.

The source corresponding to the ARM Core1 blob is included where required by the applicable third-party licenses. The Amiga-side launcher and GUI remain proprietary.

---

# ZZ-MPEG

Directory:

```text
mpegplayer/
```

ZZ-MPEG is an MPEG-1 / MP2 multimedia player for Amiga systems equipped with the ZZ9000.

The current line remains designated:

```text
ZZ-MPEG 1.0 Advanced Beta
```

The application demonstrates ARM/Core1 video decoding combined with Amiga-side display and audio integration.

Main capabilities include:

```text
MPEG-1 Program Stream input
ARM / Core1 video decoding
Workbench window output
Picasso96 fullscreen output
MP2 audio through XACP
AHI playback
clean stop / return
```

Included development variants may include:

```text
ZZMpegDirect
ZZMpegStream
```

Detailed status and synchronization notes are documented in the application directory.

---

# MP3 / MP2 audio tools

Directory:

```text
mp3/
```

The MP3 directory contains some of the earliest practical XACP multimedia applications.

These established the basic architecture later reused by more advanced projects:

```text
compressed data on the Amiga
shared XACP control structures
ARM-side decoding
PCM shared-memory ring
AHI playback on AmigaOS
```

Components include:

### ZZMP3Play / ZZPlayGUI

Standalone Amiga applications using ARM-accelerated MP3 / MP2 decoding.

### zz9000.engine

AmigaAMP external decoding engine using the XACP ARM path.

The filename:

```text
zz9000.engine
```

should be retained because it is the external-engine name expected by AmigaAMP.

### mpega.library

Experimental / integration work providing an MPEGA-compatible decoding interface backed by XACP.

These tools are historically important because they were the first substantial demonstration that the ZZ9000 could be used as a practical general-purpose multimedia coprocessor rather than only as an RTG board.

---

# ZZBench GUI

Directory:

```text
benchmarks/
```

ZZBench GUI is a benchmark and validation tool for the XACP architecture.

It compares the Amiga 68k system with the ZZ9000 ARM and measures the different memory paths involved in XACP.

Tests include:

```text
68k Dhrystone
68k Whetstone
ARM Core1 Dhrystone
ARM Core1 Whetstone
Amiga Chip RAM bandwidth
Amiga Fast RAM bandwidth
ZZ9000 DDR bandwidth through ARM
ZZ9000 DDR bandwidth through Zorro III
```

ZZBench is primarily a platform-validation tool rather than a normal end-user multimedia application.

---

# JuliaV2

Directory:

```text
fractals/core1-julia-v2/
```

JuliaV2 is an important Core1 validation application.

It demonstrated the complete application lifecycle later reused by ZZDoom, ZZPicoDrive, ZZRastan and other projects:

```text
load ARM blob into ZZ9000 DDR
start Core1
communicate through shared memory
render / compute independently
stop Core1 cleanly
return to AmigaOS
launch another Core1 application afterwards
```

JuliaV2 remains useful as a simple validation and non-regression application for the Core1 launch / stop architecture.

---

# Historical early fractal demos

Directory:

```text
fractals/
```

The repository also retains very early ZZFractal / Core1 demonstration programs developed during the initial XACP experiments in **early May 2026**.

These programs predate the current XACP Core1 architecture and memory-map conventions.

They are preserved because they document the early development of:

```text
ARM-side computation
shared DDR communication
framebuffer rendering
basic XACP graphics offload
early Core1 experiments
```

**These early May 2026 fractal demos are historical material only.**

They are the exception to the general compatibility rule for applications in this repository and should not be considered supported applications for the current XX19a / XACP v1.6 baseline.

JuliaV2 represents the later validated Core1 model and should be used instead for current Core1 testing.

---

# Firmware compatibility

The current public baseline is:

```text
BOOT_XX19a.bin
XACP v1.6
```

All current public XACP applications in this repository are compatible with the latest firmware published here.

Users therefore do **not** normally need to downgrade the firmware to run applications that were originally developed during earlier XX firmware generations.

Older firmware builds are preserved under:

```text
../firmware/archive/
```

for:

```text
historical reference
regression testing
development history
reproduction of older release environments
```

They are not the recommended firmware for a current XACP installation.

The only compatibility exception in the application tree is the set of very early fractal / Core1 demonstrations from early May 2026, which are retained solely for historical purposes.

---

# Core0 and Core1 applications

XACP supports two main ARM execution models.

## Persistent Core0 services

These are integrated into the main firmware and can remain available while AmigaOS applications use them.

Examples:

```text
MP3 / MP2 decoding services
ZZMIDI SoundFont synthesis
shared audio services
```

## Dynamic Core1 applications

These load a complete ARM program onto the second Cortex-A9 core.

Examples:

```text
ZZDoom
ZZPicoDrive
ZZRastan
ZZSpeech
JuliaV2
ZZ-MPEG Core1 paths
```

Only one dynamic Core1 application can execute on Core1 at a time.

Applications must stop Core1 cleanly before another Core1 program is launched.

---

# XACP v1.6 memory-map rule

The current XACP shared DDR layout is part of the platform ABI.

Applications should therefore use documented XACP memory regions rather than independently selecting hard-coded DDR addresses.

This is particularly important because DDR is shared between:

```text
persistent firmware services
Core1 executable blobs
Core1 runtime memory
framebuffers
ROM / input data
audio buffers
SoundFont and MIDI data
save-state / persistent buffers
firmware-private memory
```

XACP v1.6 formalizes ownership of these areas to prevent collisions between independent projects.

---

# Installation rule

For a current XACP system, use:

```text
latest public Xanxi firmware
matching zz9000.card
current application release
application-specific documentation
```

After replacing the ZZ9000 firmware or `zz9000.card`, fully power off the Amiga before testing.

Do not assume that firmware or `zz9000.card` files from unrelated ZZ9000 firmware branches implement the XACP interfaces required by these applications.

---

# ROMs and copyrighted game data

Emulators and arcade projects in this repository do not include commercial game ROMs.

Users must provide legally obtained game data where required.

This applies in particular to:

```text
ZZPicoDriveMD
ZZPicoDriveSMS
ZZRastan
ZZDoom game data / WAD files where applicable
```

Third-party game, console and arcade trademarks and intellectual property remain property of their respective rights holders.

---

# Licensing and source availability

Applications in the XACP repository do not all use the same license.

The repository contains a mixture of:

```text
Xanxi closed-source freeware
Xanxi proprietary components
published Xanxi source
open-source third-party engines
third-party components requiring source redistribution
historical experimental source
binary-only application components
```

Source availability for one XACP component does not imply that another component is open source.

Each application's own README and license files are authoritative for its distribution and source terms.

Third-party copyrights and license notices must be preserved.

---

# Directory structure

```text
applications/
│
├── ZZMIDI/
│
├── ZZSpeech/
│
├── benchmarks/
│
├── emulators/
│   └── ZZPicodrive/
│       ├── ZZPicodriveMD/
│       └── ZZPicodriveSMS/
│
├── fractals/
│   └── core1-julia-v2/
│
├── games/
│   ├── ZZDoom/
│   └── ZZRastan/
│
├── mp3/
│
└── mpegplayer/
```

Each substantial application should maintain its own README containing installation, requirements, usage, compatibility, source status and licensing information.

This `applications/README.md` is the index of the public XACP application ecosystem.
