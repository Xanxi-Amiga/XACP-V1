/*
 * ZZMIDIPlay.c -- ZZ9000 XACP MIDI Player GUI
 * Version 2.0 / XX19
 *
 * GUI : Intuition/GadTools AmigaOS 3.1, same style as ZZPlayGUI XX16.
 * Uses OP_MIDI_SF2 XACP service (TinySoundFont + TinyMidiLoader on ARM).
 *
 * v2 changes vs v1:
 *  - Non-blocking GUI state machine: no wait_done_seq() in GUI thread
 *  - DDR copy chunked 16KB with Intuition event processing between chunks
 *  - GUI refresh throttled to ~10 Hz (every 6 player_tick calls)
 *  - Separate counters: arm_backpressure_skips vs amiga_ahi_underruns
 *  - Display: sf2_size, heap_used, heap_highwater, backpressure, underruns
 *  - Clear error messages: XMID_ERR_NO_HEAP displayed explicitly
 *  - SF2 retained between tracks if unchanged
 *
 * GUI states:
 *  GSTATE_IDLE          no SF2/MIDI loaded
 *  GSTATE_COPYING_SF2   copying SF2 to DDR (chunked)
 *  GSTATE_COPYING_MIDI  copying MIDI to DDR (chunked)
 *  GSTATE_ARM_LOADING   waiting for ARM LOAD_MIDI to complete (non-blocking)
 *  GSTATE_PLAYING       AHI double-buffer active
 *  GSTATE_STOPPING      waiting for drain
 *  GSTATE_ERROR         error displayed, waiting for user action
 *
 * Build (two-step required):
 *   m68k-amigaos-gcc -O2 -noixemul -m68020 -c -o ZZMIDIPlay.o ZZMIDIPlay.c
 *   m68k-amigaos-gcc      -noixemul -m68020 -o ZZMIDIPlay   ZZMIDIPlay.o -lamiga -lm
 *
 * Usage:
 *   ZZMIDIPlay [sf2file] [midi1.mid midi2.mid ...]
 *   Button SF2 : select SoundFont (ASL)
 *   Button OUV : add MIDI files to playlist (ASL multi-select)
 *   Button PLS : toggle playlist window
 *   Button RPT : toggle loop mode
 *   Space      : play/pause toggle
 *   Escape     : stop
 *   Left/Right : previous/next track
 *
 * ASCII-only source file -- no UTF-8 characters.
 */

#define __USE_SYSBASE

#include <exec/types.h>
#include <exec/memory.h>
#include <exec/io.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <intuition/gadgetclass.h>
#include <libraries/gadtools.h>
#include <libraries/asl.h>
#include <libraries/configvars.h>
#include <devices/ahi.h>
#include <exec/lists.h>
#include <graphics/rastport.h>
#include <graphics/text.h>
#include <workbench/startup.h>

#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/gadtools.h>
#include <proto/asl.h>
#include <proto/expansion.h>
#include <proto/graphics.h>

#include <string.h>
#include <stdio.h>

struct Library *GadToolsBase = NULL;
struct Library *AslBase      = NULL;

/* ================================================================
 * XACP / ZZ-MIDI constants (must match zz_midi_sf2.h exactly)
 * ================================================================ */
#define ZZ9000_MANUF        0x6D6Eu
#define ZZ9000_PROD_AX      0x0Au
#define ZZ9000_PROD         0x04u

#define MNT_FB_BASE         0x00010000UL
#define REG_CMD             0x64
#define REG_STATUS          0x64

#define OP_MIDI_SF2         0x0120

#define XMID_CTRL_OFFSET    0x04010000UL
/* MIDI staging: XACP v1.5 RAM map fixed. Moved to 0x07800000, right AFTER
 * the 32 MB SF2 zone (0x05800000-0x07800000). The old 0x07000000 overlapped
 * the SF2 tail for SF2 > 24 MB, corrupting the SF2 on reload (Guru).
 * Must match XMID_MIDI_OFFSET in the firmware zz_midi_sf2.h. */
#define XMID_MIDI_OFFSET    0x07800000UL
#define XMID_MIDI_MAX_SIZE  (6UL  * 1024UL * 1024UL)   /* 6 MB (heap C limit) */
#define XMID_PCM_RING_OFFSET 0x04800000UL
#define XMID_PCM_RING_SIZE  (1UL * 1024UL * 1024UL)
#define XMID_SF2_OFFSET     0x05800000UL   /* big staging: 32 MB window      */
#define XMID_SF2_MAX_SIZE   (32UL * 1024UL * 1024UL)   /* XACP v1.5: REAL 32 MB.
                                                         * RAM map fixed (MIDI
                                                         * moved after SF2), so
                                                         * the full 32 MB is now
                                                         * usable without
                                                         * overlapping MIDI.    */

#define XMID_CMD_RESET      0UL
#define XMID_CMD_LOAD_MIDI  3UL
#define XMID_CMD_START      4UL
#define XMID_CMD_STOP       5UL
#define XMID_CMD_STATUS     9UL
/* SF2 chunked upload (XX19) */
#define XMID_CMD_SF2_BEGIN        12UL
#define XMID_CMD_SF2_CHUNK        13UL
#define XMID_CMD_SF2_COMMIT_LOAD  14UL
#define XMID_CMD_SF2_FAKE_COMMIT  15UL  /* diagnostic: skip tsf_load */

#define XMID_STATE_EMPTY    0UL
#define XMID_STATE_READY    1UL
#define XMID_STATE_PLAYING  2UL
#define XMID_STATE_DONE     4UL
#define XMID_STATE_ERROR    0x80000000UL

/* SF2 upload sub-states (must match firmware zz_midi_sf2.h) */
#define XMID_SF2_STATE_EMPTY      0UL
#define XMID_SF2_STATE_UPLOADING  1UL
#define XMID_SF2_STATE_CHUNK_DONE 2UL
#define XMID_SF2_STATE_READY      3UL
#define XMID_SF2_STATE_ERROR      4UL

#define XMID_ERR_NONE       0UL
#define XMID_ERR_BAD_SF2    2UL
#define XMID_ERR_BAD_MIDI   3UL
#define XMID_ERR_NO_HEAP    4UL
#define XMID_ERR_TSF_FAIL   5UL
#define XMID_ERR_TML_FAIL   6UL
#define XMID_ERR_FPU_FAIL   7UL
#define XMID_ERR_SF2_TOO_LARGE       9UL
#define XMID_ERR_UPLOAD_INCOMPLETE  13UL
#define XMID_ERR_TSF_LOAD_FAILED    14UL

#define XMID_MAGIC          0x584D4944UL
#define XMID_VERSION        1UL
#define XMID_DEFAULT_RATE   32000UL
#define XMID_DEFAULT_VOL    65536UL
#define XMID_DEFAULT_VOICES 64UL

/* SF2 chunk size (32 MB = one big staging window, matches firmware) */
#define XMID_SF2_CHUNK_SIZE (32UL * 1024UL * 1024UL)

/* Control block field offsets */
#define XMID_OFF_MAGIC              0x00
#define XMID_OFF_VERSION            0x04
#define XMID_OFF_STRUCT_SIZE        0x08
#define XMID_OFF_CMD_SEQ            0x0C
#define XMID_OFF_DONE_SEQ           0x10
#define XMID_OFF_SUBCMD             0x14
#define XMID_OFF_STATE              0x18
#define XMID_OFF_ERROR              0x1C
#define XMID_OFF_FLAGS              0x20
#define XMID_OFF_SAMPLE_RATE        0x24
#define XMID_OFF_CHANNELS           0x28
#define XMID_OFF_PCM_FORMAT         0x2C
#define XMID_OFF_SF2_OFFSET         0x30
#define XMID_OFF_SF2_SIZE           0x34
#define XMID_OFF_MIDI_OFFSET        0x38
#define XMID_OFF_MIDI_SIZE          0x3C
#define XMID_OFF_PCM_RING_OFFSET    0x40
#define XMID_OFF_PCM_RING_SIZE      0x44
#define XMID_OFF_PCM_WRITE_TOTAL    0x48
#define XMID_OFF_PCM_READ_TOTAL     0x4C
#define XMID_OFF_RENDERED_SAMPLES   0x50
#define XMID_OFF_PLAY_MS            0x54
#define XMID_OFF_TOTAL_MS           0x58
#define XMID_OFF_ACTIVE_VOICES      0x5C
#define XMID_OFF_PEAK_ABS           0x60
#define XMID_OFF_RMS_LAST           0x64
#define XMID_OFF_UNDERRUNS          0x68
#define XMID_OFF_HEAP_USED          0x6C
#define XMID_OFF_HEAP_HIGHWATER     0x70
#define XMID_OFF_VOLUME_Q16         0x74
#define XMID_OFF_MAX_VOICES         0x78
#define XMID_OFF_DEBUG0             0x7C
#define XMID_OFF_DEBUG1             0x80
#define XMID_OFF_DEBUG2             0x84
#define XMID_OFF_DEBUG3             0x88
/* SF2 upload progress (XX19) */
#define XMID_OFF_SF2_TOTAL_SIZE     0x8C
#define XMID_OFF_SF2_UPLOADED_BYTES 0x90
#define XMID_OFF_SF2_CHUNK_OFFSET   0x94
#define XMID_OFF_SF2_CHUNK_SIZE     0x98
#define XMID_OFF_SF2_CHUNKS_DONE    0x9C
#define XMID_OFF_SF2_STATE          0xA0
#define XMID_OFF_SF2_PROGRESS_PCT   0xA4

#define CTRL_WR32(base, off, val) \
    (*((volatile ULONG *)((UBYTE *)(base) + (off))) = (ULONG)(val))
#define CTRL_RD32(base, off) \
    (*((volatile ULONG *)((UBYTE *)(base) + (off))))

#define ZZ_WR(b,o,v) (*((volatile UWORD *)((UBYTE *)(b) + (o))) = (UWORD)(v))
#define ZZ_RD(b,o)   (*((volatile UWORD *)((UBYTE *)(b) + (o))))

/* AHI buffer: 2048 samples * 2ch * 2 bytes = 8192 bytes per buffer.
 * At 32000 Hz: 2048/32000 = 64ms per buffer. */
#define AHI_BUF_SAMPLES  2048u
#define AHI_BUF_BYTES    (AHI_BUF_SAMPLES * 2u * 2u)

/* DDR copy chunk size: 16KB per chunk with GUI processing between chunks.
 * Prevents GUI freeze on large files (270KB MIDI = 17 chunks = 17 GUI polls). */
#define DDR_COPY_CHUNK   (16UL * 1024UL)

/* GUI refresh throttle: update_display() called every N player_tick() calls.
 * At ~64ms/tick: 6 ticks = ~384ms -> ~2.6 Hz display refresh.
 * Set lower for faster display, higher to reduce CPU. */
#define GUI_REFRESH_TICKS  6u

/* ARM loading timeout in Delay(1) units (1 unit = ~20ms on PAL Amiga).
 * SC55 10MB TSF load takes ~3-5 seconds on Cortex-A9.
 * 300 iterations x 20ms = 6 seconds -- generous margin. */
#define ARM_LOAD_TIMEOUT  300UL

/* ================================================================
 * Gadget IDs
 * ================================================================ */
#define GAD_PREV     1
#define GAD_REW      2
#define GAD_STOP     3
#define GAD_PLAY     4
#define GAD_PAUSE    5
#define GAD_FF       6
#define GAD_NEXT     7
#define GAD_OPEN     8
#define GAD_PLS      9
#define GAD_LOOP     10
#define GAD_SF2      11
#define GAD_PROGRESS 12
#define GAD_COUNT    13

/* Playlist-window button gadget IDs (separate gadget context). */
#define PLG_LIST     1
#define PLG_ADD      2
#define PLG_DEL      3
#define PLG_UP       4
#define PLG_DOWN     5
#define PLG_CLEAR    6
#define PLG_PLAY     7

/* ================================================================
 * Transport button bitmap icons -- 16x8 pixels, 1 bitplane
 * ================================================================ */
#define BTN_ICON_W  16
#define BTN_ICON_H   8

static const UWORD g_idata_prev[BTN_ICON_H]  = { 0x0000,0x0408,0x0C08,0x1C08,0x1C08,0x0C08,0x0408,0x0000 };
static const UWORD g_idata_stop[BTN_ICON_H]  = { 0x0000,0x07E0,0x07E0,0x07E0,0x07E0,0x07E0,0x07E0,0x0000 };
static const UWORD g_idata_play[BTN_ICON_H]  = { 0x1000,0x1800,0x1C00,0x1E00,0x1C00,0x1800,0x1000,0x0000 };
static const UWORD g_idata_pause[BTN_ICON_H] = { 0x0000,0x1B00,0x1B00,0x1B00,0x1B00,0x1B00,0x1B00,0x0000 };
static const UWORD g_idata_next[BTN_ICON_H]  = { 0x0000,0x1080,0x1880,0x1C80,0x1C80,0x1880,0x1080,0x0000 };
static const UWORD g_idata_open[BTN_ICON_H]  = { 0x0000,0x1800,0x3FC0,0x2040,0x2040,0x2040,0x3FC0,0x0000 };
static const UWORD g_idata_pls[BTN_ICON_H]   = { 0x0000,0x3FE0,0x0000,0x3FE0,0x0000,0x3FE0,0x0000,0x0000 };
static const UWORD g_idata_loop[BTN_ICON_H]  = { 0x0000,0x1FC0,0x2020,0x2070,0x7020,0x1020,0x0FC0,0x0000 };
static const UWORD g_idata_sf2[BTN_ICON_H]   = { 0x0F00,0x0F00,0x0900,0x0900,0x8900,0xC900,0x7800,0x0000 };
/* Blank icon for the disabled REW/FF buttons. */
static const UWORD g_idata_blank[BTN_ICON_H] = { 0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000 };

static struct Image g_btn_img[11];

#define PROGRESS_MAX 1000UL
#define WIN_WIDTH    340

/* ================================================================
 * Playlist
 * ================================================================ */
#define MAX_PLAYLIST  256
#define PATH_BUF_SIZE 512

/* ================================================================
 * GUI state machine
 * ================================================================ */
typedef enum {
    GSTATE_IDLE         = 0,
    GSTATE_COPYING_SF2  = 1,
    GSTATE_COPYING_MIDI = 2,
    GSTATE_ARM_LOADING  = 3,
    GSTATE_PLAYING      = 4,
    GSTATE_PAUSED       = 5,
    GSTATE_STOPPING     = 6,
    GSTATE_ERROR        = 7
    /* SF2_BEGIN/CHUNK/COMMIT states removed -- chunked path disabled v1 */
} GUIState;

/* ================================================================
 * Global state
 * ================================================================ */
static struct Window  *g_win    = NULL;
static struct Screen  *g_scr    = NULL;
static APTR            g_vi     = NULL;
static struct Gadget  *g_glist  = NULL;
static struct Gadget  *g_gad[GAD_COUNT];

static WORD g_tx, g_ty[3], g_th;
static WORD g_iw;

/* ZZ9000 */
static UBYTE *g_board = NULL;
static UBYTE *g_fb    = NULL;
static UBYTE *g_ctrl  = NULL;

/* GUI state machine */
static GUIState g_gstate    = GSTATE_IDLE;
static ULONG    g_arm_load_timer = 0;  /* countdown for ARM_LOADING timeout */
static int      g_next_track = -1;     /* track to start after ARM_LOADING */

/* SF2 */
static char  g_sf2path[PATH_BUF_SIZE] = "";
static ULONG g_sf2size = 0;
static BOOL  g_sf2_loaded = FALSE;

/* SF2 chunked upload state (XX19) */
static UBYTE *g_sf2_buf        = NULL;   /* full SF2 in Amiga RAM           */
static ULONG  g_sf2_total      = 0;      /* total SF2 size                  */
static ULONG  g_sf2_chunk_off  = 0;      /* offset of next chunk to send    */
static BOOL   g_sf2_chunked    = FALSE;  /* TRUE = using chunked path        */

/* DDR copy state (for chunked non-blocking copy) */
static UBYTE *g_copy_src   = NULL;  /* Amiga RAM source buffer       */
static ULONG  g_copy_size  = 0;     /* total bytes to copy           */
static ULONG  g_copy_pos   = 0;     /* bytes copied so far           */
static ULONG  g_copy_midi_size = 0; /* midi_size to pass to ARM      */
static int    g_copy_next_track = -1;

/* Playlist */
static char  *g_playlist[MAX_PLAYLIST];
static int    g_pcount = 0;
static int    g_cur    = -1;

/* Playback */
static ULONG  g_cmd_seq        = 0;
static ULONG  g_total_ms       = 0;
static ULONG  g_play_ms        = 0;
static ULONG  g_pcm_read_total = 0;
static ULONG  g_arm_bp_skips   = 0;  /* ARM backpressure skips (ring full) */
static ULONG  g_ahi_underruns  = 0;  /* AHI underruns (ring empty, silence) */

/* AHI double-buffer */
static struct AHIRequest *g_req[2]  = {NULL, NULL};
static struct MsgPort    *g_ahiport = NULL;
static BOOL               g_act[2] = {FALSE, FALSE};
static int                g_acur = 0, g_anext = 1;

/* Local PCM buffers (Fast RAM) */
static UWORD g_buf0[AHI_BUF_SAMPLES * 2];
static UWORD g_buf1[AHI_BUF_SAMPLES * 2];

/* Display */
static char  g_dispname[80];
static BOOL  g_quit = FALSE;
static BOOL  g_loop = FALSE;
/* Set during a stop+reload sequence so no path mistakes the player
 * for still-playing while TSF/heap are being destroyed and reloaded. */
static BOOL  g_reloading = FALSE;

static char  g_row_cache[3][80];
static ULONG g_last_slider  = 0xFFFFFFFFUL;
static ULONG g_refresh_tick = 0;  /* throttle counter for update_display */

/* Playlist window */
static struct Window *g_plswin   = NULL;
static struct Gadget *g_plsglist = NULL;
static struct Gadget *g_plsgad   = NULL;
/* Playlist editing: button gadgets + currently highlighted row. */
static struct Gadget *g_plsbtn[PLG_PLAY + 1] = { NULL };
static int            g_pls_sel  = -1;   /* selected row (for Del/Up/Dn) */
static struct List    g_pls_execlist;
static struct Node    g_pls_nodes[MAX_PLAYLIST];
/* Display strings with a leading ASCII marker showing selected/current row.
 * GadTools 3.1 has no reliable persistent listview highlight, so we mark the
 * text instead: "*> " sel+current, "*  " current, ">  " selected, "   " none. */
#define PLS_DISP_MAX 280
static char           g_pls_display[MAX_PLAYLIST][PLS_DISP_MAX];

/* ================================================================
 * load_file: ExamineFH-based, reliable on AmigaOS
 * ================================================================ */
static UBYTE *load_file(const char *path, ULONG *size_out)
{
    BPTR                  fh;
    UBYTE                *buf;
    LONG                  sz;
    struct FileInfoBlock *fib;

    fh = Open((STRPTR)path, MODE_OLDFILE);
    if (!fh) return NULL;

    fib = (struct FileInfoBlock *)AllocMem(sizeof(struct FileInfoBlock),
                                           MEMF_PUBLIC | MEMF_CLEAR);
    if (!fib) { Close(fh); return NULL; }
    if (!ExamineFH(fh, fib)) {
        FreeMem(fib, sizeof(struct FileInfoBlock)); Close(fh); return NULL;
    }
    sz = fib->fib_Size;
    FreeMem(fib, sizeof(struct FileInfoBlock));
    if (sz <= 0) { Close(fh); return NULL; }

    buf = (UBYTE *)AllocMem((ULONG)sz, MEMF_ANY);
    if (!buf) { Close(fh); return NULL; }
    if (Read(fh, buf, sz) != sz) {
        FreeMem(buf, (ULONG)sz); Close(fh); return NULL;
    }
    Close(fh);
    *size_out = (ULONG)sz;
    return buf;
}

/* ================================================================
 * DBG_STAGE: checkpoint for locating crashes.
 *
 * Two independent switches:
 *   ZZMIDI_DEBUG_STAGES : show the stage label in the window title.
 *                         Light -- a single SetWindowTitles, no Delay.
 *   ZZMIDI_DEBUG_DELAY  : add Delay(10) after each checkpoint.
 *                         THIS is what can mask a 68k/ARM race condition,
 *                         so keep it 0 for the baseline stability test.
 *
 * For the baseline test (per ChatGPT): STAGES=1, DELAY=0.
 * Title still updates (so TRIG_1/TRIG_2 localize a crash) but no Delay
 * slows the 68k, so a real race condition will reappear if it exists.
 * ================================================================ */
#define ZZMIDI_DEBUG_STAGES 1
#define ZZMIDI_DEBUG_DELAY  0

static void draw_row(int row, const char *str);  /* forward decl */
static void pls_refresh_listview(void);           /* forward decl */

#if ZZMIDI_DEBUG_STAGES
static void dbg_stage(const char *label)
{
    /* No-op in release: must not touch the window title. */
    (void)label;
}
#define DBG_STAGE(s)  dbg_stage(s)
#else
#define DBG_STAGE(s)  ((void)0)
#endif

/* ================================================================
 * ddr_copy_chunk: copy one DDR_COPY_CHUNK from Amiga RAM to DDR.
 * Writes as ULONGs (4 bytes/bus cycle) for Zorro III efficiency.
 * Big-endian reconstruction preserves byte order for ARM blobs.
 * Returns TRUE when copy is complete.
 * ================================================================ */
static BOOL ddr_copy_chunk(volatile ULONG *ddrdst)
{
    UBYTE *sb;
    ULONG  rem, chunk, n4, nr, k, tmp;
    volatile ULONG *dl;
    volatile UBYTE *db;

    if (!g_copy_src || g_copy_pos >= g_copy_size)
        return TRUE;

    rem   = g_copy_size - g_copy_pos;
    chunk = (rem > DDR_COPY_CHUNK) ? DDR_COPY_CHUNK : rem;
    sb    = g_copy_src + g_copy_pos;
    dl    = ddrdst + g_copy_pos / 4;
    n4    = chunk / 4UL;
    nr    = chunk % 4UL;

    for (k = 0; k < n4; k++) {
        tmp  = ((ULONG)sb[0] << 24) | ((ULONG)sb[1] << 16) |
               ((ULONG)sb[2] <<  8) |  (ULONG)sb[3];
        *dl++ = tmp;
        sb   += 4;
    }
    if (nr > 0) {
        db = (volatile UBYTE *)dl;
        for (k = 0; k < nr; k++) db[k] = sb[k];
    }

    g_copy_pos += chunk;
    return (g_copy_pos >= g_copy_size);
}

/* ================================================================
 * set_dispname: extract basename, strip .mid/.sf2 extension
 * ================================================================ */
static void set_dispname(const char *path)
{
    const char *n = path, *p = path;
    int len;
    while (*p) { if (*p == '/' || *p == ':') n = p + 1; p++; }
    len = (int)strlen(n);
    if (len >= (int)sizeof(g_dispname)) len = (int)sizeof(g_dispname) - 1;
    memcpy(g_dispname, n, (size_t)len);
    g_dispname[len] = '\0';
    if (len > 4) {
        char *ext = g_dispname + len - 4;
        if (ext[0] == '.') *ext = '\0';
    }
}

/* ================================================================
 * draw_row: direct RastPort text with cache
 * ================================================================ */
static void draw_row(int row, const char *str)
{
    struct RastPort *rp;
    WORD x, y;
    const char *s = (str && str[0]) ? str : "";

    if (strcmp(g_row_cache[row], s) == 0) return;
    strncpy(g_row_cache[row], s, 79);
    g_row_cache[row][79] = '\0';

    rp = g_win->RPort;
    x  = g_tx;
    y  = g_ty[row];
    SetAPen(rp, 0);
    RectFill(rp, x, y, x + g_iw - 1, y + g_th - 1);
    if (s[0]) {
        SetAPen(rp, 1);
        SetDrMd(rp, JAM1);
        Move(rp, x + 2, y + rp->Font->tf_Baseline);
        Text(rp, (STRPTR)s, (LONG)strlen(s));
    }
}

/* ================================================================
 * error_name: human-readable XMID error code
 * ================================================================ */
static const char *error_name(ULONG err)
{
    switch (err) {
    case 0:  return "NONE";
    case 1:  return "BAD_MAGIC";
    case 2:  return "BAD_SF2";
    case 3:  return "BAD_MIDI";
    case 4:  return "NO_HEAP (SF2 too large)";
    case 5:  return "TSF_FAIL";
    case 6:  return "TML_FAIL";
    case 7:  return "FPU_FAIL";
    case 8:  return "UNDERRUN";
    default: return "UNKNOWN";
    }
}

/* ================================================================
 * update_display -- throttled to GUI_REFRESH_TICKS
 * ================================================================ */
static void update_display(void)
{
    char buf[80];
    ULONG play_ms, total_ms, pm, ps, tm, ts;
    const char *sf2n;
    const char *p2;

    /* Throttle: only redraw every GUI_REFRESH_TICKS calls */
    if (g_gstate == GSTATE_PLAYING || g_gstate == GSTATE_PAUSED) {
        g_refresh_tick++;
        if (g_refresh_tick < GUI_REFRESH_TICKS) return;
        g_refresh_tick = 0;
    }

    if (g_gstate == GSTATE_PLAYING) {
        CacheClearU();
        play_ms  = CTRL_RD32(g_ctrl, XMID_OFF_PLAY_MS);
        total_ms = g_total_ms;
    } else {
        play_ms  = g_play_ms;
        total_ms = g_total_ms;
    }

    pm = play_ms  / 60000UL; ps = (play_ms  % 60000UL) / 1000UL;
    tm = total_ms / 60000UL; ts = (total_ms % 60000UL) / 1000UL;

    sf2n = g_sf2path[0] ? g_sf2path : "no SF2";
    p2 = sf2n;
    while (*p2) { if (*p2 == '/' || *p2 == ':') sf2n = p2 + 1; p2++; }

    /* Row 0: track name */
    draw_row(0, g_dispname);

    /* Row 1: time + SF2 info */
    switch (g_gstate) {
    case GSTATE_IDLE:
        /* MB with one decimal, integer-only (no float, avoids __mulsf3 on
         * FPU-less 68020). tenths = (bytes * 10) / (1024*1024), but compute
         * via KB first to avoid overflow on large SF2. */
        {
            ULONG kb     = g_sf2size / 1024UL;          /* up to ~32768 */
            ULONG mb_int = kb / 1024UL;                 /* whole MB      */
            ULONG mb_dec = ((kb % 1024UL) * 10UL) / 1024UL; /* 0..9      */
            sprintf(buf, "SF2: %s  (%lu.%lu MB)",
                    sf2n, (unsigned long)mb_int, (unsigned long)mb_dec);
        }
        break;
    case GSTATE_COPYING_SF2:
        sprintf(buf, "Copying SF2... %lu/%lu KB",
                g_copy_pos / 1024UL, g_copy_size / 1024UL);
        break;
    case GSTATE_COPYING_MIDI:
        sprintf(buf, "Copying MIDI... %lu/%lu KB",
                g_copy_pos / 1024UL, g_copy_size / 1024UL);
        break;
    case GSTATE_ARM_LOADING:
        sprintf(buf, "ARM loading... (timeout in %lu s)",
                g_arm_load_timer / 1000UL);
        break;
    case GSTATE_PLAYING:
    case GSTATE_PAUSED:
        sprintf(buf, "%lu:%02lu / %lu:%02lu  [%s]", pm, ps, tm, ts, sf2n);
        break;
    case GSTATE_ERROR:
        sprintf(buf, "ERROR -- heap: %lu KB / %lu KB",
                CTRL_RD32(g_ctrl, XMID_OFF_HEAP_USED) / 1024UL,
                CTRL_RD32(g_ctrl, XMID_OFF_HEAP_HIGHWATER) / 1024UL);
        break;
    default:
        sprintf(buf, "SF2: %s", sf2n);
        break;
    }
    draw_row(1, buf);

    /* Row 2: status + counters */
    switch (g_gstate) {
    case GSTATE_IDLE:
        sprintf(buf, "Ready -- OUV to load MIDI");
        break;
    case GSTATE_PLAYING: {
        ULONG arm_bp = CTRL_RD32(g_ctrl, XMID_OFF_UNDERRUNS);
        ULONG r_us   = CTRL_RD32(g_ctrl, XMID_OFF_DEBUG0);
        ULONG r_max  = CTRL_RD32(g_ctrl, XMID_OFF_DEBUG1);
        ULONG ringlo = CTRL_RD32(g_ctrl, XMID_OFF_DEBUG2);
        /* render us per block vs ~32000us budget at 32kHz/1024 frames.
         * ringlo in bytes; /4 = frames; low value = near underrun. */
        sprintf(buf, "rnd %lu/%lu us  ringlo %luK  bp=%lu",
                r_us, r_max, ringlo / 1024UL, arm_bp);
        break;
    }
    case GSTATE_PAUSED:
        sprintf(buf, "[PAUSED]  %lu:%02lu / %lu:%02lu", pm, ps, tm, ts);
        break;
    case GSTATE_ERROR: {
        ULONG err = CTRL_RD32(g_ctrl, XMID_OFF_ERROR);
        sprintf(buf, "Err: %s", error_name(err));
        break;
    }
    default:
        buf[0] = '\0';
        break;
    }
    draw_row(2, buf);

    /* Progress bar */
    if (g_gad[GAD_PROGRESS] && total_ms > 0 &&
        (g_gstate == GSTATE_PLAYING || g_gstate == GSTATE_PAUSED)) {
        ULONG level = (play_ms < total_ms)
                    ? (play_ms * PROGRESS_MAX / total_ms)
                    : PROGRESS_MAX;
        if (level >= g_last_slider + 5 || level + 5 <= g_last_slider
                || g_last_slider == 0xFFFFFFFFUL) {
            struct RastPort *rp = g_win->RPort;
            WORD gx = g_gad[GAD_PROGRESS]->LeftEdge + 2;
            WORD gy = g_gad[GAD_PROGRESS]->TopEdge  + 2;
            WORD gw = g_gad[GAD_PROGRESS]->Width  - 4;
            WORD gh = g_gad[GAD_PROGRESS]->Height - 4;
            WORD fw = (gw > 0) ? (WORD)((ULONG)gw * level / PROGRESS_MAX) : 0;
            SetAPen(rp, 1);
            if (fw > 0) RectFill(rp, gx, gy, gx + fw - 1, gy + gh - 1);
            SetAPen(rp, 0);
            if (fw < gw) RectFill(rp, gx + fw, gy, gx + gw - 1, gy + gh - 1);
            g_last_slider = level;
        }
    } else if (g_gstate == GSTATE_IDLE || g_gstate == GSTATE_ERROR) {
        if (g_gad[GAD_PROGRESS] && g_last_slider != 0UL) {
            struct RastPort *rp = g_win->RPort;
            WORD gx = g_gad[GAD_PROGRESS]->LeftEdge + 2;
            WORD gy = g_gad[GAD_PROGRESS]->TopEdge  + 2;
            WORD gw = g_gad[GAD_PROGRESS]->Width  - 4;
            WORD gh = g_gad[GAD_PROGRESS]->Height - 4;
            SetAPen(rp, 0);
            RectFill(rp, gx, gy, gx + gw - 1, gy + gh - 1);
            g_last_slider = 0UL;
        }
    }
}

static void display_force_refresh(void)
{
    g_row_cache[0][0] = '\0';
    g_row_cache[1][0] = '\0';
    g_row_cache[2][0] = '\0';
    g_last_slider     = 0xFFFFFFFFUL;
    g_refresh_tick    = GUI_REFRESH_TICKS; /* trigger immediate redraw */
    update_display();
}

/* ================================================================
 * AHI helpers
 * ================================================================ */
static void ahi_stop_all(void)
{
    int i;
    for (i = 0; i < 2; i++) {
        if (g_req[i] && g_act[i]) {
            if (!CheckIO((struct IORequest *)g_req[i]))
                AbortIO((struct IORequest *)g_req[i]);
            WaitIO((struct IORequest *)g_req[i]);
            g_act[i] = FALSE;
        }
    }
}

static void send_ahi_chunk(int slot, UWORD *buf, ULONG len_bytes)
{
    g_req[slot]->ahir_Std.io_Command = CMD_WRITE;
    g_req[slot]->ahir_Std.io_Data    = buf;
    g_req[slot]->ahir_Std.io_Length  = len_bytes;
    g_req[slot]->ahir_Frequency      = XMID_DEFAULT_RATE;
    g_req[slot]->ahir_Type           = AHIST_S16S;
    g_req[slot]->ahir_Volume         = 0x10000L;
    g_req[slot]->ahir_Position       = 0x8000L;
    g_req[slot]->ahir_Link           = g_act[slot ^ 1] ? g_req[slot ^ 1] : NULL;
    SendIO((struct IORequest *)g_req[slot]);
    g_act[slot] = TRUE;
}

/* ================================================================
 * fill_ahi_buf: copy from DDR PCM ring to local AHI buffer.
 * ARM writes s16 LE stereo; AHI AHIST_S16S expects s16 BE stereo.
 * Swap each UWORD on the fly.
 * If not enough data: fill silence and increment g_ahi_underruns.
 * ================================================================ */
static void fill_ahi_buf(UWORD *dst, ULONG samples_wanted)
{
    volatile UBYTE *pcm_ring = g_fb + XMID_PCM_RING_OFFSET;
    ULONG pcm_write, available, bytes_wanted, read_pos;
    ULONG i, n_words;

    CacheClearU();
    pcm_write    = CTRL_RD32(g_ctrl, XMID_OFF_PCM_WRITE_TOTAL);
    available    = pcm_write - g_pcm_read_total;
    bytes_wanted = samples_wanted * 2UL * 2UL;

    if (available < bytes_wanted) {
        /* Real AHI underrun: ring empty, play silence */
        for (i = 0; i < samples_wanted * 2UL; i++) dst[i] = 0;
        g_ahi_underruns++;
        return;
    }

    read_pos = g_pcm_read_total % XMID_PCM_RING_SIZE;
    n_words  = samples_wanted * 2UL;

    for (i = 0; i < n_words; i++) {
        ULONG byte_pos = (read_pos + i * 2UL) % XMID_PCM_RING_SIZE;
        UBYTE lo = pcm_ring[byte_pos];
        UBYTE hi = pcm_ring[byte_pos + 1UL];
        dst[i] = (UWORD)((hi << 8) | lo);
    }

    g_pcm_read_total += bytes_wanted;
    CTRL_WR32(g_ctrl, XMID_OFF_PCM_READ_TOTAL, g_pcm_read_total);
}

/* ================================================================
 * trigger_opcode: write subcmd + seq, fire opcode, wait ACK
 * Returns TRUE on DONE ACK, FALSE on error/timeout.
 * ================================================================ */
static BOOL trigger_opcode(ULONG subcmd)
{
    ULONG timeout;
    UWORD st;

    g_cmd_seq++;
    CTRL_WR32(g_ctrl, XMID_OFF_CMD_SEQ, g_cmd_seq);
    CTRL_WR32(g_ctrl, XMID_OFF_SUBCMD,  subcmd);
    { volatile ULONG d = 20000UL; while (d--); }

    DBG_STAGE("TRIG_1 before REG_CMD write");
    ZZ_WR(g_board, REG_CMD, OP_MIDI_SF2);
    DBG_STAGE("TRIG_2 after REG_CMD, polling status");

    /* Poll REG_STATUS with Delay(1) between reads.
     * Without delay, 500000 tight Zorro reads saturate the bus when the
     * ARM is busy (large SF2 load), causing Guru 0x80000004 on 68060.
     * Delay(1) = ~20ms per iteration, timeout 100 = ~2s total.
     * The ARM ACKs within one idle_poll cycle (<20ms). */
    timeout = 100UL;
    do {
        Delay(1);
        st = ZZ_RD(g_board, REG_STATUS);
        if (st == 2 || st == 3) break;
    } while (--timeout > 0);

    DBG_STAGE("TRIG_3 status poll done");
    return (timeout > 0 && st == 2);
}

/* ================================================================
 * check_done_seq: poll done_seq once, non-blocking.
 * Returns TRUE if ARM has completed the current command.
 *
 * NOTE: no CacheClearU() here. CacheClearU() during ARM D-cache
 * flush (large SF2 load) causes Guru 0x80000004 on 68060.
 * The ctrl block is in non-cached Zorro III space -- direct volatile
 * reads always see the current DDR value without cache operations.
 * ================================================================ */
static BOOL check_done_seq(void)
{
    /* Direct volatile read -- no CacheClearU needed for Zorro III space */
    ULONG done_seq = *((volatile ULONG *)((UBYTE *)g_ctrl + XMID_OFF_DONE_SEQ));
    return (done_seq == g_cmd_seq);
}

/* ================================================================
 * fill_ctrl_block: write standard XMID_Ctrl fields before trigger
 * ================================================================ */
static void fill_ctrl_block(ULONG midi_size)
{
    CTRL_WR32(g_ctrl, XMID_OFF_MAGIC,            XMID_MAGIC);
    CTRL_WR32(g_ctrl, XMID_OFF_VERSION,          XMID_VERSION);
    CTRL_WR32(g_ctrl, XMID_OFF_STRUCT_SIZE,      0xD0UL);  /* 208 bytes (XX19) */
    CTRL_WR32(g_ctrl, XMID_OFF_DONE_SEQ,         0UL);
    CTRL_WR32(g_ctrl, XMID_OFF_STATE,            XMID_STATE_EMPTY);
    CTRL_WR32(g_ctrl, XMID_OFF_ERROR,            XMID_ERR_NONE);
    CTRL_WR32(g_ctrl, XMID_OFF_FLAGS,            0UL);
    CTRL_WR32(g_ctrl, XMID_OFF_SAMPLE_RATE,      XMID_DEFAULT_RATE);
    CTRL_WR32(g_ctrl, XMID_OFF_CHANNELS,         2UL);
    CTRL_WR32(g_ctrl, XMID_OFF_PCM_FORMAT,       1UL);
    CTRL_WR32(g_ctrl, XMID_OFF_SF2_OFFSET,       XMID_SF2_OFFSET);
    CTRL_WR32(g_ctrl, XMID_OFF_SF2_SIZE,         g_sf2size);
    CTRL_WR32(g_ctrl, XMID_OFF_MIDI_OFFSET,      XMID_MIDI_OFFSET);
    CTRL_WR32(g_ctrl, XMID_OFF_MIDI_SIZE,        midi_size);
    CTRL_WR32(g_ctrl, XMID_OFF_PCM_RING_OFFSET,  XMID_PCM_RING_OFFSET);
    CTRL_WR32(g_ctrl, XMID_OFF_PCM_RING_SIZE,    XMID_PCM_RING_SIZE);
    CTRL_WR32(g_ctrl, XMID_OFF_PCM_WRITE_TOTAL,  0UL);
    CTRL_WR32(g_ctrl, XMID_OFF_PCM_READ_TOTAL,   0UL);
    CTRL_WR32(g_ctrl, XMID_OFF_VOLUME_Q16,       XMID_DEFAULT_VOL);
    CTRL_WR32(g_ctrl, XMID_OFF_MAX_VOICES,       XMID_DEFAULT_VOICES);
}

/* ================================================================
 * gstate_set_error: display error and transition to ERROR state
 * ================================================================ */
static void gstate_set_error(const char *msg)
{
    g_gstate = GSTATE_ERROR;
    strncpy(g_dispname, msg, sizeof(g_dispname) - 1);
    display_force_refresh();
}

/* ================================================================
 * player_ahi_stop: stop AHI and update state
 * ================================================================ */
static void player_ahi_stop(void)
{
    ahi_stop_all();
    g_pcm_read_total = 0;
    g_ahi_underruns  = 0;
    g_arm_bp_skips   = 0;
    g_last_slider    = 0xFFFFFFFFUL;
}

/* ================================================================
 * player_send_stop: send STOP to ARM (best effort, non-blocking)
 * ================================================================ */
static void player_send_stop(void)
{
    if (g_gstate == GSTATE_PLAYING || g_gstate == GSTATE_PAUSED) {
        trigger_opcode(XMID_CMD_STOP);
        /* Do not wait: STOP is fire-and-forget */
    }
}

/* ================================================================
 * player_stop_for_reload: the ONE safe way to stop before any reload.
 *
 * Used by EVERY path that triggers a destructive LOAD_MIDI while audio
 * may still be active: auto-advance, loop, Next, Previous, replay, new
 * file. The Guru on large SF2 was a STOP/LOAD race: begin_midi_load()
 * (firmware tsf_close + heap reset + multi-MB SF2 reload) could start
 * while AHI was still consuming the ring and the ARM was still PLAYING.
 * Small SF2 = reload too short to hit the window; large SF2 = long
 * reload = wide window = crash.
 *
 * Order matters: stop the 68k AHI consumer FIRST, then tell the ARM to
 * STOP and WAIT for it to actually finish (done_seq) before returning.
 * player_send_stop() alone is asynchronous and is NOT enough.
 * ================================================================ */
static void player_stop_for_reload(void)
{
    ULONG spin;

    g_reloading = TRUE;

    /* 1. Stop the 68k AHI consumer first so nothing reads the ring while
     *    the ARM tears down TSF/heap. */
    player_ahi_stop();

    /* 2. Tell the ARM to STOP, only if it thinks it is playing/paused. */
    if (g_gstate == GSTATE_PLAYING || g_gstate == GSTATE_PAUSED) {
        if (trigger_opcode(XMID_CMD_STOP)) {
            /* 3. WAIT until the ARM really finished the STOP (done_seq).
             *    Bounded spin with Delay(1) so Intuition still breathes.
             *    ~100 * 20ms = ~2s ceiling; STOP completes in one idle
             *    poll (<20ms) in practice. */
            spin = 100UL;
            while (spin-- > 0UL && !check_done_seq())
                Delay(1);
        }
    }

    /* 4. Reset local consumer counters so the next session starts clean. */
    g_pcm_read_total = 0;
    g_ahi_underruns  = 0;
    g_arm_bp_skips   = 0;
    g_last_slider    = 0xFFFFFFFFUL;

    /* Mark not-playing so the GUI state machine cannot re-enter PLAYING
     * logic before begin_midi_load() drives the next load. */
    g_gstate = GSTATE_IDLE;

    g_reloading = FALSE;
}

/* ================================================================
 * begin_sf2_load: load SF2 into staging DDR.
 * ZZMIDI v1 hard limit: 32 MB. SF2 > 32MB refused with clear error.
 * ================================================================ */
static BOOL begin_sf2_load(const char *path)
{
    UBYTE *buf;
    ULONG  sz;

    if (g_copy_src) { FreeMem(g_copy_src, g_copy_size); g_copy_src = NULL; }
    if (g_sf2_buf)  { FreeMem(g_sf2_buf, g_sf2_total);  g_sf2_buf  = NULL; }
    g_sf2_loaded  = FALSE;
    g_sf2_chunked = FALSE;
    g_sf2size     = 0;

    strncpy(g_dispname, "Reading SF2 from disk...", sizeof(g_dispname) - 1);
    g_row_cache[0][0] = '\0';
    draw_row(0, g_dispname);
    draw_row(1, "");
    draw_row(2, "");

    buf = load_file(path, &sz);
    if (!buf) { gstate_set_error("Cannot open SF2"); return FALSE; }
    if (sz == 0) { FreeMem(buf, sz); gstate_set_error("SF2 empty"); return FALSE; }

    if (sz > XMID_SF2_MAX_SIZE) {
        FreeMem(buf, sz);
        gstate_set_error("SF2 >32MB: not supported in ZZMIDI v0.5");
        return FALSE;
    }

    strncpy(g_sf2path, path, PATH_BUF_SIZE - 1);
    g_sf2size   = sz;
    g_copy_src  = buf;
    g_copy_size = sz;
    g_copy_pos  = 0;

    g_gstate = GSTATE_COPYING_SF2;
    strncpy(g_dispname, "Loading SF2...", sizeof(g_dispname) - 1);
    display_force_refresh();
    return TRUE;
}


/* ================================================================
 * begin_midi_load: start MIDI load sequence (chunked DDR copy)
 * Called after SF2 is loaded and DDR SF2 copy is done.
 * ================================================================ */
static BOOL begin_midi_load(int track)
{
    UBYTE      *buf;
    ULONG       sz;
    const char *path;

    if (track < 0 || track >= g_pcount) return FALSE;
    if (!g_sf2_loaded) {
        gstate_set_error("No SF2 -- press SF2 button");
        return FALSE;
    }

    path  = g_playlist[track];
    g_cur = track;
    set_dispname(path);

    /* Keep the playlist window's "*" current marker in sync if it is open. */
    if (g_plswin && g_plsgad) pls_refresh_listview();

    /* Show "Reading..." immediately so the user sees activity.
     * load_file() blocks on disk I/O -- even 89KB can take >0.5s
     * on slow SCSI, causing a perceived freeze without this message. */
    g_row_cache[1][0] = '\0';  /* force redraw of row 1 */
    draw_row(0, g_dispname);
    draw_row(1, "Reading MIDI from disk...");
    draw_row(2, "");

    buf = load_file(path, &sz);
    if (!buf) { gstate_set_error("Cannot open MIDI file"); return FALSE; }
    if (sz > XMID_MIDI_MAX_SIZE) {
        FreeMem(buf, sz);
        gstate_set_error("MIDI too large (>8MB staging limit)");
        return FALSE;
    }

    if (g_copy_src) { FreeMem(g_copy_src, g_copy_size); g_copy_src = NULL; }

    g_copy_src        = buf;
    g_copy_size       = sz;
    g_copy_midi_size  = sz;
    g_copy_pos        = 0;

    g_gstate = GSTATE_COPYING_MIDI;
    display_force_refresh();
    return TRUE;
}

/* ================================================================
 * start_playing: AHI pre-fill and start after ARM reports READY
 * ================================================================ */
static BOOL start_playing(void)
{
    ULONG wait_count;

    CTRL_WR32(g_ctrl, XMID_OFF_DONE_SEQ, 0UL);
    if (!trigger_opcode(XMID_CMD_START)) {
        gstate_set_error("START: no ACK from ARM");
        return FALSE;
    }

    /* Wait for START done_seq -- short synchronous wait is acceptable here
     * since START completes almost immediately (just a state change on ARM) */
    wait_count = 2000000UL;
    while (wait_count-- > 0 && !check_done_seq()) {
        { volatile ULONG d = 10UL; while (d--); }
    }
    if (!check_done_seq()) {
        gstate_set_error("START timeout");
        return FALSE;
    }

    /* Pre-fill: wait for ARM to produce at least one AHI buffer */
    wait_count = 3000000UL;
    while (wait_count-- > 0) {
        CacheClearU();
        if (CTRL_RD32(g_ctrl, XMID_OFF_PCM_WRITE_TOTAL) >= AHI_BUF_BYTES)
            break;
        { volatile ULONG d = 10UL; while (d--); }
    }

    g_pcm_read_total = 0;
    g_ahi_underruns  = 0;
    g_arm_bp_skips   = 0;
    g_acur           = 0;
    g_anext          = 1;
    g_refresh_tick   = GUI_REFRESH_TICKS;

    fill_ahi_buf(g_buf0, AHI_BUF_SAMPLES);
    send_ahi_chunk(0, g_buf0, AHI_BUF_BYTES);

    g_gstate = GSTATE_PLAYING;
    display_force_refresh();
    return TRUE;
}

/* ================================================================
 * player_tick: one AHI iteration when PLAYING.
 * Returns FALSE when track is done.
 * Critical: fill+send g_anext BEFORE WaitIO on g_acur (no gap).
 * ================================================================ */
static BOOL player_tick(void)
{
    ULONG arm_state;
    UWORD *buf;

    if (g_gstate != GSTATE_PLAYING) return TRUE;

    CacheClearU();
    arm_state = CTRL_RD32(g_ctrl, XMID_OFF_STATE);

    /* Fill next buffer and send BEFORE waiting on current */
    buf = (g_anext == 0) ? g_buf0 : g_buf1;
    fill_ahi_buf(buf, AHI_BUF_SAMPLES);
    send_ahi_chunk(g_anext, buf, AHI_BUF_BYTES);

    /* Now wait for previously playing slot */
    if (g_act[g_acur]) {
        WaitIO((struct IORequest *)g_req[g_acur]);
        g_act[g_acur] = FALSE;
    }

    { int t = g_acur; g_acur = g_anext; g_anext = t; }

    /* Update ARM backpressure counter (read from ctrl block) */
    g_arm_bp_skips = CTRL_RD32(g_ctrl, XMID_OFF_UNDERRUNS);

    if (arm_state == XMID_STATE_DONE) {
        if (g_act[g_acur]) {
            WaitIO((struct IORequest *)g_req[g_acur]);
            g_act[g_acur] = FALSE;
        }
        g_play_ms = g_total_ms;
        g_gstate  = GSTATE_IDLE;
        player_ahi_stop();
        return FALSE;
    }

    update_display();
    return TRUE;
}

/* ================================================================
 * main_loop_tick: state machine step called once per main loop.
 * Handles non-blocking transitions between states.
 * ================================================================ */
static void main_loop_tick(void)
{
    switch (g_gstate) {

    case GSTATE_COPYING_SF2: {
        /* Copy one chunk of SF2 to DDR */
        volatile ULONG *dst = (volatile ULONG *)(g_fb + XMID_SF2_OFFSET);
        BOOL done = ddr_copy_chunk(dst);
        update_display();
        if (done) {
            FreeMem(g_copy_src, g_copy_size);
            g_copy_src   = NULL;
            g_sf2_loaded = TRUE;
            g_gstate     = GSTATE_IDLE;
            strncpy(g_dispname, "SF2 loaded -- OUV to load MIDI",
                    sizeof(g_dispname) - 1);
            display_force_refresh();
            if (g_copy_next_track >= 0) {
                int t = g_copy_next_track;
                g_copy_next_track = -1;
                begin_midi_load(t);
            }
        }
        break;
    }

    case GSTATE_COPYING_MIDI: {
        /* Copy one chunk of MIDI to DDR */
        volatile ULONG *dst = (volatile ULONG *)(g_fb + XMID_MIDI_OFFSET);
        BOOL done = ddr_copy_chunk(dst);
        update_display();
        if (done) {
            DBG_STAGE("M1: MIDI copied, before FreeMem");
            /* MIDI copy complete -- trigger ARM LOAD_MIDI */
            FreeMem(g_copy_src, g_copy_size);
            g_copy_src = NULL;
            DBG_STAGE("M2: after FreeMem, before fill_ctrl");

            fill_ctrl_block(g_copy_midi_size);
            DBG_STAGE("M3: after fill_ctrl, before trigger");
            if (!trigger_opcode(XMID_CMD_LOAD_MIDI)) {
                gstate_set_error("LOAD_MIDI: no ACK");
                break;
            }
            DBG_STAGE("M4: after trigger, entering ARM_LOADING");
            /* Transition to non-blocking ARM wait */
            g_arm_load_timer = ARM_LOAD_TIMEOUT;
            g_gstate = GSTATE_ARM_LOADING;
            display_force_refresh();
        }
        break;
    }
    case GSTATE_ARM_LOADING: {
        /* Non-blocking poll for ARM completion.
         *
         * Rules (per ChatGPT review):
         * 1. No CacheClearU() -- done_seq read as volatile direct (see check_done_seq).
         * 2. No update_display() -- only static draw_row, no DDR stat reads.
         * 3. Delay(1) every iteration -- no 100% CPU free loop hammering Intuition.
         * 4. Only read done_seq, state, error after completion -- never heap/play_ms.
         * 5. done_seq is written last by ARM after full ctrl block flush -- safe.
         * 6. Never read/display fb+XMID_HEAP_DDR_OFFSET -- ARM private zone. */

        /* Delay(1) = ~1/50s = 20ms -- keeps CPU low, lets Intuition breathe */
        Delay(1);

        if (check_done_seq()) {
            /* ARM finished -- now safe to read state and error only */
            ULONG state = CTRL_RD32(g_ctrl, XMID_OFF_STATE);
            ULONG err   = CTRL_RD32(g_ctrl, XMID_OFF_ERROR);
            if (state == XMID_STATE_READY) {
                g_total_ms = CTRL_RD32(g_ctrl, XMID_OFF_TOTAL_MS);
                start_playing();
            } else {
                char msg[80];
                sprintf(msg, "ARM error: %s", error_name(err));
                gstate_set_error(msg);
            }
        } else {
            /* Decrement timeout and update display statically (~every 500ms) */
            if (g_arm_load_timer > 0) {
                g_arm_load_timer--;
                /* Delay(1) = 20ms, so 25 iterations = ~500ms */
                if ((g_arm_load_timer % 25UL) == 0UL) {
                    char buf[80];
                    ULONG secs = g_arm_load_timer / 50UL; /* 50 = 1s at Delay(1) */
                    sprintf(buf, "ARM loading SF2+MIDI... (%lu s)", secs);
                    g_row_cache[1][0] = '\0';
                    draw_row(1, buf);
                }
            } else {
                gstate_set_error("ARM LOAD_MIDI timeout");
            }
        }
        break;
    }

    case GSTATE_PLAYING: {
        BOOL cont = player_tick();
        if (!cont) {
            /* Track finished: auto-advance. Use the single synchronous
             * stop-for-reload helper so AHI and the ARM are fully stopped
             * before the destructive LOAD_MIDI begins (fixes large-SF2
             * Guru). */
            player_stop_for_reload();

            if (g_cur < g_pcount - 1) {
                begin_midi_load(g_cur + 1);
            } else if (g_loop && g_pcount > 0) {
                begin_midi_load(0);
            } else {
                strncpy(g_dispname, "End of playlist",
                        sizeof(g_dispname) - 1);
                display_force_refresh();
            }
        }
        break;
    }

    case GSTATE_IDLE:
    case GSTATE_PAUSED:
    case GSTATE_ERROR:
    default:
        break;
    }
}

/* ================================================================
 * Icon rendering
 * ================================================================ */
static void init_btn_images(void)
{
    static const UWORD * const src[11] = {
        g_idata_prev, g_idata_blank, g_idata_stop,
        g_idata_play, g_idata_pause, g_idata_blank,
        g_idata_next, g_idata_open, g_idata_pls,
        g_idata_loop, g_idata_sf2
    };
    int i;
    for (i = 0; i < 11; i++) {
        g_btn_img[i].LeftEdge   = 0;
        g_btn_img[i].TopEdge    = 0;
        g_btn_img[i].Width      = BTN_ICON_W;
        g_btn_img[i].Height     = BTN_ICON_H;
        g_btn_img[i].Depth      = 1;
        g_btn_img[i].ImageData  = (UWORD *)src[i];
        g_btn_img[i].PlanePick  = 0x01;
        g_btn_img[i].PlaneOnOff = 0x00;
        g_btn_img[i].NextImage  = NULL;
    }
}

static void draw_icons(struct Window *win)
{
    int i;
    if (!win) return;
    for (i = 1; i <= 11; i++) {
        if (g_gad[i]) {
            WORD ix = g_gad[i]->LeftEdge +
                      (WORD)((g_gad[i]->Width  - BTN_ICON_W) / 2);
            WORD iy = g_gad[i]->TopEdge  +
                      (WORD)((g_gad[i]->Height - BTN_ICON_H) / 2);
            DrawImage(win->RPort, &g_btn_img[i - 1], ix, iy);
        }
    }
    if (g_gad[GAD_LOOP]) {
        WORD bx = g_gad[GAD_LOOP]->LeftEdge + g_gad[GAD_LOOP]->Width  - 5;
        WORD by = g_gad[GAD_LOOP]->TopEdge  + g_gad[GAD_LOOP]->Height - 5;
        SetAPen(win->RPort, g_loop ? 1 : 0);
        RectFill(win->RPort, bx, by, bx + 2, by + 2);
    }
    if (g_gad[GAD_SF2]) {
        WORD bx = g_gad[GAD_SF2]->LeftEdge + g_gad[GAD_SF2]->Width  - 5;
        WORD by = g_gad[GAD_SF2]->TopEdge  + g_gad[GAD_SF2]->Height - 5;
        SetAPen(win->RPort, g_sf2_loaded ? 1 : 0);
        RectFill(win->RPort, bx, by, bx + 2, by + 2);
    }
}

/* ================================================================
 * Playlist editing helpers
 * ================================================================ */

/* Return the file name part of a full path (after the last '/' or ':'). */
static const char *pls_file_leaf(const char *p)
{
    const char *s = p;
    const char *last = p;
    if (!p) return "";
    while (*s) {
        if (*s == '/' || *s == ':') last = s + 1;
        s++;
    }
    return last;
}

/* Rebuild the exec List that backs the listview from g_playlist[], adding a
 * leading ASCII marker so the user can see which row is selected and which is
 * currently playing (g_cur). */
static void pls_rebuild_exec_list(void)
{
    int i;

    g_pls_execlist.lh_Head     = (struct Node *)&g_pls_execlist.lh_Tail;
    g_pls_execlist.lh_Tail     = NULL;
    g_pls_execlist.lh_TailPred = (struct Node *)&g_pls_execlist;

    for (i = 0; i < g_pcount; i++) {
        const char *prefix;
        if (i == g_cur && i == g_pls_sel)      prefix = "*> ";
        else if (i == g_cur)                   prefix = "*  ";
        else if (i == g_pls_sel)               prefix = ">  ";
        else                                   prefix = "   ";

        g_pls_display[i][0] = '\0';
        strncpy(g_pls_display[i], prefix, PLS_DISP_MAX - 1);
        strncpy(g_pls_display[i] + 3, pls_file_leaf(g_playlist[i]),
                PLS_DISP_MAX - 1 - 3);
        g_pls_display[i][PLS_DISP_MAX - 1] = '\0';

        g_pls_nodes[i].ln_Name = g_pls_display[i];
        g_pls_nodes[i].ln_Type = NT_USER;
        g_pls_nodes[i].ln_Pri  = 0;
        AddTail(&g_pls_execlist, &g_pls_nodes[i]);
    }
}

/* Detach the list from the gadget, rebuild it, then re-attach so GadTools
 * redraws. Must never modify the List while it is attached. Safe whether or
 * not the playlist window is open. */
static void pls_refresh_listview(void)
{
    if (g_plsgad && g_plswin)
        GT_SetGadgetAttrs(g_plsgad, g_plswin, NULL,
                          GTLV_Labels, (ULONG)~0UL, TAG_END);

    pls_rebuild_exec_list();

    if (g_plsgad && g_plswin) {
        GT_SetGadgetAttrs(g_plsgad, g_plswin, NULL,
                          GTLV_Labels, (ULONG)&g_pls_execlist, TAG_END);
        GT_RefreshWindow(g_plswin, NULL);
    }
}

/* Append one path (already a full "drawer/file" string) to the playlist.
 * Returns TRUE on success. */
static BOOL pls_append_path(const char *full)
{
    char *slot;
    if (g_pcount >= MAX_PLAYLIST) return FALSE;
    slot = (char *)AllocMem(PATH_BUF_SIZE, MEMF_ANY | MEMF_CLEAR);
    if (!slot) return FALSE;
    strncpy(slot, full, PATH_BUF_SIZE - 1);
    slot[PATH_BUF_SIZE - 1] = '\0';
    g_playlist[g_pcount++] = slot;
    return TRUE;
}

/* Open an ASL multi-select requester and append every chosen MIDI file. */
static void pls_add_via_asl(void)
{
    struct FileRequester *fr;
    int old_count = g_pcount;
    struct TagItem fr_tags[] = {
        {ASLFR_TitleText,     (ULONG)"Add MIDI files to playlist"},
        {ASLFR_DoMultiSelect, TRUE},
        {TAG_END,             0}
    };

    fr = AllocAslRequest(ASL_FileRequest, fr_tags);
    if (fr && AslRequest(fr, NULL)) {
        char full[PATH_BUF_SIZE];
        int  fi;
        if (fr->fr_NumArgs > 0 && fr->fr_ArgList != NULL) {
            for (fi = 0; fi < (int)fr->fr_NumArgs && g_pcount < MAX_PLAYLIST;
                 fi++) {
                int dl = (int)strlen(fr->fr_Drawer);
                if (dl > PATH_BUF_SIZE - 2) dl = PATH_BUF_SIZE - 2;
                memcpy(full, fr->fr_Drawer, (size_t)dl);
                if (dl > 0 && full[dl-1] != '/' && full[dl-1] != ':')
                    full[dl++] = '/';
                strncpy(full + dl, fr->fr_ArgList[fi].wa_Name,
                        (size_t)(PATH_BUF_SIZE - 1 - dl));
                full[PATH_BUF_SIZE - 1] = '\0';
                pls_append_path(full);
            }
        } else if (fr->fr_File && fr->fr_File[0] && g_pcount < MAX_PLAYLIST) {
            int dl = (int)strlen(fr->fr_Drawer);
            if (dl > PATH_BUF_SIZE - 2) dl = PATH_BUF_SIZE - 2;
            memcpy(full, fr->fr_Drawer, (size_t)dl);
            if (dl > 0 && full[dl-1] != '/' && full[dl-1] != ':')
                full[dl++] = '/';
            strncpy(full + dl, fr->fr_File,
                    (size_t)(PATH_BUF_SIZE - 1 - dl));
            full[PATH_BUF_SIZE - 1] = '\0';
            pls_append_path(full);
        }
    }
    if (fr) FreeAslRequest(fr);
    if (g_pls_sel < 0 && g_pcount > old_count)
        g_pls_sel = old_count;
    pls_refresh_listview();
}

/* Delete the selected entry. Frees its buffer and shifts the rest down.
 * Keeps g_cur consistent so playback of the current track is not disturbed. */
static void pls_delete_selected(void)
{
    int i;
    if (g_pls_sel < 0 || g_pls_sel >= g_pcount) return;

    if (g_playlist[g_pls_sel]) {
        FreeMem(g_playlist[g_pls_sel], PATH_BUF_SIZE);
        g_playlist[g_pls_sel] = NULL;
    }
    for (i = g_pls_sel; i < g_pcount - 1; i++)
        g_playlist[i] = g_playlist[i + 1];
    g_playlist[g_pcount - 1] = NULL;
    g_pcount--;

    /* Fix up the "currently playing" index. */
    if (g_cur == g_pls_sel)        g_cur = -1;
    else if (g_cur > g_pls_sel)    g_cur--;

    if (g_pls_sel >= g_pcount) g_pls_sel = g_pcount - 1;
    pls_refresh_listview();
}

/* Swap the selected entry with its neighbour (dir = -1 up, +1 down) and
 * follow the selection so the user can move an item repeatedly. */
static void pls_move_selected(int dir)
{
    int j;
    char *tmp;
    if (g_pls_sel < 0 || g_pls_sel >= g_pcount) return;
    j = g_pls_sel + dir;
    if (j < 0 || j >= g_pcount) return;

    tmp                 = g_playlist[g_pls_sel];
    g_playlist[g_pls_sel] = g_playlist[j];
    g_playlist[j]       = tmp;

    /* Keep g_cur pointing at the same physical track after the swap. */
    if (g_cur == g_pls_sel)   g_cur = j;
    else if (g_cur == j)      g_cur = g_pls_sel;

    g_pls_sel = j;
    pls_refresh_listview();
}

/* Remove every entry from the playlist. */
static void pls_clear_all(void)
{
    int i;
    for (i = 0; i < g_pcount; i++) {
        if (g_playlist[i]) {
            FreeMem(g_playlist[i], PATH_BUF_SIZE);
            g_playlist[i] = NULL;
        }
    }
    g_pcount  = 0;
    g_pls_sel = -1;
    g_cur     = -1;
    pls_refresh_listview();
}

/* Play the selected row (the only normal way to start from the playlist;
 * a row click just selects). begin_midi_load sets g_cur. */
static void pls_play_selected(void)
{
    if (g_pls_sel < 0 || g_pls_sel >= g_pcount) return;
    player_stop_for_reload();
    begin_midi_load(g_pls_sel);   /* sets g_cur = g_pls_sel */
    pls_refresh_listview();
}


/* ================================================================
 * Playlist window
 * ================================================================ */
static void open_playlist_window(void)
{
    struct NewGadget ng;
    struct Gadget   *prev;
    struct TagItem   lv_tags[4];
    struct TagItem   win_tags[10];
    UWORD bx, by, ih;

    if (g_plswin) return;
    if (!g_scr || !g_vi) return;

    pls_rebuild_exec_list();

    bx = g_scr->WBorLeft;
    by = g_scr->WBorTop + g_scr->Font->ta_YSize + 1;
    ih = 120;

    prev = CreateContext(&g_plsglist);
    if (!prev) return;

    ng.ng_LeftEdge   = bx;
    ng.ng_TopEdge    = by;
    ng.ng_Width      = 320 - bx - g_scr->WBorRight;
    ng.ng_Height     = ih;
    ng.ng_GadgetText = NULL;
    ng.ng_TextAttr   = g_scr->Font;
    ng.ng_GadgetID   = PLG_LIST;
    ng.ng_Flags      = 0;
    ng.ng_VisualInfo = g_vi;

    lv_tags[0].ti_Tag  = GTLV_Labels;       lv_tags[0].ti_Data = (ULONG)&g_pls_execlist;
    lv_tags[1].ti_Tag  = GTLV_ScrollWidth;  lv_tags[1].ti_Data = 16;
    lv_tags[2].ti_Tag  = TAG_END;           lv_tags[2].ti_Data = 0;
    lv_tags[3].ti_Tag  = TAG_END;           lv_tags[3].ti_Data = 0;

    prev = g_plsgad = CreateGadgetA(LISTVIEW_KIND, prev, &ng, lv_tags);
    if (!prev) { FreeGadgets(g_plsglist); g_plsglist = NULL; return; }

    /* Row of buttons under the listview: Play | Add | Del | Up | Dn | Clear */
    {
        static const char *labels[6] =
            { "Play", "Add", "Del", "Up", "Dn", "Clear" };
        static const UWORD ids[6] =
            { PLG_PLAY, PLG_ADD, PLG_DEL, PLG_UP, PLG_DOWN, PLG_CLEAR };
        UWORD listw = (UWORD)(320 - bx - g_scr->WBorRight);
        UWORD gap   = 4;
        UWORD bw    = (UWORD)((listw - 5 * gap) / 6);
        UWORD bh    = (UWORD)(g_scr->Font->ta_YSize + 6);
        UWORD btop  = (UWORD)(by + ih + 4);
        int   k;

        for (k = 0; k < 6; k++) {
            ng.ng_LeftEdge   = (WORD)(bx + k * (bw + gap));
            ng.ng_TopEdge    = (WORD)btop;
            ng.ng_Width      = bw;
            ng.ng_Height     = bh;
            ng.ng_GadgetText = (UBYTE *)labels[k];
            ng.ng_GadgetID   = ids[k];
            ng.ng_Flags      = 0;
            prev = CreateGadgetA(BUTTON_KIND, prev, &ng, NULL);
            g_plsbtn[ids[k]] = prev;
            if (!prev) { FreeGadgets(g_plsglist); g_plsglist = NULL; return; }
        }

        win_tags[0].ti_Tag  = WA_Left;      win_tags[0].ti_Data = 40;
        win_tags[1].ti_Tag  = WA_Top;       win_tags[1].ti_Data = 180;
        win_tags[2].ti_Tag  = WA_Width;     win_tags[2].ti_Data = 320;
        win_tags[3].ti_Tag  = WA_Height;    win_tags[3].ti_Data =
                               (ULONG)(btop + bh + g_scr->WBorBottom + 2);
        win_tags[4].ti_Tag  = WA_Title;     win_tags[4].ti_Data = (ULONG)"MIDI Playlist";
        win_tags[5].ti_Tag  = WA_Gadgets;   win_tags[5].ti_Data = (ULONG)g_plsglist;
        win_tags[6].ti_Tag  = WA_Flags;     win_tags[6].ti_Data =
                               WFLG_DRAGBAR | WFLG_CLOSEGADGET | WFLG_DEPTHGADGET |
                               WFLG_ACTIVATE | WFLG_SMART_REFRESH;
        win_tags[7].ti_Tag  = WA_IDCMP;     win_tags[7].ti_Data =
                               IDCMP_CLOSEWINDOW | IDCMP_REFRESHWINDOW | LISTVIEWIDCMP;
        win_tags[8].ti_Tag  = WA_PubScreen; win_tags[8].ti_Data = (ULONG)g_scr;
        win_tags[9].ti_Tag  = TAG_END;      win_tags[9].ti_Data = 0;
    }

    g_plswin = OpenWindowTagList(NULL, win_tags);
    if (!g_plswin) { FreeGadgets(g_plsglist); g_plsglist = NULL; return; }
    GT_RefreshWindow(g_plswin, NULL);
}

static void handle_pls_idcmp(void)
{
    struct IntuiMessage *msg;
    if (!g_plswin) return;

    while ((msg = GT_GetIMsg(g_plswin->UserPort)) != NULL) {
        ULONG class = msg->Class;
        UWORD code  = msg->Code;
        struct Gadget *gad = (struct Gadget *)msg->IAddress;
        UWORD gid = gad ? gad->GadgetID : 0xffff;
        GT_ReplyIMsg(msg);

        if (class == IDCMP_REFRESHWINDOW) {
            GT_BeginRefresh(g_plswin);
            GT_EndRefresh(g_plswin, TRUE);
            continue;
        }
        if (class == IDCMP_CLOSEWINDOW) {
            CloseWindow(g_plswin); g_plswin = NULL;
            if (g_plsglist) { FreeGadgets(g_plsglist); g_plsglist = NULL; }
            g_plsgad = NULL;
            memset(g_plsbtn, 0, sizeof(g_plsbtn));
            return;
        }

        if (class == IDCMP_GADGETUP) {
            /* LISTVIEW row click: msg->Code is the selected row index.
             * A row click must ONLY select, not start playback. */
            if (gad == g_plsgad || gid == PLG_LIST) {
                if ((int)code >= 0 && (int)code < g_pcount) {
                    g_pls_sel = (int)code;
                    pls_refresh_listview();
                }
                continue;
            }
            switch (gid) {
            case PLG_PLAY:  pls_play_selected();                    break;
            case PLG_ADD:   pls_add_via_asl();                      break;
            case PLG_DEL:   pls_delete_selected();                  break;
            case PLG_UP:    pls_move_selected(-1);                  break;
            case PLG_DOWN:  pls_move_selected(+1);                  break;
            case PLG_CLEAR: player_stop_for_reload(); pls_clear_all(); break;
            default: break;
            }
        }
    }
}

/* ================================================================
 * handle_idcmp: main window events
 * ================================================================ */
static void handle_idcmp(void)
{
    struct IntuiMessage *msg;

    while ((msg = GT_GetIMsg(g_win->UserPort)) != NULL) {
        ULONG  class = msg->Class;
        UWORD  code  = msg->Code;
        struct Gadget *gad = (struct Gadget *)msg->IAddress;
        GT_ReplyIMsg(msg);

        if (class == IDCMP_CLOSEWINDOW) { g_quit = TRUE; return; }

        if (class == IDCMP_REFRESHWINDOW) {
            GT_BeginRefresh(g_win);
            GT_EndRefresh(g_win, TRUE);
            display_force_refresh();
            draw_icons(g_win);
            continue;
        }

        if (class == IDCMP_GADGETUP && gad) {
            switch (gad->GadgetID) {

            case GAD_PLAY:
                if (g_gstate == GSTATE_PAUSED) {
                    UWORD *buf = (g_acur == 0) ? g_buf0 : g_buf1;
                    g_gstate = GSTATE_PLAYING;
                    send_ahi_chunk(g_acur, buf, AHI_BUF_BYTES);
                } else if ((g_gstate == GSTATE_IDLE)
                           && g_pcount > 0 && g_sf2_loaded) {
                    int t;
                    if (g_pls_sel >= 0 && g_pls_sel < g_pcount)
                        t = g_pls_sel;
                    else if (g_cur >= 0 && g_cur < g_pcount)
                        t = g_cur;
                    else
                        t = 0;
                    begin_midi_load(t);
                } else if (g_gstate == GSTATE_ERROR) {
                    /* Retry current track */
                    g_gstate = GSTATE_IDLE;
                    if (g_cur >= 0 && g_sf2_loaded) begin_midi_load(g_cur);
                }
                break;

            case GAD_PAUSE:
                if (g_gstate == GSTATE_PLAYING) {
                    ahi_stop_all();
                    g_gstate = GSTATE_PAUSED;
                    display_force_refresh();
                }
                break;

            case GAD_STOP:
                if (g_gstate == GSTATE_PLAYING || g_gstate == GSTATE_PAUSED) {
                    player_send_stop();
                    player_ahi_stop();
                }
                g_gstate = GSTATE_IDLE;
                strncpy(g_dispname, "Stopped", sizeof(g_dispname) - 1);
                display_force_refresh();
                break;

            case GAD_PREV:
                if (g_cur > 0) {
                    player_stop_for_reload();
                    begin_midi_load(g_cur - 1);
                }
                break;

            case GAD_NEXT:
                if (g_cur < g_pcount - 1) {
                    player_stop_for_reload();
                    begin_midi_load(g_cur + 1);
                }
                break;

            case GAD_REW:
            case GAD_FF:
                /* REW/FF disabled: inherited from the MP3
                 * player (ZZPlay) and are not meaningful for MIDI playback.
                 * Use PREV/NEXT for track changes. No action here. */
                break;

            case GAD_LOOP:
                g_loop = !g_loop;
                break;

            case GAD_PLS:
                if (g_plswin) {
                    CloseWindow(g_plswin); g_plswin = NULL;
                    if (g_plsglist) { FreeGadgets(g_plsglist); g_plsglist = NULL; }
                } else {
                    open_playlist_window();
                }
                break;

            case GAD_SF2: {
                struct FileRequester *fr;
                struct TagItem fr_tags[] = {
                    {ASLFR_TitleText,     (ULONG)"Select SoundFont SF2"},
                    {ASLFR_DoMultiSelect, FALSE},
                    {TAG_END,             0}
                };
                /* Stop playback before changing SF2 (synchronous). */
                player_stop_for_reload();

                fr = AllocAslRequest(ASL_FileRequest, fr_tags);
                if (fr && AslRequest(fr, NULL)) {
                    char full[PATH_BUF_SIZE];
                    int dl = (int)strlen(fr->fr_Drawer);
                    if (dl > PATH_BUF_SIZE - 2) dl = PATH_BUF_SIZE - 2;
                    memcpy(full, fr->fr_Drawer, (size_t)dl);
                    if (dl > 0 && full[dl-1] != '/' && full[dl-1] != ':')
                        full[dl++] = '/';
                    strncpy(full + dl, fr->fr_File,
                            (size_t)(PATH_BUF_SIZE - 1 - dl));
                    full[PATH_BUF_SIZE - 1] = '\0';
                    begin_sf2_load(full);
                }
                if (fr) FreeAslRequest(fr);
                draw_icons(g_win);
                break;
            }

            case GAD_OPEN: {
                struct FileRequester *fr;
                struct TagItem fr_tags[] = {
                    {ASLFR_TitleText,     (ULONG)"Select MIDI files"},
                    {ASLFR_DoMultiSelect, TRUE},
                    {TAG_END,             0}
                };
                fr = AllocAslRequest(ASL_FileRequest, fr_tags);
                if (fr && AslRequest(fr, NULL)) {
                    int fi, added = 0;
                    if (fr->fr_NumArgs > 0 && fr->fr_ArgList != NULL) {
                        for (fi = 0; fi < (int)fr->fr_NumArgs && g_pcount < MAX_PLAYLIST; fi++) {
                            char *full = (char *)AllocMem(PATH_BUF_SIZE, MEMF_ANY | MEMF_CLEAR);
                            if (full) {
                                int dl = (int)strlen(fr->fr_Drawer);
                                if (dl > PATH_BUF_SIZE - 2) dl = PATH_BUF_SIZE - 2;
                                memcpy(full, fr->fr_Drawer, (size_t)dl);
                                if (dl > 0 && full[dl-1] != '/' && full[dl-1] != ':')
                                    full[dl++] = '/';
                                strncpy(full + dl, fr->fr_ArgList[fi].wa_Name,
                                        (size_t)(PATH_BUF_SIZE - 1 - dl));
                                full[PATH_BUF_SIZE - 1] = '\0';
                                g_playlist[g_pcount++] = full;
                                added++;
                            }
                        }
                    }
                    if (added == 0 && fr->fr_File && fr->fr_File[0] && g_pcount < MAX_PLAYLIST) {
                        char *full = (char *)AllocMem(PATH_BUF_SIZE, MEMF_ANY | MEMF_CLEAR);
                        if (full) {
                            int dl = (int)strlen(fr->fr_Drawer);
                            if (dl > PATH_BUF_SIZE - 2) dl = PATH_BUF_SIZE - 2;
                            memcpy(full, fr->fr_Drawer, (size_t)dl);
                            if (dl > 0 && full[dl-1] != '/' && full[dl-1] != ':')
                                full[dl++] = '/';
                            strncpy(full + dl, fr->fr_File,
                                    (size_t)(PATH_BUF_SIZE - 1 - dl));
                            full[PATH_BUF_SIZE - 1] = '\0';
                            g_playlist[g_pcount++] = full;
                            added++;
                        }
                    }
                    if (added > 0 && g_gstate == GSTATE_IDLE && g_sf2_loaded) {
                        begin_midi_load(g_pcount - added);
                    }
                }
                if (fr) FreeAslRequest(fr);
                break;
            }

            case GAD_PROGRESS:
                break; /* no seek for MIDI */
            }

            if (gad->GadgetID >= GAD_PREV && gad->GadgetID <= GAD_SF2)
                draw_icons(g_win);
        }

        /* Keyboard shortcuts */
        if (class == IDCMP_RAWKEY && !(code & 0x80)) {
            switch (code) {
            case 0x40: /* Space */
                if (g_gstate == GSTATE_PLAYING) {
                    ahi_stop_all(); g_gstate = GSTATE_PAUSED; display_force_refresh();
                } else if (g_gstate == GSTATE_PAUSED) {
                    UWORD *buf = (g_acur == 0) ? g_buf0 : g_buf1;
                    g_gstate = GSTATE_PLAYING;
                    send_ahi_chunk(g_acur, buf, AHI_BUF_BYTES);
                } else if ((g_gstate == GSTATE_IDLE)
                           && g_pcount > 0 && g_sf2_loaded) {
                    int t;
                    if (g_pls_sel >= 0 && g_pls_sel < g_pcount)
                        t = g_pls_sel;
                    else if (g_cur >= 0 && g_cur < g_pcount)
                        t = g_cur;
                    else
                        t = 0;
                    begin_midi_load(t);
                }
                break;
            case 0x45: /* Escape: stop */
                player_send_stop(); player_ahi_stop();
                g_gstate = GSTATE_IDLE;
                strncpy(g_dispname, "Stopped", sizeof(g_dispname) - 1);
                display_force_refresh();
                break;
            case 0x4F: /* Left arrow */
                if (g_cur > 0) { player_stop_for_reload(); begin_midi_load(g_cur - 1); }
                break;
            case 0x4E: /* Right arrow */
                if (g_cur < g_pcount - 1) { player_stop_for_reload(); begin_midi_load(g_cur + 1); }
                break;
            }
        }
    }
}

/* ================================================================
 * build_gui
 * ================================================================ */
static BOOL build_gui(void)
{
    struct NewGadget ng;
    struct Gadget   *prev;
    UWORD bx, by, iw, fh, btn_w, btn_h;
    int   i, win_h;
    struct TagItem win_tags[16];

    memset(g_gad, 0, sizeof(g_gad));

    g_scr = LockPubScreen(NULL);
    if (!g_scr) return FALSE;

    g_vi = GetVisualInfoA(g_scr, NULL);
    if (!g_vi) { UnlockPubScreen(NULL, g_scr); g_scr = NULL; return FALSE; }

    fh  = g_scr->Font->ta_YSize;
    bx  = g_scr->WBorLeft;
    by  = g_scr->WBorTop + fh + 1;
    iw  = WIN_WIDTH - bx - g_scr->WBorRight;

    g_th    = fh + 2;
    g_tx    = bx + 2;
    g_ty[0] = by + 2;
    g_ty[1] = g_ty[0] + g_th;
    g_ty[2] = g_ty[1] + g_th;

    {
        UWORD prog_y = g_ty[2] + g_th + 2;
        UWORD btn_y  = prog_y + 14;

        btn_w = (iw - 10 * 2) / 11;
        btn_h = 12;
        win_h = (int)btn_y + (int)btn_h + (int)g_scr->WBorBottom + 4;

        prev = CreateContext(&g_glist);
        if (!prev) goto fail;

        /* Progress slider */
        ng.ng_LeftEdge   = bx + 2;
        ng.ng_TopEdge    = prog_y;
        ng.ng_Width      = iw - 4;
        ng.ng_Height     = 12;
        ng.ng_GadgetText = NULL;
        ng.ng_TextAttr   = g_scr->Font;
        ng.ng_GadgetID   = GAD_PROGRESS;
        ng.ng_Flags      = 0;
        ng.ng_VisualInfo = g_vi;
        {
            struct TagItem sl_tags[] = {
                {GTSL_Min,         0},
                {GTSL_Max,         PROGRESS_MAX},
                {GTSL_Level,       0},
                {GTSL_MaxLevelLen, 5},
                {GA_RelVerify,     TRUE},
                {TAG_END,          0}
            };
            prev = g_gad[GAD_PROGRESS] = CreateGadgetA(SLIDER_KIND, prev, &ng, sl_tags);
        }
        if (!prev) goto fail;

        /* 11 transport buttons (IDs 1-11) */
        for (i = 1; i <= 11; i++) {
            ng.ng_LeftEdge   = bx + 2 + (UWORD)((i - 1) * (btn_w + 2));
            ng.ng_TopEdge    = btn_y;
            ng.ng_Width      = btn_w;
            ng.ng_Height     = btn_h;
            ng.ng_GadgetText = NULL;
            ng.ng_TextAttr   = g_scr->Font;
            ng.ng_GadgetID   = (UWORD)i;
            ng.ng_Flags      = 0;
            ng.ng_VisualInfo = g_vi;
            prev = g_gad[i] = CreateGadgetA(BUTTON_KIND, prev, &ng, NULL);
            if (!prev) goto fail;
        }

        win_tags[0].ti_Tag  = WA_Left;      win_tags[0].ti_Data = 40;
        win_tags[1].ti_Tag  = WA_Top;       win_tags[1].ti_Data = 40;
        win_tags[2].ti_Tag  = WA_Width;     win_tags[2].ti_Data = WIN_WIDTH;
        win_tags[3].ti_Tag  = WA_Height;    win_tags[3].ti_Data = (ULONG)win_h;
        win_tags[4].ti_Tag  = WA_Title;     win_tags[4].ti_Data = (ULONG)"ZZMIDIPlay - Xanxi 2026";
        win_tags[5].ti_Tag  = WA_Gadgets;   win_tags[5].ti_Data = (ULONG)g_glist;
        win_tags[6].ti_Tag  = WA_Flags;     win_tags[6].ti_Data =
                               WFLG_DRAGBAR | WFLG_CLOSEGADGET | WFLG_DEPTHGADGET |
                               WFLG_ACTIVATE | WFLG_SMART_REFRESH;
        win_tags[7].ti_Tag  = WA_IDCMP;     win_tags[7].ti_Data =
                               IDCMP_CLOSEWINDOW | IDCMP_REFRESHWINDOW |
                               IDCMP_GADGETUP | IDCMP_RAWKEY;
        win_tags[8].ti_Tag  = WA_PubScreen; win_tags[8].ti_Data = (ULONG)g_scr;
        win_tags[9].ti_Tag  = TAG_END;      win_tags[9].ti_Data = 0;

        g_win = OpenWindowTagList(NULL, win_tags);
        if (!g_win) goto fail;

        GT_RefreshWindow(g_win, NULL);
        draw_icons(g_win);
        g_iw = (WORD)(g_win->Width - g_win->BorderLeft - g_win->BorderRight);
    }

    UnlockPubScreen(NULL, g_scr);
    return TRUE;

fail:
    if (g_vi)    { FreeVisualInfo(g_vi); g_vi = NULL; }
    if (g_glist) { FreeGadgets(g_glist); g_glist = NULL; }
    UnlockPubScreen(NULL, g_scr);
    g_scr = NULL;
    return FALSE;
}

/* ================================================================
 * AHI init/cleanup
 * ================================================================ */
static BOOL init_ahi(void)
{
    int i;
    g_ahiport = CreateMsgPort();
    if (!g_ahiport) return FALSE;
    for (i = 0; i < 2; i++) {
        g_req[i] = (struct AHIRequest *)
                   CreateIORequest(g_ahiport, sizeof(struct AHIRequest));
        if (!g_req[i]) return FALSE;
        g_act[i] = FALSE;
    }
    g_req[0]->ahir_Version = 4;
    if (OpenDevice(AHINAME, AHI_DEFAULT_UNIT,
                   (struct IORequest *)g_req[0], 0) != 0)
        return FALSE;
    CopyMem(g_req[0], g_req[1], sizeof(struct AHIRequest));
    return TRUE;
}

static void cleanup_ahi(void)
{
    int i;
    ahi_stop_all();
    for (i = 0; i < 2; i++) {
        if (g_req[i]) {
            if (i == 0) CloseDevice((struct IORequest *)g_req[i]);
            DeleteIORequest((struct IORequest *)g_req[i]);
            g_req[i] = NULL;
        }
    }
    if (g_ahiport) { DeleteMsgPort(g_ahiport); g_ahiport = NULL; }
}

/* ================================================================
 * XMID reset
 * ================================================================ */
static void xmid_reset(void)
{
    CTRL_WR32(g_ctrl, XMID_OFF_MAGIC,   XMID_MAGIC);
    CTRL_WR32(g_ctrl, XMID_OFF_VERSION, XMID_VERSION);
    trigger_opcode(XMID_CMD_RESET);
}

/* ================================================================
 * main
 * ================================================================ */
int main(int argc, char *argv[])
{
    int rc = 0, i;

    SetTaskPri(FindTask(NULL), 20);

    IntuitionBase = (struct IntuitionBase *)OpenLibrary("intuition.library", 37);
    GfxBase       = (struct GfxBase *)      OpenLibrary("graphics.library",  37);
    GadToolsBase  =                         OpenLibrary("gadtools.library",  37);
    AslBase       =                         OpenLibrary("asl.library",       37);

    if (!IntuitionBase || !GfxBase || !GadToolsBase || !AslBase) {
        PutStr("ZZMIDIPlay: missing libraries (OS 3.1 required)\n");
        rc = 20; goto bye_libs;
    }

    /* Find ZZ9000 */
    {
        struct ConfigDev *cd;
        ExpansionBase = (struct ExpansionBase *)OpenLibrary("expansion.library", 37);
        if (!ExpansionBase) {
            PutStr("ZZMIDIPlay: expansion.library not found\n");
            rc = 20; goto bye_libs;
        }
        cd = FindConfigDev(NULL, ZZ9000_MANUF, ZZ9000_PROD_AX);
        if (!cd) cd = FindConfigDev(NULL, ZZ9000_MANUF, ZZ9000_PROD);
        CloseLibrary((struct Library *)ExpansionBase);
        if (!cd) {
            PutStr("ZZMIDIPlay: ZZ9000 not found\n");
            rc = 10; goto bye_libs;
        }
        g_board = (UBYTE *)cd->cd_BoardAddr;
        g_fb    = g_board + MNT_FB_BASE;
        g_ctrl  = g_fb + XMID_CTRL_OFFSET;
    }

    if (!init_ahi()) {
        PutStr("ZZMIDIPlay: ahi.device not found\n");
        rc = 10; goto bye_libs;
    }

    xmid_reset();

    init_btn_images();

    if (!build_gui()) {
        PutStr("ZZMIDIPlay: window open failed\n");
        rc = 10; goto bye_ahi;
    }

    g_copy_next_track = -1;

    /* Parse argv */
    for (i = 1; i < argc; i++) {
        int len = (int)strlen(argv[i]);
        if (len > 4) {
            const char *ext = argv[i] + len - 4;
            if ((ext[1] == 's' || ext[1] == 'S') &&
                (ext[2] == 'f' || ext[2] == 'F') &&
                ext[3] == '2') {
                begin_sf2_load(argv[i]);
                continue;
            }
        }
        if (g_pcount < MAX_PLAYLIST) {
            char *s = (char *)AllocMem(PATH_BUF_SIZE, MEMF_ANY | MEMF_CLEAR);
            if (s) {
                strncpy(s, argv[i], PATH_BUF_SIZE - 1);
                g_playlist[g_pcount++] = s;
            }
        }
    }

    strncpy(g_dispname, "ZZMIDIPlay XX19", sizeof(g_dispname) - 1);
    display_force_refresh();

    /* ============================================================
     * MAIN LOOP
     * ============================================================ */
    while (!g_quit) {

        /* State machine step */
        main_loop_tick();

        /* Process window events (always, every iteration) */
        handle_idcmp();
        handle_pls_idcmp();

        /* When idle/paused/error: Wait for window signal to avoid
         * burning 100% CPU. When copying or loading: no Wait so
         * the state machine runs continuously. */
        if (g_gstate == GSTATE_IDLE     ||
            g_gstate == GSTATE_PAUSED   ||
            g_gstate == GSTATE_ERROR) {
            ULONG sigs = 1L << g_win->UserPort->mp_SigBit;
            if (g_plswin) sigs |= 1L << g_plswin->UserPort->mp_SigBit;
            Wait(sigs);
        }
        /* PLAYING: no Wait -- player_tick() drives the loop via WaitIO */
        /* COPYING / ARM_LOADING: no Wait -- chunked loop must run freely */
    }

    /* ============================================================
     * Cleanup
     * ============================================================ */
    player_send_stop();
    player_ahi_stop();
    xmid_reset();

    /* Free any in-progress copy buffer */
    if (g_copy_src) { FreeMem(g_copy_src, g_copy_size); g_copy_src = NULL; }
    if (g_sf2_buf)  { FreeMem(g_sf2_buf, g_sf2_total);  g_sf2_buf  = NULL; }

    for (i = 0; i < g_pcount; i++)
        if (g_playlist[i]) {
            FreeMem(g_playlist[i], PATH_BUF_SIZE);
            g_playlist[i] = NULL;
        }

    if (g_plswin)   { CloseWindow(g_plswin); g_plswin = NULL; }
    if (g_plsglist) { FreeGadgets(g_plsglist); g_plsglist = NULL; }
    if (g_win)      { CloseWindow(g_win); g_win = NULL; }
    if (g_glist)    { FreeGadgets(g_glist); g_glist = NULL; }
    if (g_vi)       { FreeVisualInfo(g_vi); g_vi = NULL; }

bye_ahi:
    cleanup_ahi();

bye_libs:
    if (AslBase)       { CloseLibrary(AslBase);                           AslBase       = NULL; }
    if (GadToolsBase)  { CloseLibrary(GadToolsBase);                      GadToolsBase  = NULL; }
    if (GfxBase)       { CloseLibrary((struct Library *)GfxBase);         GfxBase       = NULL; }
    if (IntuitionBase) { CloseLibrary((struct Library *)IntuitionBase);   IntuitionBase = NULL; }

    return rc;
}