/*
 * zz_camd.c - CAMD MIDI backend for ZZDoom.
 * LVO offsets verified from camd.library v37 binary FuncTable.
 * Tag constants verified from camd.library v2.0.
 * ASCII only.
 */
#include "zz_camd.h"
#include <exec/types.h>
#include <exec/nodes.h>
#include <exec/libraries.h>
#include <proto/exec.h>
#include <stdio.h>

/* --- CAMD tag constants (verified from camd.library v2.0 binary) --- */
#define CAMD_TAG_USER     0x80000000UL
#define CAMD_MIDI_BASE    (CAMD_TAG_USER + 65UL)
#define CAMD_MIDI_Name    (CAMD_MIDI_BASE + 0UL)  /* 0x80000041 */
#define CAMD_MIDI_MsgQ    (CAMD_MIDI_BASE + 6UL)  /* 0x80000047 */
#define CAMD_MIDI_SysEx   (CAMD_MIDI_BASE + 7UL)  /* 0x80000048 */
#define CAMD_MLINK_Loc    (CAMD_MIDI_BASE + 0UL)  /* 0x80000041 same base */
#define CAMD_MLTYPE_SEND  1L
#define CAMD_TAG_END      0UL

/* --- LVO offsets from camd.library v37 FuncTable (slot*6) --- */
#define LVO_CreateMidiA   (-42)
#define LVO_DeleteMidi    (-48)
#define LVO_AddMidiLinkA  (-84)
#define LVO_RemoveMidiLink (-90)
#define LVO_PutMidi       (-138)

/* --- Minimal structs --- */
struct CamdMidiNode { struct Node n; UWORD f; UWORD p; APTR u; APTR q; };
struct CamdMidiLink { struct Node n; struct CamdMidiNode *mn;
                      APTR u; APTR nm; ULONG fl; UBYTE cm[2]; UWORD p; };

/* --- State --- */
static struct Library     *g_camd_base = NULL;
static struct CamdMidiNode *g_midi_node = NULL;
static struct CamdMidiLink *g_midi_link = NULL;
static int                  g_camd_ok   = 0;

/* --- LVO wrappers --- */
static struct CamdMidiNode *lvo_CreateMidiA(ULONG *tags)
{
    register ULONG *a0 __asm("a0") = tags;
    register struct Library *a6 __asm("a6") = g_camd_base;
    register ULONG rv __asm("d0");
    __asm volatile ("jsr -42(a6)" : "=r"(rv) : "r"(a6),"r"(a0)
                    : "d1","a1","cc","memory");
    return (struct CamdMidiNode*)rv;
}

static void lvo_DeleteMidi(struct CamdMidiNode *mn)
{
    register struct CamdMidiNode *a0 __asm("a0") = mn;
    register struct Library      *a6 __asm("a6") = g_camd_base;
    __asm volatile ("jsr -48(a6)" : : "r"(a6),"r"(a0)
                    : "d0","d1","a0","a1","cc","memory");
}

static struct CamdMidiLink *lvo_AddMidiLinkA(struct CamdMidiNode *mn,
                                              LONG type, ULONG *tags)
{
    register struct CamdMidiNode *a0 __asm("a0") = mn;
    register LONG                 d0 __asm("d0") = type;
    register ULONG               *a1 __asm("a1") = tags;
    register struct Library      *a6 __asm("a6") = g_camd_base;
    register ULONG rv __asm("d0");
    __asm volatile ("jsr -84(a6)" : "=r"(rv)
                    : "r"(a6),"r"(a0),"0"(d0),"r"(a1)
                    : "d1","a1","cc","memory");
    return (struct CamdMidiLink*)rv;
}

static void lvo_RemoveMidiLink(struct CamdMidiLink *ml)
{
    register struct CamdMidiLink *a0 __asm("a0") = ml;
    register struct Library      *a6 __asm("a6") = g_camd_base;
    __asm volatile ("jsr -90(a6)" : : "r"(a6),"r"(a0)
                    : "d0","d1","a0","a1","cc","memory");
}

static void lvo_PutMidi(struct CamdMidiLink *ml, ULONG msg)
{
    register struct CamdMidiLink *a0 __asm("a0") = ml;
    register ULONG                d0 __asm("d0") = msg;
    register struct Library      *a6 __asm("a6") = g_camd_base;
    __asm volatile ("jsr -138(a6)" : : "r"(a6),"r"(a0),"r"(d0)
                    : "d1","a1","cc","memory");
}

/* --- Public API --- */

int zz_camd_init(const char *portname)
{
    ULONG create_tags[7];
    ULONG link_tags[3];
    const char *port = portname ? portname : "out.0";

    g_camd_ok = 0;
    g_camd_base = OpenLibrary("camd.library", 37L);
    if(!g_camd_base){
        printf("CAMD: camd.library not found\n");
        return 0;
    }
    printf("CAMD: opened v%ld\n", (LONG)g_camd_base->lib_Version);

    create_tags[0] = CAMD_MIDI_Name;  create_tags[1] = (ULONG)"ZZDoom";
    create_tags[2] = CAMD_MIDI_MsgQ;  create_tags[3] = 0UL;
    create_tags[4] = CAMD_MIDI_SysEx; create_tags[5] = 0UL;
    create_tags[6] = CAMD_TAG_END;
    g_midi_node = lvo_CreateMidiA(create_tags);
    if(!g_midi_node){
        printf("CAMD: CreateMidi failed\n");
        CloseLibrary(g_camd_base); g_camd_base=NULL;
        return 0;
    }

    link_tags[0] = CAMD_MLINK_Loc; link_tags[1] = (ULONG)port;
    link_tags[2] = CAMD_TAG_END;
    g_midi_link = lvo_AddMidiLinkA(g_midi_node, CAMD_MLTYPE_SEND, link_tags);
    if(!g_midi_link){
        printf("CAMD: AddMidiLink failed (port '%s')\n", port);
        lvo_DeleteMidi(g_midi_node); g_midi_node=NULL;
        CloseLibrary(g_camd_base); g_camd_base=NULL;
        return 0;
    }
    printf("CAMD: link %s OK\n", port);
    g_camd_ok = 1;
    return 1;
}

void zz_camd_shutdown(void)
{
    if(!g_camd_base) return;
    zz_camd_all_notes_off();
    if(g_midi_link){ lvo_RemoveMidiLink(g_midi_link); g_midi_link=NULL; }
    if(g_midi_node){ lvo_DeleteMidi(g_midi_node);     g_midi_node=NULL; }
    CloseLibrary(g_camd_base); g_camd_base=NULL;
    g_camd_ok = 0;
    printf("CAMD: closed\n");
}

int zz_camd_ready(void) { return g_camd_ok; }

void zz_camd_put3(UBYTE status, UBYTE data1, UBYTE data2)
{
    if(!g_camd_ok) return;
    lvo_PutMidi(g_midi_link,
        ((ULONG)status<<24)|((ULONG)data1<<16)|((ULONG)data2<<8));
}

void zz_camd_put2(UBYTE status, UBYTE data1)
{
    if(!g_camd_ok) return;
    lvo_PutMidi(g_midi_link,
        ((ULONG)status<<24)|((ULONG)data1<<16));
}

void zz_camd_note_on(UBYTE ch, UBYTE note, UBYTE vel)
{
    zz_camd_put3(0x90|(ch&0x0F), note&0x7F, vel&0x7F);
}

void zz_camd_note_off(UBYTE ch, UBYTE note)
{
    zz_camd_put3(0x80|(ch&0x0F), note&0x7F, 0);
}

void zz_camd_program(UBYTE ch, UBYTE prog)
{
    zz_camd_put2(0xC0|(ch&0x0F), prog&0x7F);
}

void zz_camd_control(UBYTE ch, UBYTE ctrl, UBYTE val)
{
    zz_camd_put3(0xB0|(ch&0x0F), ctrl&0x7F, val&0x7F);
}

void zz_camd_pitch_bend(UBYTE ch, UWORD bend)
{
    /* bend 0..16383, center=8192. LSB=bits0..6, MSB=bits7..13 */
    UBYTE lsb = (UBYTE)(bend & 0x7F);
    UBYTE msb = (UBYTE)((bend>>7) & 0x7F);
    zz_camd_put3(0xE0|(ch&0x0F), lsb, msb);
}

void zz_camd_all_notes_off(void)
{
    UBYTE ch;
    if(!g_camd_ok) return;
    for(ch=0; ch<16; ch++)
        zz_camd_put3(0xB0|ch, 123, 0); /* CC 123 = all notes off */
}

/* === Poll test sequence (non-blocking) ===
 * C major arpeggio: C4 E4 G4 C5, 250ms per note.
 * Used to validate CAMD in Doom loop before MUS integration. */
#define POLL_TEST_NOTES 4
static UBYTE g_poll_notes[POLL_TEST_NOTES] = {60, 64, 67, 72};
static ULONG g_poll_t0    = 0;   /* ms when current step started */
static int   g_poll_step  = -1;  /* -1=idle, 0..7=active */
static int   g_poll_active = 0;

void zz_camd_poll_test_start(void)
{
    if(!g_camd_ok) return;
    g_poll_step   = 0;
    g_poll_active = 1;
    g_poll_t0     = 0;
    printf("CAMD: poll test active (arpeggio every 5s)\n");
}

void zz_camd_poll(ULONG now_ms)
{
    ULONG cycle_ms, step_ms, phase_ms;
    int step;
    if(!g_camd_ok || !g_poll_active) return;

    if(g_poll_t0 == 0){ g_poll_t0 = now_ms; return; }

    ULONG elapsed = now_ms - g_poll_t0;

    /* Arpeggio takes 4*250=1000ms, then 4000ms silence = 5000ms cycle */
    cycle_ms = elapsed % 5000UL;

    if(cycle_ms >= 1000UL){
        /* Silent period - ensure notes off at boundary */
        if(g_poll_step >= 0 && g_poll_step < POLL_TEST_NOTES){
            zz_camd_note_off(0, g_poll_notes[g_poll_step]);
            g_poll_step = -1;
        }
        return;
    }

    /* Active arpeggio: 4 notes x 250ms */
    step_ms  = cycle_ms / 250UL;
    phase_ms = cycle_ms % 250UL;
    step = (int)step_ms;
    if(step >= POLL_TEST_NOTES) return;

    if(step != g_poll_step){
        if(g_poll_step >= 0 && g_poll_step < POLL_TEST_NOTES)
            zz_camd_note_off(0, g_poll_notes[g_poll_step]);
        g_poll_step = step;
        zz_camd_program(0, 0);
        zz_camd_note_on(0, g_poll_notes[step], 100);
    } else if(phase_ms >= 200 && g_poll_step >= 0 && g_poll_step < POLL_TEST_NOTES){
        zz_camd_note_off(0, g_poll_notes[g_poll_step]);
        g_poll_step = -1;
    }
}
