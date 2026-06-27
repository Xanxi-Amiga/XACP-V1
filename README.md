
# XACP v1.5

**eXtended ARM Coprocessor Protocol for the MNT ZZ9000 Amiga RTG board.**

XACP is the shared protocol, ABI and memory-map convention used by the Xanxi ZZ9000 firmware branch and its Amiga-side applications.

It uses the ARM Cortex-A9 cores inside the ZZ9000's Xilinx Zynq-7020 to offload multimedia and compute workloads from the Amiga 68k CPU, while AmigaOS keeps control of display, input, audio output, files, GUI and system integration.

XACP v1.5 is the current validated multimedia baseline.

---

## Versioning

XACP protocol versions and ZZ9000 firmware build numbers are separate.

```text
XACP v1.5 = protocol / ABI / shared-memory map / documentation baseline

XX16c     = firmware build line for MP3 / MP2 / ZZMPEG
XX18c     = firmware build line for the ZZDoom public baseline
XX18m     = firmware build introducing ZZMIDI and fixing ZZ9000AX cold-boot init
XX19      = firmware build implementing the public XACP v1.5 baseline
```

Public release naming:

```text
BOOT_ZZ9000_XX19_XACP_v1.5.bin
```

GitHub release tag:

```text
v1.5.0
```

Do not refer to "firmware v1.5" or "XACP XX19" as if they were the same thing.

XX19 is the firmware. XACP v1.5 is the protocol and memory-map baseline implemented by that firmware.

---

## Validated feature line

XACP currently covers:

```text
ARM MP3 decoding
MP3 streaming pipeline
MP2 audio path
AmigaAMP external engine path: zz9000.engine
mpega.library XACP integration
Core1 dynamic launch
Fractal / JuliaV2 rendering
Shared DDR mailbox
XACP opcode protocol
ZZMPEG MPEG-1 video player with MP2 audio
ZZDoom Core1 Doom port
ZZBench GUI CPU and memory-bandwidth benchmark
ZZMIDI SoundFont playback path
XACP v1.5 shared multimedia memory map
```

---

## Firmware history

### XX16c — audio, MP2 and early Core1 baseline

XX16c is the first major public XACP multimedia baseline.

Validated work:

```text
MP3 decoding
MP3 streaming pipeline
MP2 audio support
AmigaAMP external engine: zz9000.engine
mpega.library XACP integration work
ZZMPEG MPEG-1 / MP2 playback path
JuliaV2 / Core1 validation path
```

### XX18c — ZZDoom baseline

XX18c is the ZZDoom-era public firmware baseline.

Validated work:

```text
Core1 application launch
shared DDR command path
deferred PAN / fullscreen RTG synchronization
ZZDoom 320x200 and 640x400
AHI sound effects
CAMD external MIDI path
save/load through AmigaOS files
```

### XX18m — first ZZMIDI firmware line

XX18m is the firmware line that introduced the ZZMIDI work into the public XACP tree.

Validated work:

```text
initial ZZMIDI / SoundFont firmware path
OP_MIDI_SF2 = 0x0120 groundwork
ZZMIDI daemon / player integration work
ZZ9000AX cold-boot initialization fix
```

The ZZ9000AX cold-boot fix is historically important: earlier builds could require a reset after cold power-on before the card initialized correctly. XX18m fixed that initialization issue.

### XX19 — XACP v1.5 baseline

XX19 is the public firmware baseline implementing the XACP v1.5 multimedia memory map.

Validated work:

```text
ZZMIDI SoundFont playback path
OP_MIDI_SF2 = 0x0120
SF2 staging fixed at 32 MB
MIDI staging moved after SF2
private ARM pool documented
large SoundFont playback stabilized
persistent SF2 across Next / Previous
```

XX19 replaces the internal XX18t development naming for the public release.

---

## Repository contents

```text
firmware/       XACP-enabled ZZ9000 firmware binaries
docs/           protocol notes, memory maps and historical documentation
sdk/            headers and SDK material for XACP clients
applications/   Amiga-side applications and tools
```

---

## Applications

### MP3 / AmigaAMP / mpega.library

ARM-side MP3 decoding through the ZZ9000, with Amiga-side playback through AHI.

This path includes:

```text
zz9000.engine    AmigaAMP external engine for XACP playback
mpega.library    XACP-backed mpega.library work
```

Location:

```text
applications/mp3/
```

### ZZMPEG

MPEG-1 Program Stream player for Amiga + ZZ9000.

Video is decoded on the ZZ9000 ARM/Core1. MP2 audio is decoded through XACP and played through AHI.

Location:

```text
applications/mpegplayer/
```

### JuliaV2

Fractal / Julia renderer and Core1 validation application.

JuliaV2 validated the basic Core1 application model later reused by heavier projects:

```text
Core1 launch
shared DDR communication
ARM-side rendering
Amiga-side display integration
clean stop / return path
```

### ZZDoom

`doomgeneric`-based Doom port for Amiga systems equipped with the ZZ9000.

The Doom engine runs on the ZZ9000 ARM Core1. AmigaOS handles RTG display, keyboard input, AHI sound effects, CAMD music path, file access and savegames.

Location:

```text
applications/ZZDoom/
```

### ZZBench GUI

Benchmark comparing the Amiga 68k CPU with the ZZ9000 ARM Core1 using Dhrystone, Whetstone and memory-bandwidth tests.

Location:

```text
applications/benchmarks/
```

### ZZMIDI

SoundFont MIDI playback path for XACP v1.5.

The v0.5 release focuses on MIDI file playback with SoundFont support up to 32 MB and the corrected v1.5 memory map.

Location:

```text
applications/ZZMIDI/
```

---

## XACP v1.5 memory map

All addresses below are relative to:

```text
fb = board + 0x00010000
```

unless otherwise stated.

```text
0x04000000  XACP command block / historical area
0x04002000  MP3 / MP2 StreamControl

0x04100000  MP3 input ring, 512 KB
0x04200000  MP3 / MP2 PCM output ring, 1 MB

0x04600000  ZZMIDI CAMD FIFO / event staging
0x04800000  ZZMIDI PCM ring, 1 MB

0x05800000  ZZMIDI SF2 staging start
0x07800000  ZZMIDI SF2 staging end / MIDI staging start
0x07E00000  MIDI staging end / ARM firmware C heap start
```

The key v1.5 correction is:

```text
SF2  staging : fb+0x05800000 - fb+0x07800000 = 32 MB
MIDI staging : fb+0x07800000 - fb+0x07E00000 = 6 MB
```

This fixes the previous overlap where large SoundFont files above approximately 24 MB could collide with the MIDI staging area.

Private ARM pool:

```text
0x22000000 - 0x26000000  raw private copy area, 64 MB
0x26000000 - 0x30000000  TSF / TML / service heap, 160 MB
```

Do not reuse `0x20000000-0x22000000` for private ARM pools. This range can overlap AmigaOS-visible Z3 memory.

---

## Documentation

Historical XACP V1 protocol documentation:

```text
docs/XACP_V1_NOTICE.md
```

XACP v1.5 firmware history and memory-map documentation:

```text
docs/XACP_V1_5_HISTORY_MEMORY_MAP_PART2.md
```

Memory-map infographic:

```text
docs/images/XACP_v1_5_memory_map.png
```

---

## Firmware notes

Firmware binaries in this repository are Xanxi/XACP builds for ZZ9000 public use.

Do not mix arbitrary firmware, `zz9000.card` files and application binaries from unrelated packages. Some applications depend on matching firmware features such as Core1 launch, deferred PAN, audio rings or the XACP v1.5 memory map.

After replacing firmware or `zz9000.card`, fully power off the Amiga before testing.

---

## Roadmap

Planned follow-up work:

```text
ZZMIDI CAMD realtime stabilization
ZZSpeech using Flite
PicoDrive MVP on Core1
ZZReSID on Core1
ZZQuake memory strategy
Further ARM/Core1 multimedia services
```

These projects build on the XACP v1.5 baseline. They are not required for the current v1.5 release.

---

## Credits

XACP / Xanxi, 2026.

Special thanks to Antony Mo for external testing and feedback during the XACP development cycle.

Special thanks to the Amiga and ZZ9000 communities for testing, feedback and technical inspiration.
