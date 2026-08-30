# ZZDoom for ZZ9000 V1.1

**ZZDoom** is a Doom port for Amiga systems equipped with the **MNT ZZ9000**.

It is not a conventional 68k Doom port. The Doom engine runs on the ZZ9000 ARM Cortex-A9 Core1, while AmigaOS handles system integration including RTG display, input, AHI sound, CAMD MIDI music, file access and savegames.

ZZDoom is based on **DoomGeneric and the original Doom source code**, adapted for the ZZ9000 Core1 architecture.

---

## Source release

The source code required to reproduce the ARM Core1 binaries distributed with ZZDoom is available in the `source/` directory.

The historical release blobs have been successfully reproduced **byte-for-byte** from this source tree.

### ZZDoom320 ARM Core1 blob

```text
Size: 381396 bytes
MD5:  8adf2938b3a7823d2d7005e0c9b8db8f
```

### ZZDoom640 ARM Core1 blob

```text
Size: 381396 bytes
MD5:  f3f4fcf1a4309205bffa15580daae7da
```

See:

```text
source/README.md
docs/REPRODUCIBLE_BUILD.md
docs/SOURCE_PROVENANCE.md
```

for build and source provenance information.

---

## Features

* Doom engine running on the ZZ9000 ARM Core1.
* Based on DoomGeneric / Doom source code.
* Fullscreen ZZ9000 RTG display.
* `ZZDoom320` and `ZZDoom640` executables.
* Deferred-PAN page flipping.
* Low 68k CPU usage.
* Keyboard and mouse controls.
* AHI sound effects.
* CAMD MIDI music support.
* Save/load support through AmigaOS files.
* Doom and Doom II support for WAD files within the current size limit.
* Workbench and Shell launching.

---

## ZZDoom V1.1

Version 1.1 improved compatibility with recent Picasso96 versions and includes:

* More compatible Picasso96 screen opening.
* Fallback framebuffer allocation when required.
* Mouse pointer hidden during gameplay.
* Improved mouse button handling.
* Left mouse button: fire.
* Right mouse button: strafe.

The ARM Core1 Doom engine embedded in V1.1 remained identical to the validated historical ARM release baseline.

---

## Requirements

* Amiga with MNT ZZ9000.
* XACP-enabled ZZ9000 firmware with Core1 launch support.
* Matching `zz9000.card`.
* Picasso96 / RTG.
* AHI.
* `camd.library` for MIDI music.
* External MIDI synthesizer for CAMD music, or music disabled.
* Your own legally obtained Doom WAD.

Use a matching XACP firmware / `zz9000.card` combination. Mixing components from unrelated releases may result in display, synchronization or Core1 launch problems.

No Doom game data is included in this repository.

---

## Supported WADs

Current intended targets include:

```text
DOOM1.WAD
DOOM.WAD
DOOMU.WAD
DOOM2.WAD
```

The current ARM WAD area is limited to WAD files below 16 MB.

Final Doom, TNT and Plutonia are therefore outside the supported WAD size range of this version.

---

## Installation

Copy the appropriate executable and your WAD to a directory on the Amiga.

Example:

```text
ZZDoom320
ZZDoom320.info
DOOM.WAD
```

or:

```text
ZZDoom640
ZZDoom640.info
DOOM2.WAD
```

Install a matching XACP-enabled firmware and `zz9000.card`.

A complete power-off is recommended after replacing ZZ9000 firmware or the RTG card driver.

---

## Shell usage

```text
ZZDoom320 DOOM.WAD
```

or:

```text
ZZDoom640 DOOM2.WAD
```

Music can be disabled with:

```text
-nomusic
```

---

## Workbench usage

Place the WAD in the program directory and use the icon ToolType:

```text
WAD=DOOM.WAD
```

Optional ToolTypes include:

```text
NOMUSIC
CAMDPORT=out.1
```

---

## Music

Music is sent through `camd.library` to a MIDI output.

An external MIDI synthesizer can therefore be used directly.

Without an external MIDI synthesizer, ZZDoom can be run with music disabled.

---

## Savegames

Save/load is supported through AmigaOS files stored in the program directory.

Six save slots are available.

---

## Source layout

```text
source/amiga/
```

contains the Amiga-side launcher, CAMD and integration sources.

```text
source/core1/
```

contains the DoomGeneric/Doom ARM Core1 source tree and the ZZ9000-specific code used to build the embedded ARM binaries.

The ARM source tree is intentionally kept close to the historical build layout to preserve reproducibility.

---

## Reproducible ARM builds

The released ARM blobs are reproducible using the documented Debian/Ubuntu ARM bare-metal GCC/newlib toolchain.

Running the supplied verification script after a successful build must produce:

```text
ZZDoom320:
8adf2938b3a7823d2d7005e0c9b8db8f

ZZDoom640:
f3f4fcf1a4309205bffa15580daae7da
```

See `docs/REPRODUCIBLE_BUILD.md` for details.

---

## License

The Doom / DoomGeneric derived source code is distributed under the GNU General Public License according to the notices contained in the source files.

ZZDoom-specific source code published in this directory is also distributed under the GNU General Public License, version 2 or later.

See `COPYING`.

Doom game data, WAD files and other commercial assets are **not** part of this source release.

---

## Credits

* id Software — original Doom source code.
* DoomGeneric contributors.
* MNT Research — ZZ9000 hardware and software ecosystem.
* Amiga and ZZ9000 community testers and contributors.
* Xanxi-Amiga — ZZ9000/Core1 adaptations and Amiga integration.
