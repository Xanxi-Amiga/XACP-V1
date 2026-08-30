#!/bin/sh
set -eu
CC=${CC:-arm-none-eabi-gcc}
OBJCOPY=${OBJCOPY:-arm-none-eabi-objcopy}
B=build640
rm -rf "$B"
mkdir -p "$B"
EXTRA_CPPFLAGS=${EXTRA_CPPFLAGS:-}
EXTRA_LDFLAGS=${EXTRA_LDFLAGS:-}
CFLAGS640='-mcpu=cortex-a9 -marm -mfpu=vfpv3-d16 -mfloat-abi=hard -mno-unaligned-access -Os -ffreestanding -ffunction-sections -fdata-sections -DNORMALUNIX -DLINUX -DDOOMGENERIC_RESX=640 -DDOOMGENERIC_RESY=400 -I.'
CFLAGS320='-mcpu=cortex-a9 -marm -mfpu=vfpv3-d16 -mfloat-abi=hard -mno-unaligned-access -Os -ffreestanding -ffunction-sections -fdata-sections -DNORMALUNIX -DLINUX -DDOOMGENERIC_RESX=320 -DDOOMGENERIC_RESY=200 -I.'
CFLAGS640="$CFLAGS640 $EXTRA_CPPFLAGS"
CFLAGS320="$CFLAGS320 $EXTRA_CPPFLAGS"
SAFEFLAGS='-mcpu=cortex-a9 -marm -mfpu=vfpv3-d16 -mfloat-abi=hard -O0 -ffreestanding -fno-builtin-memcpy -fno-builtin-memmove -fno-builtin-memset'
cp julia_doom_core1_640.c "$B/julia_doom_core1.c"
$CC $SAFEFLAGS -c safe_mem.c -o "$B/safe_mem.o"
$CC $CFLAGS640 -c abort_handler.S -o "$B/abort_handler.o"
SRCS='am_map.c audio_zz_sfx.c d_event.c d_items.c d_iwad.c d_loop.c d_main.c d_mode.c d_net.c doomdef.c doomgeneric.c doomstat.c dstrings.c dummy.c f_finale.c f_wipe.c g_game.c hu_lib.c hu_stuff.c i_allegromusic.c i_cdmus.c i_endoom.c i_error_override.c i_input.c i_joystick.c i_scale.c i_sound.c i_timer.c i_zz9000sound.c info.c m_argv.c m_bbox.c m_cheat.c m_config.c m_controls.c m_fixed.c m_menu.c m_misc.c m_random.c memio.c mmu_diag_helpers.c mmu_init.c p_ceilng.c p_doors.c p_enemy.c p_floor.c p_inter.c p_lights.c p_map.c p_maputl.c p_mobj.c p_plats.c p_pspr.c p_saveg.c p_setup.c p_sight.c p_spec.c p_switch.c p_telept.c p_tick.c p_user.c r_bsp.c r_data.c r_draw.c r_main.c r_plane.c r_segs.c r_sky.c r_things.c s_sound.c sha1.c sounds.c st_lib.c st_stuff.c statdump.c sys_stub.c tables.c v_video.c w_checksum.c w_file.c w_file_mem.c w_file_stdc.c w_main.c w_wad.c wi_stuff.c z_zone.c'
for f in $SRCS; do $CC $CFLAGS640 -c "$f" -o "$B/${f%.c}.o"; done
# Historical 640 build detail: i_video.c was compiled with 320x200 defines.
$CC $CFLAGS320 -c i_video.c -o "$B/i_video.o"
$CC $CFLAGS640 -c "$B/julia_doom_core1.c" -o "$B/julia_doom_core1.o"
# Historical 640 link order: doomgeneric/r_draw/r_main/v_video are placed after z_zone.
OBJS='abort_handler.o am_map.o audio_zz_sfx.o d_event.o d_items.o d_iwad.o d_loop.o d_main.o d_mode.o d_net.o doomdef.o doomstat.o dstrings.o dummy.o f_finale.o f_wipe.o g_game.o hu_lib.o hu_stuff.o i_allegromusic.o i_cdmus.o i_endoom.o i_error_override.o i_input.o i_joystick.o i_scale.o i_sound.o i_timer.o i_video.o i_zz9000sound.o info.o julia_doom_core1.o m_argv.o m_bbox.o m_cheat.o m_config.o m_controls.o m_fixed.o m_menu.o m_misc.o m_random.o memio.o mmu_diag_helpers.o mmu_init.o p_ceilng.o p_doors.o p_enemy.o p_floor.o p_inter.o p_lights.o p_map.o p_maputl.o p_mobj.o p_plats.o p_pspr.o p_saveg.o p_setup.o p_sight.o p_spec.o p_switch.o p_telept.o p_tick.o p_user.o r_bsp.o r_data.o r_plane.o r_segs.o r_sky.o r_things.o s_sound.o sha1.o sounds.o st_lib.o st_stuff.o statdump.o sys_stub.o tables.o w_checksum.o w_file.o w_file_mem.o w_file_stdc.o w_main.o w_wad.o wi_stuff.o z_zone.o doomgeneric.o r_draw.o r_main.o v_video.o'
set --
for o in $OBJS; do set -- "$@" "$B/$o"; done
$CC -mcpu=cortex-a9 -marm -mfpu=vfpv3-d16 -mfloat-abi=hard -nostartfiles -T linker.ld -Wl,--gc-sections -Wl,-s -Wl,-Map="$B/julia_doom_640.map" $EXTRA_LDFLAGS "$B/safe_mem.o" "$@" -lgcc -lc -lm -o "$B/julia_doom_640.elf"
$OBJCOPY -O binary "$B/julia_doom_640.elf" "$B/julia_doom_640.bin"
md5sum "$B/julia_doom_640.bin"
