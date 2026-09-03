/*
 * iEngine_ZZ9000.c -- AmigaAMP3 external engine backend using XACP.
 *
 * Only the six functions requested are implemented:
 *     iEngine_fifo_init   - set up the XACP session (no decode)
 *     iEngine_fifo_fill   - push MP3 bytes into the XACP MP3 ring
 *     iEngine_fifo_clear  - flush/restart the ring (uses OP_STREAM_CLOSE)
 *     iEngine_amp_init    - open the stream (OP_STREAM_OPEN), arm the ARM
 *     iEngine_amp_decode  - copy ready PCM out of the XACP PCM ring (no decode)
 *     iEngine_amp_exit    - close the stream (OP_STREAM_CLOSE)
 *
 * Everything else is left unchanged.
 *
 * The actual MP3 decoding is done on the ARM (minimp3) exactly as in the
 * working ZZPlayGUI player. This 68k side only moves bytes:
 *     AmigaAMP -> fifo_fill -> MP3 ring  -> ARM minimp3 -> PCM ring -> amp_decode -> AmigaAMP
 *
 * Ring helpers, constants and the XACP_StreamControl layout are copied
 * verbatim from ZZPlayGUI so the byte-for-byte behaviour is identical.
 *
 * BUILD RULE: ASCII only. No accented characters anywhere in this file.
 *
 * Build (two-step, same toolchain as ZZPlayGUI):
 *   m68k-amigaos-gcc -O2 -noixemul -m68020 -c -o iEngine_ZZ9000.o iEngine_ZZ9000.c
 */

#include <proto/exec.h>
#include <proto/expansion.h>
#include <libraries/configvars.h>
#include <string.h>

/* iEngine_ZZ9000 provides iEngine. */
#include "iEngine.h"

/*
 * hton.h : Tom Wenzel's Engine-ABI byte-order macros (HTONL/NTOHL/NTOHS).
 * Two DISTINCT ABIs live in this file, do not mix them:
 *   - Engine ABI  (AmigaAMP <-> iEngine_* params: dp/ffp/ifp/cfp) -> hton macros
 *   - XACP  ABI   (68k <-> ARM StreamControl fields)              -> BE32 macro
 * On big-endian m68k both reduce to identity; the ARM does the real swap for
 * XACP fields (cpu_to_be32), and the hton macros keep us consistent with
 * however AmigaAMP fills the Engine structures, whatever they expand to.
 */
#include "hton.h"

/* ============================================================
 * XACP constants -- must match the ZZ9000 firmware exactly.
 * Copied verbatim from the working ZZPlayGUI player.
 * ============================================================ */
#define ZZ9000_MANUF        0x6D6Eu
#define ZZ9000_PROD_AX      0x0Au
#define ZZ9000_PROD         0x04u

#define MNT_FB_BASE         0x00010000UL
#define REG_CMD             0x64

#define XACP_STREAM_OFFSET  0x04002000UL
#define XACP_MP3_OFFSET     0x04100000UL
#define XACP_PCM_OFFSET     0x04200000UL

#define MP3_RING_SIZE       (512UL  * 1024UL)
#define PCM_RING_SIZE       (1024UL * 1024UL)

/*
 * Engine-ABI FIFO size advertised to AmigaAMP. Tom's reference FIFO
 * (fifo/fifo.c) uses FIFOSIZE = 1152*20. We keep the physical XACP MP3 ring
 * at MP3_RING_SIZE (512 KB) but report/cap the Engine free space to this
 * value, so AmigaAMP tops up the FIFO in the same small chunks it uses for
 * the reference software engine.
 */
#define ENGINE_FIFO_SIZE    (1152UL * 20UL)   /* 23040 bytes, == Tom's FIFOSIZE */

#define OP_STREAM_OPEN      4
#define OP_STREAM_CLOSE     5
#define STREAM_DECODE_DONE  2

/* 68k is big-endian native, identical to ZZPlayGUI. */
#define BE32(x) (x)

#define ZZ_WR(b,o,v) (*((volatile UWORD *)((UBYTE *)(b) + (o))) = (UWORD)(v))
#define ZZ_RD(b,o)   (*((volatile UWORD *)((UBYTE *)(b) + (o))))

/* Same layout as ZZPlayGUI -- the ARM reads/writes these fields. */
typedef struct {
    volatile ULONG mp3_base, mp3_size, mp3_write, mp3_read;
    volatile ULONG mp3_need_refill, mp3_eof;
    volatile ULONG pcm_base, pcm_size, pcm_write, pcm_read;
    volatile ULONG sample_rate, channels;
    volatile ULONG status, error;
    volatile ULONG underrun_count, frames_decoded, flags;
} XACP_StreamControl;

/* ============================================================
 * Engine-provided shared buffers (REAL, like iEngine_AmigaOS.c).
 * AmigaAMP only ever ran with PPC engines that PROVIDE these buffers,
 * so it expects non-NULL pointers here (the "Engine provides the memory"
 * path). Returning NULL takes an untested "AmigaAMP allocates" branch
 * and makes engine init fail -> fallback to mpeg.library.
 * OutBuffer is sized for the fixed stereo 16-bit stride (1152*4).
 * SpecRaw/Equalizer exist but stay unfilled in Phase 1.
 * ============================================================ */
#define MAXFRAMES 20
static unsigned char  OutBuffer[1152*4*MAXFRAMES];
static float          EqualizerL[576];
static float          EqualizerR[576];
static unsigned short SpecRawL[576*MAXFRAMES];
static unsigned short SpecRawR[576*MAXFRAMES];

struct ExpansionBase *ExpansionBase;

/* ============================================================
 * XACP session state (single instance -- one engine, one stream).
 * The handle returned by fifo_init / amp_init is &g_xacp.
 * ============================================================ */
typedef struct {
    UBYTE              *board;          /* cd_BoardAddr                */
    UBYTE              *fb;             /* board + MNT_FB_BASE         */
    XACP_StreamControl *sc;             /* fb + XACP_STREAM_OFFSET     */
    UBYTE              *mring;          /* fb + XACP_MP3_OFFSET        */
    UBYTE              *pring;          /* fb + XACP_PCM_OFFSET        */
    ULONG               wptr;           /* MP3 ring write index (68k)  */
    ULONG               prd;            /* PCM ring read index  (68k)  */
    ULONG               ch;             /* channel count hint          */
    ULONG               open_requested; /* amp_init asked to open      */
    ULONG               opened;         /* OP_STREAM_OPEN was issued    */
    ULONG               active;         /* session initialised         */
} XACPSession;

static XACPSession g_xacp;

/* ============================================================
 * Ring helpers -- copied verbatim from ZZPlayGUI.
 * ============================================================ */
static ULONG ring_avail(ULONG w, ULONG r, ULONG sz) {
    return (w >= r) ? (w - r) : (sz - r + w);
}
static ULONG ring_free(ULONG w, ULONG r, ULONG sz) {
    return (r + sz - w - 1) % sz;
}
static void ring_push(UBYTE *ring, ULONG rsz, ULONG *wp,
                      const UBYTE *src, ULONG len)
{
    ULONG w   = *wp;
    ULONG ste = rsz - w;
    if (len <= ste) {
        CopyMem((APTR)src, ring + w, len);
    } else {
        CopyMem((APTR)src,       ring + w, ste);
        CopyMem((APTR)(src+ste), ring,     len - ste);
    }
    *wp = (w + len) % rsz;
}
static void ring_copy_out(UBYTE *ring, ULONG rsz,
                          ULONG rd, UBYTE *dst, ULONG len)
{
    ULONG ste = rsz - rd;
    if (len <= ste) {
        CopyMem((APTR)(ring + rd), dst, len);
    } else {
        CopyMem((APTR)(ring + rd), dst,       ste);
        CopyMem((APTR)ring,        dst + ste, len - ste);
    }
}

/* ============================================================
 * Engine identity (unchanged)
 * ============================================================ */
char* iEngine_get_name(void) {
    return "ZZ9000";
}

char* iEngine_get_capability(unsigned long capability) {
    switch(capability) {
        case ENGINECAP_FORMATS:
            return "audio/mpeg{audio/mp2,audio/mp3}";
        break;
    }
    return NULL;
}

/* ============================================================
 * Custom libraries (unchanged)
 * ============================================================ */
ULONG iEngine_OpenLibs(void) {
    ExpansionBase = (struct ExpansionBase *) OpenLibrary("expansion.library", 0);
    if(!ExpansionBase) return FALSE;
    return TRUE;
}

void iEngine_CloseLibs(void) {
    if(ExpansionBase) CloseLibrary((struct Library *)ExpansionBase);
    ExpansionBase = NULL;
}

/* ============================================================
 * Shared memory getters (unchanged)
 * ============================================================ */
unsigned long iEngine_getNumFrames(void) {
    return MAXFRAMES;
}
unsigned char* iEngine_getOutBuffer(void) {
    return OutBuffer;
}
float* iEngine_getEqualizerL(void) {
    return EqualizerL;
}
float* iEngine_getEqualizerR(void) {
    return EqualizerR;
}
unsigned short* iEngine_getSpecRawL(void) {
    return SpecRawL;
}
unsigned short* iEngine_getSpecRawR(void) {
    return SpecRawR;
}

/* ============================================================
 * XACP internal helpers
 * ============================================================ */

/* Locate the ZZ9000 board and set up the framebuffer pointers. */
static BOOL xacp_find_board(XACPSession *s) {
    struct ConfigDev *cd;
    if(!ExpansionBase) iEngine_OpenLibs();
    if(!ExpansionBase) return FALSE;
    cd = FindConfigDev(NULL, ZZ9000_MANUF, ZZ9000_PROD_AX);
    if(!cd) cd = FindConfigDev(NULL, ZZ9000_MANUF, ZZ9000_PROD);
    if(!cd) return FALSE;
    s->board = (UBYTE *)cd->cd_BoardAddr;
    s->fb    = s->board + MNT_FB_BASE;
    return TRUE;
}

/* Initialise the StreamControl block exactly like ZZPlayGUI stream_open. */
static void xacp_init_control(XACPSession *s) {
    if(!s->sc) return;
    memset((void *)s->sc, 0, sizeof(XACP_StreamControl));
    s->sc->mp3_base = BE32(XACP_MP3_OFFSET);
    s->sc->mp3_size = BE32(MP3_RING_SIZE);
    s->sc->pcm_base = BE32(XACP_PCM_OFFSET);
    s->sc->pcm_size = BE32(PCM_RING_SIZE);
}

/*
 * Make the XACP session ready: find the board, set the ring pointers, init
 * the control block. Idempotent. Returns TRUE if the ZZ9000 is present and
 * the session is usable, FALSE only if the board is genuinely absent.
 *
 * Called by BOTH fifo_init and amp_init so the engine no longer depends on
 * AmigaAMP's INIT_FIFO/INIT_AMP ordering (the reference iEngine_AmigaOS has
 * no such dependency). It does NOT fake success when the card is missing:
 * decoding happens on the card's ARM, so no board => no decoder.
 */
static BOOL xacp_session_ensure(XACPSession *s) {
    if(s->active) return TRUE;

    s->board = NULL; s->fb = NULL; s->sc = NULL;
    s->mring = NULL; s->pring = NULL;
    s->wptr = 0; s->prd = 0; s->ch = 2;
    s->open_requested = 0; s->opened = 0;

    if(!xacp_find_board(s)) { return FALSE; }

    s->sc    = (XACP_StreamControl *)(s->fb + XACP_STREAM_OFFSET);
    s->mring = s->fb + XACP_MP3_OFFSET;
    s->pring = s->fb + XACP_PCM_OFFSET;
    xacp_init_control(s);

    s->active = 1;
    return TRUE;
}

/*
 * Issue OP_STREAM_OPEN once, and only after the MP3 ring holds data.
 * This reproduces ZZPlayGUI's "prefill then open" behaviour and makes the
 * code robust to either fill/init ordering AmigaAMP may use.
 */
static void xacp_ensure_open(XACPSession *s) {
    if(!s->active || !s->board || !s->sc) return;
    if(s->opened || !s->open_requested)  return;
    if(s->wptr == 0)                     return;   /* wait for prefill */
    s->sc->mp3_write = BE32(s->wptr);
    ZZ_WR(s->board, REG_CMD, OP_STREAM_OPEN);
    s->opened = 1;
}

/* ============================================================
 * fifo access
 * ============================================================ */

/* Set up the XACP session. No decode, no opcode. */
unsigned long iEngine_fifo_init(struct InitFifoParameters *ifp) {
    XACPSession *s = &g_xacp;

    /* force a fresh session for the new stream, then (re)establish it */
    s->active = 0;
    if(!xacp_session_ensure(s)) {
        if(ifp) ifp->Size = HTONL(0);
        return 0;
    }

    if(ifp) ifp->Size = HTONL(ENGINE_FIFO_SIZE);
    return (unsigned long)s;   /* itsFifo handle (raw, like Tom's fifo_init) */
}

/* fifo_exit left unchanged -- nothing is allocated on the 68k side. */
unsigned long iEngine_fifo_exit(struct ExitFifoParameters *efp) {
    unsigned long Result = 0;
    (void)efp;
    return Result;
}

/* Flush and restart the ring. Uses only OP_STREAM_CLOSE (allowed). */
unsigned long iEngine_fifo_clear(struct ClearFifoParameters *cfp) {
    XACPSession *s = &g_xacp;

    if(!s->active) {
        if(cfp) cfp->Size = HTONL(0);
        return HTONL(0);
    }

    /* halt the ARM decoder if it is running (same as ZZPlayGUI stream_close) */
    if(s->board && s->opened) {
        if(s->sc) s->sc->mp3_eof = BE32(1);   /* XACP field -> BE32 */
        ZZ_WR(s->board, REG_CMD, OP_STREAM_CLOSE);
    }
    s->opened = 0;

    /* fresh control block, reset 68k mirrors */
    s->wptr = 0;
    s->prd  = 0;
    xacp_init_control(s);

    /* next fifo_fill re-primes the ring, then xacp_ensure_open reopens */
    s->open_requested = 1;

    if(cfp) cfp->Size = HTONL(ENGINE_FIFO_SIZE);
    return HTONL(0);
}

/*
 * Push MP3 bytes into the XACP MP3 ring.
 * Bounded by the free space in the ring (standard FIFO back-pressure).
 * Returns the number of free bytes left in the ring after the push.
 *
 * Engine ABI fields (SrcBuffer/NumBytes/return) go through hton macros;
 * XACP StreamControl fields (mp3_read/mp3_write) stay on BE32.
 */
unsigned long iEngine_fifo_fill(struct FillFifoParameters *ffp) {
    XACPSession *s = &g_xacp;
    const UBYTE *src;
    ULONG num, mp3rd, mfree, push, left, adv;

    if(!s->active || !ffp) { return 0; }

    src = (const UBYTE *)NTOHL((ULONG)ffp->SrcBuffer);   /* Engine ABI */
    num = NTOHL(ffp->NumBytes);                          /* Engine ABI */

    /* current free space (ARM advances mp3_read) */
    CacheClearU();
    mp3rd = BE32(s->sc->mp3_read);                        /* XACP ABI */
    mfree = ring_free(s->wptr, mp3rd, MP3_RING_SIZE);

    if(!src || num == 0) {
        /* probe: report free space, capped to the advertised FIFO size */
        adv = (mfree < ENGINE_FIFO_SIZE) ? mfree : ENGINE_FIFO_SIZE;
        return HTONL(adv);
    }

    push = (num < mfree) ? num : mfree;   /* never overrun the physical ring */
    if(push > 0) {
        ring_push(s->mring, MP3_RING_SIZE, &s->wptr, src, push);
        s->sc->mp3_write = BE32(s->wptr);                /* XACP ABI */
    }

    /* complete a pending open as soon as the ring has data */
    xacp_ensure_open(s);

    left = (mfree >= push) ? (mfree - push) : 0;
    adv  = (left < ENGINE_FIFO_SIZE) ? left : ENGINE_FIFO_SIZE;
    return HTONL(adv);                                    /* Engine ABI */
}

/* ============================================================
 * equalizer access (Phase 1: flat / no-op EQ).
 * IMPORTANT: equalizer_init must return a NON-ZERO handle (it is the
 * itsEqualizer consumed by Config/Set/Run/Exit), exactly like fifo_init
 * and amp_init return handles. Returning 0 here looks like an init failure
 * and can make AmigaAMP reject the decoder. The other EQ ops are status
 * (0 = ok, like fifo_clear) and are no-ops: the PCM passes through unchanged.
 * ============================================================ */
static UBYTE g_eq_handle;   /* opaque, non-NULL EQ handle; never dereferenced */

unsigned long iEngine_equalizer_init(struct InitEqualizerParameters *iep) {
    (void)iep;
    return (unsigned long)&g_eq_handle;   /* non-zero handle: EQ present but flat */
}
unsigned long iEngine_equalizer_exit(struct ExitEqualizerParameters *eep) {
    unsigned long Result = 0; (void)eep; return Result;
}
unsigned long iEngine_equalizer_config(struct ConfigEqualizerParameters *cep) {
    { unsigned long Result = 0; (void)cep; return Result; }
}
unsigned long iEngine_equalizer_set(struct SetEqualizerParameters *sep) {
    unsigned long Result = 0; (void)sep; return Result;
}
unsigned long iEngine_equalizer_run(struct RunEqualizerParameters *rep) {
    unsigned long Result = 0; (void)rep; return Result;   /* no-op: audio unchanged */
}

/* ============================================================
 * amp access
 * ============================================================ */

/* amp_start / amp_stop left unchanged. */
unsigned long iEngine_amp_start(void) {
    return 0;
}
unsigned long iEngine_amp_stop(void) {
    return 0;
}

/* Open the XACP stream. Arm the ARM decoder via OP_STREAM_OPEN. */
unsigned long iEngine_amp_init(struct InitAmpParameters *iap) {
    XACPSession *s = &g_xacp;
    (void)iap;   /* Delay / Padding not used: ARM handles timing */

    /* Establish the session ourselves: do NOT depend on fifo_init having
     * run first. Returns 0 only if the ZZ9000 board is genuinely absent. */
    if(!xacp_session_ensure(s)) { return 0; }

    /* AmigaAMP supplies layer/channels/rate itself; we only arm the ARM. */
    s->prd = 0;
    if(s->sc) s->sc->pcm_read = BE32(0);

    s->open_requested = 1;
    s->opened = 0;

    /* open immediately if the ring is already primed, else fifo_fill will */
    xacp_ensure_open(s);

    return (unsigned long)s;   /* itsAmp handle */
}

/* Close the XACP stream. */
unsigned long iEngine_amp_exit(struct ExitAmpParameters *eap) {
    XACPSession *s = &g_xacp;
    (void)eap;

    if(!s->active) return HTONL(0);

    if(s->sc) s->sc->mp3_eof = BE32(1);   /* XACP field -> BE32 */
    if(s->board && s->opened) {
        ZZ_WR(s->board, REG_CMD, OP_STREAM_CLOSE);
    }
    s->opened = 0;
    s->open_requested = 0;
    return HTONL(0);   /* Engine ABI */
}

/*
 * Copy ready PCM out of the XACP PCM ring into DstBuffer.
 * No MPEG decode, no DSP, no long wait, no refill -- bounded work only.
 *
 *   1. read how much PCM is available in the ring
 *   2. copy whole frames (up to DstBuffer capacity) into DstBuffer
 *   3. return the number of PCM bytes written to DstBuffer
 *
 * ABI (confirmed against Tom Wenzel's reference engine):
 *   - DstBuffer uses a FIXED stereo 16-bit stride of 1152 * 4 bytes per
 *     frame, regardless of channel count (matches OutBuffer[1152*4*MAXFRAMES]).
 *   - The return value is the number of PCM BYTES written to DstBuffer.
 *   - DecodeEOF may be set on the same call that returns the final bytes;
 *     the caller still plays those bytes.
 *
 * SYNCHRONOUS like the WarpOS reference (DoEngineOperation blocks for the
 * coprocessor's reply): this call waits, bounded, until the ARM has decoded
 * at least the first frame (or the stream ends), so AmigaAMP never gets a
 * "0 = not ready yet" that it would read as a failed/empty decoder. The ARM
 * is an independent processor, so the wait does not stall decoding -- the
 * 68k just polls the shared PCM ring until data appears.
 *
 * END OF STREAM (the important part): AmigaAMP loops DECODE_AMP until
 * dp->DecodeEOF != 0. The reference software engine (amp.c) sets EOF when its
 * INPUT FIFO is empty (fifo.c FillLevel <= 0), NOT from any decoder "done"
 * status. We mirror that exactly: EOF is driven by the MP3 INPUT ring being
 * drained (the ARM has consumed every byte we pushed), after the PCM output
 * ring has also been delivered. AmigaAMP keeps the input ring topped up while
 * the file has data, so an empty input ring reliably means end of file --
 * just like FillLevel <= 0 in the reference. Keying EOF off the ARM status
 * (which never reaches DONE during normal playback) is what made the decode
 * loop spin forever at 99% CPU at end of song.
 *
 * The ARM writes the PCM ring at its native frame size 1152 * channels * 2.
 * For stereo this equals the destination stride (straight copy). For mono
 * each 16-bit sample is duplicated into L and R to fill the stereo stride.
 * PCM in the ring is already big-endian (the ARM byte-swaps), so no swap here.
 */
unsigned long iEngine_amp_decode(struct DecodeParameters *dp) {
    XACPSession *s = &g_xacp;
    UBYTE *dst;
    ULONG  ch_src, src_fb, dst_fb, want_frames, pcm_w, avail, done;
    ULONG  frames, consumed, left, produced, polls, drain, mp3_pend, input_done;

    if(!s->active || !dp) return 0;

    dst = (UBYTE *)NTOHL((ULONG)dp->DstBuffer);            /* Engine ABI */
    if(!dst) return 0;

    /* make sure the ARM decoder is actually running */
    xacp_ensure_open(s);

    dst_fb      = 1152UL * 4UL;                            /* stereo 16-bit stride */
    want_frames = NTOHL(dp->NumFrames) * NTOHL(dp->VisFrames); /* Engine ABI */
    if(want_frames == 0) {
        dp->DecodeEOF = HTONL(0);
        return HTONL(0);
    }

    /*
     * Wait, bounded, for one of three outcomes:
     *   (a) a whole PCM frame is ready  -> deliver it;
     *   (b) the MP3 input ring is empty (ARM consumed all we pushed) -> after a
     *       short grace to let the last frame flush, stop and go to the EOF path;
     *   (c) the ARM signalled DONE.
     * While the input ring still holds data (normal playback) we keep waiting
     * for the next frame under a high safety ceiling so we can never hang.
     */
    polls = 0;
    drain = 0;
    for(;;) {
        CacheClearU();
        done     = (BE32(s->sc->status) == STREAM_DECODE_DONE);
        ch_src   = BE32(s->sc->channels);
        pcm_w    = BE32(s->sc->pcm_write);
        avail    = ring_avail(pcm_w, s->prd, PCM_RING_SIZE);
        mp3_pend = ring_avail(s->wptr, BE32(s->sc->mp3_read), MP3_RING_SIZE);

        if(ch_src >= 1 && ch_src <= 2 && avail >= 1152UL * ch_src * 2UL) {
            break;                            /* (a) a whole frame is ready */
        }
        if(mp3_pend == 0 || done) {
            if(++drain > 64UL) break;         /* (b)/(c) brief flush grace, then EOF path */
        } else {
            if(++polls > 4000UL) break;       /* normal wait: bounded safety ceiling */
        }
        { volatile int d = 500; while(d--); } /* short pause between polls */
    }

    /* re-read after the wait */
    CacheClearU();
    done       = (BE32(s->sc->status) == STREAM_DECODE_DONE);
    ch_src     = BE32(s->sc->channels);
    mp3_pend   = ring_avail(s->wptr, BE32(s->sc->mp3_read), MP3_RING_SIZE);
    input_done = (mp3_pend == 0) || done;     /* == reference FillLevel <= 0 */

    if(ch_src < 1 || ch_src > 2) {
        /* no frame to deliver: EOF iff the input ring has been drained */
        dp->DecodeEOF = HTONL(input_done ? 1UL : 0UL);
        return HTONL(0);
    }
    s->ch = ch_src;

    src_fb = 1152UL * ch_src * 2UL;                        /* bytes per frame in the ring */

    pcm_w = BE32(s->sc->pcm_write);                        /* XACP ABI */
    avail = ring_avail(pcm_w, s->prd, PCM_RING_SIZE);

    /* whole frames deliverable this call, bounded by DstBuffer capacity */
    frames = avail / src_fb;
    if(frames > want_frames) frames = want_frames;

    produced = 0;
    if(frames > 0) {
        if(ch_src == 2) {
            /* stereo: src_fb == dst_fb, straight ring copy */
            ULONG bytes = frames * src_fb;
            ring_copy_out(s->pring, PCM_RING_SIZE, s->prd, dst, bytes);
            s->prd   = (s->prd + bytes) % PCM_RING_SIZE;
            produced = bytes;                          /* == frames * dst_fb */
        } else {
            /* mono: expand each 16-bit sample to L,R into the stereo stride */
            ULONG  samples = frames * 1152UL;
            ULONG  rd      = s->prd;
            UWORD *out     = (UWORD *)dst;
            ULONG  i;
            for(i = 0; i < samples; i++) {
                UWORD v = *(UWORD *)(s->pring + rd);   /* already big-endian */
                *out++ = v;                            /* L */
                *out++ = v;                            /* R */
                rd += 2UL;
                if(rd >= PCM_RING_SIZE) rd -= PCM_RING_SIZE;
            }
            s->prd   = rd;
            produced = frames * dst_fb;                /* stereo bytes written */
        }
        s->sc->pcm_read = BE32(s->prd);                    /* XACP ABI */
    }

    /*
     * End of stream: the input ring is drained (ARM consumed all MP3) AND less
     * than one whole frame of PCM remains to deliver. Mirrors amp.c setting
     * DecodeEOF when FillLevel <= 0, but only after the PCM tail is drained so
     * the last frames are not cut off. The final bytes are returned WITH
     * DecodeEOF set on the same call, which AmigaAMP still plays before exiting
     * its decode loop.
     */
    consumed = frames * src_fb;
    left     = (avail >= consumed) ? (avail - consumed) : 0UL;
    dp->DecodeEOF = HTONL((input_done && left < src_fb) ? 1UL : 0UL); /* Engine ABI */

    return HTONL(produced);   /* Engine ABI: PCM bytes written to DstBuffer */
}