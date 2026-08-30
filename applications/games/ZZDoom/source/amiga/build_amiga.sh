#!/bin/sh
set -eu

CC=${CC:-m68k-amigaos-gcc}
CFLAGS="-O2 -noixemul -m68020 -Wno-pointer-sign"
CORE1=${CORE1:-../core1}

BIN320="$CORE1/build320/julia_doom_320.bin"
BIN640="$CORE1/build640/julia_doom_640.bin"

[ -f "$BIN320" ] || { echo "Missing $BIN320 - run ../core1/build_320.sh first"; exit 1; }
[ -f "$BIN640" ] || { echo "Missing $BIN640 - run ../core1/build_640.sh first"; exit 1; }

python3 make_blob.py "$BIN320" ZZDoom320_blob.c zzdoom_blob
python3 make_blob.py "$BIN640" ZZDoom640_blob.c zzdoom_blob

$CC $CFLAGS -c ZZDoomCAMD.c zz_camd.c zz_mus.c
$CC $CFLAGS -o ZZDoom320 ZZDoom320_wrap.c ZZDoom320_blob.c \
    ZZDoomCAMD.o zz_camd.o zz_mus.o -lamiga
$CC $CFLAGS -o ZZDoom640 ZZDoom640_wrap.c ZZDoom640_blob.c \
    ZZDoomCAMD.o zz_camd.o zz_mus.o -lamiga

echo "Built ZZDoom320 and ZZDoom640"
