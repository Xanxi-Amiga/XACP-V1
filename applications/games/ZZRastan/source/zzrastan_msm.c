/*
 * zzrastan_msm.c - MSM5205 ADPCM decoder for ZZRastan (milestone A3).
 *
 * Wiring per rastan.cpp (verified):
 *   Z80 0xB000 -> msm5205_address_w : adpcm_pos = (pos & 0x00ff) | (data << 8)
 *   Z80 0xC000 -> msm5205_start_w   : msm reset_w(0), adpcm_ff = false
 *   Z80 0xD000 -> msm5205_stop_w    : msm reset_w(1), adpcm_pos &= 0xff00
 *   VCK (8 kHz, prescaler S48_4B) toggles a flip-flop; on every SECOND edge a new ROM byte is
 *   fetched and adpcm_pos advances. So one byte feeds TWO nibbles: high first, then low.
 * The VCK does NOT interrupt the Z80 - it only clocks the LS157 that feeds the chip.
 *
 * Standard MSM5205/OKI 4-bit ADPCM: a 49-entry step table, index adjusted per nibble.
 * Output is 12-bit signed on real hardware; we scale to S16 and MAME routes it at 0.60 gain
 * alongside the YM2151.
 *
 * ASCII only.
 */

#include "zzrastan.h"

#define MSM_RATE 8000u          /* S48_4B: 384000 / 48 = 8 kHz */

extern const zzr_u8 *g_zzr_adpcm_rom;   /* b04-20.76 at ZZR_ARM_ADPCM */

static const int g_step_table[49] = {
     16,  17,  19,  21,  23,  25,  28,  31,  34,  37,  41,  45,  50,  55,  60,  66,
     73,  80,  88,  97, 107, 118, 130, 143, 157, 173, 190, 209, 230, 253, 279, 307,
    337, 371, 408, 449, 494, 544, 598, 658, 724, 796, 876, 963,1060,1166,1282,1411,
   1552
};
static const int g_index_adjust[8] = { -1, -1, -1, -1, 2, 4, 6, 8 };

static zzr_u32 g_pos;           /* byte index into the 64 KB ADPCM ROM */
static int     g_ff;            /* nibble flip-flop: 0 = high nibble next */
static int     g_running;
static int     g_signal;        /* running ADPCM accumulator (12-bit domain) */
static int     g_step_index;
static zzr_u8  g_cur_byte;
static zzr_u32 g_acc;           /* fractional accumulator: MSM rate vs output rate */
zzr_u32 g_msm_starts, g_msm_stops, g_msm_nibbles, g_msm_peak;

void zzrastan_msm_reset(void)
{
    g_pos = 0; g_ff = 0; g_running = 0;
    g_signal = 0; g_step_index = 0; g_cur_byte = 0; g_acc = 0;
    g_msm_starts = g_msm_stops = g_msm_nibbles = g_msm_peak = 0;
}

/* --- Z80 register writes (called from zzrastan_sound.c) --- */
void zzrastan_msm_address_w(zzr_u8 data)
{
    g_pos = (g_pos & 0x00FFu) | ((zzr_u32)data << 8);
}
void zzrastan_msm_start_w(void)
{
    g_running = 1; g_msm_starts++;
    g_ff = 0;
    g_signal = 0;
    g_step_index = 0;
}
void zzrastan_msm_stop_w(void)
{
    g_running = 0; g_msm_stops++;
    g_pos &= 0xFF00u;
}

/* Decode one 4-bit ADPCM nibble into the running signal. */
static int msm_step(zzr_u8 nib)
{
    int step = g_step_table[g_step_index];
    int delta = step >> 3;
    if (nib & 1) delta += step >> 2;
    if (nib & 2) delta += step >> 1;
    if (nib & 4) delta += step;
    if (nib & 8) delta = -delta;

    g_signal += delta;
    if (g_signal > 2047) g_signal = 2047;      /* 12-bit signed range */
    if (g_signal < -2048) g_signal = -2048;

    g_msm_nibbles++;
    g_step_index += g_index_adjust[nib & 7];
    if (g_step_index < 0) g_step_index = 0;
    if (g_step_index > 48) g_step_index = 48;
    return g_signal;
}

/* Produce 'count' samples at the OUTPUT rate, adding into dst (the YM is already there).
   The chip runs at 8 kHz, the ring at out_rate, so we hold each ADPCM sample for several
   output samples via a fractional accumulator - the same zero-order hold the real chip's
   output filter approximates. */
void zzrastan_msm_mix(short *dst, unsigned int count, unsigned int out_rate)
{
    unsigned int i;
    if (!g_zzr_adpcm_rom) return;
    for (i = 0; i < count; i++) {
        int v;
        if (g_running) {
            g_acc += MSM_RATE;
            while (g_acc >= out_rate) {          /* time for the next nibble */
                g_acc -= out_rate;
                if (!g_ff) {
                    g_cur_byte = g_zzr_adpcm_rom[g_pos & 0xFFFFu];
                    msm_step((zzr_u8)(g_cur_byte >> 4));
                    g_ff = 1;
                } else {
                    msm_step((zzr_u8)(g_cur_byte & 0x0Fu));
                    g_ff = 0;
                    g_pos = (g_pos + 1u) & 0xFFFFu;
                }
            }
            /* 12-bit -> S16 with MAME's 0.60 route on the MSM side */
            v = (g_signal * 16 * 6) / 10;
            { zzr_u32 a = (zzr_u32)(v < 0 ? -v : v); if (a > g_msm_peak) g_msm_peak = a; }
        } else {
            v = 0;
        }
        {
            int s = (int)dst[i] + v;
            if (s > 32767) s = 32767;
            if (s < -32768) s = -32768;
            dst[i] = (short)s;
        }
    }
}

int zzrastan_msm_active(void) { return g_running; }
