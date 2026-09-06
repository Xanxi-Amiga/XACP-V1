# XACP v1.6

**eXtended ARM Coprocessor Protocol for the MNT ZZ9000 Amiga RTG board.**

XACP is an independent software platform built around the ARM Cortex-A9 processors and DDR memory of the ZZ9000.

It allows Amiga applications to offload CPU-intensive workloads to the ZZ9000 while AmigaOS remains responsible for system integration such as RTG display, input, files, GUI, AHI audio and CAMD MIDI.

The current public baseline is:

```text
Firmware:  XX19a
Protocol:  XACP v1.6
```

Firmware build numbers and XACP versions are deliberately separate.

**XX19a is the firmware build. XACP v1.6 is the protocol, ABI and shared-memory baseline implemented by that firmware.**

---

## XACP architecture

The ZZ9000 contains a Xilinx Zynq-7020 with two ARM Cortex-A9 cores and its own DDR memory.

XACP uses this hardware in two complementary ways.

### Core0 services

Persistent services can run as part of the ZZ9000 firmware and expose operations to Amiga-side applications through XACP commands and shared memory.

Examples include:

```text
MP3 / MP2 decoding
MIDI / SoundFont synthesis
shared audio buffers
firmware-side multimedia services
```

### Core1 applications

The second ARM core can be dynamically loaded with complete application engines.

This model is used for workloads such as:

```text
ZZDoom
ZZPicoDrive
ZZPPC
Julia / fractal rendering
video decoding
benchmarks
experimental game engines
```

The ARM performs the heavy computation while AmigaOS remains the host operating environment.

---

## XACP v1.6

XACP v1.6 formalizes the shared DDR layout used simultaneously by firmware services and dynamically loaded Core1 applications.

Earlier XACP versions grew progressively as new applications were added. Individual projects therefore reserved DDR areas independently, creating the possibility of collisions between persistent firmware services, Core1 application data, framebuffers, ROM areas, save buffers and firmware-private memory.

XACP v1.6 turns this shared DDR layout into an explicit part of the ABI.

The v1.6 baseline provides:

```text
defined ownership of shared DDR regions
separation of Core0 service memory and Core1 application memory
protected ZZMIDI SoundFont and MIDI staging areas
protected firmware-private memory
defined Core1 blob and application work areas
separation of framebuffers, ROM/data buffers and persistent save areas
room for future XACP services without silently reusing existing regions
```

This is an architectural change rather than an application-specific workaround.

New XACP applications should use the v1.6 memory-map definitions rather than introducing undocumented DDR addresses.

---

## Current firmware

The current firmware is:

```text
firmware/BOOT_XX19a.bin
```

XX19a implements the XACP v1.6 baseline while retaining the established XACP multimedia and Core1 infrastructure.

Older firmware builds are preserved for historical and compatibility purposes in:

```text
firmware/archive/
```

See:

```text
firmware/README.md
```

for firmware-specific information.

---

## Applications

XACP has evolved from an experimental coprocessor protocol into a general ARM acceleration platform for the ZZ9000.

Current and historical projects in this repository include:

| Project                       | Role                                                         |
| ----------------------------- | ------------------------------------------------------------ |
| **ZZMIDI**                    | SoundFont MIDI synthesis on the ZZ9000 ARM with AHI playback |
| **ZZPicoDrive**               | Mega Drive / Genesis emulation using ZZ9000 Core1            |
| **ZZPPC**                     | Experimental PPC32/FPU execution on ZZ9000 Core1             |
| **ZZDoom**                    | Doom engine running on ZZ9000 Core1                          |
| **ZZRastan**                  | Rastan arcade engine running on ZZ9000 Core1 |
| **ZZSpeech**                  | Speech synthesis accelerated by the ZZ9000                   |
| **ZZ-MPEG**                   | MPEG-1 video and MP2 audio playback                          |
| **ZZPlayGUI / MP3 tools**     | ARM-accelerated MP3 / MP2 decoding                           |
| **zz9000.engine**             | AmigaAMP external engine using XACP                          |
| **mpega.library integration** | MPEGA-compatible ARM decode path                             |
| **ZZBench GUI**               | 68k, ARM and memory-bandwidth benchmarking                   |
| **JuliaV2**                   | Core1 launch, rendering and clean-return validation          |
| **Legacy fractal demos**      | Early XACP / Core1 graphical experiments                     |

Application-specific documentation and release material are stored under:

```text
applications/
```

All current public XACP applications are compatible with the latest firmware published in this repository.

Historical firmware builds are retained for reference, regression testing and documentation of the platform's evolution, but they are not required for normal use of current applications.

The only exception is the set of very early fractal / Core1 demonstration programs from May 2026. These predate the current XACP architecture and are preserved for historical purposes only; they should not be considered part of the current compatibility baseline.


---

## ZZMIDI

ZZMIDI demonstrates the persistent-service side of XACP.

TinySoundFont runs on the ZZ9000 ARM side and performs SoundFont synthesis, while the Amiga application handles the user interface, files and AHI audio playback.

The current ZZMIDI generation supports SoundFont playback using the shared XACP MIDI service and the protected ZZMIDI memory regions defined by the protocol.

The XACP v1.6 memory model ensures that these persistent firmware resources can coexist with the memory requirements of Core1 applications without undocumented overlap.

See:

```text
applications/ZZMIDI/
```

---

## ZZPicoDrive

ZZPicoDrive demonstrates the complete Core1 application model.

The emulation engine runs on ZZ9000 ARM Core1 while the Amiga side handles application launch, RTG integration, input, AHI audio, configuration and file operations.

See:

```text
applications/emulators/ZZPicodrive/
```

---

## ZZPPC

ZZPPC is an experimental PowerPC execution proof of concept for the ZZ9000.

On 6 September 2026, the original and unmodified FlashMandelWOS application rendered a Mandelbrot fractal on an Amiga 4000/060 equipped with a ZZ9000 and no physical PowerPC accelerator. Its original PPC32/FPU workload was interpreted by a custom engine running on the ARM Cortex-A9 Core1 and communicating with the Amiga through XACP.

ZZPPC remains an application-specific research prototype. It is not yet a complete PowerPC emulator or WarpOS implementation.

See:

```text
applications/emulators/ZZPPC/

---

## ZZDoom

ZZDoom is based on `doomgeneric`.

The Doom engine runs on ZZ9000 ARM Core1 while AmigaOS provides RTG display, keyboard input, AHI sound effects, MIDI integration, file access and savegames.

ZZDoom was one of the applications that established the dynamic Core1 execution model later reused by other XACP projects.

---

## Repository structure

```text
applications/   Amiga applications, Core1 programs and release material
docs/           XACP protocol, architecture and historical documentation
firmware/       Current firmware, firmware documentation and archive
sdk/            Headers and material for XACP developers
CHANGELOG.md    Project history
README.md       Current XACP overview
```

Historical material is retained where useful, but the top-level directories are intended to represent the current XACP structure.

---

## Versioning

XACP protocol versions and firmware build numbers are separate.

Examples:

```text
XX16c     early MP3 / MP2 / ZZMPEG firmware line
XX18c     public ZZDoom firmware baseline
XX18m     first ZZMIDI firmware line
XX19      XACP v1.5 public baseline
XX19a     XACP v1.6 public baseline
```

Therefore:

```text
XX19a = firmware build
XACP v1.6 = protocol / ABI / shared-memory baseline
```

They should not be described interchangeably as "firmware v1.6" or "XACP XX19a".

---

## Compatibility

Do not assume that binaries from unrelated XACP generations are interchangeable.

For released software, the safest combination is always:

```text
application
firmware
zz9000.card
documentation
```

from a matching or explicitly validated release generation.

After replacing the ZZ9000 firmware or `zz9000.card`, perform a complete power-off before testing.

Incorrect combinations can result in missing XACP services, incorrect shared-memory offsets, silent audio, RTG corruption, crashes or lock-ups.

---

## Documentation

Historical XACP documentation is retained under:

```text
docs/
```

The original XACP V1 material remains available for reference.

XACP v1.5 documentation records the transition to the large SoundFont / MIDI shared-memory layout.

XACP v1.6 supersedes v1.5 as the current shared-memory and application-allocation baseline.

The SDK directory contains definitions intended to prevent applications from independently assigning conflicting shared DDR regions.

---

## Source availability

XACP contains software with different origins and licensing requirements.

Firmware source publication is handled together with the applicable third-party source and license notices.

Application source availability is independent from firmware source availability. Some Xanxi applications are distributed as binaries and documentation only, while third-party components retain their respective upstream licenses.

Each application or source package should therefore be considered according to the license information distributed with that specific component.

The corresponding XX19a firmware source is available under:

firmware/source/XX19a/

---

## Development

XACP continues to be developed as a general-purpose acceleration platform for Amiga systems equipped with the ZZ9000.

Areas under active development include additional Core1 applications, emulation, game engines, multimedia services and an SDK intended to make the XACP execution model reusable by third-party Amiga developers.

---

## Credits

XACP / Xanxi, 2026.

Thanks to the MNT ZZ9000 project and to the Amiga and ZZ9000 communities for the hardware platform, testing, feedback and technical discussion.

Third-party software included or used by individual XACP projects remains credited to its respective authors and retains its original license.
