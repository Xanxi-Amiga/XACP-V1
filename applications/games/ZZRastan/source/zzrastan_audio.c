/*
 * zzrastan_audio.c - ZZRastan A0a: PCM ring transport (test-tone).
 *
 * Milestone A0a: NO emulation yet. Core1 produces a mono S16 sine
 * wave into a byte ring at ARM 0x05700000 (NC), and the 68k consumes it via AHI. This locks the
 * DDR ring / endianness / AHI path before any Z80/YM/MSM work.
 *
 * Ring layout: bytes, S16 little-endian mono. pcm_write_pos (ARM) chases pcm_read_pos (68k);
 * ARM never overtakes the reader (leaves >= 2 bytes gap). Rate 22050 Hz.
 * MMU: 0x057 is already mapped Normal-NC (zzrastan_mmu.c). No cache ops on the ring.
 *
 * ASCII only.
 */

#include "zzrastan.h"

/* A2b: real FM audio. The YM2151 runs at 4 MHz; ymfm's native rate is clock/64 = 62500 Hz.
   We generate native samples and decimate to the ring rate with a fixed-point accumulator.
   ZZR_YM_TESTTONE 1 falls back to the validated sine (for A0-style transport checks). */
#define ZZR_YM_TESTTONE 0   /* A2b: ring carries the real FM */
extern unsigned int zzr_ym_sample_rate(unsigned int clock);
/* FM FIFO produced on emulated time by the sound board; we only transport it. */
#define ZZR_FM_FIFO_SAMPLES 1024
extern zzr_s16 g_fm_fifo[ZZR_FM_FIFO_SAMPLES];
extern zzr_u32 g_fm_wr, g_fm_rd;

#define ZZR_PCM_ARM_BASE  0x05700000u
#define ZZR_PCM_SIZE      (512u * 1024u)     /* 512 KB */
#define ZZR_PCM_RATE      22050u  /* back to normal after the 8000 Hz probe (AHI proven exact) */
#define ZZR_PCM_TARGET    1024u      /* UNUSED since the pump drains the FIFO instead of levelling
                                        the ring; kept only for reference.
                                        Was 8192 (186 ms): with ymfm the producer was too slow to
                                        ever fill it, but the Jarek core keeps it brim-full, so the
                                        whole cushion turned into ~1 s of audible SFX latency. */
/* LAT1: the 512 KB DDR ring must NEVER be used as an audio backlog. Measurement found ~18500
   bytes permanently queued = 420 ms of pure latency. Keep a small, bounded cushion instead:
     TARGET 1024 bytes = 512 samples = 23.2 ms
     MAX    1536 bytes = 768 samples = 34.8 ms
   This IS a return to levelling the ring, but it is safe now in a way it was not before: back
   then, stopping the pump filled the FIFO and the YM stopped advancing (shredded music). Today
   the YM always generates, the FIFO only has to absorb AHI's block consumption, and fifo_high=37
   shows the producer is very well regulated. */
#define ZZR_RING_TARGET_BYTES 1024u
#define ZZR_RING_MAX_BYTES    1536u
#define ZZR_PCM_MAX_CHUNK 1024u      /* cap per frame: 512 samples = ~23 ms */

/* Full-cycle sine LUT: 64 entries covering one complete period, amplitude +/-16383 (half scale).
   The first version was positive-only (a repeated half-wave) -> huge DC, harmonics and a wrap
   discontinuity: correct pitch but a filthy buzzing tone. Integer only, no float in the hot path. */
static const short g_sine64[64] = {
     0,  1606,  3196,  4756,  6270,  7723,  9102, 10393,
 11585, 12664, 13622, 14449, 15136, 15678, 16068, 16304,
 16383, 16304, 16068, 15678, 15136, 14449, 13622, 12664,
 11585, 10393,  9102,  7723,  6270,  4756,  3196,  1606,
     0, -1606, -3196, -4756, -6270, -7723, -9102,-10393,
-11585,-12664,-13622,-14449,-15136,-15678,-16068,-16304,
-16383,-16304,-16068,-15678,-15136,-14449,-13622,-12664,
-11585,-10393, -9102, -7723, -6270, -4756, -3196, -1606
};

static zzr_u32 g_res_acc;
static zzr_u32 g_ring_pre_min = 0xFFFFFFFFu, g_ring_pre_max, g_ring_post_min = 0xFFFFFFFFu, g_ring_post_max;
static unsigned long long g_ring_pre_sum, g_ring_post_sum;
static zzr_u32 g_ring_pre_n, g_ring_post_n;     /* 16.16 decimation accumulator (native -> ring rate) */
static zzr_u32 g_res_step;    /* native_rate / ring_rate in 16.16 */
static zzr_u32 g_phase;       /* 16.16 phase accumulator into the 64-entry table */
static zzr_u32 g_phase_inc;   /* per-sample increment for the target tone */
static int     g_audio_ready;

void zzrastan_audio_reset(void)
{
    g_phase = 0;
    /* 440 Hz tone: inc = 440 * 64 / rate, in 16.16 fixed point.
       (440 * 64) << 16 / 22050. Compute in 64-bit to avoid overflow. */
    /* Normal generator again: 440 Hz at the declared rate. (The 8000 Hz probe with a pinned
       22050 increment produced ~160 Hz exactly as predicted -> AHI honours ahir_Frequency.) */
    g_phase_inc = (zzr_u32)(((unsigned long long)440u * 64u << 16) / ZZR_PCM_RATE);

    g_zzr_shared->pcm_base   = ZZR_PCM_ARM_BASE;
    g_zzr_shared->pcm_size   = ZZR_PCM_SIZE;
    g_zzr_shared->pcm_write_pos = 0;
    g_zzr_shared->pcm_read_pos  = 0;
    g_zzr_shared->pcm_rate    = ZZR_PCM_RATE;
    g_zzr_shared->pcm_underruns = 0;
    g_zzr_shared->pcm_enable  = 0;      /* launcher sets 1 once AHI is up */
    {
        unsigned int native = zzr_ym_sample_rate(4000000u);
        if (native == 0u) native = 62500u;
        g_res_step = (zzr_u32)(((unsigned long long)native << 16) / ZZR_PCM_RATE);
        g_res_acc = 0;
        g_zzr_shared->snd_ym_native_rate = native;
    }
    g_audio_ready = 1;
}

/* Produce up to 'max_samples' S16 mono samples into the ring, never overtaking the reader.
   Called once per frame; we top the ring up toward ~half full so the 68k always has data. */
/* J1d: the FIFO -> DDR ring transfer, callable at the 600 Hz slice rate. Keeping the drain rate
   equal to the fill rate is what stops the FIFO from saturating (which used to make us skip YM
   generation and shred the music). */
void zzrastan_audio_pump(void)
{
    volatile zzr_u8 *ring = (volatile zzr_u8 *)ZZR_PCM_ARM_BASE;
    zzr_u32 wr, rd, used, space, target, produce, i;

    if (!g_audio_ready) return;
    if (!g_zzr_shared->pcm_enable) return;

    wr = g_zzr_shared->pcm_write_pos;   /* ARM's own value: atomic single store, no tear */
    /* pcm_read_pos is written by the 68k in two 16-bit halves -> read until stable. */
    {
        zzr_u32 a = g_zzr_shared->pcm_read_pos, c; int tries = 4;
        while (tries--) { c = g_zzr_shared->pcm_read_pos; if (c == a) break; a = c; }
        rd = a;
    }

    /* bytes currently buffered (write - read, modulo ring) */
    if (wr >= rd) used = wr - rd;
    else          used = ZZR_PCM_SIZE - rd + wr;

    /* A3c-LAT: ring occupancy BEFORE the transfer. This is the one reservoir we never measured,
       and the pump is free to run ahead (512 KB of physical room, 600 calls/s), so it could be
       holding far more than the few ms we assume. Measurement only - no behaviour change. */
    {
        zzr_u32 pre = used;
        if (pre < g_ring_pre_min) g_ring_pre_min = pre;
        if (pre > g_ring_pre_max) g_ring_pre_max = pre;
        g_ring_pre_sum += pre; g_ring_pre_n++;
    }

    /* free space, keep a 2-byte guard so wr never equals rd on a full ring */
    space = ZZR_PCM_SIZE - used - 2u;

    /* A PUMP MUST DRAIN, NOT LEVEL. The old code returned immediately whenever the ring already
       held ZZR_PCM_TARGET bytes, so with the ring sitting near its cushion nothing was ever
       transferred: the FIFO overflowed (highwater 1023, drop_old 27433) while the ring
       simultaneously starved (191 underruns) - two ends of the same blocked pipe.
       Now we move everything the FIFO holds, limited only by the real free space in the ring. */
    {
        /* Drain the FIFO, but NEVER let the ring run further ahead than ZZR_RING_MAX_AHEAD.
           Measurement showed the ring sitting at ~18500 bytes = 420 ms of audio permanently in
           flight: the pump had taken that lead during start-up (Core1 running while AHI was not
           consuming yet) and, producer and consumer being at the same rate, the offset never
           resorbed. This is NOT the old "level the ring" bug - we still transfer everything the
           FIFO holds, we simply stop adding once the ring already holds enough. */
        zzr_u32 fifo_bytes = ((g_fm_wr - g_fm_rd) & (ZZR_FM_FIFO_SAMPLES - 1u)) * 2u;
        if (used >= ZZR_RING_TARGET_BYTES) return;      /* enough queued: add nothing */
        produce = ZZR_RING_TARGET_BYTES - used;
        if (produce > fifo_bytes) produce = fifo_bytes;
        if (used + produce > ZZR_RING_MAX_BYTES) produce = ZZR_RING_MAX_BYTES - used;
        if (produce > space) produce = space;
        if (produce > ZZR_PCM_MAX_CHUNK) produce = ZZR_PCM_MAX_CHUNK;
        produce &= ~1u;
        (void)target;
        if (produce == 0u) return;
    }

#if ZZR_YM_TESTTONE
    for (i = 0; i < produce; i += 2u) {
        zzr_u32 idx = (g_phase >> 16) & 63u;
        short s = g_sine64[idx];
        ring[wr]     = (zzr_u8)(s & 0xFF);
        ring[wr + 1] = (zzr_u8)((s >> 8) & 0xFF);
        wr += 2u;
        if (wr >= ZZR_PCM_SIZE) wr = 0;
        g_phase += g_phase_inc;
    }
#else
    /* Pull FM samples at the chip's native rate and decimate to ZZR_PCM_RATE.
       step = native/ring in 16.16; we consume 'step' native samples per ring sample. */
    /* Decimate the FM FIFO (62500 Hz) down to the ring rate. The ring NEVER asks the chip to
       produce future samples; it only consumes what emulated time has already generated. */
    for (i = 0; i < produce; i += 2u) {
        zzr_u32 avail = (g_fm_wr - g_fm_rd) & (ZZR_FM_FIFO_SAMPLES - 1u);
        zzr_u32 want, k;
        short v;
        g_res_acc += g_res_step;
        want = g_res_acc >> 16;
        if (want == 0u) want = 1u;
        if (avail < want) { g_res_acc -= (want << 16); break; }   /* not generated yet */
        g_res_acc -= want << 16;
        for (k = 0; k < want; k++) {
            v = g_fm_fifo[g_fm_rd];
            g_fm_rd = (g_fm_rd + 1u) & (ZZR_FM_FIFO_SAMPLES - 1u);
        }
        ring[wr]     = (zzr_u8)(v & 0xFF);
        ring[wr + 1] = (zzr_u8)((v >> 8) & 0xFF);
        wr += 2u;
        if (wr >= ZZR_PCM_SIZE) wr = 0;
    }
#endif
    __asm__ volatile("dsb" ::: "memory");
    g_zzr_shared->pcm_write_pos = wr;

    {   /* ring occupancy AFTER the transfer */
        zzr_u32 post;
        if (wr >= rd) post = wr - rd; else post = ZZR_PCM_SIZE - rd + wr;
        if (post < g_ring_post_min) g_ring_post_min = post;
        if (post > g_ring_post_max) g_ring_post_max = post;
        g_ring_post_sum += post; g_ring_post_n++;
        g_zzr_shared->ring_pre_min  = (g_ring_pre_n  ? g_ring_pre_min  : 0);
        g_zzr_shared->ring_pre_max  = g_ring_pre_max;
        g_zzr_shared->ring_pre_avg  = (g_ring_pre_n  ? (zzr_u32)(g_ring_pre_sum  / g_ring_pre_n)  : 0);
        g_zzr_shared->ring_post_min = (g_ring_post_n ? g_ring_post_min : 0);
        g_zzr_shared->ring_post_max = g_ring_post_max;
        g_zzr_shared->ring_post_avg = (g_ring_post_n ? (zzr_u32)(g_ring_post_sum / g_ring_post_n) : 0);
    }
}


/* Frame-level entry point kept for the main loop; the real work now happens per slice. */
void zzrastan_audio_produce(void)
{
    zzrastan_audio_pump();
}
