# ZZDoom Reproducible ARM Builds

This document describes how to rebuild the ARM Core1 binaries distributed with ZZDoom and verify them against the release hashes.

## Source directory

Run the ARM build commands from:

```text
applications/games/ZZDoom/source/core1/
```

The complete DoomGeneric/Doom source tree required by the build is included in this directory.

The DoomGeneric baseline is recorded in:

```text
DOOMGENERIC_COMMIT.txt
```

## Required ARM toolchain

Bit-for-bit reproduction was validated with:

```text
arm-none-eabi-gcc 13.2.1 20231009
newlib 4.4.0.20231231
```

The Debian/Ubuntu packaged ARM bare-metal GCC/newlib toolchain was used.

A different GCC or newlib build may produce a functional binary but may not reproduce the release hashes exactly.

Check the compiler version with:

```sh
arm-none-eabi-gcc --version
```

## Build ZZDoom320

From `source/core1/`:

```sh
./build_320.sh
```

The generated ARM binary is:

```text
build320/julia_doom_320.bin
```

Expected result:

```text
Size: 381396 bytes
MD5:  8adf2938b3a7823d2d7005e0c9b8db8f
```

The 320 build uses:

```text
DOOMGENERIC_RESX=320
DOOMGENERIC_RESY=200
```

## Build ZZDoom640

From `source/core1/`:

```sh
./build_640.sh
```

The generated ARM binary is:

```text
build640/julia_doom_640.bin
```

Expected result:

```text
Size: 381396 bytes
MD5:  f3f4fcf1a4309205bffa15580daae7da
```

The main 640 build uses:

```text
DOOMGENERIC_RESX=640
DOOMGENERIC_RESY=400
```

For reproducible output, `i_video.c` is compiled with:

```text
DOOMGENERIC_RESX=320
DOOMGENERIC_RESY=200
```

The object order used by `build_640.sh` is also intentional and must not be changed if the release hash is to be reproduced.

## Verify both release blobs

After building both versions:

```sh
./verify.sh
```

Expected output:

```text
320: 8adf2938b3a7823d2d7005e0c9b8db8f  OK
640: f3f4fcf1a4309205bffa15580daae7da  OK
```

## Build flags

The ARM build targets the ZZ9000 Cortex-A9 Core1 and uses the important options:

```text
-mcpu=cortex-a9
-marm
-mfpu=vfpv3-d16
-mfloat-abi=hard
-mno-unaligned-access
-Os
-ffreestanding
-ffunction-sections
-fdata-sections
```

`safe_mem.c` is deliberately compiled separately with `-O0` and builtin memory functions disabled, as encoded in the supplied build scripts.

## Output format

The linker script is:

```text
linker.ld
```

The ELF output is converted to the raw Core1 binary with:

```text
arm-none-eabi-objcopy -O binary
```

The generated `.elf`, `.map`, `.o` and build directories are build products and are not required in the source repository.

## Amiga host build

The Amiga-side sources are located in:

```text
source/amiga/
```

They include the launcher, RTG/input integration, AHI audio, CAMD MIDI support, standalone wrappers and blob generator.

The ARM MD5 values documented above apply to the embedded ARM Core1 binaries, not to the complete Amiga executables.

## Game data

No Doom WAD or commercial game data is required to compile the ARM source and none is included in the repository.

A legally obtained Doom WAD is required to run ZZDoom.
