#include "zzrastan.h"
extern zzr_u32 zzrastan_pmu_cycles(void);
extern void zzrastan_poke8(zzr_u32 addr, zzr_u8 value);
extern void zzr_bootev(zzr_u32 type, zzr_u32 value);
static unsigned long long g_pf_work = 0, g_pf_pace = 0, g_pf_aud = 0;
static zzr_u32 g_pcm_was = 0;
void zzrastan_audio_reset(void);
int  zzrastan_sound_init(void);

/* Run C++ static constructors (ymfm). Must happen before any use of those objects. */
extern void (*__init_array_start[])(void);
extern void (*__init_array_end[])(void);
static void zzr_run_init_array(void)
{
    void (**f)(void);
    for (f = __init_array_start; f != __init_array_end; f++)
        if (*f) (*f)();
}
void zzrastan_audio_produce(void);

volatile zzrastan_shared_t *g_zzr_shared =
    (volatile zzrastan_shared_t *)ZZR_ARM_SHARED;

extern void mmu_init(void);
extern void zz_install_vectors(void);
extern char _bss_start;
extern char _bss_end;

static int g_running;
static zzr_u32 g_last_cmd_seq;

static void zero_bss(void)
{
    char *p = &_bss_start;
    while (p < &_bss_end)
        *p++ = 0;
}

static void pmu_enable(void)
{
    zzr_u32 v = 1u | (1u << 2);
    zzr_u32 en = 0x80000000u;
    __asm__ volatile("mcr p15,0,%0,c9,c12,0" :: "r"(v));
    __asm__ volatile("mcr p15,0,%0,c9,c12,1" :: "r"(en));
}

zzr_u32 zzrastan_pmu_cycles(void)
{
    zzr_u32 v;
    __asm__ volatile("mrc p15,0,%0,c9,c13,0" : "=r"(v));
    return v;
}

static void shared_defaults(void)
{
    volatile zzr_u32 *p = (volatile zzr_u32 *)(void *)g_zzr_shared;
    zzr_u32 i;
    for (i = 0; i < (zzr_u32)(sizeof(*g_zzr_shared) / sizeof(zzr_u32)); i++)
        p[i] = 0;

    g_zzr_shared->magic = ZZR_MAGIC;
    g_zzr_shared->version = ZZR_VERSION;
    g_zzr_shared->status = ZZR_ST_BOOTING;
    g_zzr_shared->diag = ZZR_DIAG_BOOT;

    g_zzr_shared->input_p1 = 0xFFu;
    g_zzr_shared->input_p2 = 0xFFu;
    g_zzr_shared->input_special = 0x8Fu;
    g_zzr_shared->input_system = 0x1Fu;
    g_zzr_shared->input_dswa = 0xFEu;
    g_zzr_shared->input_dswb = 0xFFu;
}

static void set_error(zzr_u32 error)
{
    g_running = 0;
    g_zzr_shared->error = error;
    g_zzr_shared->status = ZZR_ST_ERROR;
    zzrastan_debug_publish(0, 0);
}

static void cmd_init(void)
{
    zzrastan_mem_reset();
    if (!zzrastan_mem_selftest()) {
        set_error(ZZR_ERR_MEMMAP);
        return;
    }
    zzrastan_mem_reset();
    if (!zzrastan_pc060ha_selftest()) {
        set_error(ZZR_ERR_PC060HA);
        return;
    }
    zzrastan_mem_reset();
    g_zzr_shared->error = ZZR_ERR_NONE;
    g_zzr_shared->diag = ZZR_DIAG_A0_INIT_OK;
    g_zzr_shared->status = ZZR_ST_READY;
}

static void cmd_load_roms(void)
{
    if (!zzrastan_rom_bind_and_deinterleave()) {
        g_zzr_shared->status = ZZR_ST_ERROR;
        return;
    }
    if (!zzrastan_mem_selftest()) {
        set_error(ZZR_ERR_MEMMAP);
        return;
    }
    g_zzr_shared->diag = ZZR_DIAG_A0_MEMMAP_OK;
    zzrastan_mem_reset();
    if (!zzrastan_pc060ha_selftest()) {
        set_error(ZZR_ERR_PC060HA);
        return;
    }
    g_zzr_shared->diag = ZZR_DIAG_A0_PC060HA_OK;
    zzrastan_mem_reset();
    if (!zzrastan_gfx_bind_and_deinterleave()) {
        set_error(ZZR_ERR_GFX_SIZE);
        return;
    }
    if (!zzrastan_obj_bind_and_deinterleave()) {
        set_error(ZZR_ERR_GFX_SIZE);
        return;
    }
    g_zzr_shared->status = ZZR_ST_READY;
}

#define ZZR_CYCLES_PER_FRAME 11111111u

static zzr_u32 g_frame_t0;
static zzr_u32 g_run_t0;

static zzr_u32 g_next_deadline;
static zzr_u32 g_pace_waits, g_pace_catchup, g_pace_hard, g_pace_max_late;

static void pace_60hz_init(void)
{
    g_frame_t0 = zzrastan_pmu_cycles();
    g_run_t0 = g_frame_t0;
    g_next_deadline = g_frame_t0 + ZZR_CYCLES_PER_FRAME;
    g_pace_waits = g_pace_catchup = g_pace_hard = g_pace_max_late = 0;
}

/* Delta-based 60 Hz pace, wrap-safe. Provable property: it can never run FASTER than 60 Hz,
 * because any frame whose real work is under one period busy-waits the exact remainder measured
 * from a self-consistent base (g_frame_t0). If a frame's work already exceeds one period (heavy
 * render), it does NOT bank credit and does NOT wait: it resyncs the base to now, so the worst
 * case is running SLOWER than 60, never faster.
 * The previous absolute-deadline version accumulated credit (deadline += period) and, under a
 * render load that overshot by less than a full frame, let the deadline fall behind real time
 * so the wait loop stopped waiting -> free-run (~275 fps in B2). */
/* P60: absolute deadline with catch-up bounded to ONE frame.
 * The previous version reset the origin to 'now' whenever a frame overran, throwing the debt
 * away. Since almost every RENDERED frame overruns slightly (rendered=1373 vs pace_resyncs=1369),
 * those 1-3 ms were lost for good and the average frame became ~17.5 ms -> 57 fps.
 * Now a light frame simply waits less and absorbs the previous overrun; only a lateness of a
 * whole frame or more triggers a hard resync, which prevents any runaway catch-up. */
static void pace_60hz(void)
{
    zzr_u32 now = zzrastan_pmu_cycles();
    zzr_s32 delta = (zzr_s32)(g_next_deadline - now);

    if (delta > 0) {
        g_pace_waits++;
        do { now = zzrastan_pmu_cycles(); }
        while ((zzr_s32)(g_next_deadline - now) > 0);
    } else {
        zzr_u32 late = now - g_next_deadline;
        if (late > g_pace_max_late) g_pace_max_late = late;
        if (late >= ZZR_CYCLES_PER_FRAME) {
            g_next_deadline = now;                 /* too far behind: resynchronise */
            g_pace_hard++;
            g_zzr_shared->pace_resyncs++;
        } else {
            g_pace_catchup++;                      /* small overrun: absorbed next frame */
        }
    }
    g_next_deadline += ZZR_CYCLES_PER_FRAME;

    g_zzr_shared->pace_waits = g_pace_waits;
    g_zzr_shared->pace_catchup = g_pace_catchup;
    g_zzr_shared->pace_hard = g_pace_hard;
    g_zzr_shared->pace_max_late_us = g_pace_max_late / 666u;
    g_zzr_shared->total_run_cyc = zzrastan_pmu_cycles() - g_run_t0;
}

static void cmd_start(void)
{
    if (!zzrastan_cpu_init_and_reset()) {
        set_error(ZZR_ERR_CPU_RESET);
        return;
    }
    zzr_run_init_array();      /* construct ymfm globals BEFORE sound init */
    zzrastan_video_reset();
    zzrastan_audio_reset();
    zzrastan_sound_init();
    /* The sound Z80 must be running the instant the 68000 starts: Rastan sends the sound-enable
       command 0xEF very early in its init (68k ROM ~0x014A), and the Z80 driver starts with
       RAM[0x8F26]=0, which makes track init at 0x02FE return immediately - no voice, no key-on,
       silence. Enabling this from Core1 (rather than from the launcher across the Zorro bus)
       removes any start-up race. Note this is the SOUND LOGIC enable only; PCM output stays
       gated by pcm_enable until AHI is up. */
    g_zzr_shared->snd_enable = 1;
    g_zzr_shared->diag = ZZR_DIAG_A1_CPU_RESET_OK;
    g_zzr_shared->frame_counter = 0;
    g_zzr_shared->debug.irq5_count = 0;
    pace_60hz_init();
    g_zzr_shared->pace_resyncs = 0;
    g_running = 1;
    g_zzr_shared->status = ZZR_ST_RUNNING;
    g_zzr_shared->diag = ZZR_DIAG_A1_RUNNING;
}


static void process_command(zzr_u32 cmd)
{
    switch (cmd) {
    case ZZR_CMD_INIT:
        cmd_init();
        break;
    case ZZR_CMD_LOAD_ROMS:
        cmd_load_roms();
        break;
    case ZZR_CMD_START:
        cmd_start();
        break;
    case ZZR_CMD_STEP_FRAME:
        if (g_zzr_rom_ready) {
            if (g_zzr_shared->status != ZZR_ST_RUNNING &&
                !zzrastan_cpu_init_and_reset()) {
                set_error(ZZR_ERR_CPU_RESET);
                break;
            }
            zzrastan_run_one_frame_debug();
            g_zzr_shared->step_done = g_zzr_shared->step_req;
        }
        break;
    case ZZR_CMD_SELFTEST:
        __asm__ volatile("udf #0");
        break;
    case ZZR_CMD_STOP:
        g_running = 0;
        g_zzr_shared->status = ZZR_ST_STOPPING;
        break;
    default:
        break;
    }
}

__attribute__((section(".text.core1_entry")))
void core1_entry(void)
{
    zzr_u32 seq;

    zero_bss();
    mmu_init();
    shared_defaults();
    g_zzr_shared->diag = ZZR_DIAG_MMU_OK;
    zz_install_vectors();
    g_zzr_shared->diag = ZZR_DIAG_VECTORS_OK;
    pmu_enable();

    g_running = 0;
    g_last_cmd_seq = g_zzr_shared->cmd_seq;
    g_zzr_shared->status = ZZR_ST_READY;
    g_zzr_shared->diag = ZZR_DIAG_READY;
    __asm__ volatile("dsb" ::: "memory");

    for (;;) {
        g_zzr_shared->heartbeat++;
        seq = g_zzr_shared->cmd_seq;
        if (seq != g_last_cmd_seq) {
            g_last_cmd_seq = seq;
            process_command(g_zzr_shared->command);
            g_zzr_shared->ack_seq = seq;
            __asm__ volatile("dsb" ::: "memory");
        }

        if (g_zzr_shared->status == ZZR_ST_STOPPING)
            break;

        if (g_running && g_zzr_shared->status == ZZR_ST_RUNNING) {
            /* PAUSE: freeze the whole emulation - 68000, Z80, YM, MSM and the renderer - but keep
               pacing so we neither spin nor drift. The display simply holds the last completed
               frame, and with nothing feeding the ring AHI pads silence, which is what an arcade
               pause should sound like. Everything resumes exactly where it stopped: no state is
               reset, no counter is cleared. */
            if (g_zzr_shared->pause_flag) {
                g_zzr_shared->paused_frames++;
                pace_60hz();
                continue;
            }
            /* TRAINER, addresses from the MAME cheat database (rastan.xml), which is the
               authoritative source here - and identical across rastan / rastanu / rastsaga, so
               this works on every ROM set including the Japanese one:
                   Infinite Lives  : maincpu.pb@10C101 = 03      ONE BYTE, value 3
                   Infinite Energy : maincpu.pb@10C13A = 30      ONE BYTE, value 0x30
               Note these are BYTES, and they are NOT the 0x10C040/44 debug flags documented in
               rastan.cpp - those are a different mechanism, and infinite ENERGY alone does not
               stop you dying instantly in water or fire, which is why the LIVES counter is the
               one that matters.
               Held before the frame runs, so the game sees the value for the whole frame. */
            if (g_zzr_shared->trainer_flags) {
                if (g_zzr_shared->trainer_flags & 1u) zzrastan_poke8(0x10C101u, 0x03u); /* lives  */
                if (g_zzr_shared->trainer_flags & 2u) zzrastan_poke8(0x10C13Au, 0x30u); /* energy */
                g_zzr_shared->trainer_writes++;
            }
            {   /* read the two counters back, so the report shows what the game really holds */
                extern zzr_u8 zzrastan_peek8(zzr_u32 addr);
                g_zzr_shared->trainer_lives  = zzrastan_peek8(0x10C101u);
                g_zzr_shared->trainer_energy = zzrastan_peek8(0x10C13Au);
            }
            {
                zzr_u32 fw0 = zzrastan_pmu_cycles(), fw1, ap0, pc0;
                zzrastan_run_one_frame_debug();
                zzrastan_video_present();
                ap0 = zzrastan_pmu_cycles();
                zzrastan_audio_produce();
                g_pf_aud += (unsigned long long)(zzrastan_pmu_cycles() - ap0);
                fw1 = zzrastan_pmu_cycles();
                g_pf_work += (unsigned long long)(fw1 - fw0);
                if (g_zzr_shared->status == ZZR_ST_ERROR)
                    g_running = 0;
                else {
                    pc0 = zzrastan_pmu_cycles();
                    pace_60hz();
                    g_pf_pace += (unsigned long long)(zzrastan_pmu_cycles() - pc0);
                }
                g_zzr_shared->prof_frames++;
                {
                    zzr_u32 f = g_zzr_shared->prof_frames;
                    g_zzr_shared->prof_frame_work    = (zzr_u32)(g_pf_work / 666ULL / f);
                    g_zzr_shared->prof_pace_wait     = (zzr_u32)(g_pf_pace / 666ULL / f);
                    g_zzr_shared->prof_audio_produce = (zzr_u32)(g_pf_aud  / 666ULL / f);
                }
                if (!g_pcm_was && g_zzr_shared->pcm_enable) {
                    g_pcm_was = 1;
                    zzr_bootev(4u, 1u);      /* 4 = pcm_enable 0->1 */
                }
            }
        }
    }

    g_zzr_shared->status = ZZR_ST_STOPPED;
    __asm__ volatile("dsb" ::: "memory");
    for (;;) {
        __asm__ volatile("wfe");
    }
}
