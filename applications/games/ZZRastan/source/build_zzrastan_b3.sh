#!/bin/bash
set -e

OUT=${1:-zzrastan_b3}
PICO=${PICO:-picodrive}
CROSS=${CROSS:-arm-none-eabi-}
CC=${CROSS}gcc
OBJCOPY=${CROSS}objcopy
ROOT=$(pwd)

CPU="-mcpu=cortex-a9 -marm -mfpu=vfpv3-d16 -mfloat-abi=hard"
INC="-I. -I$PICO/cpu/cyclone"
CFLAGS="-O2 $CPU $INC -ffreestanding -fno-builtin -fno-common -fomit-frame-pointer -fno-stack-protector -Wno-unused"

if [ ! -f "$PICO/cpu/cyclone/Cyclone.h" ]; then
  echo "Missing $PICO/cpu/cyclone. Checkout PicoDrive commit 26ecb2b and init submodules."
  exit 1
fi

rm -f "$PICO/cpu/cyclone/Cyclone.s"
make -C "$PICO/cpu/cyclone" CONFIG_FILE="$ROOT/cyclone_config_rastan.h" HAVE_ARMv6=1

$CC $CFLAGS -c zzrastan_main.c     -o zzrastan_main.o
$CC $CFLAGS -c zzrastan_rom.c      -o zzrastan_rom.o
$CC $CFLAGS -c zzrastan_video.c    -o zzrastan_video.o
$CC $CFLAGS -c zzrastan_audio.c    -o zzrastan_audio.o
$CC $CFLAGS -c zzrastan_msm.c      -o zzrastan_msm.o
$CC $CFLAGS -I"$PICO" -I"$PICO/cpu/cz80" -c zzrastan_sound.c -o zzrastan_sound.o
$CC $CFLAGS -I"$PICO" -I"$PICO/cpu/cz80" -c "$PICO/cpu/cz80/cz80.c" -o cz80.o
CXX=${CXX:-arm-none-eabi-g++}
CXXFLAGS="$CPU -O3 -std=c++14 -fno-exceptions -fno-rtti -fno-threadsafe-statics -I../ymfm/src -I."
$CXX $CXXFLAGS -c ../ymfm/src/ymfm_opm.cpp -o ymfm_opm.o
$CXX $CXXFLAGS -c zzrastan_ym.cpp          -o zzrastan_ym.o
$CC $CFLAGS -c ym2151.c        -o ym2151.o
$CC $CFLAGS -c zzr_ym_jarek.c  -o zzr_ym_jarek.o
$CXX $CXXFLAGS -c zzrastan_cxx_stubs.cpp   -o zzrastan_cxx_stubs.o
$CC $CFLAGS -c zzrastan_mem.c      -o zzrastan_mem.o
$CC $CFLAGS -c zzrastan_cpu.c      -o zzrastan_cpu.o
$CC $CFLAGS -c zzrastan_platform.c -o zzrastan_platform.o
$CC $CFLAGS -c zzrastan_mmu.c      -o zzrastan_mmu.o
$CC $CFLAGS -c zzrastan_vectors.S  -o zzrastan_vectors.o
$CC $CFLAGS -c "$PICO/cpu/cyclone/Cyclone.s" -o Cyclone.o

OBJS="zzrastan_main.o zzrastan_rom.o zzrastan_video.o zzrastan_mem.o zzrastan_cpu.o \
zzrastan_platform.o zzrastan_mmu.o zzrastan_audio.o zzrastan_sound.o zzrastan_msm.o cz80.o ymfm_opm.o zzrastan_ym.o zzrastan_cxx_stubs.o ym2151.o zzr_ym_jarek.o zzrastan_vectors.o Cyclone.o"

LIBGCC_DIR=$($CC $CPU -print-libgcc-file-name | sed 's/libgcc.a//')
$CC $CPU -nostdlib -T zzrastan.ld $OBJS -lc -lm -lgcc -L"$LIBGCC_DIR" \
  -Wl,-Map,$OUT.map -o $OUT.elf
$OBJCOPY -O binary $OUT.elf $OUT.bin

SIZE=$(stat -c%s "$OUT.bin")
MD5=$(md5sum "$OUT.bin" | awk '{print $1}')
echo "BIN: $SIZE bytes MD5=$MD5 -> $OUT.bin"
