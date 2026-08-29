# XACP Firmware Builds

This directory contains Xanxi/XACP firmware builds for the MNT ZZ9000.

The current public firmware is:

```text
BOOT_XX19a.bin
```

Current protocol / ABI baseline:

```text
XACP v1.6
```

Firmware build numbers and XACP protocol versions are separate.

```text
XX19a     = firmware build
XACP v1.6 = protocol / ABI / shared-memory baseline
```

XX19a is therefore the firmware implementing XACP v1.6.

---

# Current firmware: XX19a

XX19a replaces XX19 as the current XACP firmware baseline.

XX19 introduced the XACP v1.5 multimedia map and the large ZZMIDI SoundFont staging area.

XX19a extends this work with the XACP v1.6 shared DDR architecture.

The main objective of v1.6 is to make DDR ownership explicit across the complete XACP environment rather than allowing individual applications to reserve unrelated addresses independently.

---

## XACP v1.6 changes

As XACP expanded, the ZZ9000 DDR became shared by several different classes of software:

```text
Core0 firmware services
Core1 application blobs
Core1 private data
framebuffers
ROM and input data
audio rings
SoundFont data
MIDI data
save-state and persistent buffers
firmware-private heaps
```

Earlier layouts were sufficient when these components were developed independently, but they were no longer suitable as a general shared platform.

XACP v1.6 establishes a coordinated memory-allocation model for these resources.

The XX19a baseline provides:

```text
explicit DDR ownership
separation of persistent Core0 services from Core1 applications
protected ZZMIDI SF2 and MIDI regions
protected firmware-private memory
defined Core1 application areas
separation of application blobs and runtime data
separation of framebuffers and ROM/data buffers
dedicated persistent/save-buffer allocation
removal of known cross-project DDR overlaps
space reserved for future XACP expansion
```

New XACP software should use the current SDK definitions rather than introducing undocumented shared-memory addresses.

---

## ZZMIDI service

XX19a retains the firmware-side SoundFont synthesis service used by ZZMIDI.

The synthesis engine uses:

```text
TinySoundFont
TinyMidiLoader
```

The Amiga-side ZZMIDI application handles files, GUI and AHI playback while the ZZ9000 ARM performs MIDI parsing and SoundFont synthesis.

The established large staging areas remain reserved for the MIDI service:

```text
SF2 staging : 32 MB
MIDI staging:  6 MB
```

These areas are part of the XACP ABI and must not be reused by Core1 applications.

XX19a integrates these regions into the wider v1.6 DDR allocation scheme so that application memory and persistent firmware services remain separated.

---

## Core1 applications

XACP can dynamically launch complete ARM applications on the second Cortex-A9 core.

Examples include:

```text
ZZDoom
ZZPicoDrive
JuliaV2
ZZBench ARM tests
video and graphics engines
```

Core1 applications may require several distinct types of memory, including:

```text
executable blob
application-private memory
framebuffer
ROM or input data
communication mailbox
persistent data or save state
```

XACP v1.6 assigns these resources within a coordinated map instead of allowing each application to reuse arbitrary DDR regions.

This allocation model is intended to remain reusable for future Core1 projects.

---

## Firmware history

Older firmware builds are stored in:

```text
firmware/archive/
```

They are retained for historical reference and for applications tied to a particular release generation.

### XX19a — XACP v1.6

Current public firmware baseline.

```text
XACP v1.6 shared DDR architecture
coordinated Core0 / Core1 memory allocation
protected ZZMIDI service areas
protected firmware-private memory
defined Core1 application regions
removal of known cross-project DDR overlaps
current base for XACP development
```

Current binary:

```text
BOOT_XX19a.bin
```

---

### XX19 — XACP v1.5

XX19 introduced the public XACP v1.5 multimedia memory map.

Important changes included:

```text
ZZMIDI SoundFont service
OP_MIDI_SF2 = 0x0120
32 MB SF2 staging area
6 MB MIDI staging area
large SoundFont playback stabilization
persistent SoundFont reuse
private ARM service-pool documentation
```

XX19 solved the previous SF2 / MIDI staging overlap and established the first formal large multimedia memory map.

It has now been superseded by XX19a / XACP v1.6.

---

### XX18m — first ZZMIDI firmware line

XX18m introduced the first ZZMIDI / SoundFont firmware work.

It also fixed the ZZ9000AX cold-boot initialization issue found in earlier firmware generations.

Validated work included:

```text
initial ZZMIDI service
OP_MIDI_SF2 groundwork
ZZMIDI player / daemon integration
ZZ9000AX cold-boot initialization fix
```

---

### XX18c — ZZDoom baseline

XX18c was the public ZZDoom-era firmware baseline.

Validated features included:

```text
dynamic Core1 launch
shared DDR command path
deferred PAN / fullscreen RTG synchronization
ZZDoom 320x200
ZZDoom 640x400
AHI sound effects
CAMD MIDI
AmigaOS file access
save / load support
clean Core1 STOP / return
```

---

### XX16c — MP3 / MP2 / ZZMPEG baseline

XX16c was the first major public multimedia XACP firmware line.

It established:

```text
ARM MP3 decoding
MP3 streaming
MP2 audio
ZZPlayGUI
AmigaAMP zz9000.engine
mpega.library integration
ZZ-MPEG
early Core1 applications
Julia / fractal validation
```

---

## Compatibility overview

| Firmware  | XACP generation | Main role                                    |
| --------- | --------------- | -------------------------------------------- |
| **XX19a** | **XACP v1.6**   | Current shared DDR / Core0 / Core1 baseline  |
| XX19      | XACP v1.5       | ZZMIDI and large multimedia staging baseline |
| XX18m     | pre-v1.5        | Initial ZZMIDI and ZZ9000AX cold-boot fix    |
| XX18c     | earlier XACP    | ZZDoom public baseline                       |
| XX16c     | earlier XACP    | MP3 / MP2 / ZZMPEG multimedia baseline       |

A newer firmware may retain functionality introduced by older generations, but applications should only be considered compatible when that combination has been explicitly validated.

---

## Installation

The current firmware binary is:

```text
BOOT_XX19a.bin
```

Firmware may be installed using the normal ZZ9000 SD-card procedure or a compatible firmware-update tool.

When updating:

```text
1. Close applications using the ZZ9000.
2. Install the new BOOT firmware.
3. Do not interrupt the flashing or file replacement process.
4. Completely power off the Amiga.
5. Power on again.
6. Test the required XACP applications.
```

A complete power cycle is recommended after replacing the firmware or `zz9000.card`.

Do not rely only on a warm reset when validating a new firmware installation.

---

## Firmware / application matching

Do not arbitrarily mix:

```text
firmware
zz9000.card
Amiga-side executable
Core1 blob
shared-memory definitions
```

from unrelated development generations.

A mismatch can cause:

```text
missing XACP operations
incorrect DDR addresses
black screen
RTG corruption
silent audio
incorrect save data
crash or lock-up
failure of Core1 to return cleanly
```

The fact that an application starts does not prove that its memory map is compatible with the installed firmware.

---

## Source code

XX19a is the source baseline selected for publication of the current Xanxi/XACP firmware changes.

The firmware uses third-party components which retain their respective original licenses.

In particular:

```text
TinySoundFont   - Bernhard Schelling - MIT license
TinyMidiLoader  - Bernhard Schelling - zlib license
```

Original copyright and license notices must be preserved with the corresponding source.

Publication of the XX19a firmware source does not imply that all Amiga-side XACP applications are open source. Application licensing and source availability are defined independently by each application package.

The cleaned XX19a source package will be added to this repository together with the required third-party license material.

---

## Archived builds

Historical firmware binaries belong in:

```text
firmware/archive/
```

The root of `firmware/` should contain only the current firmware generation and its documentation/source structure.

Archived firmware should not be selected simply because a particular build number is older or newer. Some historical applications were released and validated against a specific firmware generation and may need that exact environment.

---

## Development rule

The XACP v1.6 memory map is now part of the platform ABI.

Future firmware services and Core1 applications must therefore:

```text
use documented XACP memory regions
avoid private hard-coded DDR allocations outside their assigned areas
preserve firmware-service memory
preserve application-independent shared areas
update the XACP version if an incompatible ABI change becomes necessary
```

This rule is intended to prevent the memory-map fragmentation that developed during the earlier experimental XACP generations.

---

## Credits

XACP / Xanxi, 2026.

Thanks to the MNT ZZ9000 project and the Amiga / ZZ9000 community.

Third-party components retain their original authorship and licenses.
