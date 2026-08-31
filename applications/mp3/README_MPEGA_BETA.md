# XACP mpega.library

`mpega.library` is an XACP-backed MPEG audio decoding library for AmigaOS systems equipped with an MNT ZZ9000.

It provides compatibility with applications using the classic Amiga `mpega.library` API while offloading MPEG audio decoding to the ZZ9000 ARM through the XACP audio pipeline.

The current recommended XACP platform baseline is:

```text
Firmware:  XX19a
Protocol:  XACP v1.6
```

The original XACP `mpega.library` implementation was developed and validated during the earlier **XX16c** firmware line.

---

## Purpose

The objective of the XACP `mpega.library` is to allow existing Amiga software using the traditional MPEG audio decoding API to benefit from ARM-assisted decoding without requiring application-specific XACP support.

The general path is:

```text
Amiga application
       |
       v
mpega.library API
       |
       v
XACP streaming interface
       |
       v
ZZ9000 ARM MPEG audio decoder
       |
       v
decoded PCM audio
       |
       v
Amiga application / audio output
```

This reduces the MPEG decoding workload performed by the Amiga 68k CPU.

---

## Implementation

The XACP `mpega.library` is an independent implementation written for the XACP project.

It does not contain source code from the historical Amiga `mpega.library` implementation.

The library provides an implementation of the established `mpega.library` API while using the asynchronous XACP streaming architecture internally.

Because some classic Amiga applications rely on implementation-specific behaviour beyond the documented API, compatibility may differ between programs.

---

## Current implementation status

The XACP decoding pipeline is operational and has been used successfully for practical MPEG audio playback.

Implemented capabilities include:

```text
ARM/XACP MPEG audio decoding
shared-memory streaming
repeated playback
long playback sessions
reduced 68k decoding workload
legacy mpega.library application compatibility
```

The current library should still be considered an integration / compatibility implementation rather than a guaranteed drop-in replacement for every historical `mpega.library` use case.

---

## Application compatibility

The following compatibility observations originate from testing performed during development of the XACP `mpega.library`.

They are retained because they document useful real-world behaviour of classic applications using the library.

### SongPlayer

Status:

```text
Supported / working well
```

SongPlayer provided the best compatibility among the tested third-party applications using the legacy `mpega.library` interface.

Playback, seeking and timing were functional and stable during testing.

---

### AmigaAMP

Status:

```text
Audio playback working
Some legacy API / metadata compatibility differences
```

Audio playback through the XACP `mpega.library` works.

During development testing, some metadata and interface elements were incomplete or inaccurate when using the legacy `mpega.library` path, including:

```text
total duration display
bitrate display
slider / timeline behaviour
```

These differences appear to be related to implementation-specific expectations in the historical AmigaAMP `mpega.library` interface rather than to the underlying XACP MPEG decoder.

For AmigaAMP, the dedicated XACP external engine:

```text
zz9000.engine
```

is also available and provides a more direct integration path.

---

### HippoPlayer

Status:

```text
Experimental / not recommended through mpega.library
```

During development testing, HippoPlayer showed unstable behaviour and CPU / scheduling spikes when used with the asynchronous XACP streaming model.

Compatibility work may be revisited in the future, but HippoPlayer should not currently be considered a validated `mpega.library` client for XACP.

---

## Reference XACP player

The native XACP player remains:

```text
ZZPlayGUI
```

Unlike applications using the compatibility `mpega.library` layer, ZZPlayGUI communicates directly with the XACP streaming backend.

This allows XACP-specific optimizations for:

```text
buffering
stream refill
backpressure
PCM scheduling
reduced refill overhead
asynchronous ARM decoding
```

For this reason, ZZPlayGUI remains the reference implementation for testing the XACP MPEG audio pipeline itself.

Problems observed only through a legacy `mpega.library` client do not necessarily indicate a problem in the underlying ARM decoder or XACP audio service.

---

## Firmware history

The XACP `mpega.library` was originally developed during the:

```text
XX16c
```

firmware generation.

XX16c established the practical XACP MPEG audio streaming architecture used by:

```text
ZZMP3Play
ZZPlayGUI
mpega.library
zz9000.engine
later XACP multimedia applications
```

Historical XX16c documentation is retained under:

```text
archive/XX16c/
```

The current recommended platform is:

```text
Firmware XX19a
XACP v1.6
```

Users should normally use the current XACP firmware rather than installing XX16c specifically for `mpega.library`.

---

## XACP audio architecture

The library uses the same basic XACP MPEG audio architecture as the other audio tools:

```text
compressed MPEG audio
        |
        v
shared DDR input buffer
        |
        v
ZZ9000 ARM decoder
        |
        v
PCM shared-memory ring
        |
        v
Amiga-side client
```

This streaming model allows ARM decoding and Amiga application execution to proceed asynchronously.

The architecture was originally developed for MP3 / MP2 playback and became one of the foundations of the broader XACP platform.

---

## Performance

The main purpose of the XACP implementation is to move MPEG audio decoding away from the Amiga CPU.

Actual CPU usage depends on:

```text
host Amiga CPU
client application
buffering behaviour
AHI configuration
task scheduling
filesystem and system activity
```

Historical XX16c measurements and tuning information are retained in the archived development documentation and should not be interpreted as guaranteed performance figures for every current configuration.

---

## Limitations

The main compatibility limitation is the age and variability of the classic `mpega.library` ecosystem.

Some applications may depend on:

```text
undocumented behaviour
specific timing assumptions
particular metadata handling
synchronous decoding behaviour
implementation-specific return values
```

The XACP backend is fundamentally asynchronous because decoding is performed by the ZZ9000 ARM and communicated through shared memory.

Applications making assumptions specific to older synchronous 68k implementations may therefore require additional compatibility handling.

---

## Recommended use

For direct XACP MPEG audio playback:

```text
ZZPlayGUI
```

is the recommended reference implementation.

For existing Amiga software requiring the traditional API:

```text
mpega.library
```

provides the compatibility layer.

For AmigaAMP:

```text
zz9000.engine
```

provides a dedicated XACP integration path.

---

## Distribution

The current `mpega.library` binary is provided in this directory.

Its distribution terms are documented in:

```text
LICENSE_MPEGA.txt
```

Older experimental builds are retained under:

```text
archive/
```

for historical reference only.

---

## Source code status

The current XACP `mpega.library` source code is not distributed.

The library is an independent XACP implementation and is distributed according to the terms described in `LICENSE_MPEGA.txt`.

Source availability for other XACP audio applications does not imply source availability for `mpega.library`.

---

## Author

XACP `mpega.library` implementation, integration and real-hardware validation:

**Xanxi**

2026

ZZ9000 is a product of MNT Research GmbH.

XACP and the XACP `mpega.library` are independent software projects and are not official MNT Research products.