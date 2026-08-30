# ZZDoom Source Code

This directory contains the source code for the Amiga and ARM Core1 components of ZZDoom.

## Directory structure

```text
amiga/
    AmigaOS launcher, RTG, input, AHI and CAMD integration.

core1/
    Doom/DoomGeneric engine and ZZ9000 ARM Core1 integration.
```

## ARM Core1 source

The `core1/` directory is the Corresponding Source for the ARM binaries embedded in the released ZZDoom executables.

The source tree is based on DoomGeneric, with the upstream baseline corresponding to commit:

```text
dcb7a8dbc7a16ce3dda29382ac9aae9d77d21284
```

and includes the ZZ9000-specific changes required for:

* execution on the ZZ9000 ARM Cortex-A9 Core1;
* MMU/cache setup;
* shared-memory communication with the Amiga;
* framebuffer handling;
* keyboard and mouse input;
* AHI sound-effect transport;
* CAMD-oriented music integration;
* WAD access from shared memory;
* savegame support;
* Doom and Doom II selection.

The full source tree is included locally. Fetching a separate DoomGeneric checkout is not required to build it.

---

## Verified historical ARM binaries

The source tree has been verified against the ARM blobs embedded in the historical ZZDoom releases.

### 320 build

```text
Output: julia_doom_320.bin
Size:   381396 bytes
MD5:    8adf2938b3a7823d2d7005e0c9b8db8f
SHA256: 8a197365104173d22a1138fb4f1a1f32476bf20437fe5805d7a65c78845f2f5
```

### 640 build

```text
Output: julia_doom_640.bin
Size:   381396 bytes
MD5:    f3f4fcf1a4309205bffa15580daae7da
SHA256: 4cfbbcfa08b1cc2083cba861c26ea6c6be27d9cfafd2d99b31af414735ed7d56
```

Both rebuilt files have been compared byte-for-byte with the released ARM binaries.

---

## ARM build environment

Bit-perfect reproduction was validated with the Debian/Ubuntu packaged ARM bare-metal toolchain:

```text
arm-none-eabi-gcc 13.2.1 20231009
newlib 4.4.0.20231231
```

The distribution-packaged newlib configuration is relevant for exact binary reproduction.

Using another GCC/newlib distribution may produce a functionally valid binary but will not necessarily reproduce the historical hashes.

In particular, exact reproduction should use the documented toolchain rather than assuming that every ARM GNU Toolchain 13.2 package contains an identical newlib build.

---

## Build

From `source/core1/`:

```text
./build_320.sh
./build_640.sh
```

Then:

```text
./verify.sh
```

The verification script checks the generated binaries against the release hashes listed above.

The build scripts preserve several historical details that matter for byte-for-byte reproduction.

### 320 build

The DoomGeneric renderer is built for:

```text
320x200
```

### 640 build

The high-resolution DoomGeneric renderer/framebuffer build uses:

```text
640x400
```

For historical compatibility, `i_video.c` is compiled with the 320x200 DoomGeneric dimensions in the 640 build.

The 640 build also preserves the historical object link ordering. In particular:

```text
doomgeneric.o
r_draw.o
r_main.o
v_video.o
```

are linked after `z_zone.o`.

These details are intentional and are required to reproduce the released binary exactly.

---

## WAD backend

The historical ARM release uses the in-memory WAD backend.

`w_file.c` selects the memory implementation provided by:

```text
w_file_mem.c
```

This reflects the ZZDoom architecture, where the WAD is uploaded by the Amiga side into shared memory before the ARM engine starts.

---

## Amiga source

The `amiga/` directory contains the Amiga-side source baseline used for ZZDoom integration, including:

* standalone 320 and 640 wrappers;
* Core1 launcher;
* Picasso96 integration;
* input handling;
* AHI sound transport;
* CAMD MIDI support;
* WAD upload;
* savegame file transfer.

The byte-for-byte verification hashes documented above apply specifically to the **ARM Core1 blobs**.

---

## Toolchains

ARM Core1:

```text
arm-none-eabi-gcc 13.2.1
```

Amiga host:

```text
m68k-amigaos-gcc
```

The historical Amiga builds were made using the Bebbo GCC Amiga toolchain.

---

## License

Doom/DoomGeneric-derived code remains under its original GNU GPL notices.

ZZDoom-specific source code in this release is distributed under the GNU General Public License, version 2 or later.

See the top-level `COPYING` file.

No Doom WAD or commercial game asset is included.
