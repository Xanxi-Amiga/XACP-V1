# ZZRastan Core1 source

This directory contains the corresponding source code for the
ZZRastan ARM Core1 blob.

It does not contain the source code of the Amiga 68k launcher or
the ZZRastan GUI. These are separate Amiga-side programs and are
distributed as proprietary binaries.

## Release baseline

ZZRastan 1.0 uses the validated Baseline B source without further
source cleanup or functional modifications.

Third-party components used by the Core1 build:

- PicoDrive: commit 26ecb2b6358fefba24e3d68b9eb2efba7f10d5ee
- Cyclone 68000: commit 3ac7cf1bdeecb60e2414980e8dc72ff092f69769
- CZ80: from the PicoDrive commit above
- ymfm: commit 81aec25ccbb98f4873a255f7551ac4dadac59b4a
- Jarek Burczynski YM2151 core, adapted from the FBNeo version

The active YM2151 backend in ZZRastan 1.0 is the Jarek Burczynski
core. ymfm remains compiled and linked as the reference backend.

## Directory layout

The build script is intended to be run from this directory.

It expects:

    picodrive/cpu/cyclone/
    picodrive/cpu/cz80/
    ../ymfm/src/

Build script:

    build_zzrastan_b3.sh

No Rastan game ROMs or other commercial game data are included in
this source distribution.

See ../licenses/ for third-party copyright and license information.
