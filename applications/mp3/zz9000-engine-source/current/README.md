# Current AmigaAMP `zz9000.engine` XACP source

This is the clean XACP-only source tree for the AmigaAMP external engine used with the MNT ZZ9000.

The engine keeps the normal AmigaAMP interface on the Amiga side while MPEG audio decoding is performed by the XACP service on the ZZ9000 ARM.

## Credits

- **Thomas Wenzel** - original AmigaAMP EngineLibrary framework and original ZZ9000 backend stub.
- **Xanxi** - ZZ9000/XACP backend implementation, integration and real-hardware validation.

## License

The EngineLibrary/XACP work is distributed under the **GNU Lesser General Public License v2.1 or later**.

```text
SPDX-License-Identifier: LGPL-2.1-or-later
```

See `LICENSE` and the notices in the parent directory. Individual files containing retained third-party attribution remain subject to those notices.

## Architecture

```text
AmigaAMP
   |
   v
zz9000.engine
   |
   v
XACP compressed-audio ring
   |
   v
ZZ9000 ARM MPEG decoder
   |
   v
XACP PCM ring
   |
   v
zz9000.engine / AmigaAMP
```

The engine communicates with the already-running XACP firmware service. It does not embed or upload another ARM executable.

## Lean build

The Makefile intentionally links only:

```text
StartUp.o
LibInit.o
Funcs.o
EngineAccess.o
iEngine_ZZ9000.o
MyStdlib.o
```

The historical `amp/`, `equalizer/` and `fifo/` decoder objects are not used by XACP and are intentionally excluded from the current build.

## Requirements

- amiga-gcc / `m68k-amigaos-gcc` toolchain
- Classic AmigaOS target
- MNT ZZ9000
- XACP-compatible firmware providing the MPEG audio streaming service

The current repository baseline is **XX19a / XACP v1.6**.

## Build

With the amiga-gcc toolchain in `PATH`:

```sh
make
```

Expected outputs:

```text
zz9000.engine.debug
zz9000.engine
```

Clean intermediate files with:

```sh
make clean
```

or all generated files with:

```sh
make mrproper
```

The resulting binary must keep the filename `zz9000.engine` for AmigaAMP.

## Release status

This clean link configuration should be compiled and validated on real Amiga/ZZ9000 hardware before it replaces the historical binary in a future release. Existing historical releases should remain unchanged.
