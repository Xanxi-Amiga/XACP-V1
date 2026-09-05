# ZZPicoDriveMD 1.1 corresponding source

This directory contains the source corresponding to the public
**ZZPicoDriveMD 1.1** release for the MNT ZZ9000.

## Release source mapping

Amiga 68k launcher:

```text
amiga/ZZPicoDriveMD_v1.1.c
amiga/blob_data.c
```

ARM Core1 side:

```text
arm/zzpico_core1.c
arm/zzpicodrive_md_v1.1.bin
arm/build_cyc_audioreal.sh
arm/zzpico_*.c / .h / .S / .ld
arm/stubs/
```

The exact embedded ARM blob is:

```text
Size:    598968 bytes
SHA-256: 07484259b5a62949322198aced99369cb3f2ae0627f32a7cc6a97946b98c671b
```

`arm/zzpico_core1.c` is byte-for-byte identical to the final MD v1.1 Core1
source retained in the original build archive.

The historical source files are intentionally preserved rather than rewritten.
Some comments therefore mention earlier internal development-stage names even
though the current public firmware baseline is XX19a / XACP v1.6.

## Upstream PicoDrive revision

ZZPicoDriveMD 1.1 is based on:

```text
notaz/picodrive
commit 26ecb2b6358fefba24e3d68b9eb2efba7f10d5ee
```

The Cyclone submodule used by this revision is:

```text
irixxxx/cyclone68000
commit 3ac7cf1bdeecb60e2414980e8dc72ff092f69769
```

Before committing this package to GitHub, run:

```powershell
.\FETCH_UPSTREAM.ps1
```

The script clones the exact PicoDrive revision, initializes only the Cyclone
submodule required by the MD build, verifies both revisions and removes nested
Git metadata so the source is committed normally inside the XACP repository.

After the script completes, this must exist:

```text
third_party/picodrive/COPYING
third_party/picodrive/AUTHORS
third_party/picodrive/pico/
third_party/picodrive/cpu/cyclone/
```

Do **not** commit the package before this step has completed successfully.

## ARM build

The historical build script uses the original `/tmp/zp` layout and the
`arm-none-eabi-` GNU toolchain. Its compiler flags and exact object list are
retained in `arm/build_cyc_audioreal.sh`.

For the historical layout, the build directory contained the files from `arm/`
and the PicoDrive tree as `/tmp/zp/picodrive`.

## 68k launcher build

The launcher source records the original build command. The general form is:

```text
m68k-amigaos-gcc -O2 -noixemul -m68020 -Wno-pointer-sign \
    -o ZZPicoDriveMD ZZPicoDriveMD_v1.1.c blob_data.c -lamiga
```

The normal AmigaOS NDK, Picasso96 and AHI headers used by the source are also
required.

## Verification

Run `VERIFY_SOURCE.ps1` to verify the retained MD release files before commit.

## Licensing

PicoDrive at the revision used here is not GPL-only. Its historical
non-commercial license requires complete source availability for modified
binary distributions.

See:

```text
../COPYING_PICODRIVE.txt
../THIRD_PARTY_NOTICES.md
third_party/picodrive/COPYING
```

Original third-party copyright and license notices must be preserved.
