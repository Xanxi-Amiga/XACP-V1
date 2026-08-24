#ifndef ZZRASTAN_H
#define ZZRASTAN_H

#include <stddef.h>
#include "zzrastan_shared.h"
#include <Cyclone.h>

#define ZZR_M68K_CLOCK 8000000u
#define ZZR_REFRESH_HZ 60u

#define ZZR_WORK_RAM_BASE   0x10C000u
#define ZZR_WORK_RAM_SIZE   0x004000u
#define ZZR_PALETTE_BASE    0x200000u
#define ZZR_PALETTE_SIZE    0x001000u
#define ZZR_TILEMAP_BASE    0xC00000u
#define ZZR_TILEMAP_SIZE    0x010000u
#define ZZR_SPRITE_BASE     0xD00000u
#define ZZR_SPRITE_SIZE     0x004000u

typedef struct zzrastan_mem_stats_s {
    zzr_u32 palette_writes;
    zzr_u32 tilemap_writes;
    zzr_u32 spriteram_writes;
    zzr_u32 yscroll_writes;
    zzr_u32 xscroll_writes;
    zzr_u32 ctrl_writes;
    zzr_u32 pc090oj_writes;
    zzr_u32 pc060ha_cmd_count;
    zzr_u32 pc060ha_port_writes;
    zzr_u32 pc060ha_comm_reads;
    zzr_u32 watchdog_writes;
    zzr_u32 bad_read_count;
    zzr_u32 bad_write_count;
    zzr_u32 bus_error_count;
    zzr_u32 illegal_opcode_count;
    zzr_u32 last_bad_addr;
    zzr_u32 last_bad_value;
    zzr_u32 last_sound_cmd;
    zzr_u32 pc060ha_port;
} zzrastan_mem_stats_t;

extern volatile zzrastan_shared_t *g_zzr_shared;
extern struct Cyclone g_zzr_cpu;
extern zzrastan_mem_stats_t g_zzr_memstats;
extern const zzr_u8 *g_zzr_main_rom;
extern int g_zzr_rom_ready;
extern const zzr_u8 *g_zzr_gfx_rom;   /* PC080SN, deinterleaved byte stream */
extern int g_zzr_gfx_ready;
extern const zzr_u8 *g_zzr_obj_rom;
extern int g_zzr_obj_ready;
int zzrastan_obj_bind_and_deinterleave(void);

void zzrastan_mem_reset(void);
int zzrastan_mem_selftest(void);
void zzrastan_pc060ha_reset(void);
int zzrastan_pc060ha_selftest(void);

zzr_u32 rastan_m68k_read8(zzr_u32 addr);
zzr_u32 rastan_m68k_read16(zzr_u32 addr);
zzr_u32 rastan_m68k_read32(zzr_u32 addr);
void rastan_m68k_write8(zzr_u32 addr, zzr_u8 data);
void rastan_m68k_write16(zzr_u32 addr, zzr_u16 data);
void rastan_m68k_write32(zzr_u32 addr, zzr_u32 data);

int zzrastan_rom_bind_and_deinterleave(void);
int zzrastan_gfx_bind_and_deinterleave(void);
const zzr_u8 *zzrastan_palette_ram(void);
const zzr_u8 *zzrastan_tilemap_ram(void);
const zzr_u16 *zzrastan_xscroll_regs(void);
const zzr_u16 *zzrastan_yscroll_regs(void);
const zzr_u16 *zzrastan_ctrl_regs(void);
const zzr_u8 *zzrastan_sprite_ram(void);
zzr_u16 zzrastan_sprite_ctrl(void);
void zzrastan_video_present(void);
void zzrastan_video_reset(void);
zzr_u32 zzrastan_crc32(const zzr_u8 *p, zzr_u32 n);

int zzrastan_cpu_init_and_reset(void);
void zzrastan_run_one_frame_debug(void);
void zzrastan_raise_irq5(void);
void zzrastan_debug_publish(zzr_u32 frame_cycles, zzr_u32 m68k_cycles);

#endif
