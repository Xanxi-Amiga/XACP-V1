/*
 * ZZMpegStream.c
 *
 * ZZ-MPEG 1.0 Advanced Beta
 * MPEG-1 Program Stream player for Amiga + ZZ9000.
 *
 * Video is decoded on ZZ9000 ARM/Core1 and rendered either to a
 * Workbench window bitmap or to a dedicated Picasso96 320x240x32 screen.
 * MP2 audio is decoded through XACP and played through AHI.
 *
 * Build:
 *   m68k-amigaos-gcc -O2 -noixemul -m68020 -o ZZMpegPlayer ZZMpegStream.c -lamiga
 *
 * Usage:
 *   ZZMpegPlayer [file.mpg]
 *
 * Recommended input: MPEG-1 PS, 320x240, 25 fps CFR, MP2 44.1 kHz stereo.
 */

#define __USE_SYSBASE

#include <exec/types.h>
#include <exec/memory.h>
#include <exec/io.h>
#include <dos/dos.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <libraries/configvars.h>
#include <devices/ahi.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/expansion.h>
#include <proto/graphics.h>
#include <proto/Picasso96.h>
#include <string.h>
#include <stdio.h>

struct Library *P96Base = NULL;

/* ZZ9000 */
#define ZZ9000_MANUF    0x6D6Eu
#define ZZ9000_PROD_AX  0x0Au
#define ZZ9000_PROD     0x04u
#define MNT_FB_BASE     0x00010000UL
#define REG_CMD         0x0064
#define REG_ARM_RUN_HI  0x0090
#define REG_ARM_RUN_LO  0x0092
#define ZZ_WR(b,o,v)    (*((volatile UWORD*)((UBYTE*)(b)+(o)))=(UWORD)(v))

/* Core1 memory map */
#define CORE1_CODE_FB   0x04300000UL
#define CORE1_SHARED_FB 0x04500000UL
#define CORE1_ARM_ADDR  0x04500000UL
#define CORE1_MAGIC     0xDEAD0004UL
#define CPU_TO_ARM(c,f) (0x00200000UL+((ULONG)(c)-(ULONG)(f)))
#define CORE1_PS_FB     0x05000000UL
#define PS_RING_SIZE    (4UL*1024UL*1024UL)

/* XACP audio buffers */
#define XACP_STREAM_OFFSET  0x04002000UL
#define XACP_MP3_OFFSET     0x04100000UL
#define XACP_PCM_OFFSET     0x04200000UL
#define MP3_RING_SIZE       (512UL  * 1024UL)
#define PCM_RING_SIZE       (1024UL * 1024UL)
#define OP_STREAM_OPEN      4
#define OP_STREAM_CLOSE     5
#define MULTIPLIER          2UL
#define AHI_BUFSIZE         65536UL
#define BE32(x)             (x)

#define BLOCK_SIZE  (64UL*1024UL)

/* Core1 mailbox slots */
#define SH_MAGIC           0
#define SH_HEARTBEAT       4
#define SH_STOP            8
#define SH_STATUS         12
#define SH_FB_ADDR        16
#define SH_FB_WIDTH       20
#define SH_FB_HEIGHT      24
#define SH_FB_PITCH       28
#define SH_PS_RING_BASE   32
#define SH_PS_RING_SIZE   36
#define SH_PS_WRITE       40
#define SH_PS_READ        44
#define SH_PS_EOF         48
#define SH_PS_NEED_REFILL 52
#define SH_PS_UNDERRUN    56
#define SH_CMD            60

#define STATUS_FRAME_READY 4UL
#define STATUS_DONE        2UL
#define MPEG_CMD_NEXT      1UL
#define MPEG_CMD_STOP      2UL

/* Embedded Core1 MPEG stream blob with STOP ACK support */
#include "mpeg_core1_blob_stream.h"

typedef struct {
    volatile ULONG mp3_base, mp3_size, mp3_write, mp3_read;
    volatile ULONG mp3_need_refill, mp3_eof;
    volatile ULONG pcm_base, pcm_size, pcm_write, pcm_read;
    volatile ULONG sample_rate, channels;
    volatile ULONG status, error;
    volatile ULONG underrun_count, frames_decoded;
    volatile ULONG flags;
} XACP_StreamControl;

/* Ring helpers */
static ULONG ring_avail(ULONG w,ULONG r,ULONG sz){return (w>=r)?(w-r):(sz-r+w);}
static ULONG ring_free(ULONG w,ULONG r,ULONG sz){return sz-1-ring_avail(w,r,sz);}
static void ring_push(UBYTE *ring,ULONG rsz,ULONG *wp,const UBYTE *src,ULONG len){
    ULONG w=*wp,first=rsz-w;
    if(first>len)first=len;
    CopyMem((APTR)src,ring+w,first);
    if(len>first)CopyMem((APTR)(src+first),ring,len-first);
    *wp=(w+len)%rsz;
}
static void ring_copy_out(UBYTE *ring,ULONG rsz,ULONG rd,UBYTE *dst,ULONG len){
    ULONG ste=rsz-rd;
    if(len<=ste)CopyMem(ring+rd,dst,len);
    else{CopyMem(ring+rd,dst,ste);CopyMem(ring,dst+ste,len-ste);}
}
static void copy_uword(volatile UBYTE *d,const UBYTE *s,ULONG sz){
    volatile UWORD *dw=(volatile UWORD*)d;
    const UWORD *sw=(const UWORD*)s;
    ULONG i;for(i=0;i<(sz+1)/2;i++)dw[i]=sw[i];
}
static void ring_push_uword(volatile UBYTE *ring,ULONG rsz,ULONG *wp,
                             const UBYTE *src,ULONG len){
    ULONG w=*wp,first=rsz-w;
    if(first>len)first=len;
    copy_uword(ring+w,src,first);
    if(len>first)copy_uword(ring,src+first,len-first);
    *wp=(w+len)%rsz;
}

static ULONG rd32(volatile UBYTE *b,ULONG o){
    volatile UWORD *p=(volatile UWORD*)(b+o);
    UWORD w0=p[0],w1=p[1];
    return ((ULONG)((UWORD)((w1>>8)|(w1<<8)))<<16)|(ULONG)((UWORD)((w0>>8)|(w0<<8)));
}
static void wr32(volatile UBYTE *b,ULONG o,ULONG v){
    volatile UWORD *p=(volatile UWORD*)(b+o);
    ULONG s=((v&0xFFUL)<<24)|((v&0xFF00UL)<<8)|((v&0xFF0000UL)>>8)|((v&0xFF000000UL)>>24);
    p[0]=(UWORD)(s>>16);p[1]=(UWORD)(s&0xFFFF);
}

/* Globals */
static volatile UBYTE     *g_board=NULL, *g_fb=NULL;
static volatile UBYTE     *g_shared=NULL, *g_ps_ring=NULL;
static XACP_StreamControl *g_sc=NULL;
static UBYTE *g_mring=NULL, *g_pring=NULL;
static ULONG  g_awptr=0, g_prd=0;
static BOOL   g_eofsent=FALSE;
static ULONG  g_sr=44100, g_ch=2, g_chunk=0;
static ULONG  g_audio_bytes_pushed=0, g_audio_packets_found=0;
static ULONG  g_ahi_sent=0, g_ahi_done=0;
static ULONG  g_ps_wptr=0;
static BOOL   g_feof_video=FALSE;
static BOOL   g_feof_audio=FALSE;
static BPTR   g_file_video=0;
static BPTR   g_file_audio=0;
static UBYTE  g_blk[BLOCK_SIZE];

/* AHI state */
static struct MsgPort    *g_ahiport=NULL;
static struct AHIRequest *g_req[2]={NULL,NULL};
static BOOL               g_act[2]={FALSE,FALSE};
static UBYTE g_buf0[AHI_BUFSIZE];
static UBYTE g_buf1[AHI_BUFSIZE];

/* Demux carry */
#define CARRY_SZ (8UL*1024UL)
static UBYTE g_carry[CARRY_SZ];
static ULONG g_carry_len=0;
static UBYTE g_demux_work[CARRY_SZ+BLOCK_SIZE];

/* AHI helpers */
static void send_ahi_chunk(int slot,UBYTE *buf,ULONG len){
    ULONG ahi_type=(g_ch==2)?AHIST_S16S:AHIST_M16S;
    g_req[slot]->ahir_Std.io_Command=CMD_WRITE;
    g_req[slot]->ahir_Std.io_Data=buf;
    g_req[slot]->ahir_Std.io_Length=len;
    g_req[slot]->ahir_Frequency=g_sr;
    g_req[slot]->ahir_Type=ahi_type;
    g_req[slot]->ahir_Volume=0x10000L;
    g_req[slot]->ahir_Position=0x8000L;
    g_req[slot]->ahir_Link=g_act[slot^1]?g_req[slot^1]:NULL;
    SendIO((struct IORequest*)g_req[slot]);
    g_act[slot]=TRUE;
}
static void ahi_stop_all(void){
    int i;
    for(i=0;i<2;i++){
        if(g_req[i]&&g_act[i]){
            if(!CheckIO((struct IORequest*)g_req[i]))
                AbortIO((struct IORequest*)g_req[i]);
            WaitIO((struct IORequest*)g_req[i]);
            g_act[i]=FALSE;
        }
    }
}
static BOOL init_ahi(void){
    int i;
    g_ahiport=CreateMsgPort();
    if(!g_ahiport)return FALSE;
    for(i=0;i<2;i++){
        g_req[i]=(struct AHIRequest*)
            CreateIORequest(g_ahiport,sizeof(struct AHIRequest));
        if(!g_req[i])return FALSE;
        g_act[i]=FALSE;
    }
    g_req[0]->ahir_Version=4;
    if(OpenDevice(AHINAME,AHI_DEFAULT_UNIT,
                  (struct IORequest*)g_req[0],0)!=0)return FALSE;
    CopyMem(g_req[0],g_req[1],sizeof(struct AHIRequest));
    return TRUE;
}
static void cleanup_ahi(void){
    int i;
    ahi_stop_all();
    for(i=0;i<2;i++){
        if(g_req[i]){
            if(i==0)CloseDevice((struct IORequest*)g_req[i]);
            DeleteIORequest((struct IORequest*)g_req[i]);
            g_req[i]=NULL;
        }
    }
    if(g_ahiport){DeleteMsgPort(g_ahiport);g_ahiport=NULL;}
}

/* Incremental MP2 demux from MPEG Program Stream */
static void demux_push_audio(const UBYTE *data,ULONG len){
    UBYTE *work=g_demux_work;
    ULONG work_len=g_carry_len+len,pos=0;
    if(work_len>CARRY_SZ+BLOCK_SIZE){g_carry_len=0;work_len=len;}
    if(g_carry_len>0)CopyMem((APTR)g_carry,work,g_carry_len);
    CopyMem((APTR)data,work+g_carry_len,len);
    while(pos+6<=work_len){
        if(work[pos]!=0||work[pos+1]!=0||work[pos+2]!=1){pos++;continue;}
        UBYTE sid=work[pos+3];
        if(sid==0xBA){pos+=12;continue;}
        if(sid==0xBB){
            if(pos+6>work_len)break;
            ULONG hl=((ULONG)work[pos+4]<<8)|work[pos+5];
            if(pos+6+hl>work_len)break;
            pos+=6+hl;continue;
        }
        if(pos+6>work_len)break;
        ULONG plen=((ULONG)work[pos+4]<<8)|work[pos+5];
        if(!plen||plen>65536UL){pos+=4;continue;}
        if(pos+6+plen>work_len)break;
        if(sid==0xC0&&g_sc&&!g_eofsent){
            const UBYTE *pes=work+pos+6;
            ULONG skip=0;
            while(skip<plen&&pes[skip]==0xFF)skip++;
            if(skip<plen&&(pes[skip]&0xC0)==0x40)skip+=2;
            if(skip<plen){
                UBYTE f=pes[skip]&0xF0;
                if(f==0x20)skip+=5;
                else if(f==0x30)skip+=10;
                else skip+=1;
            }
            if(skip<plen){
                ULONG rp=BE32(g_sc->mp3_read);
                ULONG mfree=ring_free(g_awptr,rp,MP3_RING_SIZE);
                ULONG push=plen-skip;
                if(push>mfree)push=mfree;
                if(push>0){
                    ring_push(g_mring,MP3_RING_SIZE,&g_awptr,pes+skip,push);
                    g_sc->mp3_write=BE32(g_awptr);
                    g_sc->mp3_need_refill=0;
                    g_audio_bytes_pushed+=push;
                    g_audio_packets_found++;
                }
            }
        }
        pos+=6+plen;
    }
    if(pos<work_len){
        ULONG rest=work_len-pos;
        if(rest>CARRY_SZ)rest=CARRY_SZ;
        CopyMem((APTR)(work+pos),g_carry,rest);
        g_carry_len=rest;
    }else g_carry_len=0;
}

/* Audio pump: submit decoded PCM chunks to AHI */
static void audio_tick(void){
    int s;
    if(!g_sc)return;
    for(s=0;s<2;s++){
        if(g_act[s]&&CheckIO((struct IORequest*)g_req[s])){
            WaitIO((struct IORequest*)g_req[s]);
            g_act[s]=FALSE; g_ahi_done++;
        }
    }
    {
        ULONG pcm_w=BE32(g_sc->pcm_write);
        ULONG avail=ring_avail(pcm_w,g_prd,PCM_RING_SIZE);
        for(s=0;s<2;s++){
            if(!g_act[s]&&avail>=g_chunk){
                UBYTE *buf=(s==0)?g_buf0:g_buf1;
                ring_copy_out(g_pring,PCM_RING_SIZE,g_prd,buf,g_chunk);
                g_prd=(g_prd+g_chunk)%PCM_RING_SIZE;
                g_sc->pcm_read=BE32(g_prd);
                pcm_w=BE32(g_sc->pcm_write);
                avail=ring_avail(pcm_w,g_prd,PCM_RING_SIZE);
                send_ahi_chunk(s,buf,g_chunk);
                g_ahi_sent++;
            }
        }
    }
    if(!g_eofsent&&g_feof_audio){
        ULONG rp=BE32(g_sc->mp3_read);
        if(ring_avail(g_awptr,rp,MP3_RING_SIZE)==0){
            g_sc->mp3_eof=BE32(1);g_eofsent=TRUE;
        }
    }
}

/* Refill the MP2 input ring independently from the video stream */
static void refill_audio_tick(void){
    if(g_feof_audio||!g_file_audio||!g_sc)return;
    ULONG rp=BE32(g_sc->mp3_read);
    ULONG mfree=ring_free(g_awptr,rp,MP3_RING_SIZE);
    if(mfree<BLOCK_SIZE)return;
    LONG got=Read(g_file_audio,g_blk,(LONG)BLOCK_SIZE);
    if(got<=0){g_feof_audio=TRUE;return;}
    demux_push_audio(g_blk,(ULONG)got);
    if(got<(LONG)BLOCK_SIZE)g_feof_audio=TRUE;
}

/* Refill the MPEG Program Stream ring for Core1 */
static void refill_video_tick(void){
    if(g_feof_video||!g_file_video)return;
    ULONG ps_rp  =rd32(g_shared,SH_PS_READ);
    ULONG ps_free=PS_RING_SIZE-1UL-ring_avail(g_ps_wptr,ps_rp,PS_RING_SIZE);
    if(ps_free<BLOCK_SIZE)return;
    LONG got=Read(g_file_video,g_blk,(LONG)BLOCK_SIZE);
    if(got<=0){g_feof_video=TRUE;wr32(g_shared,SH_PS_EOF,1UL);return;}
    ring_push_uword(g_ps_ring,PS_RING_SIZE,&g_ps_wptr,g_blk,(ULONG)got);
    wr32(g_shared,SH_PS_WRITE,g_ps_wptr);
    if(got<(LONG)BLOCK_SIZE){g_feof_video=TRUE;wr32(g_shared,SH_PS_EOF,1UL);}
}

/* ================================================================
 * GUI wrapper
 *
 * Windowed and fullscreen playback modes with an ASL file requester.
 *
 * Keyboard during playback:
 *   F1       Play / unpause
 *   F2       Pause
 *   F3/Esc   Stop and return to GUI
 *   F4       Stop and quit
 *   F5       Toggle debug output
 * ================================================================ */

#include <proto/gadtools.h>
#include <proto/asl.h>
#include <libraries/gadtools.h>
#include <libraries/asl.h>

struct Library *GadToolsBase = NULL;
struct Library *AslBase      = NULL;

#define KEY_F1  0x50
#define KEY_F2  0x51
#define KEY_F3  0x52
#define KEY_F4  0x53
#define KEY_F5  0x54
#define KEY_ESC 0x45

/* ASL file requester */
static BOOL select_file_asl(char *out_path, WORD max_len)
{
    struct FileRequester *freq;
    BOOL result = FALSE;
    freq = (struct FileRequester*)AllocAslRequestTags(ASL_FileRequest,
        ASLFR_TitleText,     (ULONG)"Choose MPEG-1 file",
        ASLFR_InitialDrawer, (ULONG)"DH0:",
        ASLFR_DoPatterns,    TRUE,
        ASLFR_InitialPattern,(ULONG)"#?.mpg",
        ASLFR_RejectIcons,   TRUE,
        TAG_DONE);
    if(!freq) return FALSE;
    if(AslRequestTags(freq, TAG_DONE)){
        strncpy(out_path, freq->rf_Dir, max_len-1);
        out_path[max_len-1] = '\0';
        { WORD l = strlen(out_path);
          if(l>0 && out_path[l-1]!=':' && out_path[l-1]!='/')
              strncat(out_path, "/", max_len-l-1); }
        strncat(out_path, freq->rf_File, max_len-strlen(out_path)-1);
        result = TRUE;
    }
    FreeAslRequest(freq);
    return result;
}

/* Common playback loop shared by windowed and fullscreen modes.
 *
 * win       : window receiving IDCMP events
 * ded_scr   : dedicated screen to close on stop, NULL in windowed mode
 * board/fb/shared: ZZ9000 board, framebuffer base and mailbox pointers
 * arm_fb_v  : ARM-visible framebuffer address
 * pitch     : bytes per row
 */
static int do_play(struct Window *win, struct Screen *ded_scr_arg,
                   volatile UBYTE *board_arg, volatile UBYTE *fb_arg,
                   volatile UBYTE *shared_arg, volatile UBYTE *ps_ring_arg,
                   ULONG arm_fb_v, ULONG pitch)
{
    struct Screen *ded_scr_local = ded_scr_arg;
    BOOL  video_done = FALSE;
    BOOL  paused     = FALSE;
    BOOL  quit_req   = FALSE;
    BOOL  g_dbg      = FALSE;
    char  title[80];
    ULONG frames_sent=0, frames_prev=0, stat_5s=0;
    ULONG start_secs=0, start_usecs=0;
    ULONG fps_count=0, fps_last_s=0;

    /* Mailbox */
    {ULONG i; volatile UWORD *p=(volatile UWORD*)shared_arg; for(i=0;i<128;i++) p[i]=0;}
    wr32(shared_arg, SH_FB_ADDR,         arm_fb_v);
    wr32(shared_arg, SH_FB_WIDTH,        320UL);
    wr32(shared_arg, SH_FB_HEIGHT,       240UL);
    wr32(shared_arg, SH_FB_PITCH,        pitch);
    wr32(shared_arg, SH_PS_RING_BASE,    CPU_TO_ARM(ps_ring_arg, fb_arg));
    wr32(shared_arg, SH_PS_RING_SIZE,    PS_RING_SIZE);
    wr32(shared_arg, SH_PS_WRITE,        g_ps_wptr);
    wr32(shared_arg, SH_PS_READ,         0UL);
    wr32(shared_arg, SH_PS_EOF,          g_feof_video?1UL:0UL);
    wr32(shared_arg, SH_PS_NEED_REFILL,  0UL);
    wr32(shared_arg, SH_PS_UNDERRUN,     0UL);
    wr32(shared_arg, SH_CMD,             0UL);

    /* Set globals used by refill ticks */
    g_shared  = shared_arg;
    g_ps_ring = ps_ring_arg;

    /* Launch Core1 */
    copy_uword(fb_arg+CORE1_CODE_FB,
               embedded_blob_stream_diag15,
               embedded_blob_stream_diag15_size);
    ZZ_WR(board_arg, REG_ARM_RUN_HI, (UWORD)(CORE1_ARM_ADDR>>16));
    ZZ_WR(board_arg, REG_ARM_RUN_LO, (UWORD)(CORE1_ARM_ADDR&0xFFFFUL));

    /* Wait MAGIC */
    {LONG t=500; while(t-->0 && rd32(shared_arg,SH_MAGIC)!=CORE1_MAGIC) Delay(1);}
    if(rd32(shared_arg,SH_MAGIC)!=CORE1_MAGIC){printf("MAGIC FAIL\n");goto stop;}
    printf("MAGIC OK\n");

    /* Wait first STATUS_FRAME_READY */
    {LONG t=200; while(t-->0 && (rd32(shared_arg,SH_STATUS)&0xFF)!=STATUS_FRAME_READY) Delay(1);}

    /* OP_STREAM_OPEN */
    ZZ_WR(board_arg, REG_CMD, OP_STREAM_OPEN);
    {volatile int d=20000; while(d--);}
    {ULONG to=0;
     while(g_sc->sample_rate==0){volatile int d=500;while(d--);if(++to>2000){printf("timeout sr\n");goto stop;}}
    }
    g_sr    = BE32(g_sc->sample_rate);
    g_ch    = BE32(g_sc->channels);
    if(!g_sr) g_sr=44100;
    if(!g_ch||g_ch>2) g_ch=2;
    g_chunk = (g_sr/50UL)*g_ch*2UL*MULTIPLIER;
    if(g_chunk>AHI_BUFSIZE) g_chunk=AHI_BUFSIZE;
    printf("XACP: %luHz %luch chunk=%lu\n", g_sr, g_ch, g_chunk);

    /* Wait first PCM chunk */
    CacheClearU();
    {ULONG w=BE32(g_sc->pcm_write), avail=ring_avail(w,g_prd,PCM_RING_SIZE), to=0;
     while(avail<g_chunk){
         Delay(1); CacheClearU();
         w=BE32(g_sc->pcm_write); avail=ring_avail(w,g_prd,PCM_RING_SIZE);
         if(++to>300){printf("timeout PCM\n");goto stop;}
     }
    }

    /* Start AHI - 2 slots */
    {ULONG pcm_w=BE32(g_sc->pcm_write), avail=ring_avail(pcm_w,g_prd,PCM_RING_SIZE);
     ring_copy_out(g_pring,PCM_RING_SIZE,g_prd,g_buf0,g_chunk);
     g_prd=(g_prd+g_chunk)%PCM_RING_SIZE; g_sc->pcm_read=BE32(g_prd);
     send_ahi_chunk(0,g_buf0,g_chunk); g_ahi_sent++;
     avail-=g_chunk;
     if(avail>=g_chunk){
         ring_copy_out(g_pring,PCM_RING_SIZE,g_prd,g_buf1,g_chunk);
         g_prd=(g_prd+g_chunk)%PCM_RING_SIZE; g_sc->pcm_read=BE32(g_prd);
         send_ahi_chunk(1,g_buf1,g_chunk); g_ahi_sent++;
     }
    }
    printf("Playing   F1=Play  F2=Pause  F3/ESC=Stop  F4=Quit  F5=Debug\n");

    CurrentTime(&start_secs, &start_usecs);

    /* Main playback loop */
    while(1){
        ULONG st = rd32(shared_arg,SH_STATUS)&0xFF;
        if(st==0x03) break;
        if(st==STATUS_DONE && !video_done){
            video_done=TRUE;
            printf("video done - draining audio\n");
        }

        /* IDCMP keyboard */
        {struct IntuiMessage *msg;
         while((msg=(struct IntuiMessage*)GetMsg(win->UserPort))!=NULL){
             ULONG cls=msg->Class; UWORD code=msg->Code;
             ReplyMsg((struct Message*)msg);
             if(cls==IDCMP_RAWKEY && !(code&0x80)){
                 switch(code&0x7F){
                     case KEY_F1: paused=FALSE; break;
                     case KEY_F2: paused=TRUE;  break;
                     case KEY_F3:
                     case KEY_ESC: goto stop;
                     case KEY_F4: quit_req=TRUE; goto stop;
                     case KEY_F5: g_dbg=!g_dbg; break;
                 }
             }
             if(cls==IDCMP_CLOSEWINDOW) goto stop;
         }
        }
        if(CheckSignal(SIGBREAKF_CTRL_C)) goto stop;

        /* Real-time clock */
        ULONG now_s=0, now_us=0;
        CurrentTime(&now_s, &now_us);
        ULONG elapsed_ms = (now_s-start_secs)*1000UL;
        if(now_us>=start_usecs) elapsed_ms+=(now_us-start_usecs)/1000UL;
        else                    elapsed_ms-=(start_usecs-now_us)/1000UL;

        /* 1. Audio - top priority */
        if(!paused) audio_tick();

        /* 2a. Audio refill - independent */
        if(!paused) refill_audio_tick();

        /* 2b. Video refill - stops after video_done */
        if(!paused && !video_done) refill_video_tick();

        /* 3. CMD_NEXT - real clock pacing, never after video_done */
        ULONG target_frames = elapsed_ms*25UL/1000UL;
        if(!paused && !video_done && st==STATUS_FRAME_READY){
            if(frames_sent < target_frames){
                wr32(shared_arg, SH_CMD, MPEG_CMD_NEXT);
                frames_sent++;
                fps_count++;
            }
        }

        Delay(1);

        /* Exit: video done + audio fully drained */
        if(video_done){
            ULONG mp3_r2 = BE32(g_sc->mp3_read);
            ULONG mp3_av2= ring_avail(g_awptr,mp3_r2,MP3_RING_SIZE);
            ULONG pcm_w3 = BE32(g_sc->pcm_write);
            ULONG pcm_av2= ring_avail(pcm_w3,g_prd,PCM_RING_SIZE);
            if(g_feof_audio && mp3_av2==0
               && (pcm_av2<g_chunk||pcm_av2==0)
               && !g_act[0] && !g_act[1]){
                printf("audio drained: mp3_av=%lu pcm_av=%lu\n",mp3_av2,pcm_av2);
                break;
            }
        }

        /* Window title: FPS every second */
        if(now_s != fps_last_s){
            fps_last_s = now_s;
            sprintf(title,"ZZMpegPlayer  F:%lu FPS:%lu %luHz",
                    frames_sent, fps_count, g_sr);
            SetWindowTitles(win, title, (UBYTE*)-1L);
            if(g_dbg){
                LONG delta=(LONG)target_frames-(LONG)frames_sent;
                printf("t=%lus F=%lu tgt=%lu d=%ld fps=%lu und=%lu\n",
                       now_s-start_secs, frames_sent, target_frames,
                       delta, fps_count, BE32(g_sc->underrun_count));
            }
            fps_count=0;
        }

        /* Console stats every 5s */
        ULONG cur_5s = elapsed_ms/5000UL;
        if(cur_5s > stat_5s){
            stat_5s = cur_5s;
            ULONG elapsed_s=elapsed_ms/1000UL;
            ULONG fc    =rd32(shared_arg,SH_PS_UNDERRUN);
            ULONG ps_w  =rd32(shared_arg,SH_PS_WRITE);
            ULONG ps_r  =rd32(shared_arg,SH_PS_READ);
            ULONG ps_av =ring_avail(ps_w,ps_r,PS_RING_SIZE);
            ULONG mp3_r3=BE32(g_sc->mp3_read);
            ULONG mp3_av=ring_avail(g_awptr,mp3_r3,MP3_RING_SIZE);
            ULONG pcm_w4=BE32(g_sc->pcm_write);
            ULONG pcm_av=ring_avail(pcm_w4,g_prd,PCM_RING_SIZE);
            ULONG und   =BE32(g_sc->underrun_count);
            ULONG fps5  =fc-frames_prev; frames_prev=fc;
            LONG  delta =(LONG)target_frames-(LONG)frames_sent;
            printf("t=%lus | frames=%lu target=%lu delta=%ld fps5s=%lu/5s\n",
                   elapsed_s,frames_sent,target_frames,delta,fps5);
            printf("       ps_av=%lu mp3_av=%lu pcm_av=%lu und=%lu\n",
                   ps_av,mp3_av,pcm_av,und);
            printf("       ahi_s=%lu ahi_d=%lu eof_v=%ld eof_a=%ld\n",
                   g_ahi_sent,g_ahi_done,
                   (LONG)g_feof_video,(LONG)g_feof_audio);
        }
    }

stop:
    /* Safe cleanup */
    {ULONG mp3_r4=BE32(g_sc->mp3_read);
     ULONG mp3_av=ring_avail(g_awptr,mp3_r4,MP3_RING_SIZE);
     ULONG pcm_w5=BE32(g_sc->pcm_write);
     ULONG pcm_av=ring_avail(pcm_w5,g_prd,PCM_RING_SIZE);
     printf("final audio: mp3_av=%lu pcm_av=%lu ahi_s=%lu ahi_d=%lu\n",
            mp3_av,pcm_av,g_ahi_sent,g_ahi_done);
    }
    ahi_stop_all();
    ZZ_WR(board_arg, REG_CMD, OP_STREAM_CLOSE);
    Delay(50);
    wr32(shared_arg, SH_CMD,  MPEG_CMD_STOP);
    wr32(shared_arg, SH_STOP, 1UL);
    {LONG t=250; while(t-->0){
        if((rd32(shared_arg,SH_STATUS)&0xFF)==0xFF) break;
        Delay(1);
    }}
    {ULONG fst=rd32(shared_arg,SH_STATUS)&0xFF;
     ULONG ns=0,nu=0; CurrentTime(&ns,&nu);
     printf("done. frames=%lu elapsed=%lus stop=0x%02lx\n",
            frames_sent, ns-start_secs, fst);
     if(fst==0xFF){
         Delay(50);
         if(ded_scr_local){CloseScreen(ded_scr_local); ded_scr_local=NULL;}
     } else {
         printf("WARNING: no STOP ACK - screen NOT closed\n");
         ded_scr_local=NULL;
     }
    }
    return quit_req ? 1 : 0;
}

/* Windowed playback.
 *
 * Core1 renders into the Workbench bitmap at the window position.
 */
static int play_windowed(const char *filepath)
{
    struct Screen *pub_scr=NULL;
    struct Window *window=NULL;
    volatile UBYTE *board_l=NULL, *fb_l=NULL;
    int ret=0;

    struct ConfigDev *cd_l=NULL;
    {struct ExpansionBase *eb=(struct ExpansionBase*)OpenLibrary("expansion.library",37L);
     if(!eb) return 0;
     cd_l=FindConfigDev(NULL,ZZ9000_MANUF,ZZ9000_PROD_AX);
     if(!cd_l) cd_l=FindConfigDev(NULL,ZZ9000_MANUF,ZZ9000_PROD);
     CloseLibrary((struct Library*)eb);
     if(!cd_l){printf("ZZ9000 not found\n");return 0;}
    }
    board_l=(volatile UBYTE*)cd_l->cd_BoardAddr;
    fb_l   =board_l+MNT_FB_BASE;

    if(!init_ahi()){printf("no AHI\n");return 0;}

    g_file_video=Open((CONST_STRPTR)filepath,MODE_OLDFILE);
    if(!g_file_video){cleanup_ahi();return 0;}
    g_file_audio=Open((CONST_STRPTR)filepath,MODE_OLDFILE);
    if(!g_file_audio){Close(g_file_video);g_file_video=0;cleanup_ahi();return 0;}

    /* Init XACP globals */
    g_sc    =(XACP_StreamControl*)(fb_l+XACP_STREAM_OFFSET);
    g_mring =(UBYTE*)(fb_l+XACP_MP3_OFFSET);
    g_pring =(UBYTE*)(fb_l+XACP_PCM_OFFSET);
    memset((void*)g_sc,0,sizeof(XACP_StreamControl));
    g_sc->mp3_base=BE32(XACP_MP3_OFFSET); g_sc->mp3_size=BE32(MP3_RING_SIZE);
    g_sc->pcm_base=BE32(XACP_PCM_OFFSET); g_sc->pcm_size=BE32(PCM_RING_SIZE);
    g_awptr=0;g_prd=0;g_eofsent=FALSE;g_carry_len=0;
    g_feof_video=FALSE;g_feof_audio=FALSE;g_ps_wptr=0;
    g_audio_bytes_pushed=0;g_audio_packets_found=0;
    g_ahi_sent=0;g_ahi_done=0;

    /* Prebuffer video */
    printf("Prebuffering video...\n");
    {volatile UBYTE *ps_ring_l=fb_l+CORE1_PS_FB;
     while(g_ps_wptr<PS_RING_SIZE*3UL/4UL&&!g_feof_video){
         LONG got=Read(g_file_video,g_blk,(LONG)BLOCK_SIZE);
         if(got<=0){g_feof_video=TRUE;break;}
         ring_push_uword(ps_ring_l,PS_RING_SIZE,&g_ps_wptr,g_blk,(ULONG)got);
         if(got<(LONG)BLOCK_SIZE){g_feof_video=TRUE;}
     }
    }

    /* Prebuffer audio */
    printf("Prebuffering audio...\n");
    while(g_audio_bytes_pushed<(MP3_RING_SIZE*3UL/4UL)&&!g_feof_audio){
        LONG got=Read(g_file_audio,g_blk,(LONG)BLOCK_SIZE);
        if(got<=0){g_feof_audio=TRUE;break;}
        demux_push_audio(g_blk,(ULONG)got);
        if(got<(LONG)BLOCK_SIZE){g_feof_audio=TRUE;}
    }
    g_sc->mp3_write=BE32(g_awptr);
    printf("PS=%lu MP2=%lu pkt=%lu\n",g_ps_wptr,g_audio_bytes_pushed,g_audio_packets_found);

    /* Open window on Workbench */
    pub_scr=LockPubScreen("Workbench");
    if(!pub_scr) goto wquit;
    window=OpenWindowTags(NULL,
        WA_Left,       (pub_scr->Width -320)/2,
        WA_Top,        (pub_scr->Height-240)/2,
        WA_InnerWidth, 320, WA_InnerHeight, 240,
        WA_Title,      (ULONG)"ZZMpegPlayer",
        WA_Flags,      WFLG_CLOSEGADGET|WFLG_DRAGBAR|WFLG_DEPTHGADGET|WFLG_ACTIVATE,
        WA_IDCMP,      IDCMP_CLOSEWINDOW|IDCMP_RAWKEY,
        WA_PubScreen,  (ULONG)pub_scr,
        TAG_DONE);
    UnlockPubScreen(NULL,pub_scr); pub_scr=NULL;
    if(!window) goto wquit;

    /* arm_fb with window position offset */
    ULONG arm_fb_v=0, pitch=0;
    {struct Screen *s2=LockPubScreen("Workbench");
     if(s2){
         struct BitMap *bm=s2->RastPort.BitMap;
         ULONG bpr=p96GetBitMapAttr(bm,P96BMA_BYTESPERROW);
         ULONG mem=p96GetBitMapAttr(bm,P96BMA_MEMORY);
         UnlockPubScreen(NULL,s2);
         pitch=bpr?bpr:1280UL;
         ULONG wx=(ULONG)window->LeftEdge+(ULONG)window->BorderLeft;
         ULONG wy=(ULONG)window->TopEdge +(ULONG)window->BorderTop;
         arm_fb_v=CPU_TO_ARM(mem,fb_l)+wy*pitch+wx*4UL;
         printf("arm_fb=0x%08lx wx=%lu wy=%lu pitch=%lu\n",arm_fb_v,wx,wy,pitch);
     }
    }

    ret=do_play(window, NULL, board_l, fb_l, fb_l+CORE1_SHARED_FB,
                fb_l+CORE1_PS_FB, arm_fb_v, pitch);
    CloseWindow(window);

wquit:
    if(g_file_video){Close(g_file_video);g_file_video=0;}
    if(g_file_audio){Close(g_file_audio);g_file_audio=0;}
    cleanup_ahi();
    return ret;
}

/* Fullscreen playback.
 *
 * Core1 renders into a dedicated Picasso96 320x240x32 screen.
 */
static int play_fullscreen(const char *filepath)
{
    struct Screen *ded_scr=NULL;
    struct Window *ded_win=NULL;
    volatile UBYTE *board_l=NULL, *fb_l=NULL;
    int ret=0;

    struct ConfigDev *cd_l=NULL;
    {struct ExpansionBase *eb=(struct ExpansionBase*)OpenLibrary("expansion.library",37L);
     if(!eb) return 0;
     cd_l=FindConfigDev(NULL,ZZ9000_MANUF,ZZ9000_PROD_AX);
     if(!cd_l) cd_l=FindConfigDev(NULL,ZZ9000_MANUF,ZZ9000_PROD);
     CloseLibrary((struct Library*)eb);
     if(!cd_l){printf("ZZ9000 not found\n");return 0;}
    }
    board_l=(volatile UBYTE*)cd_l->cd_BoardAddr;
    fb_l   =board_l+MNT_FB_BASE;

    if(!init_ahi()){printf("no AHI\n");return 0;}

    g_file_video=Open((CONST_STRPTR)filepath,MODE_OLDFILE);
    if(!g_file_video){cleanup_ahi();return 0;}
    g_file_audio=Open((CONST_STRPTR)filepath,MODE_OLDFILE);
    if(!g_file_audio){Close(g_file_video);g_file_video=0;cleanup_ahi();return 0;}

    /* Init XACP globals */
    g_sc    =(XACP_StreamControl*)(fb_l+XACP_STREAM_OFFSET);
    g_mring =(UBYTE*)(fb_l+XACP_MP3_OFFSET);
    g_pring =(UBYTE*)(fb_l+XACP_PCM_OFFSET);
    memset((void*)g_sc,0,sizeof(XACP_StreamControl));
    g_sc->mp3_base=BE32(XACP_MP3_OFFSET); g_sc->mp3_size=BE32(MP3_RING_SIZE);
    g_sc->pcm_base=BE32(XACP_PCM_OFFSET); g_sc->pcm_size=BE32(PCM_RING_SIZE);
    g_awptr=0;g_prd=0;g_eofsent=FALSE;g_carry_len=0;
    g_feof_video=FALSE;g_feof_audio=FALSE;g_ps_wptr=0;
    g_audio_bytes_pushed=0;g_audio_packets_found=0;
    g_ahi_sent=0;g_ahi_done=0;

    /* Prebuffer video */
    printf("Prebuffering video...\n");
    {volatile UBYTE *ps_ring_l=fb_l+CORE1_PS_FB;
     while(g_ps_wptr<PS_RING_SIZE*3UL/4UL&&!g_feof_video){
         LONG got=Read(g_file_video,g_blk,(LONG)BLOCK_SIZE);
         if(got<=0){g_feof_video=TRUE;break;}
         ring_push_uword(ps_ring_l,PS_RING_SIZE,&g_ps_wptr,g_blk,(ULONG)got);
         if(got<(LONG)BLOCK_SIZE){g_feof_video=TRUE;}
     }
    }

    /* Prebuffer audio */
    printf("Prebuffering audio...\n");
    while(g_audio_bytes_pushed<(MP3_RING_SIZE*3UL/4UL)&&!g_feof_audio){
        LONG got=Read(g_file_audio,g_blk,(LONG)BLOCK_SIZE);
        if(got<=0){g_feof_audio=TRUE;break;}
        demux_push_audio(g_blk,(ULONG)got);
        if(got<(LONG)BLOCK_SIZE){g_feof_audio=TRUE;}
    }
    g_sc->mp3_write=BE32(g_awptr);
    printf("PS=%lu MP2=%lu pkt=%lu\n",g_ps_wptr,g_audio_bytes_pushed,g_audio_packets_found);

    /* Open dedicated P96 screen */
    {ULONG mode_id=p96BestModeIDTags(
         P96BIDTAG_NominalWidth,320,P96BIDTAG_NominalHeight,240,
         P96BIDTAG_Depth,32,TAG_DONE);
     if(mode_id==INVALID_ID)
         mode_id=p96BestModeIDTags(P96BIDTAG_Depth,32,TAG_DONE);
     if(mode_id==INVALID_ID){printf("No P96 mode\n");goto fsquit;}
     ded_scr=(struct Screen*)OpenScreenTags(NULL,
         SA_Width,320,SA_Height,240,SA_Depth,32,
         SA_DisplayID,mode_id,SA_Quiet,TRUE,
         SA_ShowTitle,FALSE,SA_Behind,FALSE,TAG_DONE);
     if(!ded_scr){printf("OpenScreen failed\n");goto fsquit;}
    }

    /* Invisible IDCMP window on dedicated screen */
    ded_win=OpenWindowTags(NULL,
        WA_CustomScreen,(ULONG)ded_scr,
        WA_Left,0,WA_Top,0,WA_Width,320,WA_Height,240,
        WA_Borderless,TRUE,WA_Activate,TRUE,
        WA_IDCMP,IDCMP_RAWKEY|IDCMP_CLOSEWINDOW,
        TAG_DONE);
    if(!ded_win){printf("OpenWindow failed\n");goto fsquit;}

    /* arm_fb: no window offset for fullscreen */
    ULONG arm_fb_v=0, pitch=0;
    {struct BitMap *bm=ded_scr->RastPort.BitMap;
     ULONG mem=p96GetBitMapAttr(bm,P96BMA_MEMORY);
     ULONG bpr=p96GetBitMapAttr(bm,P96BMA_BYTESPERROW);
     if(!mem){printf("mem=0\n");goto fsquit;}
     pitch=bpr?bpr:1280UL;
     arm_fb_v=CPU_TO_ARM(mem,fb_l);
     printf("arm_fb=0x%08lx bpr=%lu\n",arm_fb_v,pitch);
    }

    ret=do_play(ded_win, ded_scr, board_l, fb_l, fb_l+CORE1_SHARED_FB,
                fb_l+CORE1_PS_FB, arm_fb_v, pitch);
    /* do_play closes ded_scr on clean STOP ACK */
    ded_scr=NULL;

fsquit:
    if(ded_win){CloseWindow(ded_win);ded_win=NULL;}
    if(ded_scr){CloseScreen(ded_scr);ded_scr=NULL;}
    if(g_file_video){Close(g_file_video);g_file_video=0;}
    if(g_file_audio){Close(g_file_audio);g_file_audio=0;}
    cleanup_ahi();
    return ret;
}

/* ================================================================
 * GadTools main window
 *
 * Buttons: Open... | Window | Full Screen
 * ================================================================ */
int main(int argc, char *argv[])
{
    struct Screen  *pub_scr  = NULL;
    APTR            vi       = NULL;
    struct Gadget  *glist    = NULL;
    struct Gadget  *g_open=NULL, *g_win_btn=NULL, *g_full_btn=NULL;
    struct Window  *main_win = NULL;
    char filepath[256];
    BOOL file_ok = FALSE;

    SetTaskPri(FindTask(NULL), 20);

    GadToolsBase = OpenLibrary("gadtools.library", 37L);
    AslBase      = OpenLibrary("asl.library",      38L);
    P96Base      = OpenLibrary("Picasso96API.library", 0L);

    if(!GadToolsBase||!AslBase||!P96Base){
        printf("Missing libraries (gadtools/asl/P96)\n");
        goto quit;
    }

    printf("ZZMpegPlayer v1.0\n");
    printf("Supported: MPEG-1 PS, 320x240, 25fps CFR, MP2 44.1kHz stereo\n");
    printf("Known limitations: no seeking, badly muxed files may fail\n");

    filepath[0] = '\0';
    if(argc >= 2){
        strncpy(filepath, argv[1], 255);
        filepath[255] = '\0';
        file_ok = TRUE;
        printf("File: %s\n", filepath);
    }

    /* Main window */
    pub_scr = LockPubScreen(NULL);
    if(!pub_scr) goto quit;
    vi = GetVisualInfo(pub_scr, TAG_END);
    {
        struct NewGadget ng;
        struct Gadget *gad;
        WORD scr_w = pub_scr->Width;
        WORD scr_h = pub_scr->Height;
        UnlockPubScreen(NULL, pub_scr); pub_scr=NULL;

        gad = CreateContext(&glist);
        ng.ng_TextAttr   = NULL;
        ng.ng_VisualInfo = vi;
        ng.ng_Flags      = 0;
        ng.ng_TopEdge    = 12;
        ng.ng_Height     = 14;
        WORD x = 6;

#define MKBTN(txt,id,w) \
    ng.ng_LeftEdge=x; ng.ng_Width=(w); \
    ng.ng_GadgetText=(txt); ng.ng_GadgetID=(id); \
    gad=CreateGadget(BUTTON_KIND,gad,&ng,TAG_END); \
    if(gad&&(id)==1) g_open    =gad; \
    if(gad&&(id)==2) g_win_btn =gad; \
    if(gad&&(id)==3) g_full_btn=gad; \
    x+=(w)+4;

        MKBTN("Open...",     1, 70)
        MKBTN("Window",      2, 60)
        MKBTN("Full Screen", 3, 84)

        main_win = OpenWindowTags(NULL,
            WA_Left,   (scr_w - 248) / 2,
            WA_Top,    (scr_h -  46) / 2,
            WA_Width,  248,
            WA_Height,  46,
            WA_Title,  (ULONG)"ZZMpegPlayer v1",
            WA_Flags,  WFLG_CLOSEGADGET|WFLG_DRAGBAR|WFLG_ACTIVATE,
            WA_IDCMP,  IDCMP_CLOSEWINDOW|IDCMP_GADGETUP|BUTTONIDCMP,
            WA_Gadgets,(ULONG)glist,
            TAG_DONE);
        if(!main_win) goto quit;
        GT_RefreshWindow(main_win, NULL);

        if(!file_ok){
            GT_SetGadgetAttrs(g_win_btn,  main_win, NULL, GA_Disabled, TRUE, TAG_DONE);
            GT_SetGadgetAttrs(g_full_btn, main_win, NULL, GA_Disabled, TRUE, TAG_DONE);
        }
    }

    /* GUI event loop */
    while(1){
        struct IntuiMessage *imsg;
        Wait(1L << main_win->UserPort->mp_SigBit);

        while((imsg=(struct IntuiMessage*)GT_GetIMsg(main_win->UserPort))!=NULL){
            ULONG cls = imsg->Class;
            struct Gadget *igad = (struct Gadget*)imsg->IAddress;
            GT_ReplyIMsg(imsg);

            if(cls==IDCMP_CLOSEWINDOW) goto quit;

            if(cls==IDCMP_GADGETUP){
                int res=0;
                switch(igad->GadgetID){
                case 1: /* Open... */
                    filepath[0]='\0';
                    if(select_file_asl(filepath,255)){
                        file_ok=TRUE;
                        printf("File: %s\n",filepath);
                        GT_SetGadgetAttrs(g_win_btn, main_win,NULL,GA_Disabled,FALSE,TAG_DONE);
                        GT_SetGadgetAttrs(g_full_btn,main_win,NULL,GA_Disabled,FALSE,TAG_DONE);
                    }
                    break;
                case 2: if(file_ok) res=play_windowed(filepath);  break;
                case 3: if(file_ok) res=play_fullscreen(filepath); break;
                }
                if(res==1) goto quit;
            }
        }
    }

quit:
    if(main_win)     CloseWindow(main_win);
    if(glist)        FreeGadgets(glist);
    if(vi)           FreeVisualInfo(vi);
    if(pub_scr)      UnlockPubScreen(NULL,pub_scr);
    if(AslBase)      CloseLibrary(AslBase);
    if(GadToolsBase) CloseLibrary(GadToolsBase);
    if(P96Base)      CloseLibrary(P96Base);
    return 0;
}
