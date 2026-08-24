/*
 * zzrastan_ym.cpp - YM2151 (OPM) via ymfm, wrapped behind a plain C API.
 *
 * A2: the real chip. Provides what the A1 stub could not: status register, timers and the IRQ
 * line back to the Z80 - which is what the Rastan sound program uses as its clock. Output is
 * generated at the chip's native rate and decimated to the ring rate in zzrastan_sound.c.
 *
 * ymfm is BSD-3-Clause (Aaron Giles). Used as a library, unmodified.
 * ASCII only.
 */
#include "ymfm_opm.h"

extern "C" void zzr_sound_bankswitch(unsigned int bank);
extern "C" void zzr_bootev(unsigned int type, unsigned int value);

/* --- FM synthesis backend -------------------------------------------------------------------
 * ymfm measured 16.5 ms/frame on this A9 (the whole 60 Hz budget), so the synthesis engine is
 * switched to the Jarek Burczynski core. Timers, status, stats, key-on tracking and the reg 0x1B
 * bankswitch all stay in THIS file - they are validated and unchanged. */
#define ZZR_YM_YMFM   0
#define ZZR_YM_JAREK  1
#define ZZR_YM_BACKEND ZZR_YM_JAREK
extern "C" void zzr_jarek_reset(void);
extern "C" void zzr_jarek_write_addr(unsigned char v);
extern "C" void zzr_jarek_write_data(unsigned char v);
extern "C" void zzr_jarek_generate(short *dst, unsigned int count);
extern "C" int  zzr_jarek_advance(unsigned int clocks);
extern "C" unsigned char zzr_jarek_status(void);
extern "C" unsigned int zzr_jarek_stat(unsigned int i);

/* A2.1 instrumentation (read by the launcher through the sound board) */
unsigned long long g_ym_now = 0;          /* monotonic YM clock, in chip clocks */
unsigned int g_timer_sets[2]   = { 0, 0 };
unsigned int g_timer_fires[2]  = { 0, 0 };
unsigned int g_timer_period[2] = { 0, 0 };
unsigned int g_irq_asserts = 0, g_irq_clears = 0;
unsigned int g_reg10 = 0, g_reg11 = 0, g_reg12 = 0, g_reg14 = 0;
/* A2b.1: does the sound program actually trigger notes, and does the chip emit anything? */
unsigned int g_key_writes = 0, g_key_on = 0, g_key_off = 0;
int g_jarek_irq = 0;
extern "C" void zzr_ym_count_irq(int level)
{
    g_jarek_irq = level ? 1 : 0;
    if (level) g_irq_asserts++; else g_irq_clears++;
}
unsigned int g_nonzero_samples = 0, g_peak_mono = 0;

namespace {

/* The interface object is how ymfm reports timers/IRQ back to us. */
class ZZRYmIface : public ymfm::ymfm_interface
{
public:
    ZZRYmIface() : m_irq(false)
    {
        m_timer_target[0] = m_timer_target[1] = 0;
        m_timer_active[0] = m_timer_active[1] = false;
        m_deadline[0] = m_deadline[1] = 0;
    }

    /* ymfm asks us to schedule (or cancel) a timer, in clocks. We keep it simple and count
       chip clocks ourselves in ym_advance(). duration_in_clocks < 0 means "cancel". */
    virtual void ymfm_set_timer(uint32_t tnum, int32_t duration_in_clocks) override
    {
        if (tnum > 1) return;
        if (duration_in_clocks < 0) { m_timer_active[tnum] = false; return; }
        /* Absolute deadline on the monotonic YM clock. engine_timer_expired() may re-arm the
           timer synchronously, so this must be safe to call re-entrantly. */
        m_deadline[tnum] = g_ym_now + (unsigned long long)duration_in_clocks;
        m_timer_active[tnum] = true;
        g_timer_sets[tnum]++;
        g_timer_period[tnum] = (uint32_t)duration_in_clocks;
    }
    bool     timer_armed(int t) const { return m_timer_active[t]; }
    unsigned long long deadline(int t) const { return m_deadline[t]; }
    void     disarm(int t) { m_timer_active[t] = false; }

    virtual void ymfm_update_irq(bool asserted) override
    {
        if (asserted) g_irq_asserts++; else g_irq_clears++;
        m_irq = asserted;
    }

    /* YM2151 output port -> Z80 ROM bank (rastan.cpp sound_bankswitch_w). ymfm delivers it as
       ACCESS_IO with the top two bits already shifted down (ymfm_opm.cpp: data >> 6). MAME's
       banks start at the BEGINNING of the sound ROM: bank n -> n * 0x4000. */
    virtual void ymfm_external_write(ymfm::access_class type, uint32_t, uint8_t data) override
    {
        if (type == ymfm::ACCESS_IO) zzr_sound_bankswitch(data & 3);
    }

    /* m_engine is protected in ymfm_interface, so the trampoline lives in the derived class. */
    void fire_timer(uint32_t tnum) { if (m_engine) m_engine->engine_timer_expired(tnum); }

    bool     irq() const { return m_irq; }
    bool     timer_active(int t) const { return m_timer_active[t]; }
    uint32_t timer_target(int t) const { return m_timer_target[t]; }
    void     timer_clear(int t) { m_timer_active[t] = false; }

private:
    bool     m_irq;
    bool     m_timer_active[2];
    uint32_t m_timer_target[2];
    unsigned long long m_deadline[2];
};

ZZRYmIface   g_iface;
ymfm::ym2151 g_opm(g_iface);

} /* anonymous namespace */

extern "C" {

void zzr_ym_reset(void)
{
#if ZZR_YM_BACKEND == ZZR_YM_JAREK
    zzr_jarek_reset();
#endif
    g_opm.reset();
    g_ym_now = 0;
}

static unsigned char g_last_addr;
void zzr_ym_write_addr(unsigned char v)
{
    g_last_addr = v;
#if ZZR_YM_BACKEND == ZZR_YM_YMFM
    g_opm.write_address(v);
#else
    zzr_jarek_write_addr(v);
#endif
}
void zzr_ym_write_data(unsigned char v)
{
    switch (g_last_addr) {
        case 0x10: g_reg10 = v; break;   /* timer A high */
        case 0x11: g_reg11 = v; break;   /* timer A low  */
        case 0x12: g_reg12 = v; break;   /* timer B      */
        case 0x14: g_reg14 = v; break;   /* mode / IRQ enable */
        case 0x08:                       /* KEY ON/OFF: ch in bits0-2, operator mask in 3-6 */
            g_key_writes++;
            if (v & 0x78) { g_key_on++; zzr_bootev(3u, v); } else g_key_off++;
            break;
        default: break;
    }
#if ZZR_YM_BACKEND == ZZR_YM_YMFM
    g_opm.write_data(v);
#else
    zzr_jarek_write_data(v);
#endif
}

void zzr_ym_get_stats(unsigned int *out)
{
#if ZZR_YM_BACKEND == ZZR_YM_JAREK
    out[0] = zzr_jarek_stat(0); out[1] = zzr_jarek_stat(1);
    out[2] = zzr_jarek_stat(2); out[3] = zzr_jarek_stat(3);
    out[4] = zzr_jarek_stat(4); out[5] = zzr_jarek_stat(5);
#else
    out[0] = g_timer_sets[0];  out[1] = g_timer_sets[1];
    out[2] = g_timer_fires[0]; out[3] = g_timer_fires[1];
    out[4] = g_timer_period[0];out[5] = g_timer_period[1];
#endif
    out[6] = g_irq_asserts;    out[7] = g_irq_clears;
    out[8] = g_reg10; out[9] = g_reg11; out[10] = g_reg12; out[11] = g_reg14;
    out[12] = g_key_writes; out[13] = g_key_on; out[14] = g_key_off;
    out[15] = g_nonzero_samples; out[16] = g_peak_mono;
}
unsigned char zzr_ym_read_status(void)
{
#if ZZR_YM_BACKEND == ZZR_YM_JAREK
    return zzr_jarek_status();      /* single authority: Jarek's own flags */
#else
    return g_opm.read_status();
#endif
}

/* Advance the chip timers by 'clocks' chip clocks; returns 1 if the IRQ line is asserted. */
int zzr_ym_advance(unsigned int clocks)
{
#if ZZR_YM_BACKEND == ZZR_YM_JAREK
    /* Jarek drives status and IRQ itself from ym2151_timer_over(); we only run the clock. */
    return zzr_jarek_advance(clocks);
#endif
    unsigned long long end = g_ym_now + (unsigned long long)clocks;
    int guard = 0;

    /* Fire every timer whose absolute deadline falls inside this slice, earliest first.
       engine_timer_expired() can re-arm the timer synchronously, hence the loop. */
    for (;;) {
        int best = -1;
        unsigned long long best_dl = 0;
        int t;
        for (t = 0; t < 2; t++) {
            if (!g_iface.timer_armed(t)) continue;
            if (g_iface.deadline(t) > end) continue;
            if (best < 0 || g_iface.deadline(t) < best_dl) { best = t; best_dl = g_iface.deadline(t); }
        }
        if (best < 0) break;
        g_ym_now = best_dl;
        g_iface.disarm(best);
        g_timer_fires[best]++;
        g_iface.fire_timer((uint32_t)best);
        if (++guard > 64) break;          /* safety against a pathological period of 0 */
    }
    g_ym_now = end;
    return g_iface.irq() ? 1 : 0;
}

/* Generate 'count' native-rate samples, summing L+R to mono (Rastan is mono). */
void zzr_ym_generate(short *dst, unsigned int count)
{
#if ZZR_YM_BACKEND == ZZR_YM_JAREK
    zzr_jarek_generate(dst, count);
    return;
}
static void zzr_ym_generate_ymfm(short *dst, unsigned int count)
{
#endif
    /* Generate in BLOCKS: ymfm loops internally over numsamples, so one call for N samples
       instead of N calls of 1 (was ~62500 C++ calls/s, now ~600/s for the same FM work). */
    static ymfm::ym2151::output_data tmp[160];
    unsigned int done = 0;
    while (done < count) {
        unsigned int n = count - done;
        unsigned int i;
        if (n > 160u) n = 160u;
        g_opm.generate(tmp, n);
        for (i = 0; i < n; i++) {
            /* Mono downmix: AVERAGE the two channels. Summing them overflowed to full scale
               (we measured peak=32768, i.e. permanent clipping and audible distortion). */
            int v = (tmp[i].data[0] + tmp[i].data[1]) >> 1;
            int a;
            if (v > 32767) v = 32767;
            if (v < -32768) v = -32768;
            dst[done + i] = (short)v;
            (void)a;   /* per-sample stats removed from the hot path (62500/s) */
        }
        done += n;
    }
}

/* Native sample rate of the chip for a given input clock (4 MHz on Rastan). */
int zzr_ym_irq_state(void)
{
#if ZZR_YM_BACKEND == ZZR_YM_JAREK
    return g_jarek_irq;              /* set by the core's IRQ handler */
#else
    return g_iface.irq() ? 1 : 0;
#endif
}

unsigned int zzr_ym_sample_rate(unsigned int clock)
{
#if ZZR_YM_BACKEND == ZZR_YM_JAREK
    (void)clock; return 22050u;      /* Jarek renders directly at the ring rate */
#endif
    return g_opm.sample_rate(clock);
}

} /* extern "C" */
