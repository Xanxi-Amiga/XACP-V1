# ZZRastan — Third-party licenses and copyright notices

ZZRastan is an independent recreation of the Rastan arcade hardware for
Amiga computers equipped with an MNT ZZ9000.

ZZRastan does not contain or distribute any Rastan game ROMs, graphics,
music, samples or other commercial game data.

Users must provide their own legally obtained ROM dumps.

This directory contains the copyright notices and license terms for
third-party software components used by ZZRastan.

---

## Original ZZRastan code

Copyright (c) 2026 Xanxi.
All rights reserved.

This copyright notice applies only to original ZZRastan-specific code.

Third-party components included in or used by ZZRastan remain copyright
of their respective authors and are distributed under their respective
license terms described below.

Nothing in this notice changes, replaces or overrides a third-party
license.

---

## Non-commercial distribution

ZZRastan includes third-party software whose licenses restrict use and/or
distribution to non-commercial purposes.

Accordingly, ZZRastan is distributed as a non-commercial project.

The ZZRastan release, including binaries derived from these components,
must not be sold or used as part of a commercial product or commercial
activity where prohibited by the applicable third-party licenses.

See the individual license files in this directory for the authoritative
terms.

---

# Cyclone 68000

ZZRastan uses the Cyclone Motorola 68000 emulator.

Cyclone copyright:

Copyright (c) 2004, 2011 FinalDave
Copyright (c) 2005-2011 Grazvydas "notaz" Ignotas

Cyclone is dual-licensed under:

- GNU General Public License version 2.0
- MAME License

For ZZRastan, Cyclone is used and distributed under the MAME License
option, not under GPLv2.

ZZRastan uses a generated/configured Cyclone core and therefore distributes
the corresponding source and configuration used to produce the released
binary.

The original Cyclone copyright notices and license terms are reproduced
in:

    Cyclone-License.txt
    Cyclone-Credits.txt

Those files must be copied verbatim from the exact Cyclone source revision
used to build the release.

---

# CZ80

ZZRastan uses the CZ80 Z80 CPU emulator.

CZ80:

Copyright 2004-2005 Stephane Dallongeville

Modified by NJ.

The original CZ80 documentation permits the emulator to be freely
distributed and used for non-commercial projects provided that the
original author is credited.

The original CZ80 documentation containing the copyright and distribution
terms is reproduced verbatim in:

    CZ80-readme.txt

The copyright headers in the CZ80 source files must also be retained.

---

# YM2151 sound emulation

ZZRastan uses a YM2151 software implementation based on the work of
Jarek Burczynski and derived from the implementation distributed with
FinalBurn Neo (FBNeo).

YM2151 sound core:

Jarek Burczynski

The ZZRastan version contains adaptations required for the ZZ9000
Core1 environment.

FBNeo contains code originating from and/or based on the historical
MAME project and applies its own distribution conditions in addition
to applicable MAME licensing terms.

The authoritative license texts applicable to the source used by
ZZRastan are reproduced verbatim in:

    FBNeo-License.txt
    MAME-License.txt

The original copyright and attribution headers in the YM2151 source
files must be retained.

Any modifications made to FBNeo-derived source that are distributed
with ZZRastan are included in the corresponding ZZRastan source release.

---

# Other third-party components

Any additional third-party source present in the final ZZRastan build
must retain its original copyright and license notices.

If the final release source contains a third-party implementation of
another emulated device, library or support component, its license must
be added to this directory before the release is published.

Original ZZRastan-specific implementations do not require a separate
third-party license notice.

---

# Corresponding source

The source distributed with ZZRastan corresponds to the ARM Core1
blob included in the same release.

It includes:

- ZZRastan ARM Core1 source
- shared protocol and memory-map headers
- linker scripts
- build scripts
- modified third-party source
- CPU-core configuration/generation files
- other source necessary to reproduce the Core1 blob

The Amiga 68k launcher and GUI are separate programs. Their source
code is not part of the corresponding source distribution for the
Core1 blob.

---

# Commercial game data

No Rastan ROM image or other copyrighted arcade game data is included
with ZZRastan.

The presence of emulator source code or binaries in this repository
does not grant any rights to the original Rastan game software or data.

Rastan and the original arcade game software and hardware-related
intellectual property remain property of their respective rights
holders.

---

# License precedence

The individual third-party license texts are authoritative for their
respective components.

If this README differs from, summarizes incompletely, or conflicts with
an original third-party license, the original third-party license text
takes precedence.
