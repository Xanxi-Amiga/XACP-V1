/*
 * ZZDoomCAMD.c - AmigaOS launcher and host-side integration for ZZDoom.
 *
 * Handles Core1 startup, WAD upload, Picasso96 display, input,
 * AHI sound effects, CAMD MIDI music and savegame transfers.
 *
 * ASCII only.
 */

#include <proto/Picasso96.h>
#include <exec/types.h>
#include <libraries/configvars.h>
#include <intuition/intuition.h>
#include <intuition/pointerclass.h>
#include <intuition/screens.h>
#include <graphics/videocontrol.h>
#include <graphics/gfx.h>
#include <graphics/displayinfo.h>
#include <proto/exec.h>
#include <proto/expansion.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <devices/timer.h>
#include <proto/timer.h>
#include <exec/io.h>
#include <exec/ports.h>
#include <stdio.h>

struct Library *P96Base      = NULL;
struct Device  *TimerBase    = NULL;   /* definition (proto/timer.h declares extern) */
struct ExpansionBase *ExpansionBase = NULL;

/* Validated constants - DO NOT MODIFY */
#define ZZ9000_MANUF   0x6D6E
#define ZZ9000_PROD    0x04
#define MNT_FB_BASE    0x00010000UL
#define REG_ARM_RUN_HI 0x0090
#define REG_ARM_RUN_LO 0x0092
#define ZZ_WR(b,o,v)   (*((volatile UWORD*)((UBYTE*)(b)+(o))) = (UWORD)(v))
#define ZZ_RD(b,o)     (*((volatile UWORD*)((UBYTE*)(b)+(o))))

/* ZZ9000 direct page flip registers */
#define REG_ZZ_PAN_HI        0x0A
#define REG_ZZ_PAN_LO        0x0C
#define REG_ZZ_X1            0x10
#define REG_ZZ_Y1            0x12
#define REG_ZZ_X2            0x14
#define REG_ZZ_COLORMODE     0x30
#define REG_ZZ_VBLANK_STATUS 0x4C
#define ZZ_COLORMODE_32      2
#define REG_ZZ_USER3         0x44  /* write: 1=enable deferred PAN, 0=disable */
#define REG_ZZ_USER4         0x46  /* read:  deferred PAN ack counter (upper 16 bits) */

/* Write deferred PAN request - firmware applies at next vblank rising edge */
static void zz_deferred_pan_request(volatile UBYTE *board,
                                     ULONG byte_offset, ULONG pitch_bytes)
{
    ZZ_WR(board, REG_ZZ_X1, 0);
    ZZ_WR(board, REG_ZZ_Y1, 0);
    ZZ_WR(board, REG_ZZ_X2, (UWORD)(pitch_bytes >> 2));
    ZZ_WR(board, REG_ZZ_COLORMODE, ZZ_COLORMODE_32);
    ZZ_WR(board, REG_ZZ_PAN_HI, (UWORD)(byte_offset >> 16));
    ZZ_WR(board, REG_ZZ_PAN_LO, (UWORD)(byte_offset & 0xFFFF));
    /* Firmware intercepts PAN_LO and defers to vblank */
}

/* Read ack counter from firmware (upper 16 bits of USER4) */
static ULONG zz_pan_ack(volatile UBYTE *board)
{
    return (ULONG)(ZZ_RD(board, REG_ZZ_USER4) & 0xFFFF);
}

#define CORE1_CODE_FB   0x04000000UL   /* blob at fb+0x04000000 = ARM 0x04200000 */
#define CORE1_SHARED_FB 0x04100000UL   /* shared at fb+0x04100000 = ARM 0x04300000 */
#define CORE1_ARM_ADDR  0x04200000UL
#define CORE1_WAD_FB    0x04300000UL   /* WAD at fb+0x04300000 = ARM 0x04500000 */
#define CORE1_WAD_ARM   0x04500000UL   /* ARM address of WAD */
#define CORE1_MAGIC     0xDEAD0004UL

#define MAX_WAD_SIZE    (16UL*1024UL*1024UL)

/* Official Julia shared table - byte offsets (idx*4) */
#define SH_MAGIC      0    /* [0]  */
#define SH_STATUS     4    /* [1]  */
#define SH_HEARTBEAT  8    /* [2]  */
#define SH_STOP      12    /* [3] COMMAND */
#define SH_FRAME     16    /* [4]  */
#define SH_DIAG      20    /* [5]  */
#define SH_ERROR     24    /* [6]  */
#define SH_ERR4      28    /* [7]  */
#define SH_WAD_ADDR  32    /* [8]  */
#define SH_WAD_SIZE  36    /* [9]  */
#define SH_WAD_TYPE  120   /* [30] WAD type: 0=Doom1 1=Doom2 */
#define SH_FB_ADDR   40    /* [10] */
#define SH_FB_PITCH  44    /* [11] */
#define SH_FB_WIDTH  48    /* [12] */
#define SH_FB_HEIGHT 52    /* [13] */
#define SH_INPUT     56    /* [14] = input buttons bitfield */
#define SH_MOUSE_DX  60    /* [15] = accumulated mouse X delta (signed) */
#define MOUSE_TURN_THRESH 2   /* mouse counts to trigger one turn-tic (like arrow key) */
#define SH_MOUSE_BTN 64    /* [16] = mouse buttons: 1=left 2=right */
#define SH_TIME_MS   68    /* [17] = elapsed milliseconds (from 68k) */
#define SH_TIC35     72    /* [18] = elapsed tics at 35Hz */
#define SH_FRAME_READY 80  /* [20] = ARM: frame rendered into back buffer */
#define SH_FLIP_SEQ    84  /* [21] = 68k: completed flip seq */
/* Music: slots 41-45 (bytes 164-180), free */
#define SH_MUSIC_CMD    (41*4)
#define SH_MUSIC_SEQ    (42*4)
#define SH_MUSIC_LOOP   (43*4)
#define SH_MUSIC_NAME0  (44*4)
#define SH_MUSIC_NAME1  (45*4)
#define MUSIC_CMD_PLAY  1
#define MUSIC_CMD_STOP  2
/* Save/Load protocol - slots 70-77 */
#define SH_SAVE_CMD       (70*4)
#define SH_SAVE_SEQ       (71*4)
#define SH_SAVE_ACK       (72*4)
#define SH_SAVE_STATUS    (73*4)
#define SH_SAVE_SLOT      (74*4)
#define SH_SAVE_SIZE      (75*4)
#define SH_SAVE_ADDR_ARM  (76*4)
#define SH_SAVE_ERR       (77*4)
#define SAVE_CMD_NONE     0
#define SAVE_CMD_WRITE    1
#define SAVE_CMD_READ     2
#define SAVE_STATUS_IDLE  0
#define SAVE_STATUS_BUSY  1
#define SAVE_STATUS_OK    2
#define SAVE_STATUS_ERR   3
#define CORE1_SAVE_FB     0x07C00000UL  /* CPU: fb+0x07C00000 */
#define CORE1_SAVE_ARM    0x07E00000UL  /* ARM address - high zone, safe from WAD up to 96MB */
#define CORE1_SAVE_SIZE   (128UL*1024UL)   /* per slot, 6 slots */
#define CORE1_SAVE_SLOT_OFFSET(s) ((s)*0x20000UL)  /* 128KB per slot */
/* s_sound Core1 debug counters slots 50-55 */
#define SH_DBG_S_STOP     (50*4)
#define SH_DBG_S_CHANGE   (51*4)
#define SH_DBG_S_BADNUM   (52*4)
#define SH_DBG_S_BADNAME  (53*4)
#define SH_DBG_S_PLAYSENT (54*4)
#define SH_DBG_S_SAMEMUS  (55*4)

/* Input button bits (must match Core1 julia_doom_core1.c) */
#define BTN_UP      0x0001
#define BTN_DOWN    0x0002
#define BTN_LEFT    0x0004
#define BTN_RIGHT   0x0008
#define BTN_SL      0x0010
#define BTN_SR      0x0020
#define BTN_FIRE    0x0040
#define BTN_USE     0x0080
#define BTN_RUN     0x0100
#define BTN_ESC     0x0200
#define BTN_ENTER   0x0400
#define BTN_Y       0x0800
#define BTN_STRAFE_MOD 0x1000
#define BTN_MAP     0x2000
#define BTN_W1      0x00010000
#define BTN_W2      0x00020000
#define BTN_W3      0x00040000
#define BTN_W4      0x00080000
#define BTN_W5      0x00100000
#define BTN_W6      0x00200000
#define BTN_W7      0x00400000
#define BTN_F1      0x00800000
#define BTN_F2      0x01000000
#define BTN_F3      0x02000000
#define BTN_F4      0x04000000
#define BTN_F5      0x08000000
#define BTN_F6      0x10000000
#define BTN_F7      0x20000000
#define BTN_F8      0x40000000
#define BTN_F9      0x80000000
/* F10 via SH_INPUT2 slot 15 */
#define SH_INPUT2   60           /* [15] = F10 + extra keys */
#define BTN2_F10    0x00000001
#define BTN2_N      0x00000002   /* N = no (quit confirm) */

/* Amiga raw key codes */
#define RK_UP       0x4C
#define RK_DOWN     0x4D
#define RK_RIGHT    0x4E
#define RK_LEFT     0x4F
#define RK_LSHIFT   0x60
#define RK_RSHIFT   0x61
#define RK_LCTRL    0x63
#define RK_SPACE    0x40
#define RK_ESC      0x45
#define RK_ENTER    0x44
#define RK_RETURN   0x43
#define RK_A        0x20
#define RK_Z        0x31
#define RK_X        0x32
#define RK_Y_KEY    0x15
#define RK_COMMA    0x38
#define RK_PERIOD   0x39
#define RK_LALT     0x64
#define RK_RALT     0x65
#define RK_TAB      0x42
#define RK_F1       0x50
#define RK_F2       0x51
#define RK_F3       0x52
#define RK_F4       0x53
#define RK_F5       0x54
#define RK_F6       0x55
#define RK_F7       0x56
#define RK_F8       0x57
#define RK_F9       0x58
#define RK_F10      0x59
#define RK_1        0x01
#define RK_2        0x02
#define RK_3        0x03
#define RK_4        0x04
#define RK_5        0x05
#define RK_6        0x06
#define RK_7        0x07

/* CPU_TO_ARM: validated formula from ZZFractalDemo */
#define CPU_TO_ARM(fb_cpu, fb_base) \
    (0x00200000UL + ((ULONG)(fb_cpu) - (ULONG)(fb_base)))

/* rd32/wr32 - identical to ZZFractalDemo */
static ULONG rd32(volatile UBYTE *b, ULONG o)
{
    volatile UWORD *p = (volatile UWORD *)(b + o);
    UWORD w0=p[0], w1=p[1];
    return ((ULONG)((UWORD)((w1>>8)|(w1<<8)))<<16)|
            (ULONG)((UWORD)((w0>>8)|(w0<<8)));
}

static void wr32(volatile UBYTE *b, ULONG o, ULONG v)
{
    volatile UWORD *p = (volatile UWORD *)(b + o);
    ULONG s=((v&0xFFUL)<<24)|((v&0xFF00UL)<<8)|
            ((v&0xFF0000UL)>>8)|((v&0xFF000000UL)>>24);
    p[0]=(UWORD)(s>>16); p[1]=(UWORD)(s&0xFFFF);
}

static void copy_uword(volatile UBYTE *d, const UBYTE *s, LONG sz)
{
    volatile UWORD *dw=(volatile UWORD*)d;
    const UWORD *sw=(const UWORD*)s;
    LONG i; for(i=0;i<(sz+1)/2;i++) dw[i]=sw[i];
}

/* AHI streaming player (uses rd32/wr32 defined above) */
#include "zz_ahi.c"
/* Optional embedded blob - defined in wrapper (ZZDoom320.c / ZZDoom640.c) */
extern const unsigned char zzdoom_blob[] __attribute__((weak));
extern const unsigned int  zzdoom_blob_size __attribute__((weak));
#include "zz_camd.h"
#include "zz_mus.h"

void zz_bus_copy_lump(const UBYTE *wad_base, ULONG fpos, UBYTE *dst, ULONG len);
extern char g_lump_name[9];
extern ULONG ahi_refill_count;

#define ZZ_BUILD_ID "CAMD_RESTART_DBG_01"

static void musdbg_append(const char *tag, const char *name,
    ULONG seq, ULONG cmd, ULONG wad_sz,
    ULONG fpos, ULONG size, ULONG sig)
{
    BPTR fh = Open("RAM:MUSDBG.txt", MODE_READWRITE);
    if(fh){ Seek(fh,0,OFFSET_END);
        FPrintf(fh, "%s seq=%lu cmd=%lu name='%.8s' wad_size=%lu fpos=%08lx size=%lu sig=%08lx\n",
            (ULONG)tag, seq, cmd, name?(ULONG)name:(ULONG)"", wad_sz, fpos, size, sig);
        Close(fh);
    }
}

int zzdoom_main(int argc, char **argv)
{
    struct ConfigDev     *cd;
    volatile UBYTE       *board, *fb, *shared;
    struct Screen        *wb_scr=NULL;
    struct Screen        *my_scr=NULL;
    struct ScreenBuffer  *sb[2]={NULL,NULL};
    struct BitMap        *man_bm[2]={NULL,NULL};
    int                   front_idx=0, back_idx=1;
    ULONG                 buf_arm[2]={0,0};
    ULONG                 buf_off[2]={0,0};
    ULONG                 buf_mem[2]={0,0};
    ULONG                 dbuf_sig=0;
    struct MsgPort       *dbuf_port=NULL;
    struct Window        *win=NULL;
    APTR                 g_MausObj=NULL;   /* invisible pointer object */
    struct BitMap        *g_MausBM=NULL;   /* empty bitmap for pointer */
    struct IntuiMessage  *msg=NULL;
    struct IntuitionBase *IntuitionBase=NULL;
    struct Library       *GfxBase_=NULL;
    BPTR    fh;
    LONG    size;
    LONG    wad_size=0;
    UBYTE  *buf;
    ULONG   last_hb=0xDEADBEEFUL;
    ULONG   g_buttons=0;
    ULONG   g_buttons2=0;
    LONG    g_mouse_dx=0;
    LONG    g_mturn_acc=0;    /* mouse horizontal accumulator -> arrow turn */
    int     g_mturn_bit=0;    /* current mouse-driven turn bit (BTN_LEFT/RIGHT) */
    ULONG   g_mouse_vkeys=0;  /* virtual keys contributed by mouse (fire/strafe/turn) */
    ULONG   g_input_last=0xFFFFFFFFUL; /* last SH_INPUT written (dedup) */
    ULONG   g_mouse_btn=0;
    char    title[64];
    int     rc=0;
    ULONG   arm_fb=0, fb_w=0, fb_h=0, fb_pitch=0;
    struct timerequest *tr=NULL;
    struct MsgPort *tport=NULL;
    struct EClockVal ecv_start, ecv_now;
    ULONG   eclock_freq=0;
    UBYTE  *mus_copy_ptr=NULL;
    ULONG   mus_copy_sz=0;
    ULONG   mus_last_seq=0;
    int     mus_stop_pending=0;
    ULONG   mus_stop_deadline_ms=0;
    ULONG   save_last_seq=0;
#define MUS_STOP_GRACE_MS 300UL
    ULONG   dbg_mus_sig=0;
    ULONG   dbg_mus_size=0;
    ULONG   dbg_mus_fpos=0;
    int     use_camd=0, camd_test=0, camd_debug=0;
    const char *camd_port=NULL;
    int     _ci;
    int     use_320=0;   /* -320 option */
    int     _oi;

    printf("ZZDoomDemo - Doom on ZZ9000 Core1\n");
    if(argc<3){printf("Usage: ZZDoomDemo DOOM1.WAD julia_doom.bin\n");return 5;}
    { int _oistart=(zzdoom_blob&&zzdoom_blob_size>0)?2:3;
    for(_oi=_oistart;_oi<argc;_oi++){
        if(!strcmp(argv[_oi],"-320"))     use_320=1;
        if(!strcmp(argv[_oi],"-camd"))     use_camd=1;
        if(!strcmp(argv[_oi],"-nomusic")) use_camd=0;
        if(!strcmp(argv[_oi],"-camdtest")) camd_test=1;
        if(!strcmp(argv[_oi],"-camddebug")) camd_debug=1;
        if(!strcmp(argv[_oi],"-camdport") && _oi+1<argc){ camd_port=argv[++_oi]; }
    } }
    printf("Mode: %s\n", use_320?"320x240":"640x480");

    /* Open timer.device for high-res elapsed time (EClock) */
    tport=(struct MsgPort*)CreateMsgPort();
    if(tport){
        tr=(struct timerequest*)CreateIORequest(tport,sizeof(struct timerequest));
        if(tr){
            if(OpenDevice("timer.device",UNIT_ECLOCK,(struct IORequest*)tr,0)==0){
                TimerBase=(struct Device*)tr->tr_node.io_Device;
            }
        }
    }

    /* Open ZZ9000 - identical to ZZFractalDemo */
    ExpansionBase=(struct ExpansionBase*)OpenLibrary("expansion.library",37);
    if(!ExpansionBase) return 10;
    cd=FindConfigDev(NULL,ZZ9000_MANUF,ZZ9000_PROD);
    if(!cd) cd=FindConfigDev(NULL,ZZ9000_MANUF,0x0A);
    CloseLibrary((struct Library*)ExpansionBase);
    if(!cd){printf("ZZ9000 not found\n");return 10;}

    board=(volatile UBYTE*)cd->cd_BoardAddr;
    fb   =board+MNT_FB_BASE;
    shared=fb+CORE1_SHARED_FB;

    IntuitionBase=OpenLibrary("intuition.library",39L);
    GfxBase_     =OpenLibrary("graphics.library", 39L);
    P96Base      =OpenLibrary("Picasso96API.library",2L);
    if(!IntuitionBase||!GfxBase_){printf("libs missing\n");rc=20;goto cleanup;}
    if(!P96Base){printf("Picasso96API.library missing\n");rc=15;goto cleanup;}

    /* Load WAD into DDR at fb+CORE1_WAD_FB */
    printf("Loading %s...\n", argv[1]);
    fh=Open(argv[1],MODE_OLDFILE);
    if(!fh){printf("Cannot open WAD\n");rc=10;goto cleanup;}
    Seek(fh,0,OFFSET_END); size=(LONG)Seek(fh,0,OFFSET_END); wad_size=size;
    Seek(fh,0,OFFSET_BEGINNING);
    if(size<=0||(ULONG)size>MAX_WAD_SIZE){
        printf("WAD size %ld invalid\n",size);Close(fh);rc=10;goto cleanup;}
    printf("WAD: %ld bytes -> fb+0x%08lX (ARM 0x%08lX)\n",
           size,CORE1_WAD_FB,CORE1_WAD_ARM);
    {
        UBYTE *chunk=(UBYTE*)AllocMem(65536L,MEMF_PUBLIC);
        if(!chunk){printf("AllocMem failed\n");Close(fh);rc=10;goto cleanup;}
        ULONG offset=0; LONG got;
        while((got=Read(fh,chunk,65536L))>0){
            copy_uword(fb+CORE1_WAD_FB+offset,chunk,(LONG)got);
            offset+=(ULONG)got;
        }
        FreeMem(chunk,65536L); Close(fh); fh=0;
        printf("WAD loaded OK (%lu bytes)\n",offset);
    }

    /* Open Workbench window - identical to ZZFractalDemo */
    {
        ULONG modeid = p96BestModeIDTags(
            P96BIDTAG_NominalWidth, (ULONG)(use_320?320UL:640UL),
            P96BIDTAG_NominalHeight,(ULONG)(use_320?240UL:480UL),
            P96BIDTAG_Depth,        32,
            TAG_DONE);
        if(modeid==(ULONG)INVALID_ID){
            printf("No RTG mode\n"); rc=15; goto cleanup;
        }
        /* Method A: strict (historical, most configs) */
        my_scr=OpenScreenTags(NULL,
            SA_DisplayID,  modeid,
            SA_Width,      (ULONG)(use_320?320UL:640UL),
            SA_Height,     (ULONG)(use_320?240UL:480UL),
            SA_Depth,      32,
            SA_Quiet,      TRUE,
            SA_ShowTitle,  FALSE,
            SA_Type,       CUSTOMSCREEN,
            TAG_DONE);
        /* Method C: permissive (recent P96/iComp) */
        if(!my_scr){
            my_scr=OpenScreenTags(NULL,
                SA_DisplayID, modeid,
                SA_Type,      CUSTOMSCREEN,
                SA_Quiet,     TRUE,
                TAG_DONE);
        }
        /* Method D: minimal */
        if(!my_scr){
            my_scr=OpenScreenTags(NULL,
                SA_DisplayID, modeid,
                SA_Type,      CUSTOMSCREEN,
                TAG_DONE);
        }
        if(!my_scr){
            printf("OpenScreen failed. Run ZZP96Test to diagnose.\n");
            rc=15; goto cleanup;
        }
    }
    wb_scr = my_scr;
    win=(struct Window*)OpenWindowTags(NULL,
        WA_Left,   0,
        WA_Top,    0,
        WA_Width,  (ULONG)(use_320?320UL:640UL),
        WA_Height, (ULONG)(use_320?240UL:480UL),
        WA_Flags,  WFLG_BACKDROP|WFLG_BORDERLESS|WFLG_RMBTRAP|WFLG_ACTIVATE|WFLG_REPORTMOUSE,
        WA_IDCMP,  IDCMP_RAWKEY|IDCMP_MOUSEMOVE|IDCMP_MOUSEBUTTONS|IDCMP_DELTAMOVE,
        WA_CustomScreen,(ULONG)my_scr,
        TAG_DONE);
    UnlockPubScreen(NULL,wb_scr); wb_scr=NULL;
    if(!win){printf("OpenWindow failed\n");rc=15;goto cleanup;}

    /* Hide mouse pointer via empty pointerclass object (DoomAttack method).
       Works with WFLG_REPORTMOUSE on OS 3.0+. */
    g_MausBM=AllocBitMap(16,16,2,BMF_CLEAR,0);
    if(g_MausBM){
        g_MausObj=NewObject(NULL,"pointerclass",
            POINTERA_BitMap,(ULONG)g_MausBM,
            TAG_DONE);
        if(g_MausObj) SetWindowPointer(win, WA_Pointer,(ULONG)g_MausObj, TAG_DONE);
    }

    /* Allocate screen buffers - buf[0]=screen bitmap (always VRAM) */
    dbuf_port=(struct MsgPort*)CreateMsgPort();
    fb_w=use_320?320UL:640UL; fb_h=use_320?240UL:480UL;
    sb[0]=AllocScreenBuffer(my_scr,NULL,SB_SCREEN_BITMAP);
    sb[1]=AllocScreenBuffer(my_scr,NULL,0);
    if(!sb[0]){printf("AllocScreenBuffer sb[0] failed\n");rc=15;goto cleanup;}

    /* Check sb[1]: if not real VRAM (mem==0 or < fb), use p96AllocBitMap */
    {
        ULONG fb_base=(ULONG)(board+MNT_FB_BASE);
        ULONG mem1=sb[1]?p96GetBitMapAttr(sb[1]->sb_BitMap,P96BMA_MEMORY):0;
        if(!sb[1] || mem1==0 || mem1<fb_base){
            printf("sb[1] not VRAM (mem=0x%08lX), using p96AllocBitMap\n",mem1);
            if(sb[1]){ FreeScreenBuffer(my_scr,sb[1]); sb[1]=NULL; }
            { struct BitMap *friend=my_scr->RastPort.BitMap;
              ULONG rgbf=p96GetBitMapAttr(friend,P96BMA_RGBFORMAT);
              man_bm[1]=p96AllocBitMap(fb_w,fb_h,32,
                  BMF_DISPLAYABLE|BMF_CLEAR,friend,rgbf);
              if(!man_bm[1]){
                  printf("p96AllocBitMap failed. Run ZZP96Test.\n");
                  rc=15; goto cleanup;
              }
            }
        }
    }

    /* Compute ARM address per bitmap - validate VRAM, no Planes[0] */
    {
        ULONG fb_base=(ULONG)(board+MNT_FB_BASE);
        int i;
        for(i=0;i<2;i++){
            struct BitMap *bm=(i==0)?sb[0]->sb_BitMap:
                              (man_bm[1]?man_bm[1]:sb[1]->sb_BitMap);
            ULONG bpr=p96GetBitMapAttr(bm,P96BMA_BYTESPERROW);
            ULONG mem=p96GetBitMapAttr(bm,P96BMA_MEMORY);
            ULONG exp_bpr=fb_w*4UL;
            if(bpr==0) bpr=exp_bpr;
            fb_pitch=bpr;
            if(mem==0 || mem<fb_base){
                printf("buf[%d] not VRAM (mem=0x%08lX). Run ZZP96Test.\n",i,mem);
                rc=15; goto cleanup;
            }
            if(bpr<exp_bpr){
                printf("buf[%d] bpr=%lu < %lu\n",i,bpr,exp_bpr);
                rc=15; goto cleanup;
            }
            buf_mem[i]=mem;
            buf_arm[i]=CPU_TO_ARM(mem,fb);
            buf_off[i]=mem-fb_base;
            printf("buf[%d] mem=0x%08lX arm=0x%08lX off=0x%08lX bpr=%lu\n",
                   i,mem,buf_arm[i],buf_off[i],bpr);
        }
        front_idx=0; back_idx=1;
        arm_fb=buf_arm[back_idx];
        if(buf_mem[0]==buf_mem[1]){
            printf("ERROR: buffers not distinct. Run ZZP96Test.\n"); rc=15; goto cleanup; }
        printf("PAN off[0]=0x%08lX off[1]=0x%08lX pitch=%lu\n",
               buf_off[0],buf_off[1],fb_pitch);
    }
    /* Set up double-buffer signal: ChangeScreenBuffer signals when flip done */
    if(dbuf_port){
        dbuf_sig = 1UL << dbuf_port->mp_SigBit;
        if(sb[0]->sb_DBufInfo){ sb[0]->sb_DBufInfo->dbi_SafeMessage.mn_ReplyPort=dbuf_port; }
        if(sb[1]->sb_DBufInfo){ sb[1]->sb_DBufInfo->dbi_SafeMessage.mn_ReplyPort=dbuf_port; }
    }

    /* Clear shared */
    { ULONG i; volatile UWORD *p=(volatile UWORD*)shared;
      for(i=0;i<64;i++) p[i]=0; }

    /* Write FB and WAD params to shared BEFORE launch */
    wr32(shared,SH_FB_ADDR,  arm_fb);
    wr32(shared,SH_FB_WIDTH, fb_w);
    wr32(shared,SH_FB_HEIGHT,fb_h);
    wr32(shared,SH_FB_PITCH, fb_pitch);
    wr32(shared,SH_WAD_ADDR, CORE1_WAD_ARM);
    wr32(shared,SH_WAD_SIZE, (ULONG)size);
    /* Init save slots before Core1 starts */
    wr32(shared, SH_SAVE_CMD,    SAVE_CMD_NONE);
    wr32(shared, SH_SAVE_SEQ,    0);
    wr32(shared, SH_SAVE_ACK,    0);
    wr32(shared, SH_SAVE_STATUS, SAVE_STATUS_IDLE);
    wr32(shared, SH_SAVE_ERR,    0);
    save_last_seq = 0;
    /* Init music slots before Core1 starts */
    wr32(shared,SH_MUSIC_CMD, 0);
    wr32(shared,SH_MUSIC_SEQ, 0);
    wr32(shared,SH_MUSIC_LOOP, 0);
    wr32(shared,SH_MUSIC_NAME0, 0);
    wr32(shared,SH_MUSIC_NAME1, 0);
    /* Detect WAD type from filename */
    { const char *wn=argv[1]; const char *p;
      ULONG wtype=0;
      /* find basename */
      for(p=wn;*p;p++) if(*p=='/' || *p==':' || *p=='\\') wn=p+1;
      if(!strcasecmp(wn,"doom2.wad") || !strcasecmp(wn,"doom2"))
          wtype=1;
      wr32(shared,SH_WAD_TYPE,wtype);
      printf("WAD type: %s\n", wtype?"Doom II":"Doom I");
    }
    printf("FB=0x%08lX %lux%lu pitch=%lu\n",arm_fb,fb_w,fb_h,fb_pitch);
    printf("WAD ARM=0x%08lX size=%ld\n",CORE1_WAD_ARM,size);

    /* Point 6: Overlap checks */
    {
        ULONG blob_start  = (ULONG)fb + CORE1_CODE_FB;
        ULONG blob_end    = blob_start + 524288UL;
        ULONG shared_start= (ULONG)fb + CORE1_SHARED_FB;
        ULONG shared_end  = shared_start + 256UL;
        ULONG wad_start   = (ULONG)fb + CORE1_WAD_FB;
        ULONG wad_end     = wad_start + (ULONG)size;
        ULONG heap_start  = 0x05700000UL;
        ULONG heap_end    = heap_start + 8UL*1024UL*1024UL;

        printf("--- Layout check ---\n");
        printf("blob   fb+0x%08lX..0x%08lX\n",CORE1_CODE_FB,CORE1_CODE_FB+524288UL);
        printf("shared fb+0x%08lX..0x%08lX\n",CORE1_SHARED_FB,CORE1_SHARED_FB+256UL);
        printf("wad    fb+0x%08lX..0x%08lX\n",CORE1_WAD_FB,(ULONG)CORE1_WAD_FB+(ULONG)size);
        printf("heap   ARM 0x%08lX..0x%08lX\n",heap_start,heap_end);
        printf("arm_fb ARM 0x%08lX\n",arm_fb);

        /* blob vs shared */
        if(blob_end > shared_start && blob_start < shared_end)
            { printf("ERROR: blob overlaps shared\n"); rc=20; goto cleanup; }
        /* blob vs wad */
        if(blob_end > wad_start && blob_start < wad_end)
            { printf("ERROR: blob overlaps WAD\n"); rc=20; goto cleanup; }
        /* shared vs wad */
        if(shared_end > wad_start && shared_start < wad_end)
            { printf("ERROR: shared overlaps WAD\n"); rc=20; goto cleanup; }
        /* heap ARM vs WAD ARM */
        ULONG wad_arm_end = CORE1_WAD_ARM + (ULONG)size;
        if(heap_end > CORE1_WAD_ARM && heap_start < wad_arm_end)
            { printf("ERROR: heap overlaps WAD (ARM)\n"); rc=20; goto cleanup; }
        /* save buffer checks */
        { ULONG sv_arm  = CORE1_SAVE_ARM;
          ULONG sv_end  = sv_arm + CORE1_SAVE_SIZE;
          ULONG pcm_arm = 0x04400000UL;
          ULONG pcm_end = pcm_arm + 512UL*1024UL;
          printf("save   ARM 0x%08lX..0x%08lX\n", sv_arm, sv_end);
          /* WAD must not reach save buffer */
          if(wad_arm_end > sv_arm)
              { printf("ERROR: WAD (size=%lu) reaches save buffer at 0x%08lX\n",
                       (ULONG)size, sv_arm); rc=20; goto cleanup; }
          if(sv_end > heap_start && sv_arm < heap_end)
              { printf("ERROR: save overlaps heap\n"); rc=20; goto cleanup; }
          if(sv_end > pcm_arm && sv_arm < pcm_end)
              { printf("ERROR: save overlaps PCM\n"); rc=20; goto cleanup; }
          if(arm_fb && sv_end > arm_fb && sv_arm < arm_fb + fb_pitch*fb_h)
              { printf("ERROR: save overlaps framebuffer\n"); rc=20; goto cleanup; }
        }
        printf("Layout OK\n");
    }

    /* Load blob: from embedded data or file */
    if(zzdoom_blob && zzdoom_blob_size > 0){
        /* Embedded blob */
        buf=(UBYTE*)AllocMem(zzdoom_blob_size,MEMF_ANY|MEMF_CLEAR);
        if(!buf){rc=10;goto cleanup;}
        CopyMem((APTR)zzdoom_blob,buf,(LONG)zzdoom_blob_size);
        size=(LONG)zzdoom_blob_size;
        copy_uword(fb+CORE1_CODE_FB,buf,size);
        FreeMem(buf,zzdoom_blob_size);
        printf("Blob: %ld bytes (embedded)\n",size);
    } else {
        /* External blob file */
        fh=Open(argv[2],MODE_OLDFILE);
        if(!fh){printf("Cannot open blob %s\n",argv[2]);rc=10;goto cleanup;}
        buf=(UBYTE*)AllocMem(524288L,MEMF_ANY|MEMF_CLEAR);
        if(!buf){Close(fh);rc=10;goto cleanup;}
        size=Read(fh,buf,524288L); Close(fh); fh=0;
        copy_uword(fb+CORE1_CODE_FB,buf,size);
        FreeMem(buf,524288L);
        printf("Blob: %ld bytes\n",size);
    }

    /* Launch Core1 - identical to ZZFractalDemo */
    ZZ_WR(board,REG_ARM_RUN_HI,(UWORD)(CORE1_ARM_ADDR>>16));
    ZZ_WR(board,REG_ARM_RUN_LO,(UWORD)(CORE1_ARM_ADDR&0xFFFF));

    { LONG t=500;
      while(t-->0&&rd32(shared,SH_MAGIC)!=CORE1_MAGIC) Delay(1); }
    printf("Magic: 0x%08lX\n",rd32(shared,SH_MAGIC));
    if(rd32(shared,SH_MAGIC)!=CORE1_MAGIC){
        printf("Core1 not started\n");rc=10;goto cleanup;}

    printf("Core1 running. Close gadget or Ctrl-C to stop.\n");
    /* CAMD init */
    if(use_camd){
        if(zz_camd_init(camd_port)){
            DeleteFile("RAM:MUSDBG.txt");
            musdbg_append("BOOT " ZZ_BUILD_ID, "", 0, 0, (ULONG)wad_size, 0, 0, 0);
            printf("ZZDoomCAMD build %s\n", ZZ_BUILD_ID);
            if(camd_test) zz_camd_poll_test_start();
            else if(camd_debug){ zz_camd_note_on(0,48,80); zz_camd_note_off(0,48); }
        } else use_camd=0;
    }
    printf("Deferred PAN: arm-next mode (per request)\n");
    /* Diagnostic counters (per-sample window) */
    ULONG cnt_loops=0, cnt_wait_calls=0, cnt_wait_returns=0;
    ULONG cnt_ahi_sig=0, cnt_win_sig=0, cnt_flip_sig=0;
    ULONG cnt_idcmp_total=0, cnt_idcmp_move=0, cnt_idcmp_key=0, cnt_idcmp_other=0;
    ULONG cnt_ahi_refills=0, cnt_checkio_t=0, cnt_checkio_f=0;
    ULONG cnt_sample_timer=0;
    ULONG last_flipped=0xFFFFFFFFUL;
    ULONG flip_ack_before=0;
    ULONG flip_ready=0;
    int   flip_inflight=0;
    int   screen_was_front=1;     /* track screen front transitions */
    ULONG t_flip_req=0;           /* EClock tick when flip was requested */
    ULONG t_diag=0;               /* EClock tick for periodic debug */
    ULONG diag_ack=0, diag_inflight=0, diag_ready=0;
    { struct EClockVal ev; ReadEClock(&ev); t_diag=ev.ev_lo; }
    /* Timing metrics using EClock */
    ULONG eclock_freq_local = eclock_freq;  /* local copy */
    ULONG t_last_refill  = 0;   /* EClock ticks at last AHI refill */
    ULONG t_last_pump    = 0;   /* EClock ticks at last zz_ahi_poll */
    ULONG max_gap_refill = 0;   /* max ticks between refills */
    ULONG max_gap_pump   = 0;   /* max ticks between poll calls */
    ULONG t_run_start    = 0;   /* run start ticks */
    ULONG ahi_refill_total = 0; /* total AHI refills */
    ULONG ahi_under_total = 0;  /* AHI underruns */
    { struct EClockVal ev; ReadEClock(&ev); t_last_refill=ev.ev_lo;
      t_last_pump=ev.ev_lo; t_run_start=ev.ev_lo; }

    /* Init AHI streaming once ARM published PCM params (SH_PCM_ENABLED=1).
       Ring ARM 0x04400000 -> CPU = fb + 0x04200000. */
    {
        ULONG t=200; 
        while(t-->0 && rd32(shared,29*4)==0) Delay(1);  /* wait SH_PCM_ENABLED */
        if(rd32(shared,29*4)==1){
            volatile UBYTE *ring_cpu = fb + 0x04200000UL;
            if(zz_ahi_init(shared, ring_cpu)){
                zz_ahi_start();
                printf("AHI streaming started\n");
            } else {
                printf("AHI init failed - continuing without audio\n");
            }
        } else {
            printf("ARM did not enable audio\n");
        }
    }

    /* Take time reference for Doom timer */
    if(TimerBase){
        eclock_freq = ReadEClock(&ecv_start);
        wr32(shared, SH_TIME_MS, 0);
        wr32(shared, SH_TIC35, 0);
    }

    /* Main loop with DIAG display */
    {
        ULONG last_diag=0xFFFFFFFFUL;
        ULONG poll=0;
        wr32(shared, SH_MOUSE_BTN, 0);  /* neutralise native mouse path once */
        while(1){
            ULONG hb=rd32(shared,SH_HEARTBEAT);
            ULONG diag=rd32(shared,SH_DIAG);
            ULONG st=rd32(shared,SH_STATUS);


            /* Provide real time to Doom (EClock-based) */
            if(TimerBase){
                ULONG dms, hi, lo, ediff;
                ReadEClock(&ecv_now);
                /* elapsed eclock ticks (low 32 bits sufficient for our spans) */
                ediff = ecv_now.ev_lo - ecv_start.ev_lo;
                /* ms = ediff * 1000 / eclock_freq ; avoid overflow via 64-split */
                /* eclock_freq ~ 709379 (PAL) or ~715909 (NTSC) */
                dms = (ULONG)((unsigned long long)ediff * 1000ULL / (unsigned long long)eclock_freq);
                wr32(shared, SH_TIME_MS, dms);
                wr32(shared, SH_TIC35, (ULONG)((unsigned long long)dms * 35ULL / 1000ULL));
            }

            /* Track pump gap and refill gap */
            {
                struct EClockVal ev_now; ULONG now, gap;
                ReadEClock(&ev_now); now=ev_now.ev_lo;
                /* pump gap */
                gap = now - t_last_pump;
                if(gap > max_gap_pump) max_gap_pump = gap;
                t_last_pump = now;
                /* refill gap: check if new refill happened */
                { ULONG cur_refill=ahi_refill_count;
                  if(cur_refill != ahi_refill_total){
                      gap = now - t_last_refill;
                      if(gap > max_gap_refill) max_gap_refill=gap;
                      t_last_refill=now;
                      ahi_refill_total=cur_refill;
                  }
                }
                ahi_under_total=rd32(shared,SH_PCM_UNDERRUNS_O);
            }
            zz_ahi_poll();
            if(use_camd && eclock_freq>0){
                struct EClockVal _ev; ULONG _ediff, _ms;
                ReadEClock(&_ev);
                _ediff = _ev.ev_lo - ecv_start.ev_lo;
                _ms = (ULONG)((unsigned long long)_ediff * 1000ULL / (unsigned long long)eclock_freq);
                /* Music service */
                { ULONG _seq=rd32(shared,SH_MUSIC_SEQ);
                  if(_seq!=mus_last_seq){
                    mus_last_seq=_seq;
                    if(camd_debug){ zz_camd_note_on(0,50,80); zz_camd_note_off(0,50); }
                    if(rd32(shared,SH_MUSIC_CMD)==MUSIC_CMD_PLAY){
                      char _nm[9]; int _ui;
                      ULONG _n0=rd32(shared,SH_MUSIC_NAME0);
                      ULONG _n1=rd32(shared,SH_MUSIC_NAME1);
                      _nm[0]=(char)((_n0>>24)&0xFF);
                      _nm[1]=(char)((_n0>>16)&0xFF);
                      _nm[2]=(char)((_n0>> 8)&0xFF);
                      _nm[3]=(char)((_n0    )&0xFF);
                      _nm[4]=(char)((_n1>>24)&0xFF);
                      _nm[5]=(char)((_n1>>16)&0xFF);
                      _nm[6]=(char)((_n1>> 8)&0xFF);
                      _nm[7]=(char)((_n1    )&0xFF);
                      _nm[8]=0;
                      for(_ui=0;_ui<8;_ui++) if(_nm[_ui]>=(char)97&&_nm[_ui]<=(char)122) _nm[_ui]-=32;
                      { ULONG _loop=rd32(shared,SH_MUSIC_LOOP);
                        ULONG _lsz=0;
                        mus_stop_pending=0; /* PLAY cancels pending STOP */
                        const UBYTE *_wbase=(const UBYTE*)((ULONG)fb+CORE1_WAD_FB);
                        const UBYTE *_ld;
                        musdbg_append("PLAY_NAME",_nm,_seq,MUSIC_CMD_PLAY,(ULONG)wad_size,0,0,0);
                        /* Find and copy new lump BEFORE touching old music */
                        musdbg_append("FIND_ENTER",_nm,_seq,MUSIC_CMD_PLAY,(ULONG)wad_size,0xFFFFFFFFUL,0,0xDEAD0000UL);
                        _ld=zz_wad_find_lump(_wbase,(ULONG)wad_size,_nm,&_lsz);
                        if(_ld&&_lsz>0&&_lsz<256UL*1024UL){
                          musdbg_append("FIND_OK",_nm,_seq,MUSIC_CMD_PLAY,(ULONG)wad_size,(ULONG)(_ld-_wbase),_lsz,0);
                          { UBYTE *_newbuf=(UBYTE*)AllocMem(_lsz,MEMF_PUBLIC);
                            if(_newbuf){
                              ULONG _sig;
                              { int _ni; for(_ni=0;_ni<8;_ni++) g_lump_name[_ni]=_nm[_ni]; g_lump_name[8]=0; }
                              zz_bus_copy_lump(_wbase,(ULONG)(_ld-_wbase),_newbuf,_lsz);
                              dbg_mus_fpos=(ULONG)(_ld-_wbase); dbg_mus_size=_lsz;
                              _sig=(_lsz>=4)?
                                ((ULONG)_newbuf[0]<<24)|((ULONG)_newbuf[1]<<16)|
                                ((ULONG)_newbuf[2]<<8)|(ULONG)_newbuf[3] : 0;
                              dbg_mus_sig=_sig;
                              musdbg_append("COPY_OK",_nm,_seq,MUSIC_CMD_PLAY,(ULONG)wad_size,dbg_mus_fpos,_lsz,_sig);
                              /* Verify MUS signature before replacing old music */
                              if(_sig==0x4D55531AUL){
                                /* Valid MUS: stop old, swap buffers, start new */
                                zz_mus_stop();
                                if(mus_copy_ptr){FreeMem(mus_copy_ptr,mus_copy_sz);}
                                mus_copy_ptr=_newbuf; mus_copy_sz=_lsz;
                                if(camd_debug){ zz_camd_note_on(0,53,80); zz_camd_note_off(0,53); }
                                if(zz_mus_start(mus_copy_ptr,_lsz,(int)_loop)){
                                  musdbg_append("START_OK",_nm,_seq,MUSIC_CMD_PLAY,(ULONG)wad_size,dbg_mus_fpos,_lsz,_sig);
                                  if(camd_debug){ zz_camd_note_on(0,55,80); zz_camd_note_off(0,55); }
                                } else {
                                  musdbg_append("START_FAIL",_nm,_seq,MUSIC_CMD_PLAY,(ULONG)wad_size,dbg_mus_fpos,_lsz,_sig);
                                  if(camd_debug){ zz_camd_note_on(0,57,80); zz_camd_note_off(0,57); }
                                }
                              } else {
                                /* Bad MUS sig: discard new buffer, keep old music playing */
                                musdbg_append("SIG_FAIL",_nm,_seq,MUSIC_CMD_PLAY,(ULONG)wad_size,dbg_mus_fpos,_lsz,_sig);
                                FreeMem(_newbuf,_lsz);
                              }
                            }
                          }
                        } else {
                          musdbg_append("FIND_FAIL",_nm,_seq,MUSIC_CMD_PLAY,(ULONG)wad_size,0xFFFFFFFFUL,_lsz,0xDEAD0000UL);
                        }
                      }
                    } else if(rd32(shared,SH_MUSIC_CMD)==MUSIC_CMD_STOP){
                      mus_stop_pending=1;
                      mus_stop_deadline_ms=_ms+MUS_STOP_GRACE_MS;
                      musdbg_append("STOP_PENDING","",_seq,MUSIC_CMD_STOP,(ULONG)wad_size,0,0,0);
                    }
                  }
                }
                zz_camd_poll(_ms);
                zz_mus_poll(_ms);
                /* Deferred stop: execute if grace period expired */
                if(mus_stop_pending && (LONG)(_ms-mus_stop_deadline_ms)>=0){
                  musdbg_append("STOP_EXEC","",0,MUSIC_CMD_STOP,(ULONG)wad_size,0,0,0);
                  zz_mus_stop();
                  if(mus_copy_ptr){FreeMem(mus_copy_ptr,mus_copy_sz);mus_copy_ptr=NULL;mus_copy_sz=0;}
                  mus_stop_pending=0;
                }
            }
            /* === Save/Load service === */
            { ULONG _sv_seq=rd32(shared,SH_SAVE_SEQ);
              if(_sv_seq!=save_last_seq){
                save_last_seq=_sv_seq;
                { ULONG _cmd=rd32(shared,SH_SAVE_CMD);
                  ULONG _slot=rd32(shared,SH_SAVE_SLOT);
                  ULONG _sz=rd32(shared,SH_SAVE_SIZE);
                  char  _path[64];
                  volatile UBYTE *_ddr=(volatile UBYTE*)fb+CORE1_SAVE_FB+CORE1_SAVE_SLOT_OFFSET(_slot);
                  wr32(shared,SH_SAVE_STATUS,SAVE_STATUS_BUSY);
                  if(_slot>7){
                    wr32(shared,SH_SAVE_ERR,10);
                    wr32(shared,SH_SAVE_STATUS,SAVE_STATUS_ERR);
                  } else if(_cmd==SAVE_CMD_WRITE){
                    UBYTE *_tmp=(UBYTE*)AllocMem(8192,MEMF_PUBLIC);
                    sprintf(_path,"PROGDIR:zzdoom_save%lu.dsg",(unsigned long)_slot);
                    if(_tmp&&_sz>0&&_sz<=CORE1_SAVE_SIZE){
                      BPTR _f=Open(_path,MODE_NEWFILE);
                      if(_f){
                        ULONG _off=0; int _ok=1;
                        while(_off<_sz&&_ok){
                          ULONG _chunk=_sz-_off; if(_chunk>8192) _chunk=8192;
                          zz_bus_copy_lump((const UBYTE*)_ddr,_off,_tmp,_chunk);
                          if(Write(_f,(APTR)_tmp,_chunk)!=(LONG)_chunk) _ok=0;
                          _off+=_chunk;
                        }
                        Close(_f);
                        wr32(shared,SH_SAVE_STATUS,_ok?SAVE_STATUS_OK:SAVE_STATUS_ERR);
                        wr32(shared,SH_SAVE_ERR,_ok?0:20);
                      } else { wr32(shared,SH_SAVE_STATUS,SAVE_STATUS_ERR); wr32(shared,SH_SAVE_ERR,21); }
                      FreeMem(_tmp,8192);
                    } else { wr32(shared,SH_SAVE_STATUS,SAVE_STATUS_ERR); wr32(shared,SH_SAVE_ERR,22); }
                  } else if(_cmd==SAVE_CMD_READ){
                    UBYTE *_tmp=(UBYTE*)AllocMem(8192,MEMF_PUBLIC);
                    sprintf(_path,"PROGDIR:zzdoom_save%lu.dsg",(unsigned long)_slot);
                    if(_tmp){
                      BPTR _f=Open(_path,MODE_OLDFILE);
                      if(_f){
                        ULONG _off=0; LONG _rd; int _ok=1;
                        while((_rd=Read(_f,(APTR)_tmp,8192))>0){
                          if(_off+(ULONG)_rd>CORE1_SAVE_SIZE){_ok=0;break;}
                          copy_uword((volatile UBYTE*)_ddr+_off,_tmp,(LONG)_rd);
                          _off+=(ULONG)_rd;
                        }
                        Close(_f);
                        wr32(shared,SH_SAVE_SIZE,_off);
                        wr32(shared,SH_SAVE_STATUS,_ok?SAVE_STATUS_OK:SAVE_STATUS_ERR);
                        wr32(shared,SH_SAVE_ERR,_ok?0:30);
                      } else { wr32(shared,SH_SAVE_STATUS,SAVE_STATUS_ERR); wr32(shared,SH_SAVE_ERR,31); }
                      FreeMem(_tmp,8192);
                    } else { wr32(shared,SH_SAVE_STATUS,SAVE_STATUS_ERR); wr32(shared,SH_SAVE_ERR,32); }
                  } else {
                    wr32(shared,SH_SAVE_STATUS,SAVE_STATUS_ERR); wr32(shared,SH_SAVE_ERR,40);
                  }
                  wr32(shared,SH_SAVE_ACK,_sv_seq);
                }
              }
            }

            /* === Deferred PAN state machine ===
               SH_FLIP_SEQ written ONLY after real firmware ACK.
               No swap, no SH_FB_ADDR before ACK. */
            {
                int screen_front = (IntuitionBase->FirstScreen == my_scr);
                struct EClockVal ev_now; ReadEClock(&ev_now);

                /* Screen lost -> clear/disarm, stop cleanly */
                if(!screen_front && screen_was_front){
                    printf("Screen lost: clear deferred PAN\n");
                    ZZ_WR(board, REG_ZZ_USER3, 2); /* clear pending + disarm */
                    goto stop;
                }
                screen_was_front = screen_front;

                /* Check ACK (non-blocking single read) */
                if(flip_inflight){
                    ULONG cur_ack = zz_pan_ack(board);
                    if(cur_ack != flip_ack_before){
                        /* Real vblank flip confirmed by firmware */
                        last_flipped = flip_ready;
                        { int t=front_idx; front_idx=back_idx; back_idx=t; }
                        wr32(shared, SH_FB_ADDR,  buf_arm[back_idx]);
                        wr32(shared, SH_FB_PITCH, fb_pitch);
                        wr32(shared, SH_FLIP_SEQ, flip_ready);
                        flip_inflight = 0;
                    } else {
                        /* Timeout: stop cleanly without reporting a successful flip. */
                        ULONG ms = (ev_now.ev_lo - t_flip_req) * 1000UL / eclock_freq;
                        if(ms > 100UL){
                            printf("PAN ACK TIMEOUT: ack_before=%lu ack_now=%lu"
                                   " ready=%lu last=%lu\n",
                                   flip_ack_before, cur_ack,
                                   flip_ready, last_flipped);
                            ZZ_WR(board, REG_ZZ_USER3, 2);
                            goto stop;
                        }
                    }
                }

                /* Request flip: one pending max, screen must be front */
                if(!flip_inflight && screen_front){
                    ULONG ready = rd32(shared, SH_FRAME_READY);
                    if(ready != last_flipped){
                        flip_ack_before = zz_pan_ack(board);
                        ZZ_WR(board, REG_ZZ_USER3, 1); /* arm ONE next PAN */
                        zz_deferred_pan_request(board, buf_off[back_idx], fb_pitch);
                        flip_ready    = ready;
                        t_flip_req    = ev_now.ev_lo;
                        flip_inflight = 1;
                        /* No swap. No SH_FB_ADDR. No SH_FLIP_SEQ yet. */
                    }
                }

                /* Diagnostic every ~2s (only with -camddebug) */
                if(camd_debug && ev_now.ev_lo - t_diag > eclock_freq*2UL){
                    printf("[PAN] ack=%lu inflight=%d ready=%lu last=%lu\n",
                           zz_pan_ack(board), flip_inflight,
                           rd32(shared,SH_FRAME_READY), last_flipped);
                    t_diag = ev_now.ev_lo;
                }
            }
            if((st&0xFF)==0xFF){   /* Doom quit (menu Quit) - Core1 stopped */
                if(camd_debug) printf("Doom quit - Core1 stopped\n");
                goto cleanup;
            }
            if(hb!=last_hb){
                sprintf(title,"ZZDoom Frame:%lu DIAG:%08lX",hb,diag);
                SetWindowTitles(win,title,(UBYTE*)-1L);
                last_hb=hb;
            }
            if(diag!=last_diag){
                if(camd_debug){
                    printf("s_sound: STOP=%lu CHG=%lu BADNUM=%lu BADNAME=%lu PLAY=%lu SAME=%lu\n",
                           rd32(shared,SH_DBG_S_STOP),rd32(shared,SH_DBG_S_CHANGE),
                           rd32(shared,SH_DBG_S_BADNUM),rd32(shared,SH_DBG_S_BADNAME),
                           rd32(shared,SH_DBG_S_PLAYSENT),rd32(shared,SH_DBG_S_SAMEMUS));
                    printf("DIAG=0x%08lX STATUS=0x%08lX ERR=0x%08lX ERR4=0x%08lX\n",
                           diag, rd32(shared,SH_STATUS),
                           rd32(shared,SH_ERROR), rd32(shared,SH_ERR4));
                }
                last_diag=diag;
            }
            /* Every ~2s dump full state if no frames */
            if(++poll>=100 && hb==0){
                poll=0;
                if(camd_debug) printf("[poll] DIAG=0x%08lX i=%lu numtex=%lu\n", rd32(shared,SH_DIAG), rd32(shared,16), rd32(shared,56));
            }
            while((msg=(struct IntuiMessage*)GetMsg(win->UserPort))){
                ULONG cls=msg->Class;
                UWORD code=msg->Code;
                WORD  mx=msg->MouseX;
                UWORD qual=msg->Qualifier;
                ReplyMsg((struct Message*)msg);
                cnt_idcmp_total++;
                if(cls==IDCMP_MOUSEMOVE) cnt_idcmp_move++;
                else if(cls==IDCMP_RAWKEY) cnt_idcmp_key++;
                else cnt_idcmp_other++;
                if(cls==IDCMP_CLOSEWINDOW) goto stop;
                if(cls==IDCMP_RAWKEY){
                    UWORD raw = code & 0x7F;
                    int press = (code & 0x80) ? 0 : 1;  /* break bit */
                    ULONG bit = 0;
                    switch(raw){
                        /* Manual-conformant mapping (Doom defaults) */
                        case RK_UP:    bit=BTN_UP;    break;  /* forward */
                        case RK_DOWN:  bit=BTN_DOWN;  break;  /* backward */
                        case RK_LEFT:  bit=BTN_LEFT;  break;  /* turn left */
                        case RK_RIGHT: bit=BTN_RIGHT; break;  /* turn right */
                        case RK_LCTRL: bit=BTN_FIRE;  break;  /* FIRE = Ctrl */
                        case RK_SPACE: bit=BTN_USE;   break;  /* USE = Space (doors/switches) */
                        case RK_LALT:                         /* strafe modifier */
                        case RK_RALT:  bit=BTN_STRAFE_MOD; break;
                        case RK_COMMA: bit=BTN_SL;    break;  /* strafe left alt */
                        case RK_PERIOD:bit=BTN_SR;    break;  /* strafe right alt */
                        case RK_LSHIFT:                       /* RUN = Shift */
                        case RK_RSHIFT:bit=BTN_RUN;   break;
                        case RK_TAB:   bit=BTN_MAP;   break;  /* automap */
                        case RK_ESC:   bit=BTN_ESC;   break;  /* menu */
                        case RK_ENTER:
                        case RK_RETURN:bit=BTN_ENTER; break;  /* select */
                        case RK_Y_KEY: bit=BTN_Y;     break;  /* quit confirm */
                        case RK_1:     bit=BTN_W1;    break;  /* weapon 1 */
                        case RK_2:     bit=BTN_W2;    break;
                        case RK_3:     bit=BTN_W3;    break;
                        case RK_4:     bit=BTN_W4;    break;
                        case RK_5:     bit=BTN_W5;    break;
                        case RK_6:     bit=BTN_W6;    break;
                        case RK_7:     bit=BTN_W7;    break;
                        case RK_F1:    bit=BTN_F1;    break;
                        case RK_F2:    bit=BTN_F2;    break;
                        case RK_F3:    bit=BTN_F3;    break;
                        case RK_F4:    bit=BTN_F4;    break;
                        case RK_F5:    bit=BTN_F5;    break;
                        case RK_F6:    bit=BTN_F6;    break;
                        case RK_F7:    bit=BTN_F7;    break;
                        case RK_F8:    bit=BTN_F8;    break;
                        case RK_F9:    bit=BTN_F9;    break;
                        default: bit=0; break;
                    }
                    if(raw==RK_F10){
                        if(press) g_buttons2|=BTN2_F10;
                        else      g_buttons2&=~BTN2_F10;
                        wr32(shared, SH_INPUT2, g_buttons2);
                    } else if(bit){
                        if(press) g_buttons |= bit;
                        else      g_buttons &= ~bit;
                        { ULONG combined = g_buttons | g_mouse_vkeys;
                          if(combined != g_input_last){
                              g_input_last = combined;
                              wr32(shared, SH_INPUT, combined);
                          }
                        }
                    }
                }
                else if(cls==IDCMP_MOUSEMOVE){
                    /* DELTAMOVE: accumulate horizontal movement -> arrow turn. */
                    if((mx>0 && g_mturn_acc<0) || (mx<0 && g_mturn_acc>0)) g_mturn_acc=0;
                    g_mturn_acc += (LONG)mx;
                    /* Refresh absolute button state from qualifier (caught on
                       every mouse message so release is never missed). */
                    g_mouse_btn = 0;
                    if(qual & IEQUALIFIER_LEFTBUTTON) g_mouse_btn |= 1;
                    if(qual & IEQUALIFIER_RBUTTON)    g_mouse_btn |= 2;
                }
                else if(cls==IDCMP_MOUSEBUTTONS){
                    /* Absolute button state from qualifier. Injected into
                       SH_INPUT (virtual keys) below, NOT SH_MOUSE_BTN. */
                    g_mouse_btn = 0;
                    if(qual & IEQUALIFIER_LEFTBUTTON) g_mouse_btn |= 1;
                    if(qual & IEQUALIFIER_RBUTTON)    g_mouse_btn |= 2;
                }
            }
            /* Convert accumulated mouse movement to arrow-key turn bits.
               Same effect as pressing Left/Right: fixed angleturn per tic,
               constant speed, no runaway. */
            {
                int want_bit=0;
                ULONG combined;
                if(g_mturn_acc >=  MOUSE_TURN_THRESH)      want_bit=BTN_RIGHT;
                else if(g_mturn_acc <= -MOUSE_TURN_THRESH) want_bit=BTN_LEFT;
                g_mturn_acc=0;   /* consume fully each frame: no residue, no lag */
                g_mturn_bit = want_bit;

                /* Mouse-contributed virtual keys (separate from keyboard bits
                   so we never erase a held keyboard key). LMB->FIRE, RMB->STRAFE,
                   plus mouse turn. Same SH_INPUT path as keyboard = clean release. */
                g_mouse_vkeys = (ULONG)want_bit;
                if(g_mouse_btn & 1) g_mouse_vkeys |= BTN_FIRE;
                if(g_mouse_btn & 2) g_mouse_vkeys |= BTN_STRAFE_MOD;

                /* Combine keyboard state with mouse virtual keys. */
                combined = g_buttons | g_mouse_vkeys;
                if(combined != g_input_last){
                    g_input_last = combined;
                    wr32(shared, SH_INPUT, combined);
                }
            }
            /* Main wait loop: AHI + Ctrl-C only.
               Window and flip events are polled non-blocking. */
            cnt_loops++;
            cnt_wait_calls++;
            {
                ULONG win_sig  = 1UL << win->UserPort->mp_SigBit;
                ULONG flip_sig = dbuf_sig;
                ULONG sigs;
                /* Wait ONLY on AHI reply port + Ctrl-C */
                sigs = Wait(zz_ahi_signal | SIGBREAKF_CTRL_C);
                cnt_wait_returns++;
                if(sigs & SIGBREAKF_CTRL_C) break;
                if(sigs & zz_ahi_signal)    cnt_ahi_sig++;
                if(sigs & win_sig)          cnt_win_sig++;
                if(sigs & flip_sig)         cnt_flip_sig++;
                /* Service AHI: drain reply port, refill completed bufs */
                if(sigs & zz_ahi_signal)    zz_ahi_service();
                /* Window + flip polled below (non-blocking GetMsg/CheckIO) */
            }
            /* Sample diagnostics every 500 loop iterations */
            if(++cnt_sample_timer >= 500){
                printf("[DIAG] loops=%lu wait_ret=%lu ahi=%lu win=%lu flip=%lu\n",
                       cnt_loops, cnt_wait_returns,
                       cnt_ahi_sig, cnt_win_sig, cnt_flip_sig);
                printf("[DIAG] idcmp=%lu move=%lu key=%lu other=%lu underruns=%lu\n",
                       cnt_idcmp_total, cnt_idcmp_move, cnt_idcmp_key, cnt_idcmp_other,
                       rd32(shared, SH_PCM_UNDERRUNS_O));
                cnt_loops=0; cnt_wait_calls=0; cnt_wait_returns=0;
                cnt_ahi_sig=0; cnt_win_sig=0; cnt_flip_sig=0;
                cnt_idcmp_total=0; cnt_idcmp_move=0; cnt_idcmp_key=0; cnt_idcmp_other=0;
                cnt_sample_timer=0;
            }
        }
    }

stop:
    printf("Stop...\n");
    printf("MUSDBG fpos=%08lx size=%lu sig=%08lx\n",
           dbg_mus_fpos, dbg_mus_size, dbg_mus_sig);
    if(use_camd){
        zz_mus_stop();
        zz_camd_shutdown();
        if(mus_copy_ptr){FreeMem(mus_copy_ptr,mus_copy_sz);mus_copy_ptr=NULL;}
    }
    /* Restore scanout to original buffer before exit */
    if(buf_off[0]) zz_deferred_pan_request(board, buf_off[0], fb_pitch);
    Delay(5);
    /* === FINAL METRICS === */
    {
        struct EClockVal ev_end; ULONG t_end, runtime_ticks, runtime_sec;
        ReadEClock(&ev_end); t_end=ev_end.ev_lo;
        runtime_ticks = t_end - t_run_start;
        runtime_sec   = (eclock_freq_local>0) ?
                        runtime_ticks/eclock_freq_local : 1;
        if(runtime_sec==0) runtime_sec=1;
        {
            ULONG ring_min = rd32(shared, 63*4);   /* SH_RING_MIN */
            ULONG pump_calls = rd32(shared, 57*4); /* SH_PUMP_CALLS */
            ULONG mgr_ms = (eclock_freq_local>0) ?
                           max_gap_refill*1000UL/eclock_freq_local : 0;
            ULONG mgp_ms = (eclock_freq_local>0) ?
                           max_gap_pump*1000UL/eclock_freq_local : 0;
            printf("\n=== EXIT METRICS ===\n");
            printf("runtime_sec     : %lu\n", runtime_sec);
            printf("ahi_refills     : %lu (%lu/sec)\n",
                   ahi_refill_total, ahi_refill_total/runtime_sec);
            printf("ahi_underruns   : %lu\n", ahi_under_total);
            printf("loop_iter/sec   : %lu\n", cnt_loops/runtime_sec);
            printf("Wait_returns/sec: %lu\n", cnt_wait_returns/runtime_sec);
            printf("ahi_wakeups/sec : %lu\n", cnt_ahi_sig/runtime_sec);
            printf("win_wakeups/sec : %lu\n", cnt_win_sig/runtime_sec);
            printf("flip_wakeups/sec: %lu\n", cnt_flip_sig/runtime_sec);
            printf("pump_calls/sec  : %lu\n", pump_calls/runtime_sec);
            printf("ring_min_bytes  : %lu (%lu ms)\n",
                   ring_min,
                   (ring_min/2)*1000UL/22050UL);
            printf("max_gap_refill  : %lu ms\n", mgr_ms);
            printf("max_gap_pump    : %lu ms\n", mgp_ms);
            printf("sfx_starts      : %lu\n", rd32(shared,58*4));
            printf("sfx_stolen      : %lu\n", rd32(shared,59*4));
            printf("sfx_dropped     : %lu\n", rd32(shared,60*4));
            printf("sfx_maxact      : %lu\n", rd32(shared,62*4));
        }
    }
    wr32(shared,SH_STOP,1);
    { LONG t=500;
      while(t-->0){
          if((rd32(shared,SH_STATUS)&0xFF)==0xFF){
              printf("Core1 stopped OK\n"); break;}
          Delay(1);}
      if(!t) printf("Timeout\n");
    }
    Delay(25);

cleanup:
    printf("MUSDBG fpos=%08lx size=%lu sig=%08lx\n",
           dbg_mus_fpos, dbg_mus_size, dbg_mus_sig);
    /* Disable deferred PAN and restore original buffer */
    if(board) ZZ_WR(board, REG_ZZ_USER3, 2); /* disarm + clear */
    if(buf_off[0] && board){
        ZZ_WR(board, REG_ZZ_X1, 0); ZZ_WR(board, REG_ZZ_Y1, 0);
        ZZ_WR(board, REG_ZZ_X2, (UWORD)(fb_pitch>>2));
        ZZ_WR(board, REG_ZZ_COLORMODE, 2);
        ZZ_WR(board, REG_ZZ_PAN_HI, (UWORD)(buf_off[0]>>16));
        ZZ_WR(board, REG_ZZ_PAN_LO, (UWORD)(buf_off[0]&0xFFFF));
    }
    Delay(5);
    if(win) SetWindowPointer(win, TAG_DONE); /* restore default pointer */
    if(g_MausObj){ DisposeObject(g_MausObj); g_MausObj=NULL; }
    if(g_MausBM){ FreeBitMap(g_MausBM); g_MausBM=NULL; }
    if(win)           CloseWindow(win);
    zz_ahi_stop();
    /* free double buffers (restore front before close) */
    if(sb[0]) FreeScreenBuffer(my_scr, sb[0]);
    if(sb[1]) FreeScreenBuffer(my_scr, sb[1]);
    if(man_bm[1]) p96FreeBitMap(man_bm[1]);
    if(dbuf_port) DeleteMsgPort(dbuf_port);
    if(my_scr)        CloseScreen(my_scr);
    if(P96Base)       CloseLibrary(P96Base);
    if(GfxBase_)      CloseLibrary(GfxBase_);
    if(IntuitionBase) CloseLibrary(IntuitionBase);
    /* timer.device cleanup */
    if(tr){
        if(TimerBase) CloseDevice((struct IORequest*)tr);
        DeleteIORequest((struct IORequest*)tr);
    }
    if(tport) DeleteMsgPort(tport);
    return rc;
}