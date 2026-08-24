/*
 * zzrastan_sound.c - ZZRastan A1: Rastan sound board, SILENT (no PCM yet).
 *
 * Hardware (rastan.cpp, verified):
 *   Z80 @ 4 MHz (XTAL 16MHz/4)   - sound CPU, ROM b04-19.49
 *   YM2151 @ 4 MHz               - FM (A2)
 *   MSM5205 @ 384 kHz, S48_4B    - 8 kHz ADPCM (A3)
 *   PC060HA "ciu"                - 68k <-> Z80 comms, NMIs the Z80
 *
 * Z80 map (rastan.cpp sound_map):
 *   0000-3fff ROM
 *   4000-7fff banked ROM   (bank selected by the YM2151 output port)
 *   8000-8fff RAM (4 KB)
 *   9000-9001 YM2151 read/write
 *   a000      PC060HA slave_port_w
 *   a001      PC060HA slave_comm_r/w
 *   b000      MSM5205 start address (bits 15-8)
 *   c000      MSM5205 start
 *   d000      MSM5205 stop
 *
 * 68k side: 0x3e0001 master_port_w, 0x3e0003 master_comm_r/w.
 *
 * A1 scope: run the Z80, deliver PC060HA NMIs, log what it does. YM/MSM registers are latched
 * and counted but produce NO audio. Interleave: 10 slices per frame (600 Hz) exactly like MAME,
 * so the Z80 picks up commands promptly.
 *
 * ASCII only.
 */

#include "zzrastan.h"
#include "cz80.h"
extern zzr_u32 zzrastan_pmu_cycles(void);

/* 64-bit cumulative profiling. 32-bit accumulators overflow: at 666 cyc/us and ~1200 frames the
   wrap point is ~5456 us/frame, and sound_slices measured 5421 - so frame_work/pace_wait had
   already wrapped and the global budget was wrong. */
unsigned long long g_prof_z80 = 0, g_prof_ymcore = 0, g_prof_ympost = 0;
unsigned long long g_prof_timers = 0, g_prof_slice = 0;

/* Boot event ring: the FIRST 32 events, so we can see the order of EF / commands / key-on /
   pcm_enable. Core1-local, does not go through the Z80 memory maps. */
#define ZZR_BOOTEV_MAX 32
zzr_u32 g_bootev[ZZR_BOOTEV_MAX][4];   /* frame, type, value, flags */
zzr_u32 g_bootev_n = 0;
void zzr_bootev(zzr_u32 type, zzr_u32 value);

/* YM2151 (ymfm) C API - see zzrastan_ym.cpp */
extern void zzr_ym_reset(void);
extern void zzr_ym_write_addr(unsigned char v);
extern void zzr_ym_write_data(unsigned char v);
extern unsigned char zzr_ym_read_status(void);
extern int  zzr_ym_advance(unsigned int clocks);
extern void zzr_ym_generate(short *dst, unsigned int count);
extern unsigned int zzr_ym_sample_rate(unsigned int clock);
extern void zzr_ym_get_stats(unsigned int *out);
extern int  zzr_ym_irq_state(void);
/* A3: MSM5205 ADPCM (voices, percussion) */
extern void zzrastan_msm_reset(void);
extern void zzrastan_msm_address_w(zzr_u8 data);
extern void zzrastan_msm_start_w(void);
extern void zzrastan_msm_stop_w(void);
extern void zzrastan_msm_mix(short *dst, unsigned int count, unsigned int out_rate);
const zzr_u8 *g_zzr_adpcm_rom;
#define ZZR_YM_JAREK_ACTIVE 1   /* Jarek owns timers/status/IRQ (see zzrastan_ym.cpp) */
/* FM FIFO filled on emulated time; drained by the ring producer (zzrastan_audio.c). */
#define ZZR_FM_FIFO_SAMPLES 1024
zzr_s16  g_fm_fifo[ZZR_FM_FIFO_SAMPLES];
zzr_u32  g_fm_wr, g_fm_rd;
static zzr_u32 g_fm_acc;
#define ZZR_YM_OUT_RATE 22050u
extern void zzrastan_audio_pump(void);
zzr_u32 g_aq_generated, g_aq_enqueued, g_aq_drop_old, g_aq_highwater, g_aq_pumps;
zzr_u32 g_aq_discarded_off;                       /* generated while PCM output was unavailable */
zzr_u32 g_aq_act_generated, g_aq_act_enqueued;    /* statistics for the PCM-ACTIVE window only */
zzr_u32 g_aq_act_drop, g_aq_act_high, g_aq_act_pumps;
static int g_pcm_was_on;

/* PicoDrive's CZ80 build resolves memory through global page tables instead of the callbacks
   ("unused (hacked in)" in z80if.c). Each entry is (ptr >> 1); MAP_FLAG marks a handler.
   Z80_MEM_SHIFT = 10 -> 1 KB pages, 64 entries for the 64 KB Z80 space. */
#define ZZR_Z80_MEM_SHIFT 10
#define ZZR_Z80_PAGES     (0x10000 >> ZZR_Z80_MEM_SHIFT)
#define ZZR_MAP_FLAG      ((uptr)1 << (sizeof(uptr) * 8 - 1))
uptr z80_read_map [ZZR_Z80_PAGES];
uptr z80_write_map[ZZR_Z80_PAGES];

#define Z80_CLOCK_HZ    4000000u
#define FRAME_HZ        60u
#define SLICES_PER_FRAME 10u
/* 4000000 / 60 / 10 = 6666.7 -> keep a fractional accumulator so we do not drift. */
#define Z80_CYC_PER_SLICE_NUM (Z80_CLOCK_HZ)
#define Z80_CYC_PER_SLICE_DEN (FRAME_HZ * SLICES_PER_FRAME)

static cz80_struc g_z80;
static zzr_u8  g_z80_ram[0x1000];      /* 8000-8fff */
static const zzr_u8 *g_z80_rom;        /* 64 KB at ZZR_ARM_Z80ROM */
static zzr_u32 g_bank;                 /* 4000-7fff bank base within the ROM */
static int     g_snd_ready;
static zzr_u32 g_cyc_acc;              /* fractional cycle accumulator */

/* --- PC060HA / TC0140SYT, bit-exact ------------------------------------------------------------
 * Behavioural reference: MAME's pc060ha (BSD-3-Clause). Commands travel as NIBBLE PAIRS through
 * four slots; the SECOND nibble of a pair sets the "full" flag. Four independent status bits,
 * active high, directions separate:
 *     0x01 master->slave pair 0/1 full     0x04 slave->master pair 0/1 full
 *     0x02 master->slave pair 2/3 full     0x08 slave->master pair 2/3 full
 * Port writes ONLY select a mode. NMI enable/disable (modes 5/6) happen on the DATA write (A001),
 * never on the port write (A000). Mode counters run 0->1->2->3->4 and stop at 4 (master mode 4 is
 * the Z80 RESET line; slave mode 4 reads status). The Z80 NMI is a LEVEL from
 * (status & 0x03) && nmi_enabled: asserted on the rising edge, released on acknowledge.
 */
static zzr_u8  g_ym_reg;
static int     g_ym_irq;

#define CIU_S01_FULL  0x01
#define CIU_S23_FULL  0x02
#define CIU_M01_FULL  0x04
#define CIU_M23_FULL  0x08

static zzr_u8 g_mainmode, g_submode;
static zzr_u8 g_slavedata[4];
static zzr_u8 g_masterdata[4];
static zzr_u8 g_ciu_status;
static int    g_ciu_nmi_enabled;
static int    g_ciu_nmi_line;
static int    g_z80_reset_held;

/* The YM2151 IRQ line must reach CZ80 the instant it changes. Rastan's IM1 handler acknowledges
   Timer A by writing reg 0x14 = 0x15 BEFORE its EI/RET, i.e. in the middle of Cz80_Exec(); if we
   only refreshed the line after the whole ~6667-cycle slice, that EI would still see a stale high
   IRQ and re-enter the ISR forever - which is why the main loop never dequeued a command and
   key-on was never written. */
/* Called by the Jarek IRQ handler - the core is the single authority for the YM IRQ line. */
void zzr_z80_set_irq(int level)
{
    if (level != g_ym_irq) {
        g_ym_irq = level;
        Cz80_Set_IRQ(&g_z80, 0, level ? 1 : 0);
        if (level) g_zzr_shared->snd_ym_irqs++;
    }
}

/* With the Jarek backend the core calls zzr_z80_set_irq() itself from ym2151_timer_over(), so
   polling the IRQ state here would be a SECOND authority driving the same line. Kept as a no-op
   to leave the call sites untouched; it still works if the ymfm backend is selected again. */
static void sync_ym_irq(void)
{
#if !defined(ZZR_YM_JAREK_ACTIVE)
    int irq = zzr_ym_irq_state();
    if (irq != g_ym_irq) {
        g_ym_irq = irq;
        Cz80_Set_IRQ(&g_z80, 0, irq ? 1 : 0);
        if (irq) g_zzr_shared->snd_ym_irqs++;
    }
#endif
}

static void ciu_update_nmi(void)
{
    int level = g_ciu_nmi_enabled &&
                ((g_ciu_status & (CIU_S01_FULL | CIU_S23_FULL)) != 0);
    if (level != g_ciu_nmi_line) {
        g_ciu_nmi_line = level;
        Cz80_Set_IRQ(&g_z80, IRQ_LINE_NMI, level ? 1 : 0);
        if (level) g_zzr_shared->snd_nmi_count++;
    }
}

static UINT8 z80_read(UINT32 a);

void zzr_sound_bankswitch(unsigned int bank)
{
    zzr_u32 nb = (zzr_u32)(bank & 3u) * 0x4000u;
    if (nb != g_bank) {
        g_bank = nb;
        g_zzr_shared->snd_bank_changes++;
        Cz80_Set_Fetch(&g_z80, 0x4000, 0x7FFF, (FPTR)(g_z80_rom + g_bank));
        {   /* keep the direct read map in step with the new bank */
            zzr_u32 i;
            for (i = (0x4000u >> ZZR_Z80_MEM_SHIFT); i < (0x8000u >> ZZR_Z80_MEM_SHIFT); i++)
                z80_read_map[i] = ((uptr)(g_z80_rom + g_bank - 0x4000u) >> 1);
        }
        /* self-check: the direct read map must return exactly the banked ROM bytes */
        if (z80_read(0x4000u) != g_z80_rom[g_bank] ||
            z80_read(0x4123u) != g_z80_rom[g_bank + 0x123u])
            g_zzr_shared->snd_bank_mismatch++;
        g_zzr_shared->snd_bank_current = g_bank >> 14;
    }
}

void zzrastan_sound_68k_write(zzr_u32 addr, zzr_u8 data)
{
    if ((addr & 3u) == 1u) {            /* 0x3e0001 master_port_w: SELECT ONLY */
        g_mainmode = data & 0x0Fu;
        g_zzr_shared->snd_port_fwd++;
        g_zzr_shared->snd_last_mainmode = g_mainmode;
        return;
    }
    data &= 0x0Fu;                      /* 0x3e0003 master_comm_w */
    switch (g_mainmode) {
        case 0: g_slavedata[0] = data; g_mainmode = 1; break;
        case 1: g_slavedata[1] = data; g_mainmode = 2;
                g_ciu_status |= CIU_S01_FULL;
                g_zzr_shared->snd_pair01_to_z80++;
                g_zzr_shared->snd_cmds_68k++;
                g_zzr_shared->snd_last_cmd = (zzr_u32)(g_slavedata[0] | (g_slavedata[1] << 4));
                ciu_update_nmi();
                break;
        case 2: g_slavedata[2] = data; g_mainmode = 3; break;
        case 3: g_slavedata[3] = data; g_mainmode = 4;
                g_ciu_status |= CIU_S23_FULL;
                g_zzr_shared->snd_pair23_to_z80++;
                g_zzr_shared->snd_cmds_68k++;
                ciu_update_nmi();
                break;
        case 4:                          /* Z80 RESET line (high-to-low resets the sound CPU) */
                if (data != 0) {
                    g_z80_reset_held = 1;
                } else if (g_z80_reset_held) {
                    g_z80_reset_held = 0;
                    Cz80_Reset(&g_z80);
                    Cz80_Set_Reg(&g_z80, CZ80_SP, 0xFFFF);
                    g_ciu_nmi_line = 0;
                    g_zzr_shared->snd_z80_resets++;
                }
                break;
        default: break;
    }
}

zzr_u8 zzrastan_sound_68k_read(zzr_u32 addr)
{
    zzr_u8 res = 0;
    if ((addr & 3u) != 3u) return 0;
    switch (g_mainmode) {
        case 0: res = g_masterdata[0]; g_mainmode = 1; break;
        case 1: res = g_masterdata[1]; g_mainmode = 2;
                g_ciu_status &= (zzr_u8)~CIU_M01_FULL; break;
        case 2: res = g_masterdata[2]; g_mainmode = 3; break;
        case 3: res = g_masterdata[3]; g_mainmode = 4;
                g_ciu_status &= (zzr_u8)~CIU_M23_FULL; break;
        case 4: res = g_ciu_status; break;
        default: res = 0; break;
    }
    return res;
}

static void ciu_slave_port_w(zzr_u8 d)
{
    g_submode = d & 0x0Fu;               /* SELECT ONLY */
}

static zzr_u8 ciu_slave_comm_r(void)
{
    zzr_u8 res = 0;
    switch (g_submode) {
        case 0: res = g_slavedata[0]; g_submode = 1; break;
        case 1: res = g_slavedata[1]; g_submode = 2;
                {
                    zzr_u32 c = (zzr_u32)(g_slavedata[0] | (g_slavedata[1] << 4));
                    g_zzr_shared->snd_cmd_seen_by_z80 = c;
                    g_zzr_shared->snd_cmd_complete++;
                    zzr_bootev(1u, c);                 /* 1 = command completed */
                    if (c == 0xEFu) { g_zzr_shared->snd_ef_seen++; zzr_bootev(2u, c); }
                    if (c == 0xEEu) g_zzr_shared->snd_ee_seen++;
                }
                g_ciu_status &= (zzr_u8)~CIU_S01_FULL;
                g_zzr_shared->snd_ack01_by_z80++;
                ciu_update_nmi(); break;
        case 2: res = g_slavedata[2]; g_submode = 3; break;
        case 3: res = g_slavedata[3]; g_submode = 4;
                g_ciu_status &= (zzr_u8)~CIU_S23_FULL;
                g_zzr_shared->snd_ack23_by_z80++;
                ciu_update_nmi(); break;
        case 4: res = g_ciu_status; break;
        default: res = 0; break;
    }
    return res;
}

static void ciu_slave_comm_w(zzr_u8 d)
{
    d &= 0x0Fu;
    switch (g_submode) {
        case 0: g_masterdata[0] = d; g_submode = 1; break;
        case 1: g_masterdata[1] = d; g_submode = 2;
                g_ciu_status |= CIU_M01_FULL; break;
        case 2: g_masterdata[2] = d; g_submode = 3; break;
        case 3: g_masterdata[3] = d; g_submode = 4;
                g_ciu_status |= CIU_M23_FULL; break;
        case 4: break;
        case 5: g_ciu_nmi_enabled = 0; g_zzr_shared->snd_nmi_disables++;
                ciu_update_nmi(); break;
        case 6: g_ciu_nmi_enabled = 1; g_zzr_shared->snd_nmi_enables++;
                ciu_update_nmi(); break;
        default: break;
    }
}

/* --- Z80 bus --- */
static UINT8 z80_read(UINT32 a)
{
    a &= 0xFFFFu;
    if (a < 0x4000u) {
        /* Track table at 0x2300 (2 bytes per command). A read here proves the dispatcher reached
           CALL 0x02AA with that command: index = (a - 0x2300) / 2. */
        if (a == 0x02FEu || a == 0x02FFu) {
            /* fetch of the track-init routine entry = we got there */
            if (a == 0x02FEu) g_zzr_shared->snd_trackinit_entries++;
        }
        if (a >= 0x2300u && a < 0x2360u) {
            g_zzr_shared->snd_table_reads++;
            g_zzr_shared->snd_table_last_cmd = (a - 0x2300u) >> 1;
            if (((a - 0x2300u) >> 1) == 0x1Au) g_zzr_shared->snd_track1a_count++;
        }
        return g_z80_rom[a];
    }
    if (a < 0x8000u) return g_z80_rom[(g_bank + (a - 0x4000u)) & 0xFFFFu];
    if (a < 0x9000u) {
        if (a >= 0x8F02u && a <= 0x8F11u) {
            g_zzr_shared->snd_qslot_reads++;
            g_zzr_shared->snd_q_last_read = g_z80_ram[a & 0x0FFFu];
            if (g_z80_ram[a & 0x0FFFu] == 0xEFu) g_zzr_shared->snd_ef_dequeued++;
        }
        return g_z80_ram[a & 0x0FFFu];
    }
    if (a < 0x9002u) {
        /* MAME maps 0x9000-0x9001 to ym2151_device::read(offset), and ymfm distinguishes them:
           offset 0 is the (unused) data port and returns 0xFF, offset 1 is the status port.
           Returning the status for BOTH - as we did - feeds the sound program a value it never
           expects at 0x9000, which can trap it in a polling loop. */
        g_zzr_shared->snd_ym_reads++;
        if ((a & 1u) == 0u) { g_zzr_shared->snd_ym_read0++; return 0xFFu; }
        {
            /* The sound program's YM write routine (ROM 0x158C) spins on BIT 7 of this port:
                 158D: BIT 7,(HL) / 158F: JR nz,158D
               So bit 7 MUST clear or the Z80 can never write a register. Record what we return. */
            zzr_u8 st = zzr_ym_read_status();
            g_zzr_shared->snd_last_status = st;
            if (st & 0x80u) g_zzr_shared->snd_status_busy_reads++;
            return st;
        }
    }
    if (a == 0xA001u) return ciu_slave_comm_r();
    return 0;
}

static void z80_write(UINT32 a, UINT8 d)
{
    a &= 0xFFFFu;
    if (a >= 0x8000u && a < 0x9000u) {
        /* Cumulative instrumentation of the command queue (ROM: head 8F00, tail 8F01,
           slots 8F02..8F11). Snapshots of the final state proved misleading, so count events. */
        if (a == 0x8F26u) {
            /* ROM 0x02FE: LD A,(8F26) / AND A / RET Z - zero here aborts every track, so no
               voice is ever armed and no key-on happens. EE sets it to 0, EF sets it to 1. */
            if (d) g_zzr_shared->snd_gate_set1++; else g_zzr_shared->snd_gate_set0++;
            g_zzr_shared->snd_gate_value = d;
        }
        if (a == 0x8F00u) { g_zzr_shared->snd_qhead_writes++; }
        else if (a == 0x8F01u) { g_zzr_shared->snd_qtail_writes++; }
        else if (a >= 0x8F02u && a <= 0x8F11u) {
            g_zzr_shared->snd_qslot_writes++;
            g_zzr_shared->snd_q_last_written = d;
            if (d == 0xEFu) g_zzr_shared->snd_ef_queued++;
        }
        g_z80_ram[a & 0x0FFFu] = d;
        return;
    }
    if (a == 0x9000u) { g_ym_reg = d; zzr_ym_write_addr(d); return; }
    if (a == 0x9001u) {                                   /* YM register data */
        g_zzr_shared->snd_ym_writes++;
        g_zzr_shared->snd_last_ym_reg = g_ym_reg;
        g_zzr_shared->snd_last_ym_val = d;
        /* Z80 ROM bank. This used to arrive through ymfm_external_write, but ymfm is no longer
           the synthesis backend, so that callback is never invoked any more (bank_changes was 0
           in every recent trace: the bank was NEVER switched, which is why music tracks vanished
           once the sequencer jumped into another bank).
           YM2151 register 0x1B: bits 7-6 = CT2/CT1 -> Z80 bank, bits 1-0 = LFO waveform.
           MAME: m_audiobank->set_entry(data & 3) on the CT port value, i.e. (raw >> 6) & 3. */
        if (g_ym_reg == 0x1Bu) {
            g_zzr_shared->snd_reg1b_writes++;
            g_zzr_shared->snd_reg1b_raw = d;
            g_zzr_shared->snd_reg1b_ct  = (zzr_u32)((d >> 6) & 3u);
            zzr_sound_bankswitch((unsigned int)((d >> 6) & 3u));
        }
        zzr_ym_write_data(d);
        sync_ym_irq();          /* a register write can clear/assert IRQ immediately */
        return;
    }
    if (a == 0xA000u) { ciu_slave_port_w(d); return; }
    if (a == 0xA001u) { ciu_slave_comm_w(d); return; }
    if (a == 0xB000u) { g_zzr_shared->snd_msm_writes++; zzrastan_msm_address_w(d); return; }
    if (a == 0xC000u) { g_zzr_shared->snd_msm_writes++; zzrastan_msm_start_w();     return; }
    if (a == 0xD000u) { g_zzr_shared->snd_msm_writes++; zzrastan_msm_stop_w();      return; }
}

int zzrastan_sound_init(void)
{
    zzr_u32 i;
    g_z80_rom = (const zzr_u8 *)ZZR_ARM_Z80ROM;
    for (i = 0; i < sizeof(g_z80_ram); i++) g_z80_ram[i] = 0;
    g_bank = 0x0000u;   /* bank 0 = start of the sound ROM (MAME configure_entries) */
    g_cyc_acc = 0;
    { int k; for (k = 0; k < 4; k++) { g_slavedata[k] = 0; g_masterdata[k] = 0; } }
    g_mainmode = 0; g_submode = 0; g_ciu_status = 0;
    g_ciu_nmi_enabled = 0;
    g_ciu_nmi_line = 0; g_z80_reset_held = 0;
    g_ym_reg = 0;

    /* Route every 1 KB page through our handlers (simple and correct; the banked window and the
       chip registers all need decoding anyway). */
    /* DIRECT maps for plain memory, handlers only where they are actually needed. CZ80 decodes
       a non-flagged entry as (value<<1) + address, so we store (base - page_base) >> 1.
       0000-3FFF fixed ROM, 4000-7FFF banked ROM, 8000-8FFF RAM: direct reads.
       Writes: only RAM is real memory; ROM pages and 9000+ MMIO keep the handler. */
    for (i = 0; i < ZZR_Z80_PAGES; i++) {
        zzr_u32 page = i << ZZR_Z80_MEM_SHIFT;
        if (page < 0x4000u) {
            z80_read_map[i] = ((uptr)(g_z80_rom) >> 1);
        } else if (page < 0x8000u) {
            z80_read_map[i] = ((uptr)(g_z80_rom + g_bank - 0x4000u) >> 1);
        } else if (page < 0x9000u) {
            z80_read_map[i] = ((uptr)(g_z80_ram - 0x8000u) >> 1);
        } else {
            z80_read_map[i] = ((uptr)z80_read >> 1) | ZZR_MAP_FLAG;
        }
        if (page >= 0x8000u && page < 0x9000u)
            z80_write_map[i] = ((uptr)(g_z80_ram - 0x8000u) >> 1);
        else
            z80_write_map[i] = ((uptr)z80_write >> 1) | ZZR_MAP_FLAG;
    }

    zzr_ym_reset();
    g_ym_irq = 0;
    g_zzr_adpcm_rom = (const zzr_u8 *)ZZR_ARM_ADPCM;
    zzrastan_msm_reset();

    Cz80_Init(&g_z80);
    Cz80_Set_ReadB(&g_z80, z80_read);
    Cz80_Set_WriteB(&g_z80, z80_write);

    /* CZ80 has a SECOND table for OPCODE FETCH (CPU->Fetch[]), separate from z80_read_map which
       only serves data reads. Cz80_Init() fills it with 0xFF = RST 38h, so without Cz80_Set_Fetch
       the core never executes the ROM: it span in RST 38 forever (that was our PC=0x0038, and the
       "YM writes" were just stack pushes of the return address 0x0039 drifting through 0x9000). */
    Cz80_Set_Fetch(&g_z80, 0x0000, 0x3FFF, (FPTR)g_z80_rom);
    Cz80_Set_Fetch(&g_z80, 0x4000, 0x7FFF, (FPTR)(g_z80_rom + g_bank));
    Cz80_Set_Fetch(&g_z80, 0x8000, 0x8FFF, (FPTR)g_z80_ram);

    Cz80_Reset(&g_z80);
    Cz80_Set_Reg(&g_z80, CZ80_SP, 0xFFFF);   /* PicoDrive does this explicitly too */

    g_zzr_shared->snd_cmds_68k = 0;
    g_zzr_shared->snd_nmi_count = 0;
    g_zzr_shared->snd_ym_writes = 0;
    g_zzr_shared->snd_ym_reads = 0;
    g_zzr_shared->snd_bank_changes = 0;
    g_zzr_shared->snd_msm_writes = 0;
    g_fm_wr = g_fm_rd = 0; g_fm_acc = 0;
    g_snd_ready = 1;
    return 1;
}

/* Run one interleave slice of the Z80 (called 10x per frame, between 68k slices). */
void zzrastan_sound_slice(void)
{
    zzr_u32 cyc;
    zzr_u32 slice_t0;
    if (!g_snd_ready) return;
    if (!g_zzr_shared->snd_enable) return;
    slice_t0 = zzrastan_pmu_cycles();

    g_cyc_acc += Z80_CYC_PER_SLICE_NUM;
    cyc = g_cyc_acc / Z80_CYC_PER_SLICE_DEN;
    g_cyc_acc -= cyc * Z80_CYC_PER_SLICE_DEN;

    {
        zzr_u32 t0 = zzrastan_pmu_cycles();
        Cz80_Exec(&g_z80, (INT32)cyc);
        g_prof_z80 += (unsigned long long)(zzrastan_pmu_cycles() - t0);
    }

    /* The YM2151 runs at the same 4 MHz as the Z80, so one Z80 cycle = one chip clock here.
       Its timer IRQ is what paces the Rastan sound program - without it the Z80 just idles. */
    {
        zzr_u32 t2 = zzrastan_pmu_cycles();
        zzr_ym_advance(cyc);
        sync_ym_irq();
        g_prof_timers += (unsigned long long)(zzrastan_pmu_cycles() - t2);
    }

    /* Generate FM for exactly this slice of EMULATED time. TWO bugs lived here:
       (a) the rate was still 62500 (ymfm's native rate) while the Jarek core outputs at 22050,
           so we asked for 2.8x too many samples every slice and the FIFO could never drain;
       (b) n was clamped to the free space BEFORE generating, which stops envelopes/phases/LFO
           while the Z80 sequencer keeps running - the cause of the shredded music.
       Now: 22050/600 = 36.75 per slice, generation is UNCONDITIONAL, and an overflow drops the
       OLDEST queued audio afterwards (never the fresh samples, never a stale backlog). */
    {
        zzr_u32 n, used, free_sp, i;
        zzr_s16 blk[64];
        g_fm_acc += ZZR_YM_OUT_RATE;
        n = g_fm_acc / (FRAME_HZ * SLICES_PER_FRAME);
        g_fm_acc -= n * (FRAME_HZ * SLICES_PER_FRAME);
        if (n > 64u) n = 64u;
        if (n) {
            {
                zzr_u32 t1 = zzrastan_pmu_cycles();
                zzr_ym_generate(blk, n);
                g_prof_ymcore += (unsigned long long)(zzrastan_pmu_cycles() - t1);
            }
            zzrastan_msm_mix(blk, n, ZZR_YM_OUT_RATE);   /* A3: add the ADPCM voices */
            g_zzr_shared->snd_ym_samples += n;
            g_aq_generated += n;

            /* J1e: when PCM output is not available (before AHI is up, and again during
               teardown) the YM must still ADVANCE - phases, envelopes, LFO and timers keep
               running - but the samples must be DISCARDED, not queued. Queueing them filled the
               1024-sample FIFO in 46 ms and produced thousands of drop_old that had nothing to do
               with runtime behaviour. */
            if (!g_zzr_shared->pcm_enable) {
                /* Discard the audio but DO NOT leave the function: everything below (Z80 PC,
                   dispatcher probes, CIU/YM statistics) must still run every slice. The earlier
                   'return' here amputated the whole tail of the slice during the entire startup
                   phase, which is why the game never got going and the screen stayed black. */
                g_aq_discarded_off += n;
                if (g_pcm_was_on) {            /* PCM 1 -> 0 : freeze the active-window stats */
                    g_pcm_was_on = 0;
                    g_zzr_shared->aq_act_frozen = 1;
                }
                goto slice_tail;
            }
            if (!g_pcm_was_on) {               /* PCM 0 -> 1 : start a clean active window */
                g_pcm_was_on = 1;
                g_fm_rd = g_fm_wr;             /* drop any stale audio, keep the YM state */
                /* also drop whatever lead the ring accumulated before AHI started consuming,
                   otherwise that offset is carried for the whole session as pure latency */
                g_zzr_shared->pcm_write_pos = g_zzr_shared->pcm_read_pos;
                g_aq_act_generated = 0; g_aq_act_enqueued = 0;
                g_aq_act_drop = 0; g_aq_act_high = 0; g_aq_act_pumps = 0;
                g_zzr_shared->aq_act_frozen = 0;
            }
            g_aq_act_generated += n;

            used = (g_fm_wr - g_fm_rd) & (ZZR_FM_FIFO_SAMPLES - 1u);
            free_sp = (ZZR_FM_FIFO_SAMPLES - 1u) - used;
            if (free_sp < n) {
                zzr_u32 drop = n - free_sp;
                g_fm_rd = (g_fm_rd + drop) & (ZZR_FM_FIFO_SAMPLES - 1u);
                g_aq_drop_old += drop;
                g_aq_act_drop += drop;
            }
            for (i = 0; i < n; i++) {
                g_fm_fifo[g_fm_wr] = blk[i];
                g_fm_wr = (g_fm_wr + 1u) & (ZZR_FM_FIFO_SAMPLES - 1u);
            }
            g_aq_enqueued += n;
            g_aq_act_enqueued += n;
            used = (g_fm_wr - g_fm_rd) & (ZZR_FM_FIFO_SAMPLES - 1u);
            if (used > g_aq_highwater) g_aq_highwater = used;
            if (!g_zzr_shared->aq_act_frozen && used > g_aq_act_high) g_aq_act_high = used;
        }
        zzrastan_audio_pump();     /* drain at the same 600 Hz rate we fill */
        g_aq_pumps++;
        g_aq_act_pumps++;
    }

slice_tail:
    g_zzr_shared->snd_z80_cycles += cyc;
    g_zzr_shared->snd_z80_pc = Cz80_Get_Reg(&g_z80, CZ80_PC);

    /* --- Dispatcher probes (read-only, no behaviour change) -------------------------------
     * From the ROM disassembly:
     *   0x025C  consumer: compares queue head (8F00) and tail (8F01)
     *   0x027C  LD A,(HL) - the dequeued command
     *   0x0282  LD A,(8F27) / BIT 0 -> if set, command is parked at 8F28 and NEVER played
     *   0x028F  CP 0x2C  -> commands >= 0x2C are ignored
     *   0x0293  CALL 0x02AA -> actually play
     * We cannot hook mid-instruction, but the queue state and the gating flag live in Z80 RAM,
     * which we own, so we can simply report them. */
    g_zzr_shared->snd_q_head  = g_z80_ram[0x0F00];   /* 0x8F00 */
    g_zzr_shared->snd_q_tail  = g_z80_ram[0x0F01];   /* 0x8F01 */
    g_zzr_shared->snd_flag8f27 = g_z80_ram[0x0F27];  /* 0x8F27 gating flag */
    g_zzr_shared->snd_park8f28 = g_z80_ram[0x0F28];  /* 0x8F28 parked command */
    {
        /* snapshot the first few queue slots so we can see what the 68k actually sent */
        zzr_u32 v = 0; int k;
        for (k = 0; k < 4; k++) v |= ((zzr_u32)g_z80_ram[0x0F02 + k]) << (k * 8);
        g_zzr_shared->snd_q_slots = v;
    }
    {
        unsigned int st[20];
        zzr_ym_get_stats(st);
        g_zzr_shared->snd_tA_sets = st[0];  g_zzr_shared->snd_tB_sets = st[1];
        g_zzr_shared->snd_tA_fires = st[2]; g_zzr_shared->snd_tB_fires = st[3];
        g_zzr_shared->snd_tA_period = st[4];g_zzr_shared->snd_tB_period = st[5];
        g_zzr_shared->snd_irq_asserts = st[6]; g_zzr_shared->snd_irq_clears = st[7];
        g_zzr_shared->snd_reg10 = st[8]; g_zzr_shared->snd_reg11 = st[9];
        g_zzr_shared->snd_reg12 = st[10]; g_zzr_shared->snd_reg14 = st[11];
        g_zzr_shared->snd_key_writes = st[12];
        g_zzr_shared->snd_key_on = st[13];
        g_zzr_shared->snd_key_off = st[14];
        g_zzr_shared->snd_nonzero = st[15];
        g_zzr_shared->snd_peak = st[16];
        g_zzr_shared->snd_ciu_status = g_ciu_status;
        g_zzr_shared->aq_generated = g_aq_generated;
        g_zzr_shared->aq_enqueued  = g_aq_enqueued;
        g_zzr_shared->aq_drop_old  = g_aq_drop_old;
        g_zzr_shared->aq_highwater = g_aq_highwater;
        g_zzr_shared->aq_pumps     = g_aq_pumps;
        g_zzr_shared->aq_fifo_used = (g_fm_wr - g_fm_rd) & (ZZR_FM_FIFO_SAMPLES - 1u);
        g_zzr_shared->aq_discarded_off = g_aq_discarded_off;
        g_zzr_shared->aq_act_generated = g_aq_act_generated;
        g_zzr_shared->aq_act_enqueued  = g_aq_act_enqueued;
        g_zzr_shared->aq_act_drop      = g_aq_act_drop;
        g_zzr_shared->aq_act_high      = g_aq_act_high;
        g_zzr_shared->aq_act_pumps     = g_aq_act_pumps;
        g_zzr_shared->pace_period_cycles = 11111111u;
        {
            extern zzr_u32 g_msm_starts, g_msm_stops, g_msm_nibbles, g_msm_peak;
            g_zzr_shared->msm_starts = g_msm_starts;
            g_zzr_shared->msm_stops = g_msm_stops;
            g_zzr_shared->msm_nibbles = g_msm_nibbles;
            g_zzr_shared->msm_peak = g_msm_peak;
        }
    }
    g_prof_slice += (unsigned long long)(zzrastan_pmu_cycles() - slice_t0);
    /* publish as microseconds (already divided) so the 32-bit shared slots cannot overflow */
    {
        zzr_u32 f = g_zzr_shared->prof_frames; if (!f) f = 1;
        g_zzr_shared->snd_prof_z80   = (zzr_u32)(g_prof_z80    / 666ULL / f);
        g_zzr_shared->snd_prof_ym    = (zzr_u32)(g_prof_ymcore / 666ULL / f);
        g_zzr_shared->snd_prof_timer = (zzr_u32)(g_prof_timers / 666ULL / f);
        g_zzr_shared->snd_prof_slice_total = (zzr_u32)(g_prof_slice / 666ULL / f);
        g_zzr_shared->snd_prof_ympost = (zzr_u32)(g_prof_ympost / 666ULL / f);
    }
}


/* Boot event recorder - defined at the end so it can see the Z80 RAM. */
void zzr_bootev(zzr_u32 type, zzr_u32 value)
{
    if (g_bootev_n >= ZZR_BOOTEV_MAX) return;
    g_bootev[g_bootev_n][0] = g_zzr_shared->frame_counter;
    g_bootev[g_bootev_n][1] = type;
    g_bootev[g_bootev_n][2] = value;
    g_bootev[g_bootev_n][3] = (zzr_u32)(g_z80_ram[0x0F26] & 0xFFu) |
                              (g_zzr_shared->pcm_enable ? 0x100u : 0u);
    g_bootev_n++;
    g_zzr_shared->bootev_count = g_bootev_n;
    {
        zzr_u32 i, j;
        for (i = 0; i < g_bootev_n; i++)
            for (j = 0; j < 4u; j++)
                g_zzr_shared->bootev[i][j] = g_bootev[i][j];
    }
}
