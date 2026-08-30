/*
 * i_zz9000sound.c - ZZ9000 sound module, ARM Core1 (8-channel direct).
 * ZZ_StartSound finds the lump in WAD and calls zz_sfx_start() directly.
 * No shared-memory trigger. Shared is for diagnostics only.
 * ASCII only.
 */
#include "doomtype.h"
#include "i_sound.h"
#include "w_wad.h"

/* Defined in audio_zz.c */
extern volatile unsigned int *au_shared;
extern int  zz_sfx_start(const unsigned char *data, unsigned int length,
                          unsigned int src_rate, int vol);
extern void zz_sfx_stop(int ch);
extern int  zz_sfx_is_playing(int ch);

#define SH_PCM_ENABLED 29

/* WAD helpers (ARM has WAD in RAM - find lump directly) */
static unsigned int rd_le32_i(const unsigned char *p){
    return (unsigned int)p[0]|((unsigned int)p[1]<<8)|
           ((unsigned int)p[2]<<16)|((unsigned int)p[3]<<24);
}
static unsigned short rd_le16_i(const unsigned char *p){
    return (unsigned short)((unsigned int)p[0]|((unsigned int)p[1]<<8));
}

static const unsigned char *find_sfx_lump(const char *fullname,
                                           unsigned int *out_len,
                                           unsigned int *out_rate)
{
    const unsigned char *wad=(const unsigned char*)au_shared[8]; /* SH_WAD_ADDR */
    unsigned int wad_size=au_shared[9]; /* SH_WAD_SIZE */
    unsigned int num, doff, i;
    const unsigned char *dir;
    if(!wad||wad_size<12) return 0;
    num=rd_le32_i(wad+4); doff=rd_le32_i(wad+8);
    if(doff+num*16>wad_size) return 0;
    dir=wad+doff;
    for(i=0;i<num;i++){
        const unsigned char *e=dir+i*16;
        int j,ok=1;
        for(j=0;j<8;j++){
            char ec=(char)e[8+j], nc=fullname[j];
            if(ec!=nc){ok=0;break;}
            if(nc==0){int k;for(k=j+1;k<8;k++)if(e[8+k]!=0){ok=0;break;}break;}
        }
        if(ok){
            unsigned int fp=rd_le32_i(e), sz=rd_le32_i(e+4);
            const unsigned char *lump=wad+fp;
            unsigned int rate, ns;
            if(sz<9) return 0;
            rate=rd_le16_i(lump+2);
            ns  =rd_le32_i(lump+4);
            if(ns+8>sz) ns=sz-8;
            if(ns==0) return 0;
            if(out_rate) *out_rate=rate;
            if(out_len)  *out_len =ns;
            return lump+8;   /* sample data */
        }
    }
    return 0;
}

static char zz_upper(char c){
    return (c>='a'&&c<='z')?c-32:c;
}

static boolean ZZ_SoundInit(boolean use_sfx_prefix)
{
    if(!au_shared) return false;
    au_shared[40]=0xB101UL;
    return (au_shared[SH_PCM_ENABLED]==1)?true:false;
}
static void ZZ_SoundShutdown(void){}

static int ZZ_GetSfxLumpNum(sfxinfo_t *sfxinfo)
{
    char n[9]; int i;
    n[0]='D';n[1]='S';
    for(i=0;i<6;i++) n[2+i]=zz_upper(sfxinfo->name[i]);
    n[8]=0;
    return W_CheckNumForName(n);
}

static void ZZ_SoundUpdate(void){}
static void ZZ_UpdateSoundParams(int channel, int vol, int sep){}

static int ZZ_StartSound(sfxinfo_t *sfxinfo, int channel, int vol, int sep)
{
    char name[9]; int i;
    const unsigned char *data; unsigned int len=0, rate=0;
    if(!au_shared) return -1;

    /* Build DS+name uppercase */
    name[0]='D'; name[1]='S';
    for(i=0;i<6;i++) name[2+i]=zz_upper(sfxinfo->name[i]);
    name[8]=0;

    /* Find lump directly in WAD (ARM has WAD in RAM) */
    data=find_sfx_lump(name,&len,&rate);
    if(!data){ au_shared[40]=0xB1EEUL; return -1; }

    /* Start on a mixer channel directly - no shared trigger */
    return zz_sfx_start(data, len, rate, (vol>127)?127:vol);
}

static void ZZ_StopSound(int channel)
{
    zz_sfx_stop(channel);
}

static boolean ZZ_SoundIsPlaying(int channel)
{
    return zz_sfx_is_playing(channel)?true:false;
}
static void ZZ_CacheSounds(sfxinfo_t *sounds, int num_sounds){}

static snddevice_t zz_sound_devices[]={SNDDEVICE_SB};
sound_module_t zz9000_sound_module={
    zz_sound_devices, 1,
    ZZ_SoundInit, ZZ_SoundShutdown,
    ZZ_GetSfxLumpNum, ZZ_SoundUpdate, ZZ_UpdateSoundParams,
    ZZ_StartSound, ZZ_StopSound, ZZ_SoundIsPlaying,
    ZZ_CacheSounds,
};
