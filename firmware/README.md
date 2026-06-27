
# XACP Firmware Builds

This directory contains Xanxi/XACP firmware builds for the MNT ZZ9000.

Firmware build numbers and XACP protocol versions are separate.

```text
XACP v1.5 = protocol / ABI / shared-memory map / documentation baseline

XX16c     = firmware build line for MP3 / MP2 / ZZMPEG
XX18c     = firmware build line for the ZZDoom public baseline
XX18m     = firmware build introducing ZZMIDI and fixing ZZ9000AX cold-boot init
XX19      = firmware build implementing the public XACP v1.5 baseline
```

Do not refer to “firmware v1.5” or “XACP XX19” as if they were the same thing.

Correct wording:

```text
XX19 is the firmware build implementing XACP v1.5.
```

---

## Firmware list

### XX16c — MP3 / MP2 / ZZMPEG baseline

Recommended file name:

```text
BOOT_XX16c_XACP.bin
```

Validated work:

```text
ARM MP3 decoding
MP3 streaming pipeline
MP2 audio support
ZZPlayGUI / XACP MP3 player path
AmigaAMP external engine: zz9000.engine
mpega.library XACP integration work
ZZMPEG MPEG-1 / MP2 playback path
JuliaV2 / early Core1 validation path
```

XX16c is the first major public XACP multimedia baseline.

---

### XX18c — ZZDoom baseline

Recommended file name:

```text
BOOT_XX18c.bin
```

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

XX18c is the ZZDoom-era public firmware baseline.

Use this firmware line with the matching ZZDoom release files and matching `zz9000.card`.

---

### XX18m — first ZZMIDI firmware line and ZZ9000AX cold-boot fix

Recommended file name:

```text
BOOT_XX18m.bin
```

Validated work:

```text
initial ZZMIDI / SoundFont firmware path
OP_MIDI_SF2 = 0x0120 groundwork
ZZMIDI player / daemon integration work
ZZ9000AX cold-boot initialization fix
```

The ZZ9000AX cold-boot fix is historically important.

Earlier builds could require a reset after cold power-on before the ZZ9000AX initialized correctly. XX18m fixed that initialization issue.

---

### XX19 — XACP v1.5 public baseline

Recommended file name:

```text
BOOT_ZZ9000_XX19_XACP_v1.5.bin
```

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

## Compatibility matrix

| Firmware | Main baseline      | Main applications                                                     |
| -------- | ------------------ | --------------------------------------------------------------------- |
| XX16c    | MP3 / MP2 / ZZMPEG | ZZPlayGUI, AmigaAMP `zz9000.engine`, `mpega.library`, ZZMPEG, JuliaV2 |
| XX18c    | ZZDoom             | ZZDoom320, ZZDoom640                                                  |
| XX18m    | first ZZMIDI line  | ZZMIDI player / daemon, ZZ9000AX cold-boot fix                        |
| XX19     | XACP v1.5          | ZZMIDI v0.5, fixed v1.5 memory map, future XACP v1.5 services         |

---

## Installation notes

After replacing firmware or `zz9000.card`, fully power off the Amiga before testing.

Do not mix arbitrary firmware, `zz9000.card` files and application binaries from unrelated packages.

Some applications depend on matching firmware features, including:

```text
Core1 launch
deferred PAN
audio rings
XACP opcode handling
XACP v1.5 memory map
```

A mismatched firmware / card / application set may boot but can cause:

```text
black screen
tearing
missing audio
missing XACP service
wrong memory offsets
crash or lock-up during ARM service calls
```

---

## XACP v1.5 memory-map note

XX19 implements the XACP v1.5 shared-memory baseline.

The most important correction is the ZZMIDI staging map:

```text
SF2  staging : fb+0x05800000 - fb+0x07800000 = 32 MB
MIDI staging : fb+0x07800000 - fb+0x07E00000 = 6 MB
```

The old MIDI staging location at:

```text
fb+0x07000000
```

must not be reused.

Full documentation:

```text
docs/XACP_V1_5_HISTORY_MEMORY_MAP_PART2.md
```

---

## Release naming

For the public XACP v1.5 release, use:

```text
BOOT_ZZ9000_XX19_XACP_v1.5.bin
```

GitHub release tag:

```text
v1.5.0
```

Suggested release title:

```text
XACP v1.5 — XX19 public multimedia baseline
```

---


