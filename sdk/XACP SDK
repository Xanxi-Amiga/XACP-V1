# XACP SDK

This directory contains public headers and helper material for the eXtended ARM Coprocessor Protocol used by the Xanxi ZZ9000 firmware branch.

The SDK is intentionally small for now. Its first stable public component is the XACP v1.5 memory-map header.

---

## Current contents

```text
xacp_memory_map_v1_5.h
```

This header defines the XACP v1.5 shared-memory map, firmware/application offsets, ZZMIDI staging areas, private ARM pool boundaries, opcode constants and compile-time sanity checks.

It is the canonical reference for the fixed XACP v1.5 memory layout.

---

## Planned contents

```text
xacp.h
xacp_stream.h
xacp_midi.h
opcode definitions
helper accessors
example applications
```

Future SDK headers should include `xacp_memory_map_v1_5.h` rather than redefining offsets locally.

---

## Versioning note

XACP protocol versions and ZZ9000 firmware build numbers are separate.

```text
XACP v1.5 = protocol / ABI / shared-memory map / documentation baseline

XX16c     = firmware build line for MP3 / MP2 / ZZMPEG
XX18c     = firmware build line for the ZZDoom public baseline
XX18m     = firmware build introducing ZZMIDI and fixing ZZ9000AX cold-boot init
XX19      = firmware build implementing the public XACP v1.5 baseline
```

Do not refer to “firmware v1.5” or “XACP XX19” as if they were the same thing.

XX19 is the firmware build implementing XACP v1.5.

---

## Important memory-map rule

The old ZZMIDI MIDI staging offset must not be reused:

```text
old MIDI staging : fb+0x07000000
```

XACP v1.5 uses:

```text
SF2  staging : fb+0x05800000 - fb+0x07800000 = 32 MB
MIDI staging : fb+0x07800000 - fb+0x07E00000 = 6 MB
```

Full documentation:

```text
../docs/XACP_V1_5_HISTORY_MEMORY_MAP_PART2.md
```
