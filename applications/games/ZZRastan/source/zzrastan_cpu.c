#include "zzrastan.h"

struct Cyclone g_zzr_cpu;
static zzr_u32 total_m68k_cycles;
static zzr_u32 cycle_fraction;
static zzr_u32 last_pc;
static zzr_u32 same_pc_frames;

static zzr_u32 cpu_checkpc(zzr_u32 pc)
{
    /* Cyclone keeps pc as a HOST pointer with pc_68k = pc - membase, and fetches
     * opcodes DIRECTLY from that pointer. For A1 the 68000 executes from the main
     * ROM (0x000000-0x05FFFF), whose host copy is g_zzr_main_rom (the same base the
     * read callbacks use). membase must therefore be that host base, so that the
     * direct fetch reads g_zzr_main_rom[addr] and pc_68k = (base+addr) - base = addr.
     * Previously membase was set to 0 and the bare 68k address was returned, so
     * Cyclone fetched from ARM physical 0x0003A000 -> data abort. */
    zzr_u32 addr = (pc - (zzr_u32)g_zzr_cpu.membase) & 0x00FFFFFFu;
    zzr_u32 base;

    /* Defensive guard (B0b): the 68000 is only ever expected to execute from the program ROM.
     * If it ever fetches from work RAM, stop cleanly and report instead of silently fetching
     * from the wrong base. Return a SAFE base so the diagnosis survives: returning 0 would make
     * Cyclone fetch at ARM physical 0 and raise a DATA_ABORT that would overwrite this error. */
    if (addr >= ZZR_WORK_RAM_BASE && addr < ZZR_WORK_RAM_BASE + ZZR_WORK_RAM_SIZE) {
        g_zzr_shared->exec_from_ram_count++;
        g_zzr_shared->last_exec_addr = addr;
        g_zzr_shared->error = ZZR_ERR_EXEC_FROM_RAM;
        g_zzr_shared->status = ZZR_ST_ERROR;
        base = (zzr_u32)g_zzr_main_rom;
        g_zzr_cpu.membase = base;
        return base;
    }

    base = (zzr_u32)g_zzr_main_rom;
    g_zzr_cpu.membase = base;
    return base + addr;
}

static int cpu_irq_ack(int level)
{
    (void)level;
    g_zzr_cpu.irq = 0;
    return CYCLONE_INT_ACK_AUTOVECTOR;
}

static void cpu_reset_callback(void)
{
}

static int cpu_unrecognized_callback(void)
{
    g_zzr_memstats.illegal_opcode_count++;
    g_zzr_shared->error = ZZR_ERR_CPU_ILLEGAL;
    g_zzr_shared->status = ZZR_ST_ERROR;
    return 1;
}

static void clear_cpu(void)
{
    zzr_u8 *p = (zzr_u8 *)&g_zzr_cpu;
    zzr_u32 n = (zzr_u32)sizeof(g_zzr_cpu);
    while (n-- != 0u)
        *p++ = 0;
}

int zzrastan_cpu_init_and_reset(void)
{
    zzr_u32 pc;
    zzr_u32 sp;

    if (!g_zzr_rom_ready)
        return 0;

    CycloneInit();
    clear_cpu();
    g_zzr_cpu.checkpc = cpu_checkpc;
    g_zzr_cpu.read8 = rastan_m68k_read8;
    g_zzr_cpu.read16 = rastan_m68k_read16;
    g_zzr_cpu.read32 = rastan_m68k_read32;
    g_zzr_cpu.write8 = rastan_m68k_write8;
    g_zzr_cpu.write16 = rastan_m68k_write16;
    g_zzr_cpu.write32 = rastan_m68k_write32;
    g_zzr_cpu.fetch8 = rastan_m68k_read8;
    g_zzr_cpu.fetch16 = rastan_m68k_read16;
    g_zzr_cpu.fetch32 = rastan_m68k_read32;
    g_zzr_cpu.IrqCallback = cpu_irq_ack;
    g_zzr_cpu.ResetCallback = cpu_reset_callback;
    g_zzr_cpu.UnrecognizedCallback = cpu_unrecognized_callback;
    g_zzr_cpu.flags = 4;

    CycloneReset(&g_zzr_cpu);
    pc = g_zzr_cpu.pc - (zzr_u32)g_zzr_cpu.membase;
    sp = g_zzr_cpu.a[7];
    if (pc != g_zzr_shared->reset_pc || sp != g_zzr_shared->reset_ssp) {
        g_zzr_shared->error = ZZR_ERR_CPU_RESET;
        return 0;
    }

    total_m68k_cycles = 0;
    cycle_fraction = 0;
    last_pc = 0xFFFFFFFFu;
    same_pc_frames = 0;
    return 1;
}

void zzrastan_raise_irq5(void)
{
    g_zzr_cpu.irq = 5;
    g_zzr_shared->debug.irq5_count++;
    (void)CycloneFlushIrq(&g_zzr_cpu);
}

void zzrastan_run_one_frame_debug(void)
{
    zzr_u32 budget;
    zzr_u32 used;
    zzr_u32 t0;
    zzr_u32 t1;
    extern zzr_u32 zzrastan_pmu_cycles(void);
static unsigned long long g_pf_cyc = 0;

    cycle_fraction += ZZR_M68K_CLOCK;
    budget = cycle_fraction / ZZR_REFRESH_HZ;
    cycle_fraction -= budget * ZZR_REFRESH_HZ;

    t0 = zzrastan_pmu_cycles();
    /* A1: 10 interleave slices per frame (600 Hz), exactly like MAME, so the Z80 picks up
       PC060HA commands promptly instead of a frame late. */
    {
        extern void zzrastan_sound_slice(void);
        zzr_u32 slice, done = 0, per = budget / 10u;
        for (slice = 0; slice < 10u; slice++) {
            zzr_u32 want = (slice == 9u) ? (budget - done) : per;
            {
                zzr_u32 c0 = zzrastan_pmu_cycles();
                g_zzr_cpu.cycles = (int)want;
                CycloneRun(&g_zzr_cpu);
                g_pf_cyc += (unsigned long long)(zzrastan_pmu_cycles() - c0);
                {
                    zzr_u32 f = g_zzr_shared->prof_frames; if (!f) f = 1;
                    g_zzr_shared->prof_cyclone = (zzr_u32)(g_pf_cyc / 666ULL / f);
                }
            }
            done += want;
            zzrastan_sound_slice();
        }
    }
    t1 = zzrastan_pmu_cycles();

    used = budget;   /* sliced execution: account the full budget */
    total_m68k_cycles += used;

    zzrastan_raise_irq5();
    g_zzr_shared->frame_counter++;
    g_zzr_shared->debug.frame_count = g_zzr_shared->frame_counter;
    zzrastan_debug_publish(t1 - t0, used);
}

void zzrastan_debug_publish(zzr_u32 frame_cycles, zzr_u32 m68k_cycles)
{
    volatile zzrastan_debug_t *d = &g_zzr_shared->debug;
    zzr_u32 pc = g_zzr_cpu.pc - (zzr_u32)g_zzr_cpu.membase;

    if (pc == last_pc)
        same_pc_frames++;
    else
        same_pc_frames = 0;
    last_pc = pc;

    d->magic = ZZR_MAGIC;
    d->version = ZZR_VERSION;
    d->status = g_zzr_shared->status;
    d->error = g_zzr_shared->error;
    d->frame_count = g_zzr_shared->frame_counter;
    d->fps = ZZR_REFRESH_HZ;
    d->m68k_pc = pc;
    d->m68k_sp = g_zzr_cpu.a[7];
    d->m68k_sr = CycloneGetSr(&g_zzr_cpu);
    d->m68k_cycles = total_m68k_cycles;
    d->palette_writes = g_zzr_memstats.palette_writes;
    d->tilemap_writes = g_zzr_memstats.tilemap_writes;
    d->spriteram_writes = g_zzr_memstats.spriteram_writes;
    d->yscroll_writes = g_zzr_memstats.yscroll_writes;
    d->xscroll_writes = g_zzr_memstats.xscroll_writes;
    d->ctrl_writes = g_zzr_memstats.ctrl_writes;
    d->pc090oj_writes = g_zzr_memstats.pc090oj_writes;
    d->pc060ha_cmd_count = g_zzr_memstats.pc060ha_cmd_count;
    d->pc060ha_port_writes = g_zzr_memstats.pc060ha_port_writes;
    d->pc060ha_comm_reads = g_zzr_memstats.pc060ha_comm_reads;
    d->watchdog_writes = g_zzr_memstats.watchdog_writes;
    d->bad_read_count = g_zzr_memstats.bad_read_count;
    d->bad_write_count = g_zzr_memstats.bad_write_count;
    d->bus_error_count = g_zzr_memstats.bus_error_count;
    d->illegal_opcode_count = g_zzr_memstats.illegal_opcode_count;
    d->frame_total_us = frame_cycles / 667u;
    d->m68k_us = frame_cycles / 667u;
    d->video_us = 0;
    d->audio_us = 0;
    d->last_bad_addr = g_zzr_memstats.last_bad_addr;
    d->last_bad_value = g_zzr_memstats.last_bad_value;
    d->last_sound_cmd = g_zzr_memstats.last_sound_cmd;
    d->pc060ha_port = g_zzr_memstats.pc060ha_port;
    d->same_pc_frames = same_pc_frames;
    d->cpu_state_flags = (zzr_u32)g_zzr_cpu.state_flags;
    (void)m68k_cycles;
    __asm__ volatile("dsb" ::: "memory");
}
