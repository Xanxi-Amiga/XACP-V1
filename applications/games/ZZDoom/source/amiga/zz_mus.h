/*
 * zz_mus.h - MUS lump parser -> CAMD MIDI output, non-blocking.
 * ASCII only.
 */
#ifndef ZZ_MUS_H
#define ZZ_MUS_H

#include <exec/types.h>

/* Init MUS player with a MUS lump in memory.
   loop=1 to repeat. Returns 1 on success. */
int  zz_mus_start(const UBYTE *mus_data, ULONG mus_size, int loop);
void zz_mus_stop(void);
int  zz_mus_playing(void);

/* Call each frame from main loop - non-blocking */
void zz_mus_poll(ULONG now_ms);

/* Find a named lump in a WAD loaded at wad_base (CPU address).
   Returns pointer to lump data, sets *out_size. NULL if not found. */
const UBYTE *zz_wad_find_lump(const UBYTE *wad_base, ULONG wad_size,
                               const char *name, ULONG *out_size);

/* Point this at SH_MUSIC_DBG_PUTMIDI in shared memory for A3 diagnostic */
extern volatile ULONG *g_mus_putmidi_shared;
/* Point this at SH_MUSIC_DBG_* slots for WAD diagnostic */
extern volatile ULONG *g_wad_dbg;
/* Copy lump from ZZ9000 DDR to Fast RAM via bus-safe reads */
void zz_bus_copy_lump(const UBYTE *wad_base, ULONG fpos, UBYTE *dst, ULONG len);

#endif /* ZZ_MUS_H */
