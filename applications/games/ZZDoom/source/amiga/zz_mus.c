/*
 * zz_mus.c - MUS lump parser -> CAMD MIDI, non-blocking poll.
 * Based on musplay.c by cnvogelg/amiditools (DoomSnd).
 * Key differences: no realtime.library, no task, poll-based timing.
 * ASCII only.
 */
#include "zz_mus.h"
#include "zz_camd.h"
#include <exec/types.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <string.h>
#include <stdio.h>

/* MUS event types (from musplay.c - use &0x70 mask) */
#define MUS_EV_RELEASE    0x00
#define MUS_EV_PRESSKEY   0x10
#define MUS_EV_PITCHWHEEL 0x20
#define MUS_EV_SYSTEMEVENT 0x30
#define MUS_EV_CHANGECTRL 0x40
#define MUS_EV_MEASUREEND 0x50
#define MUS_EV_SCOREEND   0x60
#define MUS_EV_UNUSED     0x70

#define MUS_PERCUSSION_CHAN  15
#define MIDI_PERCUSSION_CHAN  9
#define NUM_CHANNELS         16

/* Controller map MUS->MIDI (from musplay.c g_controllerMap) */
/* Index = MUS controller number */
static const UBYTE g_ctrl_map[] = {
    0xC0, /* 0: program change (handled separately) */
    0x00, /* 1: bank select */
    0x01, /* 2: mod wheel */
    0x07, /* 3: volume */
    0x0A, /* 4: pan */
    0x0B, /* 5: expression */
    0x5B, /* 6: reverb depth */
    0x5D, /* 7: chorus depth */
    0x40, /* 8: sustain pedal */
    0x43, /* 9: soft pedal */
    /* 10..14: valueless system events */
    0x78, /* 10: all sounds off */
    0x7B, /* 11: all notes off */
    0x7E, /* 12: mono */
    0x7F, /* 13: poly */
    0x79, /* 14: reset all controllers */
};

/* Channel allocation state */
static int         g_channel_map[NUM_CHANNELS]; /* MUS->MIDI, -1=unallocated */
static UBYTE       g_active_notes[NUM_CHANNELS][128]; /* active note tracking */
static UBYTE       g_velocities[NUM_CHANNELS];  /* last velocity per MUS channel */

/* Read 16-bit little-endian safely (no alignment assumption) */
static UWORD rdle16(const UBYTE *p)
{
    return (UWORD)((UWORD)p[0] | ((UWORD)p[1]<<8));
}

static ULONG        g_error        = 0;
#define MUS_ERR_STUCK 1

/* Allocate next free MIDI channel using used[] table - never wraps blindly */
static UBYTE alloc_midi_channel(void)
{
    int used[NUM_CHANNELS];
    int i, ch;
    for(i=0;i<NUM_CHANNELS;i++) used[i]=0;
    used[MIDI_PERCUSSION_CHAN] = 1; /* reserve ch9 */
    for(i=0;i<NUM_CHANNELS;i++)
        if(g_channel_map[i] >= 0 && g_channel_map[i] < NUM_CHANNELS)
            used[g_channel_map[i]] = 1;
    for(ch=0;ch<NUM_CHANNELS;ch++)
        if(!used[ch]) return (UBYTE)ch;
    /* All melodic channels exhausted - reuse ch0, log error */
    g_error = MUS_ERR_STUCK;
    return 0;
}

/* Get MIDI channel for a MUS channel */
static UBYTE get_midi_channel(UBYTE mus_ch)
{
    if(mus_ch == MUS_PERCUSSION_CHAN) return MIDI_PERCUSSION_CHAN;
    if(g_channel_map[mus_ch] == -1){
        g_channel_map[mus_ch] = alloc_midi_channel();
        /* all notes off on newly allocated channel */
        zz_camd_control(g_channel_map[mus_ch], 0x7B, 0);
    }
    return (UBYTE)g_channel_map[mus_ch];
}

/* --- Player state --- */
static const UBYTE *g_score     = NULL; /* pointer to score start */
static ULONG        g_score_end = 0;    /* score size in bytes */
static ULONG        g_pos       = 0;    /* current byte pos in score */
static int          g_loop      = 0;
static int          g_playing   = 0;
static ULONG        g_t0_ms     = 0xFFFFFFFFUL;
static ULONG        g_tick_ms   = 0;    /* accumulated ms */
static ULONG        g_next_tick = 0;    /* next event tick */
static ULONG        g_putmidi   = 0;    /* debug counter */
static ULONG        g_event_count  = 0;
static ULONG        g_start_call   = 0;
static ULONG        g_stop_call    = 0;
char                g_lump_name[9] = "";

static void mus_log(const char *msg)
{
    BPTR _f=Open("RAM:MUSDBG.txt",MODE_READWRITE);
    if(_f){ Seek(_f,0,OFFSET_END); FPuts(_f,msg); Close(_f); }
}
/* Pointer to shared memory PUTMIDI counter (set by launcher) */
volatile ULONG *g_mus_putmidi_shared = (volatile ULONG*)0;
static ULONG        g_events    = 0;    /* debug counter */
static ULONG        g_catchup_hits = 0;

static UBYTE read_byte(void)
{
    if(g_pos >= g_score_end){ g_playing=0; return 0; }
    return g_score[g_pos++];
}


static void zz_mus_all_notes_hard_off(void); /* forward decl */

int zz_mus_start(const UBYTE *mus_data, ULONG mus_size, int loop)
{
    UWORD score_len, score_start, prim, sec, ninstr;
    ULONG id;
    int i;
    { BPTR _f=Open("RAM:MUSDBG.txt",MODE_READWRITE);
      if(_f){ Seek(_f,0,OFFSET_END);
        FPrintf(_f,"START_ENTER#%lu name=%.8s data=%08lx size=%lu gp=%d\n",
                (ULONG)(g_start_call+1), (ULONG)g_lump_name, (ULONG)mus_data, mus_size, (ULONG)g_playing);
        Close(_f);
      }
    }

    if(!mus_data || mus_size < 16){
        printf("MUS: too small\n");
        return 0;
    }

    printf("MUS: bytes= %02x %02x %02x %02x %02x %02x %02x %02x"
             " %02x %02x %02x %02x %02x %02x %02x %02x\n",
             mus_data[0],mus_data[1],mus_data[2],mus_data[3],
             mus_data[4],mus_data[5],mus_data[6],mus_data[7],
             mus_data[8],mus_data[9],mus_data[10],mus_data[11],
             mus_data[12],mus_data[13],mus_data[14],mus_data[15]);

    /* Check magic "MUS\x1A" */
    id = ((ULONG)mus_data[0]<<24)|((ULONG)mus_data[1]<<16)|
         ((ULONG)mus_data[2]<<8)|(ULONG)mus_data[3];
    printf("MUS: id=%08lX %s\n", id,
             (mus_data[0]=='M'&&mus_data[1]=='U'&&mus_data[2]=='S'&&mus_data[3]==0x1A)?"OK":"FAIL");

    if(mus_data[0]!='M'||mus_data[1]!='U'||mus_data[2]!='S'||mus_data[3]!=0x1A){
        return 0;
    }

    /* Header fields are little-endian 16-bit -> swap for Amiga */
    score_len   = rdle16(mus_data+4);
    score_start = rdle16(mus_data+6);
    prim        = rdle16(mus_data+8);
    sec         = rdle16(mus_data+10);
    ninstr      = rdle16(mus_data+12);

    printf("MUS: score_len=%u score_start=%u prim=%u sec=%u instr=%u\n",
             score_len, score_start, prim, sec, ninstr);

    if(score_start >= mus_size){
        printf("MUS: score_start %u >= size %lu - FAIL\n", score_start, mus_size);
        return 0;
    }

    /* Log first 10 bytes of score */
    { UWORD j;
      printf("MUS: score[0..9]= ");
      for(j=0;j<10&&score_start+j<mus_size;j++)
          printf("%02x ", mus_data[score_start+j]);
      printf("\n");
    }

    /* Init state */
    g_score     = mus_data + score_start;
    g_score_end = (score_start + score_len <= mus_size) ?
                  score_len : (mus_size - score_start);
    g_pos       = 0;
    g_loop      = loop;
    g_playing   = 1;
    g_t0_ms     = 0xFFFFFFFFUL;
    g_tick_ms   = 0;
    g_next_tick = 0;
    g_putmidi   = 0;
    g_events    = 0;

    /* Hard stop any previously playing notes */
    zz_mus_all_notes_hard_off();
    for(i=0;i<NUM_CHANNELS;i++){
        g_channel_map[i] = -1;
        g_velocities[i]  = 100;
    }
    for(i=0;i<NUM_CHANNELS;i++) { int n; for(n=0;n<128;n++) g_active_notes[i][n]=0; }

    g_event_count = 0;
    g_start_call++;
    { char _buf[160];
      BPTR _f=Open("RAM:MUSDBG.txt",MODE_READWRITE);
      if(_f){ Seek(_f,0,OFFSET_END);
        FPrintf(_f,"START#%lu name=%.8s gp_before=%ld->1 sclen=%lu scstart=%lu lsz=%lu loop=%d\n",
                (ULONG)g_start_call, (ULONG)g_lump_name, (LONG)g_playing,
                (ULONG)score_len,(ULONG)score_start,(ULONG)mus_size,(ULONG)loop);
        Close(_f);
      }
    }
    return 1;
}

static void zz_mus_all_notes_hard_off(void)
{
    int ch, n;
    for(ch=0;ch<NUM_CHANNELS;ch++){
        for(n=0;n<128;n++){
            if(g_active_notes[ch][n]){
                zz_camd_note_off((UBYTE)ch,(UBYTE)n);
                g_active_notes[ch][n]=0;
            }
        }
        zz_camd_control((UBYTE)ch, 64,  0); /* sustain off */
        zz_camd_control((UBYTE)ch, 123, 0); /* all notes off */
        zz_camd_control((UBYTE)ch, 120, 0); /* all sounds off */
        zz_camd_control((UBYTE)ch, 121, 0); /* reset controllers */
    }
}

void zz_mus_stop(void)
{
    g_stop_call++;
    { BPTR _f=Open("RAM:MUSDBG.txt",MODE_READWRITE);
      if(_f){ Seek(_f,0,OFFSET_END);
        FPrintf(_f,"STOP#%lu gp_before=%ld putmidi=%lu events=%lu\n",
                (ULONG)g_stop_call, (LONG)g_playing, (ULONG)g_putmidi, (ULONG)g_events);
        Close(_f);
      }
    }
    zz_mus_all_notes_hard_off();
    g_playing = 0;
}

int zz_mus_playing(void){ return g_playing; }

/* PlayNextEvent: translated from musplay.c PlayNextEvent.
 * Processes events until a delay is found, then returns.
 * On delay: advances g_next_tick and returns.
 * On scoreend: loops or stops, returns.
 * Never loops unbounded - exits on first delay or scoreend. */
static void play_next_event(void)
{
    UBYTE loop_song = 0;

    while(g_playing && g_pos < g_score_end){
        UBYTE ev, mus_ch, midi_ch, key, vel, ctrl, val, bend;
        UBYTE event_type;

        ev         = read_byte();
        event_type = ev & 0x70;
        mus_ch     = ev & 0x0F;
        midi_ch    = get_midi_channel(mus_ch);
        g_events++;
        loop_song  = 0;

        switch(event_type){

        case MUS_EV_RELEASE:
            key = read_byte() & 0x7F;
            zz_camd_note_off(midi_ch, key);
            if(midi_ch<NUM_CHANNELS) g_active_notes[midi_ch][key]=0;
            g_putmidi++;
            break;

        case MUS_EV_PRESSKEY:
            key = read_byte();
            if(key & 0x80){
                vel = read_byte() & 0x7F;
                g_velocities[mus_ch] = vel;
            }
            zz_camd_note_on(midi_ch, key & 0x7F, g_velocities[mus_ch]);
            if(midi_ch<NUM_CHANNELS) g_active_notes[midi_ch][key & 0x7F]=1;
            g_putmidi++;
            if(g_mus_putmidi_shared) *g_mus_putmidi_shared = g_putmidi;
            break;

        case MUS_EV_PITCHWHEEL:
            bend = read_byte();
            zz_camd_pitch_bend(midi_ch, (UWORD)((ULONG)bend * 64UL));
            g_putmidi++;
            break;

        case MUS_EV_SYSTEMEVENT:
            ctrl = read_byte();
            if(ctrl >= 10 && ctrl <= 14)
                zz_camd_control(midi_ch, g_ctrl_map[ctrl], 0);
            g_putmidi++;
            break;

        case MUS_EV_CHANGECTRL:
            ctrl = read_byte();
            val  = read_byte();
            if(ctrl == 0){
                zz_camd_program(midi_ch, val & 0x7F);
            } else if(ctrl >= 1 && ctrl <= 9){
                UBYTE v = (val & 0x80) ? 0x7F : val;
                zz_camd_control(midi_ch, g_ctrl_map[ctrl], v);
            }
            g_putmidi++;
            break;

        case MUS_EV_SCOREEND:
            if(!g_loop){
                zz_camd_all_notes_off();
                g_playing = 0;
                return;
            } else {
                loop_song = 1;
            }
            break;

        case MUS_EV_MEASUREEND:
            break;

        default:
            read_byte();
            break;
        }

        if(ev & 0x80){
            /* Delay follows: read variable-length quantity */
            ULONG td = 0;
            UBYTE b;
            do {
                b = read_byte();
                td = td * 128UL + (b & 0x7F);
            } while((b & 0x80) && g_pos < g_score_end);
            if(td == 0) td = 1; /* guard against zero-delay infinite loop */

            if(loop_song){
                /* rewind before returning */
                g_pos = 0;
                g_next_tick = 0;
                g_t0_ms = 0xFFFFFFFFUL;
                { int i; for(i=0;i<NUM_CHANNELS;i++) g_channel_map[i]=-1; }
            }

            g_next_tick += td;
            return; /* wait for next tick */
        } else {
            if(loop_song){
                /* scoreend with no delay: rewind and restart immediately */
                g_pos = 0;
                g_next_tick = 0;
                g_t0_ms = 0xFFFFFFFFUL;
                { int i; for(i=0;i<NUM_CHANNELS;i++) g_channel_map[i]=-1; }
                return;
            }
        }
    }
    /* Reached end of score data without scoreend event */
    g_playing = 0;
}

void zz_mus_poll(ULONG now_ms)
{
    ULONG elapsed, tick_now;
    int catchup_limit = 16;

    g_event_count++;
    if(!g_playing) return;
    if(!zz_camd_ready()) return;

    /* Init on first call */
    if(g_t0_ms == 0xFFFFFFFFUL){
        g_t0_ms  = now_ms;
        g_next_tick = 0;
    }

    elapsed  = now_ms - g_t0_ms;
    tick_now = (elapsed * 140UL) / 1000UL;

    /* Bounded catchup: process event groups due by tick_now */
    while(g_playing && (LONG)(tick_now - g_next_tick) >= 0 && catchup_limit-- > 0){
        ULONG prev_pos  = g_pos;
        ULONG prev_tick = g_next_tick;

        play_next_event();

        if(!g_playing) break;

        /* Stuck detector: neither pos nor tick advanced -> stop safely */
        if(g_pos == prev_pos && g_next_tick == prev_tick){
            g_playing = 0;
            g_error   = MUS_ERR_STUCK;
            zz_camd_all_notes_off();
            break;
        }
    }

    if(catchup_limit <= 0 && g_playing)
        g_catchup_hits++;
}


/* Safe bus read helpers for ZZ9000 DDR (UWORD-aligned access only).
 * The DDR on Zorro III may not support byte reads - access via UWORD.
 * 68k is big-endian: UWORD at address A = [A]=hi [A+1]=lo */
static UBYTE zz_bus_rb(const UBYTE *base, ULONG off)
{
    volatile UWORD *wp = (volatile UWORD *)(base + (off & ~1UL));
    UWORD v = *wp;
    return (off & 1) ? (UBYTE)(v & 0xFF) : (UBYTE)(v >> 8);
}

static ULONG zz_bus_rle32(const UBYTE *base, ULONG off)
{
    return  ((ULONG)zz_bus_rb(base, off+0))       |
            ((ULONG)zz_bus_rb(base, off+1) <<  8) |
            ((ULONG)zz_bus_rb(base, off+2) << 16) |
            ((ULONG)zz_bus_rb(base, off+3) << 24);
}

static void zz_bus_copy(const UBYTE *src_base, ULONG src_off, UBYTE *dst, ULONG len)
{
    ULONG i;
    for(i=0; i<len; i++) dst[i] = zz_bus_rb(src_base, src_off+i);
}

/* WAD lump finder.
 * Strategy: copy header (12 bytes) and full directory from DDR ZZ9000
 * into Fast RAM using UWORD-safe reads, then search in Fast RAM.
 * Also copies found lump to Fast RAM via UWORD-safe reads.
 * ASCII only. */
const UBYTE *zz_wad_find_lump(const UBYTE *wad_base, ULONG wad_size,
                               const char *name, ULONG *out_size)
{
    UBYTE  hdr[12];
    ULONG  num_lumps, dir_off, dir_size;
    UBYTE *dir_cache = NULL;
    char   padded[9];
    ULONG  i, fpos, fsz;
    const UBYTE *result = NULL;

    if(!wad_base || wad_size < 12) return NULL;

    /* Step 1: read 12-byte WAD header via UWORD-safe copies */
    zz_bus_copy(wad_base, 0, hdr, 12);

    /* Validate IWAD or PWAD */
    if(!((hdr[0]=='I'||hdr[0]=='P')&&hdr[1]=='W'&&hdr[2]=='A'&&hdr[3]=='D'))
        return NULL;

    /* Parse header fields (little-endian) */
    num_lumps = (ULONG)hdr[4]|((ULONG)hdr[5]<<8)|((ULONG)hdr[6]<<16)|((ULONG)hdr[7]<<24);
    dir_off   = (ULONG)hdr[8]|((ULONG)hdr[9]<<8)|((ULONG)hdr[10]<<16)|((ULONG)hdr[11]<<24);
    dir_size  = num_lumps * 16UL;

    if(num_lumps == 0 || dir_off + dir_size > wad_size) return NULL;

    /* Step 2: copy full directory to Fast RAM via UWORD-safe reads */
    dir_cache = (UBYTE*)AllocMem(dir_size, MEMF_PUBLIC);
    if(!dir_cache) return NULL;
    zz_bus_copy(wad_base, dir_off, dir_cache, dir_size);

    /* Step 3: search in Fast RAM directory - plain byte access, no bus concerns */
    memset(padded, 0, 9);
    strncpy(padded, name, 8);

    for(i=0; i<num_lumps; i++){
        const UBYTE *e = dir_cache + i*16;
        if(memcmp(e+8, padded, 8)==0){
            fpos = (ULONG)e[0]|((ULONG)e[1]<<8)|((ULONG)e[2]<<16)|((ULONG)e[3]<<24);
            fsz  = (ULONG)e[4]|((ULONG)e[5]<<8)|((ULONG)e[6]<<16)|((ULONG)e[7]<<24);
            if(out_size) *out_size = fsz;
            result = wad_base + fpos; /* pointer into DDR - used only for copy below */
            break;
        }
    }

    FreeMem(dir_cache, dir_size);
    return result;
}

/* Copy lump from ZZ9000 DDR to Fast RAM via UWORD-safe reads.
 * dst must already be AllocMem'd by caller. */
void zz_bus_copy_lump(const UBYTE *wad_base, ULONG fpos, UBYTE *dst, ULONG len)
{
    zz_bus_copy(wad_base, fpos, dst, len);
}
