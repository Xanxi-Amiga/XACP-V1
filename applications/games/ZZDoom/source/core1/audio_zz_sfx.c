/*
 * audio_zz.c - ZZDoom: 8-channel PCM mixer, ARM Core1.
 * ZZ_StartSound (i_zz9000sound.c) calls zz_sfx_start() directly.
 * No shared-memory trigger - shared is for diagnostics only.
 * zz_audio_pump() mixes all active channels into the PCM ring.
 * ASCII only.
 */
typedef unsigned int   u32;
typedef int            s32;
typedef unsigned short u16;
typedef short          s16;
typedef unsigned char  u8;

#define SHARED_ADDR      0x04300000UL
#define PCM_RING_ARM     0x04400000UL
#define PCM_RING_SIZE    (512UL*1024UL)
#define PCM_RATE         22050UL
#define MIX_CHUNK        128UL
#define NUM_CHANNELS     8
#define RING_TARGET      (1536UL*2UL)   /* 1536 s16 = ~70ms */

/* Shared slots */
#define SH_WAD_ADDR      8
#define SH_WAD_SIZE      9
#define SH_PCM_BASE      22
#define SH_PCM_SIZE      23
#define SH_PCM_WRITE_POS 24
#define SH_PCM_READ_POS  25
#define SH_PCM_RATE      26
#define SH_PCM_FORMAT    27
#define SH_PCM_UNDERRUNS 28
#define SH_PCM_ENABLED   29
#define SH_AUDIO_DIAG    40
#define SH_MIX_ACTIVE    56   /* bitmask of active channels */
#define SH_PUMP_CALLS    57   /* total zz_audio_pump calls */
#define SH_SFX_STARTS    58   /* total StartSound calls */
#define SH_SFX_STOLEN    59   /* total stolen channels */
#define SH_SFX_DROPPED   60   /* sounds dropped (bad lump) */
#define SH_SFX_ACTIVE    61   /* current active channel count */
#define SH_SFX_MAXACT    62   /* peak active channels seen */
#define SH_RING_MIN      63   /* minimum ring level seen (bytes) */

#define AU_RING ((volatile s16*)PCM_RING_ARM)

volatile u32 *au_shared = (volatile u32*)SHARED_ADDR;

/* --- 8-channel state ------------------------------------ */
typedef struct {
    const u8 *data;      /* pointer to u8 source samples (WAD RAM) */
    u32 length;          /* total source samples */
    u32 pos;             /* current source sample index */
    u32 frac;            /* fractional position (16-bit fixed) */
    u32 step;            /* step per output sample (src_rate/dst_rate * 65536) */
    s32 volume;          /* 0-127 */
    u32 active;          /* 1 = playing */
    u32 age;             /* incremented at start, used for LRU steal */
} ZZChannel;

static ZZChannel channels[NUM_CHANNELS];
static u32 sfx_total_starts = 0;
static u32 sfx_total_stolen = 0;
static u32 sfx_total_dropped = 0;
static u32 sfx_max_active = 0;
static u32 ch_age = 0;

/* Ring write position */
static u32 au_write_pos = 0;

/* --- WAD helpers ---------------------------------------- */
static u32 rd_le32(const u8 *p){
    return (u32)p[0]|((u32)p[1]<<8)|((u32)p[2]<<16)|((u32)p[3]<<24);
}
static u16 rd_le16(const u8 *p){
    return (u16)((u32)p[0]|((u32)p[1]<<8));
}
static const u8 *wad_find_lump(const char *name, u32 *out_size)
{
    const u8 *wad=(const u8*)au_shared[SH_WAD_ADDR];
    u32 sz=au_shared[SH_WAD_SIZE], num, doff, i;
    const u8 *dir;
    if(!wad||sz<12||( wad[0]!='I'&&wad[0]!='P')) return 0;
    num=rd_le32(wad+4); doff=rd_le32(wad+8);
    if(doff+num*16>sz) return 0;
    dir=wad+doff;
    for(i=0;i<num;i++){
        const u8 *e=dir+i*16;
        int j,ok=1;
        for(j=0;j<8;j++){
            char ec=(char)e[8+j], nc=name[j];
            if(ec!=nc){ok=0;break;}
            if(nc==0){int k;for(k=j+1;k<8;k++)if(e[8+k]!=0){ok=0;break;}break;}
        }
        if(ok){
            u32 fp=rd_le32(e),s=rd_le32(e+4);
            if(out_size)*out_size=s;
            return wad+fp;
        }
    }
    return 0;
}

/* --- Ring helpers --------------------------------------- */
static u32 ring_free_bytes(void)
{
    u32 rpos=au_shared[SH_PCM_READ_POS];
    u32 wpos=au_write_pos;
    u32 used=(wpos>=rpos)?(wpos-rpos):(PCM_RING_SIZE-rpos+wpos);
    u32 free_b=PCM_RING_SIZE-used;
    return (free_b>=2)?(free_b-2):0;
}
static void ring_write_s16(s16 s)
{
    u32 idx=au_write_pos/2;
    AU_RING[idx]=s;
    if(++idx>=PCM_RING_SIZE/2) idx=0;
    au_write_pos=idx*2;
}

/* --- Channel management --------------------------------- */
/* Find a free channel or steal the best candidate:
   prefer channel nearest end of playback (pos/length highest ratio),
   then lowest volume, then oldest. Never steal a channel with vol=127
   if one with lower vol exists. */
static int find_channel(void)
{
    int i, best=-1;
    u32 best_score=0;
    for(i=0;i<NUM_CHANNELS;i++){
        if(!channels[i].active) return i;
    }
    /* All occupied: find best steal candidate */
    for(i=0;i<NUM_CHANNELS;i++){
        u32 progress, score;
        /* progress = how far along (0=start, 65535=end) */
        if(channels[i].length>0)
            progress = (channels[i].pos * 65535UL) / channels[i].length;
        else
            progress = 65535UL;
        /* score = progress (near end = better steal) + low volume bonus */
        score = progress + (127UL - (u32)channels[i].volume) * 128UL;
        if(best<0 || score > best_score){ best_score=score; best=i; }
    }
    sfx_total_stolen++;
    return best;
}

/* Start a sound on a channel. Called directly from ZZ_StartSound. */
int zz_sfx_start(const u8 *data, u32 length, u32 src_rate, s32 vol)
{
    int ch;
    u32 step;
    if(!data||length==0) return -1;
    ch = find_channel();
    if(ch<0) return -1;
    /* step = (src_rate << 16) / PCM_RATE  (fixed 16.16) */
    step = (u32)(((unsigned long long)src_rate << 16) / PCM_RATE);
    sfx_total_starts++;
    au_shared[SH_SFX_STARTS] = sfx_total_starts;
    channels[ch].data   = data;
    channels[ch].length = length;
    channels[ch].pos    = 0;
    channels[ch].frac   = 0;
    channels[ch].step   = step;
    channels[ch].volume = (vol>127)?127:(vol<0)?0:vol;
    channels[ch].active = 1;
    channels[ch].age    = ch_age++;
    return ch;
}

void zz_sfx_stop(int ch)
{
    if(ch>=0&&ch<NUM_CHANNELS) channels[ch].active=0;
}

int zz_sfx_is_playing(int ch)
{
    if(ch<0||ch>=NUM_CHANNELS) return 0;
    return channels[ch].active;
}

/* --- Public API ----------------------------------------- */
void zz_audio_init(void)
{
    u32 i;
    for(i=0;i<PCM_RING_SIZE/2;i++) AU_RING[i]=0;
    au_write_pos=0;
    for(i=0;i<NUM_CHANNELS;i++) channels[i].active=0;
    ch_age=0;
    au_shared[SH_PCM_BASE]      = PCM_RING_ARM;
    au_shared[SH_PCM_SIZE]      = PCM_RING_SIZE;
    au_shared[SH_PCM_WRITE_POS] = 0;
    au_shared[SH_PCM_READ_POS]  = 0;
    au_shared[SH_PCM_RATE]      = PCM_RATE;
    au_shared[SH_PCM_FORMAT]    = 0;
    au_shared[SH_PCM_UNDERRUNS] = 0;
    au_shared[SH_AUDIO_DIAG]    = 0;
    au_shared[SH_MIX_ACTIVE]    = 0;
    au_shared[SH_PUMP_CALLS]    = 0;
    au_shared[SH_PCM_ENABLED]   = 1;
}

/* Mix NUM_CHANNELS active channels into ring, up to RING_TARGET ahead. */
void zz_audio_pump(void)
{
    u32 free_b, fill_b, chunks, mask;
    u32 i;

    au_shared[SH_PUMP_CALLS]++;
    /* Track minimum ring level */
    {
        u32 rpos=au_shared[SH_PCM_READ_POS], wpos=au_write_pos;
        u32 level=(wpos>=rpos)?(wpos-rpos):(PCM_RING_SIZE-rpos+wpos);
        u32 prev_min=au_shared[SH_RING_MIN];
        if(prev_min==0 || level < prev_min)
            au_shared[SH_RING_MIN]=level;
    }

    /* How much free space, capped at RING_TARGET */
    free_b = ring_free_bytes();
    {
        u32 rpos=au_shared[SH_PCM_READ_POS], wpos=au_write_pos;
        u32 level=(wpos>=rpos)?(wpos-rpos):(PCM_RING_SIZE-rpos+wpos);
        if(level >= RING_TARGET) return;   /* ring full enough, skip */
        fill_b = RING_TARGET - level;
        if(fill_b > free_b) fill_b = free_b;
    }
    chunks = fill_b / (MIX_CHUNK*2);
    if(chunks==0) return;

    /* Update active channel diagnostic */
    {
        u32 nact=0;
        mask=0;
        for(i=0;i<NUM_CHANNELS;i++){
            if(channels[i].active){ mask|=(1<<i); nact++; }
        }
        au_shared[SH_MIX_ACTIVE]=mask;
        au_shared[SH_SFX_ACTIVE]=nact;
        if(nact>sfx_max_active){
            sfx_max_active=nact;
            au_shared[SH_SFX_MAXACT]=sfx_max_active;
        }
    }

    /* Mix chunks */
    while(chunks--){
        u32 j;
        for(j=0;j<MIX_CHUNK;j++){
            s32 out=0;
            for(i=0;i<NUM_CHANNELS;i++){
                ZZChannel *c=&channels[i];
                if(!c->active) continue;
                if(c->pos >= c->length){ c->active=0; continue; }
                {
                    /* Linear interpolation between pos and pos+1 */
                    u8 s0=c->data[c->pos];
                    u8 s1=(c->pos+1<c->length)?c->data[c->pos+1]:s0;
                    s32 samp0=((s32)s0-128)<<8;
                    s32 samp1=((s32)s1-128)<<8;
                    s32 samp=samp0+(((samp1-samp0)*(s32)(c->frac>>8))>>8);
                    /* Apply volume (0-127). Gain reduced when many channels
                       active to avoid clipping. */
                    out += (samp * c->volume) / 127;
                }
                /* Advance fractional position */
                c->frac += c->step & 0xFFFF;
                c->pos  += c->step >> 16;
                c->pos  += c->frac >> 16;
                c->frac &= 0xFFFF;
            }
            /* Clamp to s16 - hard limit (simple, reliable) */
            if(out > 32767)  out = 32767;
            if(out < -32768) out = -32768;
            ring_write_s16((s16)out);
        }
    }
    au_shared[SH_PCM_WRITE_POS]=au_write_pos;
}

/* Compatibility wrapper (called from DG_DrawFrame) */
void zz_audio_fill_beep(void)
{
    volatile u32 *sh=(volatile u32*)SHARED_ADDR;
    sh[51]++;
    zz_audio_pump();
}
