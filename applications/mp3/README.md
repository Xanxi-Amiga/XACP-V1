# XACP MP3 / MP2 Audio Tools

This directory contains the MP3 / MP2 audio playback tools and integration components developed for the Xanxi XACP platform on the MNT ZZ9000.

The XACP audio path uses the ZZ9000 ARM Cortex-A9 to perform MPEG audio decoding while the Amiga side handles file access, application integration and audio playback through AHI.

The current recommended platform baseline is:

```text
Firmware:  XX19a
Protocol:  XACP v1.6
```

The original XACP MP3 streaming architecture was developed and validated during the earlier **XX16c** firmware line. That work established the compressed-data and PCM shared-memory pipeline still used by the current XACP audio services.

---

## Components

### ZZMP3Play

`ZZMP3Play` is a lightweight command-line MP3 player using the XACP ARM decoding service.

The Amiga streams compressed MPEG audio data to the ZZ9000, where decoding is performed by the ARM. Decoded PCM data is returned through the XACP shared-memory audio path and played through AHI.

Files:

```text
ZZMP3Play
ZZMP3Play.c
```

The source code is included for reference and development.

---

### ZZPlayGUI

`ZZPlayGUI` is the graphical XACP MP3 / MP2 player.

It provides a native AmigaOS interface while using the same ARM-assisted decoding architecture as the command-line player.

Files:

```text
ZZPlayGUI
zzplay-gui.c
```

The source code is included.

ZZPlayGUI was also an important development platform for the XACP streaming architecture, including buffering, refill scheduling and low-overhead communication with the ZZ9000 ARM.

---

### mpega.library

`mpega.library` provides an XACP-backed implementation of the classic Amiga MPEG audio decoding API.

It allows compatible Amiga applications to use the ZZ9000 ARM for MPEG audio decoding instead of performing the decoding entirely on the 68k CPU.

File:

```text
mpega.library
```

The XACP `mpega.library` is an independent implementation written for the XACP project. It does not contain source code from the historical Amiga `mpega.library` implementation.

Additional information is available in:

```text
README_MPEGA_BETA.md
LICENSE_MPEGA.txt
```

Compatibility with legacy applications may vary because some programs depend on implementation-specific behaviour beyond the documented API.

---

### AmigaAMP zz9000.engine

`zz9000.engine` is an external AmigaAMP decoding engine using the XACP ARM MPEG audio path.

File:

```text
zz9000.engine
```

The filename must remain `zz9000.engine`, as this is the external-engine name used by AmigaAMP.

The engine allows AmigaAMP to offload MPEG audio decoding to the ZZ9000 ARM while keeping the normal AmigaAMP user interface and playback environment.

---

## Architecture

The XACP MPEG audio pipeline follows the general model:

```text
MP3 / MP2 file on Amiga
        |
        v
compressed MPEG audio data
        |
        v
XACP shared DDR input buffer
        |
        v
ZZ9000 ARM decoding
        |
        v
PCM shared-memory ring
        |
        v
Amiga 68k
        |
        v
AHI audio playback
```

This architecture substantially reduces the amount of MPEG decoding work performed by the Amiga CPU.

It was one of the first practical demonstrations that the ZZ9000 could be used as a general-purpose application coprocessor rather than only as an RTG graphics card.

The same general XACP principles were subsequently reused for more advanced projects including MIDI synthesis, speech synthesis, video decoding, emulation and Core1 applications.

---

## Firmware compatibility

The current recommended XACP installation is:

```text
BOOT_XX19a.bin
XACP v1.6
matching zz9000.card
```

The MP3 / MP2 service originated in the earlier XX16c firmware line, but the audio path remains available in the current XACP firmware.

Users should therefore normally use the latest public XACP firmware rather than installing an historical XX16c build.

Older firmware and documentation are retained only for:

```text
historical reference
development history
regression testing
reproduction of older configurations
```

After replacing the ZZ9000 firmware or `zz9000.card`, fully power off the Amiga before testing.

Do not assume that unrelated ZZ9000 firmware branches implement the XACP MPEG audio interfaces.

---

## Historical XX16c material

The original MP3 streaming implementation was developed during the **XX16c** firmware generation.

Historical documentation describing that development state is retained under:

```text
archive/XX16c/
```

This may include:

```text
MP3_STATUS.md
ZZMP3Play_README.md
```

These files describe the development status at that time and should not be interpreted as documentation for the current XX19a / XACP v1.6 baseline.

Older experimental `mpega.library` builds are also retained under the archive directory for historical reference.

---

## Source availability

Source availability differs between the components in this directory.

Currently published source includes:

```text
ZZMP3Play.c
zzplay-gui.c
```

Other components may be distributed as binaries according to their respective licensing and distribution terms.

Source availability for one component does not imply that every XACP audio component is open source.

---

## Third-party components

The ARM-side XACP MP3 decoding service uses **minimp3**.

minimp3 is an independent third-party MPEG audio decoder and retains its original upstream licensing terms.

Third-party source code and license notices remain the property of their respective authors and projects.

---

## Relationship to XACP

The MP3 / MP2 audio path is historically important to XACP.

It established several mechanisms later reused throughout the platform:

```text
68k-to-ARM streaming
shared DDR control structures
compressed-data ring buffers
ARM-side processing
PCM output rings
AHI playback
flow control and backpressure
persistent firmware-side services
```

These experiments helped establish the general XACP model of using the ZZ9000 ARM processors as application coprocessors while retaining AmigaOS for user interface, file access, display, input and audio integration.

---

## Current status

The MP3 / MP2 tools remain part of the XACP application collection.

For normal use:

```text
use the current XACP firmware
use the current application binaries
refer to this README for the current baseline
refer to archive/ only for historical development information
```

The current XACP platform baseline is:

```text
Firmware XX19a
XACP v1.6
```

---

## Author

XACP integration, application development and real-hardware validation:

**Xanxi**

2026

ZZ9000 is a product of MNT Research GmbH.

XACP and the applications in this directory are independent software projects and are not official MNT Research products.