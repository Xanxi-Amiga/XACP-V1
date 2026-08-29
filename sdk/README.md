# XACP SDK

This directory contains public headers and helper material for the **eXtended ARM Coprocessor Protocol (XACP)** used by the Xanxi ZZ9000 firmware branch.

The current public baseline is:

```text
Firmware:  XX19a
Protocol:  XACP v1.6
```

Firmware build numbers and XACP protocol versions are separate.

```text
XX19a     = firmware build
XACP v1.6 = protocol / ABI / shared-memory baseline
```

---

## Current SDK

The current memory-map header is:

```text
xacp_memory_map_v1_6.h
```

This header defines the public XACP v1.6 DDR allocation model used by the current firmware and applications.

It includes:

```text
XACP version identifiers
framebuffer-relative addressing helpers
generic XACP / MP3 shared regions
legacy Core1 application region
ZZMPEG stream region
XACP v1.6 ZZMIDI shared-service region
ARM-private ZZMIDI storage
ARM-private service heap
guard regions
compile-time overlap checks
```

New XACP software should include this header instead of independently defining shared DDR addresses.

---

## XACP v1.6 memory model

XACP v1.6 formalizes DDR ownership between:

```text
persistent Core0 firmware services
dynamic Core1 applications
framebuffers
ROM / input data
audio buffers
MIDI event buffers
SoundFont data
temporary upload buffers
service heaps
persistent / save-state data
```

The purpose of the map is to prevent different applications from selecting apparently unused DDR addresses which may already belong to another XACP component.

---

# Shared XACP / Zorro-visible memory

Shared offsets are relative to:

```text
fb = board + 0x00010000
```

The corresponding ARM framebuffer physical base is:

```text
0x00200000
```

Therefore:

```text
ARM physical address = 0x00200000 + fb-relative offset
```

---

## Generic XACP regions

The established generic XACP regions remain available:

```text
fb+0x04000000   XACP command block
fb+0x04002000   XACP stream-control block

fb+0x04100000   MP3 input ring
                size: 512 KB

fb+0x04200000   generic PCM output ring
                size: 1 MB

fb+0x04300000
       ...
fb+0x04600000   legacy Core1 application area

fb+0x05000000
       ...
fb+0x05400000   ZZMPEG Program Stream ring
                size: 4 MB
```

These regions originate from earlier XACP generations and remain documented because current firmware retains the established interfaces.

---

# XACP v1.6 ZZMIDI shared region

XACP v1.6 assigns a dedicated contiguous shared region to the current ZZMIDI service:

```text
fb+0x06000000
       ...
fb+0x06300000
```

Total shared allocation:

```text
3 MB
```

Layout:

```text
fb+0x06000000   ZZMIDI control region
fb+0x06010000   ZZMIDI realtime FIFO region
fb+0x06100000   ZZMIDI PCM ring
fb+0x06200000   ZZMIDI upload window
fb+0x06300000   end of ZZMIDI shared region
```

Allocated extents:

```text
control region :  64 KB
FIFO region    : 960 KB reserved extent
PCM ring       :   1 MB
upload window  :   1 MB
```

The FIFO extent represents the DDR region reserved for the realtime MIDI transport. The actual FIFO data structure and protocol are defined by the ZZMIDI implementation and must remain within this allocation.

The upload window is intentionally much smaller than a complete SoundFont.

Large SoundFonts and MIDI files are transferred through this shared window and copied into ARM-private storage rather than remaining permanently in Zorro-visible staging memory.

---

## ARM view of the shared ZZMIDI region

Because the ARM framebuffer physical base is `0x00200000`, the shared ZZMIDI allocation appears on the ARM side as:

```text
ARM 0x06200000
       ...
ARM 0x06500000
```

This is the same physical DDR as:

```text
fb+0x06000000
       ...
fb+0x06300000
```

Applications must not treat the framebuffer-relative and ARM-physical forms as two independent regions.

---

# ARM-private memory

The following addresses are **ARM absolute physical addresses**.

They are not framebuffer-relative offsets.

---

## Zorro-visible boundary

```text
0x20000000
    ...
0x22000000
```

This range belongs to the existing Zorro-visible mapping and must not be used as ARM-private application storage.

ARM-private allocation begins at:

```text
0x22000000
```

---

## Legacy Core1 private area

```text
0x22000000
    ...
0x23000000
```

Size:

```text
16 MB
```

This range is reserved for the established Core1 application environment and legacy Core1 allocations.

It must not be reused by persistent Core0 services.

---

## ZZMIDI SoundFont storage

```text
0x23000000
    ...
0x25000000
```

Size:

```text
32 MB
```

This is the ARM-private SoundFont storage used by the XACP v1.6 ZZMIDI service.

Unlike XACP v1.5, the full SoundFont is no longer required to occupy a large permanent Zorro-visible staging area.

---

## ZZMIDI MIDI storage

```text
0x25000000
    ...
0x25600000
```

Size:

```text
6 MB
```

This is the ARM-private MIDI / parsed-data storage assigned to ZZMIDI.

---

## ZZMIDI / firmware service heap

```text
0x25600000
    ...
0x2F600000
```

Size:

```text
160 MB
```

This region is reserved for firmware-side service allocations including TinySoundFont / TinyMidiLoader runtime data and related service memory.

---

## Guard region

```text
0x2F600000
    ...
0x30000000
```

Size:

```text
10 MB
```

This region is intentionally left unused by the XACP v1.6 low-memory allocation.

It acts as a boundary between the v1.6 private service pool and memory above `0x30000000`.

Applications must not use it as generic scratch memory.

---

# Summary

```text
SHARED / FB-RELATIVE
────────────────────────────────────────────────────

0x04000000                 generic XACP command
0x04002000                 stream control

0x04100000 - 0x04180000   MP3 ring, 512 KB
0x04200000 - 0x04300000   generic PCM ring, 1 MB

0x04300000 - 0x04600000   legacy Core1 shared/app area

0x05000000 - 0x05400000   ZZMPEG PS ring, 4 MB

0x06000000 - 0x06010000   ZZMIDI control allocation
0x06010000 - 0x06100000   ZZMIDI FIFO allocation
0x06100000 - 0x06200000   ZZMIDI PCM ring, 1 MB
0x06200000 - 0x06300000   ZZMIDI upload window, 1 MB


ARM ABSOLUTE / PRIVATE
────────────────────────────────────────────────────

0x20000000 - 0x22000000   Zorro-visible — do not use privately

0x22000000 - 0x23000000   legacy Core1 private area, 16 MB
0x23000000 - 0x25000000   ZZMIDI SF2 storage, 32 MB
0x25000000 - 0x25600000   ZZMIDI MIDI storage, 6 MB
0x25600000 - 0x2F600000   ZZMIDI/service heap, 160 MB
0x2F600000 - 0x30000000   guard, 10 MB
```

---

# Compatibility

All current public XACP applications should use the latest firmware published in this repository:

```text
XX19a
XACP v1.6
```

Historical firmware releases remain available for development history and regression testing.

The very early Core1 / fractal experiments from early May 2026 predate the current memory-map conventions and are preserved for historical purposes only.

---

# Archived SDK versions

Older SDK definitions are retained under:

```text
archive/
```

In particular:

```text
archive/xacp_memory_map_v1_5.h
```

documents the former XACP v1.5 memory model.

Historical headers should not be used as the basis for new XACP applications.

---

# Important v1.6 rule

Do not introduce a new hard-coded shared DDR address simply because a region appears unused.

New allocations must be made as part of the XACP memory map.

In particular, applications must preserve:

```text
persistent Core0 service regions
Core1 application regions
ZZMIDI shared buffers
ZZMIDI ARM-private storage
firmware-private heaps
guard regions
```

An incompatible change to these allocations requires an explicit XACP ABI/version change.

---

# Planned SDK expansion

Future public SDK material may include:

```text
xacp.h
xacp_opcodes.h
xacp_stream.h
xacp_core1.h
xacp_midi.h
Core1 launch helpers
cache / synchronization helpers
example applications
```

These headers should use `xacp_memory_map_v1_6.h` as their common memory-map reference rather than duplicating DDR constants.

---

## Credits

XACP / Xanxi, 2026.

For use with the MNT ZZ9000 Amiga RTG / ARM platform.
