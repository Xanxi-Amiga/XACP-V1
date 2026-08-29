# XACP MP3 / MP2 Audio Tools

This directory contains the XACP audio tools and integration files for the Xanxi ZZ9000 firmware branch.

This audio path belongs to the XX16c firmware baseline and remains part of the XACP v1.5 history.

---

## Contents

This directory may contain:

```text
ZZPlayGUI          XACP MP3 player / GUI path
zz9000.engine      AmigaAMP external engine for XACP playback
mpega.library      XACP-backed mpega.library work
support files      icons, docs or helper files
```

Exact filenames may vary between releases.

---

## Firmware baseline

Primary firmware line:

```text
XX16c
```

Related protocol baseline:

```text
XACP V1 / XACP v1.5 history
```

XX16c validated the first practical public XACP audio path:

```text
ARM MP3 decoding
MP3 streaming pipeline
MP2 audio support
AHI playback on the Amiga side
AmigaAMP external engine path
mpega.library XACP integration work
ZZMPEG MPEG-1 / MP2 playback path
```

---

## ZZPlayGUI

ZZPlayGUI is the standalone XACP MP3 playback GUI.

It uses the ZZ9000 ARM side for decoding and the Amiga side for integration and audio playback.

Validated path:

```text
MP3 file on Amiga side
compressed data / stream control through XACP shared DDR
ARM-side MP3 decoding
PCM output ring
AHI playback on AmigaOS
```

---

## AmigaAMP external engine

The AmigaAMP engine file must be named:

```text
zz9000.engine
```

This is the recommended AmigaAMP filename for XACP playback.

The engine allows AmigaAMP to use the ZZ9000 ARM decoding path instead of relying on pure 68k MP3 decoding during playback.

Install it in the AmigaAMP engine directory according to the usual AmigaAMP external-engine installation method.

---

## mpega.library

The XACP-backed `mpega.library` work provides compatibility with software expecting a MPEGA-style decoding interface while using the ZZ9000 ARM side for the expensive audio decode path.

This belongs to the same XX16c audio baseline as ZZPlayGUI and the AmigaAMP engine.

### License and provenance

The XACP `mpega.library` is an independent implementation written from
scratch for the XACP project. It does not contain code from the historical
68k mpega.library implementation.

It is distributed as closed-source freeware.

See `LICENSE_MPEGA.txt` for details.

---

## Shared-memory areas

The historical MP3 / MP2 audio path uses the XACP stream-control and ring-buffer areas:

```text
fb+0x04002000  MP3 / MP2 StreamControl

fb+0x04100000  MP3 compressed input ring, 512 KB
fb+0x04200000  MP3 / MP2 PCM output ring, 1 MB
```

All offsets are relative to:

```text
fb = board + 0x00010000
```

---

## Compatibility notes

Use matching firmware, `zz9000.card` and application binaries from the same release line when possible.

Do not mix unrelated firmware and application builds unless the combination has been tested.

After replacing firmware or `zz9000.card`, fully power off the Amiga before testing.

---

## Relation to XACP v1.5

The MP3 / MP2 audio path predates XACP v1.5, but remains part of the validated XACP application line.

XACP v1.5 does not remove the XX16c audio model. It documents and extends the shared-memory layout used by later services such as ZZMIDI.
