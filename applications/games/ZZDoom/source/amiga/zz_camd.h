/*
 * zz_camd.h - CAMD MIDI backend for ZZDoom.
 * Send-only, non-blocking. No MUS parsing here.
 * ASCII only.
 */
#ifndef ZZ_CAMD_H
#define ZZ_CAMD_H

#include <exec/types.h>

/* Poll - call each frame, non-blocking */
void zz_camd_poll(ULONG now_ms);
void zz_camd_poll_test_start(void); /* arm test arpeggio */

/* Init/shutdown */
int  zz_camd_init(const char *portname); /* NULL = "out.0" */
void zz_camd_shutdown(void);
int  zz_camd_ready(void);               /* 1 if init succeeded */

/* Send raw MIDI */
void zz_camd_put3(UBYTE status, UBYTE data1, UBYTE data2);
void zz_camd_put2(UBYTE status, UBYTE data1);

/* Helpers */
void zz_camd_note_on(UBYTE ch, UBYTE note, UBYTE vel);
void zz_camd_note_off(UBYTE ch, UBYTE note);
void zz_camd_program(UBYTE ch, UBYTE prog);
void zz_camd_control(UBYTE ch, UBYTE ctrl, UBYTE val);
void zz_camd_pitch_bend(UBYTE ch, UWORD bend); /* 0..16383, center=8192 */
void zz_camd_all_notes_off(void);              /* all channels */

#endif /* ZZ_CAMD_H */
