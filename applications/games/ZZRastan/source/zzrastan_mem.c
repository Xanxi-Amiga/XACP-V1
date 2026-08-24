#include "zzrastan.h"

static zzr_u8 work_ram[ZZR_WORK_RAM_SIZE];
static zzr_u8 palette_ram[ZZR_PALETTE_SIZE];
static zzr_u8 tilemap_ram[ZZR_TILEMAP_SIZE];
static zzr_u8 sprite_ram[ZZR_SPRITE_SIZE];

/* Read-only views for the B0b renderer. Both are stored BIG-ENDIAN (be16_read/be16_write),
   the A1 endianness model is unchanged. */
/* Trainer helper: write a 16-bit value straight into work RAM. Kept next to the other accessors
   so it uses the same big-endian convention as the 68000 side. */
/* Trainer helpers: single bytes, because the MAME cheats are byte writes (pb@). Both refuse
   anything outside work RAM, so a wrong address cannot corrupt memory. */
void zzrastan_poke8(zzr_u32 addr, zzr_u8 value)
{
    zzr_u32 off = addr - ZZR_WORK_RAM_BASE;
    if (off >= ZZR_WORK_RAM_SIZE) return;
    work_ram[off] = value;
}
zzr_u8 zzrastan_peek8(zzr_u32 addr)
{
    zzr_u32 off = addr - ZZR_WORK_RAM_BASE;
    if (off >= ZZR_WORK_RAM_SIZE) return 0;
    return work_ram[off];
}

const zzr_u8 *zzrastan_palette_ram(void) { return palette_ram; }
const zzr_u8 *zzrastan_tilemap_ram(void) { return tilemap_ram; }
static zzr_u16 yscroll_regs[2];
static zzr_u16 xscroll_regs[2];
static zzr_u16 ctrl_regs[2];
static zzr_u16 sprite_ctrl;

/* Read-only views of the PC080SN scroll/ctrl registers for the B1 renderer. */
const zzr_u16 *zzrastan_xscroll_regs(void) { return xscroll_regs; }
const zzr_u16 *zzrastan_yscroll_regs(void) { return yscroll_regs; }
const zzr_u16 *zzrastan_ctrl_regs(void) { return ctrl_regs; }
const zzr_u8 *zzrastan_sprite_ram(void) { return sprite_ram; }
zzr_u16 zzrastan_sprite_ctrl(void) { return sprite_ctrl; }

static zzr_u8 pc060_port;
static zzr_u8 pc060_main_to_sound[16];
static zzr_u8 pc060_sound_to_main[16];

zzrastan_mem_stats_t g_zzr_memstats;

static void clear_bytes(zzr_u8 *p, zzr_u32 n)
{
    while (n-- != 0u)
        *p++ = 0;
}

static zzr_u16 be16_read(const zzr_u8 *p)
{
    return (zzr_u16)(((zzr_u16)p[0] << 8) | p[1]);
}

static void be16_write(zzr_u8 *p, zzr_u16 v)
{
    p[0] = (zzr_u8)(v >> 8);
    p[1] = (zzr_u8)v;
}

static void mark_bad_read(zzr_u32 addr)
{
    g_zzr_memstats.bad_read_count++;
    g_zzr_memstats.last_bad_addr = addr;
}

static void mark_bad_write(zzr_u32 addr, zzr_u32 value)
{
    g_zzr_memstats.bad_write_count++;
    g_zzr_memstats.last_bad_addr = addr;
    g_zzr_memstats.last_bad_value = value;
}

void zzrastan_pc060ha_reset(void)
{
    zzr_u32 i;
    pc060_port = 0;
    for (i = 0; i < 16u; i++) {
        pc060_main_to_sound[i] = 0;
        pc060_sound_to_main[i] = 0;
    }
    g_zzr_memstats.pc060ha_port = 0;
}

static void pc060_port_write(zzr_u8 data)
{
    pc060_port = data & 0x0Fu;
    g_zzr_memstats.pc060ha_port = pc060_port;
    g_zzr_memstats.pc060ha_port_writes++;
    /* THE MISSING LINK: master_port_w must reach the real CIU, otherwise mainmode is never
       selected by the 68000, drifts 0->1->2->3->4 on its own and then sticks at 4 - where every
       further write is read as the Z80 RESET line. That single omission produced pair01=1,
       pair23=1, cmds_68k=2, z80_resets=15 and the runaway NMI enable/disable. */
    {
        extern void zzrastan_sound_68k_write(zzr_u32 addr, zzr_u8 data);
        zzrastan_sound_68k_write(1u, data);
    }
}

static void pc060_comm_write(zzr_u8 data)
{
    pc060_main_to_sound[pc060_port] = data;
    g_zzr_memstats.pc060ha_cmd_count++;
    g_zzr_memstats.last_sound_cmd = data;

    /* A1: hand the byte to the real sound board (Z80 + PC060HA). The Z80 answers
       asynchronously; its reply is polled back below. */
    {
        extern void zzrastan_sound_68k_write(zzr_u32 addr, zzr_u8 data);
        zzrastan_sound_68k_write(3u, data);      /* master_comm_w */
    }
    pc060_sound_to_main[pc060_port] = 0u;
}

static zzr_u8 pc060_comm_read(void)
{
    extern zzr_u8 zzrastan_sound_68k_read(zzr_u32 addr);
    g_zzr_memstats.pc060ha_comm_reads++;
    return zzrastan_sound_68k_read(3u);          /* master_comm_r from the Z80 */
}

int zzrastan_pc060ha_selftest(void)
{
    zzrastan_pc060ha_reset();
    pc060_port_write(0);
    pc060_comm_write(0x55u);
    if (pc060_main_to_sound[0] != 0x55u)
        return 0;
    (void)pc060_comm_read();
    zzrastan_pc060ha_reset();
    return 1;
}

void zzrastan_mem_reset(void)
{
    clear_bytes(work_ram, sizeof(work_ram));
    clear_bytes(palette_ram, sizeof(palette_ram));
    clear_bytes(tilemap_ram, sizeof(tilemap_ram));
    clear_bytes(sprite_ram, sizeof(sprite_ram));
    clear_bytes((zzr_u8 *)yscroll_regs, sizeof(yscroll_regs));
    clear_bytes((zzr_u8 *)xscroll_regs, sizeof(xscroll_regs));
    clear_bytes((zzr_u8 *)ctrl_regs, sizeof(ctrl_regs));
    clear_bytes((zzr_u8 *)&g_zzr_memstats, sizeof(g_zzr_memstats));
    sprite_ctrl = 0;
    zzrastan_pc060ha_reset();
}

int zzrastan_mem_selftest(void)
{
    zzr_u16 old;
    old = rastan_m68k_read16(ZZR_WORK_RAM_BASE);
    rastan_m68k_write16(ZZR_WORK_RAM_BASE, 0xA55Au);
    if (rastan_m68k_read16(ZZR_WORK_RAM_BASE) != 0xA55Au)
        return 0;
    rastan_m68k_write16(ZZR_WORK_RAM_BASE, old);
    return 1;
}

zzr_u32 rastan_m68k_read8(zzr_u32 addr)
{
    addr &= 0x00FFFFFFu;

    if (addr < ZZR_MAINCPU_SIZE)
        return g_zzr_main_rom[addr ^ 1u];
    if (addr >= ZZR_WORK_RAM_BASE && addr < ZZR_WORK_RAM_BASE + ZZR_WORK_RAM_SIZE)
        return work_ram[addr - ZZR_WORK_RAM_BASE];
    if (addr >= ZZR_PALETTE_BASE && addr < ZZR_PALETTE_BASE + ZZR_PALETTE_SIZE)
        return palette_ram[addr - ZZR_PALETTE_BASE];
    if (addr >= ZZR_TILEMAP_BASE && addr < ZZR_TILEMAP_BASE + ZZR_TILEMAP_SIZE)
        return tilemap_ram[addr - ZZR_TILEMAP_BASE];
    if (addr >= ZZR_SPRITE_BASE && addr < ZZR_SPRITE_BASE + ZZR_SPRITE_SIZE)
        return sprite_ram[addr - ZZR_SPRITE_BASE];

    if (addr >= 0x390000u && addr <= 0x39000Bu) {
        zzr_u32 port = (addr - 0x390000u) >> 1;
        if ((addr & 1u) == 0u)
            return 0xFFu;
        switch (port) {
        case 0: return g_zzr_shared->input_p1 & 0xFFu;
        case 1: return g_zzr_shared->input_p2 & 0xFFu;
        case 2: return g_zzr_shared->input_special & 0xFFu;
        case 3:
            g_zzr_shared->dbg_sysrd++;
            if (g_zzr_shared->input_system & 0x20u) g_zzr_shared->dbg_coinrd++;
            return g_zzr_shared->input_system & 0xFFu;
        case 4: return g_zzr_shared->input_dswa & 0xFFu;
        default:return g_zzr_shared->input_dswb & 0xFFu;
        }
    }
    if (addr == 0x3E0003u)
        return pc060_comm_read();
    if (addr == 0x3E0001u || addr == 0x3E0000u || addr == 0x3E0002u)
        return 0xFFu;

    mark_bad_read(addr);
    return 0xFFu;
}

zzr_u32 rastan_m68k_read16(zzr_u32 addr)
{
    zzr_u32 port;
    addr &= 0x00FFFFFFu;
    if (addr & 1u) {
        g_zzr_memstats.bus_error_count++;
        g_zzr_memstats.last_bad_addr = addr;
        return 0xFFFFu;
    }

    if (addr < ZZR_MAINCPU_SIZE - 1u)
        return ((zzr_u32)g_zzr_main_rom[addr + 1u] << 8) | g_zzr_main_rom[addr];
    if (addr >= ZZR_WORK_RAM_BASE && addr < ZZR_WORK_RAM_BASE + ZZR_WORK_RAM_SIZE - 1u)
        return be16_read(work_ram + addr - ZZR_WORK_RAM_BASE);
    if (addr >= ZZR_PALETTE_BASE && addr < ZZR_PALETTE_BASE + ZZR_PALETTE_SIZE - 1u)
        return be16_read(palette_ram + addr - ZZR_PALETTE_BASE);
    if (addr >= ZZR_TILEMAP_BASE && addr < ZZR_TILEMAP_BASE + ZZR_TILEMAP_SIZE - 1u)
        return be16_read(tilemap_ram + addr - ZZR_TILEMAP_BASE);
    if (addr >= ZZR_SPRITE_BASE && addr < ZZR_SPRITE_BASE + ZZR_SPRITE_SIZE - 1u)
        return be16_read(sprite_ram + addr - ZZR_SPRITE_BASE);

    if (addr >= 0x390000u && addr <= 0x39000Au) {
        port = (addr - 0x390000u) >> 1;
        switch (port) {
        case 0: return 0xFF00u | (g_zzr_shared->input_p1 & 0xFFu);
        case 1: return 0xFF00u | (g_zzr_shared->input_p2 & 0xFFu);
        case 2: return 0xFF00u | (g_zzr_shared->input_special & 0xFFu);
        case 3:
            g_zzr_shared->dbg_sysrd++;
            if (g_zzr_shared->input_system & 0x20u) g_zzr_shared->dbg_coinrd++;
            return 0xFF00u | (g_zzr_shared->input_system & 0xFFu);
        case 4: return 0xFF00u | (g_zzr_shared->input_dswa & 0xFFu);
        default:return 0xFF00u | (g_zzr_shared->input_dswb & 0xFFu);
        }
    }
    if (addr == 0x3E0000u)
        return 0xFFFFu;
    if (addr == 0x3E0002u)
        return 0xFF00u | pc060_comm_read();

    mark_bad_read(addr);
    return 0xFFFFu;
}

zzr_u32 rastan_m68k_read32(zzr_u32 addr)
{
    zzr_u32 hi;
    zzr_u32 lo;
    if (addr & 1u) {
        g_zzr_memstats.bus_error_count++;
        g_zzr_memstats.last_bad_addr = addr;
        return 0xFFFFFFFFu;
    }
    hi = rastan_m68k_read16(addr);
    lo = rastan_m68k_read16(addr + 2u);
    return (hi << 16) | lo;
}

void rastan_m68k_write8(zzr_u32 addr, zzr_u8 data)
{
    addr &= 0x00FFFFFFu;

    if (addr >= ZZR_WORK_RAM_BASE && addr < ZZR_WORK_RAM_BASE + ZZR_WORK_RAM_SIZE) {
        work_ram[addr - ZZR_WORK_RAM_BASE] = data;
        return;
    }
    if (addr >= ZZR_PALETTE_BASE && addr < ZZR_PALETTE_BASE + ZZR_PALETTE_SIZE) {
        palette_ram[addr - ZZR_PALETTE_BASE] = data;
        g_zzr_memstats.palette_writes++;
        return;
    }
    if (addr >= ZZR_TILEMAP_BASE && addr < ZZR_TILEMAP_BASE + ZZR_TILEMAP_SIZE) {
        tilemap_ram[addr - ZZR_TILEMAP_BASE] = data;
        g_zzr_memstats.tilemap_writes++;
        return;
    }
    if (addr >= ZZR_SPRITE_BASE && addr < ZZR_SPRITE_BASE + ZZR_SPRITE_SIZE) {
        sprite_ram[addr - ZZR_SPRITE_BASE] = data;
        g_zzr_memstats.spriteram_writes++;
        g_zzr_memstats.pc090oj_writes++;
        return;
    }
    if (addr == 0x3E0001u) {
        pc060_port_write(data);
        return;
    }
    if (addr == 0x3E0003u) {
        pc060_comm_write(data);
        return;
    }
    if (addr == 0x350008u || addr == 0x350009u)
        return;

    mark_bad_write(addr, data);
}

void rastan_m68k_write16(zzr_u32 addr, zzr_u16 data)
{
    addr &= 0x00FFFFFFu;
    if (addr & 1u) {
        g_zzr_memstats.bus_error_count++;
        g_zzr_memstats.last_bad_addr = addr;
        g_zzr_memstats.last_bad_value = data;
        return;
    }

    if (addr >= ZZR_WORK_RAM_BASE && addr < ZZR_WORK_RAM_BASE + ZZR_WORK_RAM_SIZE - 1u) {
        be16_write(work_ram + addr - ZZR_WORK_RAM_BASE, data);
        return;
    }
    if (addr >= ZZR_PALETTE_BASE && addr < ZZR_PALETTE_BASE + ZZR_PALETTE_SIZE - 1u) {
        be16_write(palette_ram + addr - ZZR_PALETTE_BASE, data);
        g_zzr_memstats.palette_writes++;
        return;
    }
    if (addr >= ZZR_TILEMAP_BASE && addr < ZZR_TILEMAP_BASE + ZZR_TILEMAP_SIZE - 1u) {
        be16_write(tilemap_ram + addr - ZZR_TILEMAP_BASE, data);
        g_zzr_memstats.tilemap_writes++;
        return;
    }
    if (addr >= ZZR_SPRITE_BASE && addr < ZZR_SPRITE_BASE + ZZR_SPRITE_SIZE - 1u) {
        be16_write(sprite_ram + addr - ZZR_SPRITE_BASE, data);
        g_zzr_memstats.spriteram_writes++;
        g_zzr_memstats.pc090oj_writes++;
        return;
    }

    if (addr == 0x350008u)
        return;
    if (addr == 0x380000u) {
        sprite_ctrl = data;
        (void)sprite_ctrl;
        return;
    }
    if (addr == 0x3C0000u) {
        g_zzr_memstats.watchdog_writes++;
        return;
    }
    if (addr == 0x3E0000u) {
        pc060_port_write((zzr_u8)data);
        return;
    }
    if (addr == 0x3E0002u) {
        pc060_comm_write((zzr_u8)data);
        return;
    }
    if (addr >= 0xC20000u && addr <= 0xC20002u) {
        yscroll_regs[(addr - 0xC20000u) >> 1] = data;
        g_zzr_memstats.yscroll_writes++;
        return;
    }
    if (addr >= 0xC40000u && addr <= 0xC40002u) {
        xscroll_regs[(addr - 0xC40000u) >> 1] = data;
        g_zzr_memstats.xscroll_writes++;
        return;
    }
    if (addr >= 0xC50000u && addr <= 0xC50002u) {
        ctrl_regs[(addr - 0xC50000u) >> 1] = data;
        g_zzr_memstats.ctrl_writes++;
        return;
    }

    mark_bad_write(addr, data);
}

void rastan_m68k_write32(zzr_u32 addr, zzr_u32 data)
{
    if (addr & 1u) {
        g_zzr_memstats.bus_error_count++;
        g_zzr_memstats.last_bad_addr = addr;
        g_zzr_memstats.last_bad_value = data;
        return;
    }
    rastan_m68k_write16(addr, (zzr_u16)(data >> 16));
    rastan_m68k_write16(addr + 2u, (zzr_u16)data);
}
