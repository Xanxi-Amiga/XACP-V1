# XACP V1.5 — Firmware History and Multimedia Memory Map, Part 2

This document continues the original `XACP_V1_NOTICE.md`.

XACP means **eXtended ARM Coprocessor Protocol**.

It is the shared protocol, ABI and memory-map convention used by the Xanxi ZZ9000 firmware branch and its Amiga-side applications.

Important distinction:

```text
XACP v1.5   = protocol / ABI / shared-memory map / documentation baseline

XX16c       = firmware build line for MP3 / MP2 / ZZMPEG / early Core1 work
XX18c       = firmware build line for the ZZDoom public baseline
XX18m       = firmware build introducing ZZMIDI and fixing ZZ9000AX cold-boot init
XX19        = firmware build implementing the public XACP v1.5 baseline
```

This document records the validated evolution from XX16c to XX19 and freezes the XACP v1.5 multimedia memory map.

---

## 1. Why this Part 2 exists

The original XACP V1 notice describes the first shared-DDR protocol baseline:

```text
opcode dispatch
shared DDR command blocks
MP3 streaming
status / error reporting
endianness rules
cache-coherence rules
```

Since then, XACP has grown from an ARM-assisted MP3 path into a complete ZZ9000 multimedia coprocessor framework.

The current XACP line covers:

```text
MP3 playback
MP3 streaming
MP2 audio
ZZPlayGUI
AmigaAMP zz9000.engine
mpega.library XACP integration
ZZMPEG
JuliaV2
ZZDoom
ZZBench GUI
ZZMIDI
shared DDR services
Core1 application launch
documented XACP v1.5 memory map
```

This Part 2 documents the current validated baseline.

---

## 2. Versioning model

XACP protocol versions and ZZ9000 firmware build numbers are separate.

Do not refer to “firmware v1.5” or “XACP XX19” as if they were the same thing.

Correct wording:

```text
XACP v1.5 is the protocol and memory-map baseline.

XX19 is the firmware build implementing XACP v1.5.
```

Public release naming:

```text
BOOT_ZZ9000_XX19_XACP_v1.5.bin
```

GitHub release tag:

```text
v1.5.0
```

---

## 3. Addressing conventions

Three address spaces must not be confused.

### 3.1 Amiga board-relative address

```text
board = cd->cd_BoardAddr
```

This is the Zorro III board base as seen by the Amiga.

### 3.2 Shared DDR framebuffer-relative address

```text
fb = board + 0x00010000
```

Most XACP shared-memory regions are documented as offsets from `fb`.

### 3.3 ARM absolute physical address

The private ARM pool uses absolute ARM physical addresses.

Example:

```text
0x22000000 - 0x30000000
```

These are not fb-relative offsets.

---

## 4. XACP v1.5 shared DDR memory map

All addresses below are relative to:

```text
fb = board + 0x00010000
```

### 4.1 Core XACP / MP3 / MP2 area

```text
0x04000000  XACP command block / historical command area
0x04002000  MP3 / MP2 StreamControl

0x04100000  MP3 compressed input ring
0x04180000  end MP3 input ring, 512 KB

0x04200000  MP3 / MP2 PCM output ring
0x04300000  end PCM output ring, 1 MB
```

### 4.2 Core1 / application transition area

```text
0x04300000  reserved / Core1 application area depending on application
0x04600000  ZZMIDI CAMD FIFO / event staging
```

This area has been used by Core1 applications and must be handled per application. It is not a generic scratch zone.

### 4.3 ZZMIDI audio and staging area

```text
0x04800000  ZZMIDI PCM ring
0x04900000  end ZZMIDI PCM ring, 1 MB

0x04900000  reserved XACP / future use
0x05800000  ZZMIDI SF2 staging start

0x07800000  ZZMIDI SF2 staging end / MIDI staging start
0x07E00000  MIDI staging end / ARM firmware C heap start
```

The key XACP v1.5 correction is:

```text
SF2  staging : fb+0x05800000 - fb+0x07800000 = 32 MB
MIDI staging : fb+0x07800000 - fb+0x07E00000 = 6 MB
```

The old MIDI staging location at `fb+0x07000000` must not be reused.

---

## 5. ZZMIDI v1.5 memory correction

The previous ZZMIDI staging map placed the MIDI file too early:

```text
old SF2  : 0x05800000 - 0x07800000
old MIDI : 0x07000000 - 0x07800000
```

This meant that any SoundFont larger than approximately 24 MB could overlap the MIDI staging area.

XACP v1.5 fixes this:

```text
new SF2  : 0x05800000 - 0x07800000 = 32 MB
new MIDI : 0x07800000 - 0x07E00000 = 6 MB
```

The hard end of the ZZMIDI staging area is:

```text
0x07E00000
```

No SF2, MIDI or future staging buffer may cross that limit.

---

## 6. Private ARM pool

Some XACP services need a private ARM-side memory pool outside the fb-relative staging map.

The XACP v1.5 private ARM pool is:

```text
0x22000000 - 0x26000000  raw private copy area, 64 MB
0x26000000 - 0x30000000  TSF / TML / service heap, 160 MB
```

The following range must not be used for this purpose:

```text
0x20000000 - 0x22000000
```

Reason:

```text
0x20000000 - 0x22000000 can overlap AmigaOS-visible Z3 memory.
```

Future ARM services must not silently allocate from that range.

---

## 7. Firmware history

### 7.1 XX16c — MP3 / MP2 / ZZMPEG / early Core1 baseline

XX16c is the first major public XACP multimedia baseline.

Validated work:

```text
ARM MP3 decoding
MP3 streaming pipeline
MP2 audio support
ZZPlayGUI / XACP MP3 player path
AmigaAMP external engine: zz9000.engine
mpega.library XACP integration work
ZZMPEG MPEG-1 / MP2 playback path
early Core1 application work
```

This is the firmware line where XACP became practically useful as a real Amiga multimedia offload path.

### 7.2 JuliaV2 — Core1 validation application

JuliaV2 is part of the Core1 validation history.

It validated the application model later reused by heavier Core1 projects:

```text
Core1 launch
shared DDR communication
ARM-side rendering
Amiga-side display integration
clean STOP / return path
```

JuliaV2 is important because it proved that Core1 could run a self-contained workload while AmigaOS remained responsible for launch, synchronization and display integration.

### 7.3 ZZMPEG — MPEG-1 / MP2 multimedia path

ZZMPEG validates the Core1 multimedia model for video playback.

Validated work:

```text
MPEG-1 Program Stream input
video decode on ARM/Core1
Workbench window output
Picasso96 fullscreen output
MP2 audio through XACP
AHI playback on the Amiga side
STOP / return path
```

ZZMPEG belongs to the XACP multimedia line and remains one of the main proof points for ARM-assisted playback on the ZZ9000.

### 7.4 XX18c — ZZDoom baseline

XX18c is the ZZDoom-era public firmware baseline.

Validated work:

```text
Core1 application launch
shared DDR command path
deferred PAN / fullscreen RTG synchronization
ZZDoom 320x200
ZZDoom 640x400
AHI sound effects
CAMD external MIDI path
save/load through AmigaOS files
clean STOP / return path
```

ZZDoom is a `doomgeneric`-based Doom port where the Doom engine runs on the ZZ9000 ARM Core1 while AmigaOS handles display, keyboard, AHI, CAMD, file access and savegames.

### 7.5 ZZScanlinesV2 — related display utility

ZZScanlinesV2 is part of the broader Xanxi ZZ9000 application history.

It is not a separate XACP memory-map consumer, but it belongs in the release history because it was part of the public ZZ9000 application set developed alongside the XACP multimedia work.

### 7.6 ZZBench GUI — CPU and memory-bandwidth validation

ZZBench GUI validates the compute and memory-bandwidth model behind XACP.

Validated work:

```text
68k Dhrystone / Whetstone
ARM Core1 Dhrystone / Whetstone
Amiga Chip RAM bandwidth
Amiga Fast RAM bandwidth
ZZ9000 DDR via ARM Core1 bandwidth
ZZ9000 DDR via Zorro III bandwidth
```

ZZBench GUI documents why XACP offload is useful: the ARM reaches ZZ9000 DDR much faster than the 68k reaches the same memory over Zorro III.

### 7.7 XX18m — first ZZMIDI firmware line and ZZ9000AX cold-boot fix

XX18m is the firmware line that introduced the ZZMIDI work into the public XACP tree.

Validated work:

```text
initial ZZMIDI / SoundFont firmware path
OP_MIDI_SF2 = 0x0120 groundwork
ZZMIDI player / daemon integration work
ZZ9000AX cold-boot initialization fix
```

The ZZ9000AX cold-boot fix is historically important.

Earlier builds could require a reset after cold power-on before the ZZ9000AX initialized correctly. XX18m fixed that initialization issue.

### 7.8 XX19 — XACP v1.5 public baseline

XX19 is the public firmware baseline implementing the XACP v1.5 multimedia memory map.

Validated work:

```text
ZZMIDI SoundFont playback path
OP_MIDI_SF2 = 0x0120 implemented
SF2 staging fixed at 32 MB
MIDI staging moved after SF2
MIDI offset fixed at fb+0x07800000
MIDI staging size fixed at 6 MB
private ARM pool documented
large SoundFont playback stabilized
persistent SF2 across Next / Previous
MIDI/TML reload without reparsing unchanged SF2
```

XX19 replaces the internal XX18t development naming for the public XACP v1.5 release.

---

## 8. Application directory map

The current application tree is:

```text
applications/mp3/          MP3 tools, AmigaAMP zz9000.engine, mpega.library work
applications/mpegplayer/   ZZMPEG
applications/ZZDoom/       ZZDoom
applications/benchmarks/   ZZBench GUI
applications/ZZMIDI/       ZZMIDI
```

JuliaV2 is part of the Core1 validation history and should remain documented even if it is not grouped as a final standalone application package in the same way as ZZDoom or ZZMPEG.

---

## 9. Opcode status in XACP v1.5

The original XACP V1 notice listed several opcode ranges as reserved.

In XACP v1.5, the important implemented multimedia opcodes include:

```text
0x0003  OP_MP3_DECODE      ARM MP3 decode
0x0004  OP_STREAM_OPEN     MP3 / MP2 stream open
0x0005  OP_STREAM_CLOSE    MP3 / MP2 stream close
0x0120  OP_MIDI_SF2        ZZMIDI SoundFont service
0x0303  OP_CORE1_RESET     Core1 reset / recovery path
```

The exact application-level subcommands are service-specific and are documented by the corresponding application sources and headers.

---

## 10. Cache and endian rules

The original XACP V1 cache and endian rules still apply.

The Amiga 68k is big-endian.

The ARM Cortex-A9 is little-endian.

All shared DDR structures must have a clearly defined endian convention.

Rules:

```text
68k -> ARM shared data:
  68k writes data to shared DDR.
  ARM invalidates the corresponding D-cache range before reading.

ARM -> 68k shared data:
  ARM writes data to shared DDR.
  ARM flushes the corresponding D-cache range after writing.

FIFO / ring structures:
  ownership of read/write pointers must be explicit.
  producer owns write pointer.
  consumer owns read pointer.
```

For ZZMIDI and other large staging buffers, never assume cache coherency implicitly. Cache operations are part of the protocol.

---

## 11. XACP v1.5 rules for future work

Do not move MIDI staging back to:

```text
0x07000000
```

Do not place new service staging inside:

```text
0x05800000 - 0x07E00000
```

This range is reserved for ZZMIDI SF2 and MIDI staging.

Do not use:

```text
0x20000000 - 0x22000000
```

as a private ARM heap or private ARM pool.

Do not assume that ZZDoom, ZZMPEG, ZZMIDI, ZZSpeech, PicoDrive, ZZReSID or ZZQuake can all run simultaneously unless that exact combination has been tested.

Long-running services must not keep the 68k blocked in the immediate Zorro opcode write path.

Long operations should ACK quickly and then continue through:

```text
deferred firmware state machines
Core1 application logic
shared DDR progress/status fields
pollable done_seq / status fields
```

---

## 12. Roadmap beyond XACP v1.5

The following projects build on the XACP v1.5 baseline:

```text
ZZMIDI CAMD realtime stabilization
ZZSpeech using Flite
PicoDrive MVP on Core1
ZZReSID on Core1
ZZQuake memory strategy
Further ARM/Core1 multimedia services
```

These projects are not required for the current XACP v1.5 release.

ZZQuake is the most likely future project to require a dedicated memory strategy.

The other planned services should not require a global remap of XACP v1.5 if they respect the private ARM pool and avoid the ZZMIDI staging area.

---

## 13. Summary

XACP v1.5 is the current validated multimedia baseline for the Xanxi ZZ9000 firmware branch.

It consolidates the work from:

```text
XX16c   MP3 / MP2 / ZZMPEG / early Core1
JuliaV2 Core1 validation
XX18c   ZZDoom
XX18m   ZZMIDI introduction and ZZ9000AX cold-boot fix
XX19    public XACP v1.5 memory-map baseline
```

The most important technical change is the corrected ZZMIDI memory map:

```text
SF2  staging : fb+0x05800000 - fb+0x07800000 = 32 MB
MIDI staging : fb+0x07800000 - fb+0x07E00000 = 6 MB
```

This document is the reference for future XACP v1.5-compatible work.

