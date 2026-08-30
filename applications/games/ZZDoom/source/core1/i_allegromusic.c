/*
 * i_allegromusic.c - ZZDoom CAMD music backend.
 * Replaces Allegro MIDI with CAMD via shared memory to 68k.
 * I_RegisterSong / I_PlaySong / I_StopSong / I_UnRegisterSong.
 * ASCII only.
 */
#include <stdio.h>
#include <string.h>

#include "doomtype.h"
#include "i_sound.h"

/* Shared memory: ARM 0x04300000 / 68k fb+0x04100000 */
#define ZZ_SHARED ((volatile unsigned int*)0x04300000UL)
#define ZZ_SH_MUSIC_CMD   41
#define ZZ_SH_MUSIC_SEQ   42
#define ZZ_SH_MUSIC_LOOP  43
#define ZZ_SH_MUSIC_NAME0 44
#define ZZ_SH_MUSIC_NAME1 45

#define ZZ_MUSIC_CMD_PLAY 1
#define ZZ_MUSIC_CMD_STOP 2

/* Current lump name, set by I_SetMusicLumpName before I_RegisterSong */
static char g_music_lump_name[9] = "";
static int  g_music_initialized  = 0;

/* Called from s_sound.c before I_RegisterSong */
void I_SetMusicLumpName(const char *name)
{
    int i;
    for(i=0;i<8&&name[i];i++) g_music_lump_name[i]=name[i];
    for(;i<9;i++) g_music_lump_name[i]=0;
}

static void zz_send_play(const char *name8, int looping)
{
    unsigned int n0, n1;
    int i;
    unsigned char nm[8];

    /* Pack name: uppercase, zero-pad */
    for(i=0;i<8;i++) nm[i]=0;
    for(i=0;i<8&&name8[i];i++){
        unsigned char c=(unsigned char)name8[i];
        if(c>=(unsigned char)97&&c<=(unsigned char)122) c-=32;
        nm[i]=c;
    }
    n0=((unsigned int)nm[0]<<24)|((unsigned int)nm[1]<<16)
      |((unsigned int)nm[2]<<8) |((unsigned int)nm[3]);
    n1=((unsigned int)nm[4]<<24)|((unsigned int)nm[5]<<16)
      |((unsigned int)nm[6]<<8) |((unsigned int)nm[7]);

    ZZ_SHARED[ZZ_SH_MUSIC_NAME0] = n0;
    ZZ_SHARED[ZZ_SH_MUSIC_NAME1] = n1;
    ZZ_SHARED[ZZ_SH_MUSIC_LOOP]  = (unsigned int)looping;
    ZZ_SHARED[ZZ_SH_MUSIC_CMD]   = ZZ_MUSIC_CMD_PLAY;
    __asm volatile("dmb sy" ::: "memory");
    ZZ_SHARED[ZZ_SH_MUSIC_SEQ]   = ZZ_SHARED[ZZ_SH_MUSIC_SEQ] + 1;
}

static void zz_send_stop(void)
{
    ZZ_SHARED[ZZ_SH_MUSIC_CMD] = ZZ_MUSIC_CMD_STOP;
    __asm volatile("dmb sy" ::: "memory");
    ZZ_SHARED[ZZ_SH_MUSIC_SEQ] = ZZ_SHARED[ZZ_SH_MUSIC_SEQ] + 1;
}

/* I_* music backend */

static void I_Allegro_InitMusic(void)
{
    g_music_initialized = 1;
}

static void I_Allegro_ShutdownMusic(void)
{
    if(g_music_initialized){
        zz_send_stop();
        g_music_initialized = 0;
    }
}

static void I_Allegro_SetMusicVolume(int volume)
{
    (void)volume;
}

static void I_Allegro_PauseSong(void)  { (void)0; }
static void I_Allegro_ResumeSong(void) { (void)0; }

/* RegisterSong: store handle = data pointer.
 * Lump name already stored via I_SetMusicLumpName. */
static void *I_Allegro_RegisterSong(void *data, int len)
{
    (void)len;
    return data; /* use data ptr as opaque handle */
}

static void I_Allegro_UnRegisterSong(void *handle)
{
    (void)handle;
    /* Nothing to free - WAD cache manages the buffer */
}

/* PlaySong: send PLAY command to 68k. */
static void I_Allegro_PlaySong(void *handle, boolean looping)
{
    (void)handle;
    if(!g_music_initialized) return;
    if(!g_music_lump_name[0]) return;
    zz_send_play(g_music_lump_name, looping ? 1 : 0);
}

/* StopSong: called during normal transitions.
 * Send STOP - 68k uses deferred stop (300ms grace) so a following
 * PLAY will cancel it before music actually cuts. */
static void I_Allegro_StopSong(void)
{
    if(!g_music_initialized) return;
    zz_send_stop();
}

static boolean I_Allegro_MusicIsPlaying(void)
{
    return g_music_initialized ? true : false;
}

static void I_Allegro_PollMusic(void) { (void)0; }

music_module_t DG_music_module =
{
    I_Allegro_InitMusic,
    I_Allegro_ShutdownMusic,
    I_Allegro_SetMusicVolume,
    I_Allegro_PauseSong,
    I_Allegro_ResumeSong,
    I_Allegro_RegisterSong,
    I_Allegro_UnRegisterSong,
    I_Allegro_PlaySong,
    I_Allegro_StopSong,
    I_Allegro_MusicIsPlaying,
    I_Allegro_PollMusic,
};
