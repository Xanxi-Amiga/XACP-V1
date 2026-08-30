/*
 * julia_doom_core1.c - Doom in Julia skeleton, official shared table
 *
 * Shared table (ARM index, direct write no bswap):
 *   [0]  MAGIC     0xDEAD0004
 *   [1]  STATUS    0=init 1=running 0xEE=error 0xFF=stopped
 *   [2]  HEARTBEAT
 *   [3]  COMMAND   0=none 1=stop
 *   [4]  FRAME
 *   [5]  DIAG      A001..A00B progress markers
 *   [6]  ERROR     0xEEEEEEEE if I_Error
 *   [7]  ERR4      first 4 chars of error
 *   [8]  WAD_ADDR
 *   [9]  WAD_SIZE
 *   [10] FB_ADDR
 *   [11] FB_PITCH
 *   [12] FB_WIDTH
 *   [13] FB_HEIGHT
 *
 * Fin Julia: STATUS=0xFF + WFE.
 * ASCII only.
 */

typedef unsigned int       u32;
typedef unsigned char      u8;
typedef unsigned long long u64;

#define SHARED_ADDR  0x04300000UL
volatile u32 *shared = (volatile u32 *)SHARED_ADDR;

extern u32 read_sctlr(void);
extern u32 read_ttbr0(void);
extern u32 read_ttbcr(void);
extern u32 read_dacr(void);
extern u32 mmu_desc_for(u32 va);
extern void mmu_init_juliadoom(u32 fb_arm);
extern void dcache_clean_range(u32 start, u32 len);
extern void zz_audio_init(void);
extern void zz_audio_fill_beep(void);

#define SH_MAGIC   0
#define SH_STATUS  1
#define SH_HB      2
#define SH_TIME_MS 17
#define SH_TIC35   18
#define SH_VBLANK     19
#define SH_FRAME_READY 20
#define SH_FLIP_SEQ    21
/* Audio PCM ring (Etape 0+) */
#define SH_PCM_BASE      22   /* ARM addr of PCM ring */
#define SH_PCM_SIZE      23   /* ring size in bytes */
#define SH_PCM_WRITE_POS 24   /* ARM write byte offset */
#define SH_PCM_READ_POS  25   /* 68k read byte offset */
#define SH_PCM_RATE      26   /* sample rate Hz */
#define SH_PCM_FORMAT    27   /* 0=s16 mono LE */
#define SH_PCM_UNDERRUNS 28   /* 68k underrun counter */
#define SH_PCM_ENABLED   29   /* 1=audio active */

#define PCM_RING_ARM   0x04400000UL
#define PCM_RING_SIZE  (1024UL*1024UL)
#define PCM_RATE       22050UL
#define PCM_FMT_S16    0
#define SH_CMD     3
#define SH_FRAME   4
#define SH_DIAG    5
#define SH_ERROR   6
#define SH_ERR4    7
#define SH_WAD_ADDR  8
#define SH_WAD_SIZE  9
#define SH_FB_ADDR   10
#define SH_FB_PITCH  11
#define SH_FB_WIDTH  12
#define SH_FB_HEIGHT 13

#define MAGIC      0xDEAD0004UL
#define ST_INIT    0UL
#define ST_RUNNING 1UL
#define ST_ERROR   0xEEUL
#define ST_STOPPED 0xFFUL

#define DIAG(v) do{ shared[SH_DIAG]=(v); __asm__ volatile("dsb":::"memory"); }while(0)

/* ARM Global Timer */
#define SCU_BASE    0xF8F00000UL
#define GTIMER_LO   (*(volatile u32*)(SCU_BASE+0x0200))
#define GTIMER_HI   (*(volatile u32*)(SCU_BASE+0x0204))
#define GTIMER_CTRL (*(volatile u32*)(SCU_BASE+0x0208))
#define GTIMER_HZ   333000000ULL

static u64 gtimer_read(void)
{ u32 hi1,lo,hi2; do{hi1=GTIMER_HI;lo=GTIMER_LO;hi2=GTIMER_HI;}while(hi1!=hi2);
  return ((u64)hi1<<32)|(u64)lo; }

static void l1_clean_range(u32 addr, u32 len)
{ u32 a=addr&~31u,e=addr+len;
  for(;a<e;a+=32)__asm__ volatile("mcr p15,0,%0,c7,c10,1"::"r"(a):"memory");
  __asm__ volatile("dsb":::"memory"); }

/* heap */
#define HEAP_BASE 0x05A00000UL
#define HEAP_SIZE (8UL*1024UL*1024UL)
#define HEAP_END  (HEAP_BASE+HEAP_SIZE)
static char *heap_ptr;

void *_sbrk(int incr)
{ char *old; if(heap_ptr==0)heap_ptr=(char*)HEAP_BASE; old=heap_ptr;
  if((u32)(heap_ptr+incr)>HEAP_END)return(void*)-1; heap_ptr+=incr; return old; }

/* ZZDoom save/load via shared memory + 68k filesystem */
#define SAVE_BUF_ARM    0x07E00000UL
#define SH_SAVE_CMD     (70*4)
#define SH_SAVE_SEQ     (71*4)
#define SH_SAVE_ACK     (72*4)
#define SH_SAVE_STATUS  (73*4)
#define SH_SAVE_SLOT    (74*4)
#define SH_SAVE_SIZE    (75*4)
#define SH_SAVE_ERR     (77*4)
#define SAVE_CMD_WRITE  1
#define SAVE_CMD_READ   2
#define SAVE_STATUS_OK  2
#define SAVE_STATUS_ERR 3
#define SAVE_MAX        (128UL*1024UL)  /* per slot */

/* Extract slot from Doom save filenames:
 * "doomsav0.dsg".."doomsav7.dsg" -> slot 0..7
 * "temp.dsg" or any temp -> slot 0
 * Search backwards from end to find the digit before .dsg */
static int zz_parse_save_path(const char *p)
{
    const char *s;
    /* find "doomsav" */
    for(s=p; *s; s++){
        if(s[0]=='d'&&s[1]=='o'&&s[2]=='o'&&s[3]=='m'&&
           s[4]=='s'&&s[5]=='a'&&s[6]=='v'){
            char c=s[7];
            if(c>='0'&&c<='7') return (int)(c-'0');
        }
    }
    /* find "temp" -> slot 0 */
    for(s=p; *s; s++){
        if(s[0]=='t'&&s[1]=='e'&&s[2]=='m'&&s[3]=='p') return 0;
    }
    return -1;
}

/* Virtual file descriptor for save buffer */
static volatile u32 *g_shared = (volatile u32*)0x04300000UL;
static int    g_savefile_open  = 0; /* 1=writing, 2=reading */
static u32  g_savefile_pos   = 0;
static int    g_savefile_slot  = 0;
/* save buffer: slot N at 0x07E00000 + N*0x20000 (128KB each, 6 slots = 768KB) */
static unsigned char *zz_savebuf_for_slot(int slot)
{ return (unsigned char*)(0x07E00000UL + (u32)slot * 0x20000UL); }
static unsigned char *g_savebuf = (unsigned char*)0x07E00000UL;
#define ZZ_SAVE_FD 42  /* magic fd for save file */

static void zz_save_flush(void)
{
    /* Signal 68k to write buffer to disk */
    u32 seq;
    int timeout;
    g_shared[SH_SAVE_SLOT/4] = (u32)g_savefile_slot;
    g_shared[SH_SAVE_SIZE/4] = (u32)g_savefile_pos;
    g_shared[SH_SAVE_CMD /4] = SAVE_CMD_WRITE;
    __asm volatile("dmb sy" ::: "memory");
    seq = g_shared[SH_SAVE_SEQ/4] + 1;
    g_shared[SH_SAVE_SEQ/4] = seq;
    /* Wait for ACK (max ~500ms at ~1MHz shared poll rate) */
    timeout = 5000000;
    while(g_shared[SH_SAVE_ACK/4] != seq && --timeout > 0) {}
}

static void zz_load_request(void)
{
    /* Signal 68k to read file into buffer */
    u32 seq;
    int timeout;
    g_shared[SH_SAVE_SLOT/4] = (u32)g_savefile_slot;
    g_shared[SH_SAVE_CMD /4] = SAVE_CMD_READ;
    __asm volatile("dmb sy" ::: "memory");
    seq = g_shared[SH_SAVE_SEQ/4] + 1;
    g_shared[SH_SAVE_SEQ/4] = seq;
    timeout = 5000000;
    while(g_shared[SH_SAVE_ACK/4] != seq && --timeout > 0) {}
    /* After ACK, SH_SAVE_SIZE contains bytes read */
}

int _open(const char *p, int flags, ...)
{
    int slot = zz_parse_save_path(p);
    if(slot < 0) return -1;
    g_savefile_slot = slot;
    g_savefile_pos  = 0;
    g_savebuf = zz_savebuf_for_slot(slot);
    if(flags & 1) { /* O_WRONLY or O_RDWR with write */
        g_savefile_open = 1; /* writing */
    } else {
        g_savefile_open = 2; /* reading */
        zz_load_request();
        /* If 68k could not find/read file, return error */
        if(g_shared[SH_SAVE_STATUS/4] == SAVE_STATUS_ERR){
            g_savefile_open = 0;
            return -1;
        }
    }
    return ZZ_SAVE_FD;
}

int _write(int f, char *b, int l)
{
    if(f==ZZ_SAVE_FD && g_savefile_open==1 && l>0){
        if(g_savefile_pos + (u32)l <= SAVE_MAX){
            u32 i;
            for(i=0;i<(u32)l;i++) g_savebuf[g_savefile_pos+i]=(unsigned char)b[i];
            g_savefile_pos += (u32)l;
        }
        return l;
    }
    (void)f; (void)b;
    return l;
}

int _read(int f, char *b, int l)
{
    if(f==ZZ_SAVE_FD && g_savefile_open==2 && l>0){
        u32 avail = g_shared[SH_SAVE_SIZE/4];
        u32 rem   = (g_savefile_pos < avail) ? avail - g_savefile_pos : 0;
        u32   n     = (u32)l < rem ? (u32)l : (u32)rem;
        u32   i;
        for(i=0;i<n;i++) b[i]=(char)g_savebuf[g_savefile_pos+i];
        g_savefile_pos += n;
        return (int)n;
    }
    (void)f; (void)b; (void)l;
    return 0;
}

int _close(int f)
{
    if(f==ZZ_SAVE_FD){
        if(g_savefile_open==1) zz_save_flush();
        g_savefile_open = 0;
        g_savefile_pos  = 0;
    }
    return 0;
}

int _lseek(int f, int o, int w)
{
    if(f==ZZ_SAVE_FD){
        u32 avail = (g_savefile_open==2) ? g_shared[SH_SAVE_SIZE/4] : SAVE_MAX;
        if(w==0) g_savefile_pos=(u32)o;
        else if(w==1) g_savefile_pos+=(u32)o;
        else if(w==2) g_savefile_pos=avail+(u32)o;
        return (int)g_savefile_pos;
    }
    return 0;
}

int _fstat(int f,void*s){(void)f;(void)s;return 0;}
int _isatty(int f){(void)f;return 1;}
void _exit(int c){(void)c;while(1){}}
int _kill(int p,int s){(void)p;(void)s;return -1;}
int _getpid(void){return 1;}

#include "doomgeneric.h"
#include "w_file.h"

extern wad_file_class_t mem_wad_file;
void   *g_wad_ptr  = 0;
unsigned int g_wad_size = 0;

static u32 g_fb_arm=0,g_fb_w=320,g_fb_h=200,g_fb_pitch=1280;
static u32 g_frame=0,g_t0_ms=0;
static u32 g_fake_ms=0;   /* monotonic ms for Doom timer */

void DG_Init(void)
{
    DIAG(0xA007UL);
    g_fake_ms=0;
}

#define DOOM_W 320
#define DOOM_H 200
static u32 g_last_draw_tic = 0xFFFFFFFFUL;
void DG_DrawFrame(void)
{
    u32 y;
    shared[51]++;                                  /* DG_DrawFrame_enter (slot 51) */
    { extern void zz_audio_fill_beep(void); zz_audio_fill_beep(); }
    volatile u32 *src=(volatile u32*)DG_ScreenBuffer;
    u32 fb;
    u32 pitch=g_fb_pitch;
    u32 tic;
    if(!DG_ScreenBuffer) return;
    /* Cap rendering to 35Hz */
    tic = shared[SH_TIC35];
    if(tic == g_last_draw_tic) return;
    g_last_draw_tic = tic;
    if(g_frame==0) DIAG(0xA00BUL);

    /* Double-buffer: render into the BACK buffer addr the 68k gave us.
       SH_FB_ADDR is updated by 68k after each flip. */
    fb = shared[SH_FB_ADDR];
    if(!fb) fb = g_fb_arm;

    /* Blob 640: 2x horizontal + vertical stretch 200->480.
       For each dest line y (0..479): src_y = y*200/480, pixel-double x. */
    {
        u32 x;
        u32 dest_h = 480;
        for(y=0;y<dest_h;y++){
            u32 src_y = (y * DOOM_H) / dest_h;
            volatile u32 *s = src + src_y*DOOM_W;
            volatile u32 *d = (volatile u32*)(fb + y*pitch);
            for(x=0;x<DOOM_W;x++){
                u32 c=s[x];
                d[x*2]=c; d[x*2+1]=c;
            }
        }
    }
    dcache_clean_range(fb, pitch*480);

    g_frame++;
    shared[SH_HB]=g_frame;
    shared[SH_FRAME]=g_frame;

    /* Signal frame ready, then wait for 68k to flip (ChangeScreenBuffer).
       This ensures we never render into the buffer being scanned out. */
    shared[SH_FRAME_READY] = tic;
    {
        u32 spin = 4000000UL;
        while(shared[SH_FLIP_SEQ] != tic && spin--){ }
    }
}

void DG_SleepMs(uint32_t ms)
{ (void)ms; { extern void zz_audio_pump(void); zz_audio_pump(); } }
uint32_t DG_GetTicksMs(void)
{ return shared[SH_TIME_MS]; }
/* Input button bits (must match 68k launcher) */
#define BTN_UP      0x0001
#define BTN_DOWN    0x0002
#define BTN_LEFT    0x0004
#define BTN_RIGHT   0x0008
#define BTN_SL      0x0010   /* strafe left */
#define BTN_SR      0x0020   /* strafe right */
#define BTN_FIRE    0x0040
#define BTN_USE     0x0080
#define BTN_RUN     0x0100
#define BTN_ESC     0x0200
#define BTN_ENTER   0x0400
#define BTN_Y       0x0800   /* yes (quit confirm) */
#define BTN_STRAFE_MOD 0x1000
#define BTN_MAP     0x2000
#define BTN_W1      0x00010000
#define BTN_W2      0x00020000
#define BTN_W3      0x00040000
#define BTN_W4      0x00080000
#define BTN_W5      0x00100000
#define BTN_W6      0x00200000
#define BTN_W7      0x00400000
#define KEY_F1 0xbb
#define KEY_F2 0xbc
#define KEY_F3 0xbd
#define KEY_F4 0xbe
#define KEY_F5 0xbf
#define KEY_F6 0xc0
#define KEY_F7 0xc1
#define KEY_F8 0xc2
#define KEY_F9 0xc3
#define KEY_F10 0xc4
#define BTN_F1      0x00800000
#define BTN_F2      0x01000000
#define BTN_F3      0x02000000
#define BTN_F4      0x04000000
#define BTN_F5      0x08000000
#define BTN_F6      0x10000000
#define BTN_F7      0x20000000
#define BTN_F8      0x40000000
#define BTN_F9      0x80000000
#define SH_INPUT2   15
#define BTN2_F10    0x00000001

#define SH_INPUT 14

/* Doom key codes - exact from doomkeys.h */
#define DK_RIGHTARROW 0xae
#define DK_LEFTARROW  0xac
#define DK_UPARROW    0xad
#define DK_DOWNARROW  0xaf
#define DK_STRAFE_L   0xa0
#define DK_STRAFE_R   0xa1
#define DK_USE        0xa2
#define DK_FIRE       0xa3
#define DK_ESCAPE     27
#define DK_ENTER      13
#define DK_RSHIFT     (0x80+0x36)
#define DK_RALT       (0x80+0x38)
#define DK_TAB        9
#define DK_Y          'y'

/* Map each button bit to a Doom key code */
static const struct { u32 bit; unsigned char key; } g_keymap[] = {
    { BTN_UP,    DK_UPARROW },
    { BTN_DOWN,  DK_DOWNARROW },
    { BTN_LEFT,  DK_LEFTARROW },
    { BTN_RIGHT, DK_RIGHTARROW },
    { BTN_SL,    DK_STRAFE_L },
    { BTN_SR,    DK_STRAFE_R },
    { BTN_FIRE,  DK_FIRE },
    { BTN_USE,   DK_USE },
    { BTN_RUN,   DK_RSHIFT },
    { BTN_ESC,   DK_ESCAPE },
    { BTN_ENTER, DK_ENTER },
    { BTN_Y,     DK_Y },
    { BTN_STRAFE_MOD, DK_RALT },
    { BTN_MAP,   DK_TAB },
    { BTN_W1,    '1' },
    { BTN_W2,    '2' },
    { BTN_W3,    '3' },
    { BTN_W4,    '4' },
    { BTN_W5,    '5' },
    { BTN_W6,    '6' },
    { BTN_W7,    '7' },
    { BTN_F1,    KEY_F1 },
    { BTN_F2,    KEY_F2 },
    { BTN_F3,    KEY_F3 },
    { BTN_F4,    KEY_F4 },
    { BTN_F5,    KEY_F5 },
    { BTN_F6,    KEY_F6 },
    { BTN_F7,    KEY_F7 },
    { BTN_F8,    KEY_F8 },
    { BTN_F9,    KEY_F9 },
};
#define NKEYS (int)(sizeof(g_keymap)/sizeof(g_keymap[0]))

static u32 g_prev_buttons = 0;
static int g_key_idx = 0;       /* iterator over keymap for event generation */
static u32 g_cur_buttons = 0;   /* snapshot for this drain cycle */
static u32 g_diff = 0;

/* doomgeneric event-based: return 1 + set *pressed,*key per event, 0 when done.
   We snapshot buttons once when starting a fresh drain (g_key_idx==0). */
int DG_GetKey(int *pressed, unsigned char *key)
{
    if(g_key_idx == 0){
        /* Merge SH_INPUT and SH_INPUT2 F10 bit */
        u32 inp2 = shared[SH_INPUT2];
        g_cur_buttons = (shared[SH_INPUT] & ~0x00000000UL);
        /* Remap F10 from INPUT2 into a spare bit - use 0x80000000 already used for F9 */
        /* F10 handled separately below */
        g_diff = g_cur_buttons ^ g_prev_buttons;
        /* Check F10 separately */
        { static u32 prev_f10=0; u32 cur_f10=inp2&BTN2_F10;
          if(cur_f10!=prev_f10){ prev_f10=cur_f10;
            *pressed=cur_f10?1:0; *key=KEY_F10;
            return 1; }
        }
        if(g_diff == 0){
            g_prev_buttons = g_cur_buttons;
            return 0;   /* nothing changed */
        }
    }
    /* Walk the keymap looking for changed bits */
    while(g_key_idx < NKEYS){
        u32 bit = g_keymap[g_key_idx].bit;
        unsigned char kc = g_keymap[g_key_idx].key;
        g_key_idx++;
        if(g_diff & bit){
            *pressed = (g_cur_buttons & bit) ? 1 : 0;
            *key = kc;
            return 1;
        }
    }
    /* drain complete */
    g_prev_buttons = g_cur_buttons;
    g_key_idx = 0;
    return 0;
}
void DG_SetWindowTitle(const char*t){(void)t;}

static char arg0[]="doom";
static char arg1[]="-iwad";
static char arg2[]="DOOM1.WAD";
static char *g_argv[]={arg0,arg1,arg2,0};

void core1_entry(void *env) __attribute__((section(".text.core1_entry")));
void core1_entry(void *env)
{
    (void)env;

    /* Enable unaligned access: clear SCTLR.A (bit1), set SCTLR.U is legacy.
       On Cortex-A9 with MMU, unaligned access allowed when A=0.
       Doom reads unaligned ints from WAD texture structs. */
    {
        u32 sctlr;
        __asm__ volatile("mrc p15,0,%0,c1,c0,0":"=r"(sctlr));
        sctlr &= ~(1u<<1);   /* clear A - allow unaligned */
        __asm__ volatile("mcr p15,0,%0,c1,c0,0"::"r"(sctlr));
        __asm__ volatile("isb");
    }

    /* (abort handler install deferred until after magic write) */

    /* Zero BSS */
    { extern u32 _bss_start,_bss_end; u32 *p=&_bss_start;
      while(p<&_bss_end)*p++=0; }

    /* heap init */
    heap_ptr=(char*)HEAP_BASE;

    shared[SH_MAGIC]=MAGIC;
    shared[SH_STATUS]=ST_INIT;
    shared[SH_HB]=0;
    shared[SH_CMD]=0;
    DIAG(0xA001UL);   /* entry */

    /* Install speaking abort handlers AFTER magic is up */
    { extern void zz_install_vectors(void); zz_install_vectors(); }
    DIAG(0xA1A1UL);   /* vectors installed, still alive */

    /* Phase A: MMU diagnostic dump into shared[48..63] */
    shared[48] = read_sctlr();
    shared[49] = read_ttbr0();
    shared[50] = read_ttbcr();
    shared[51] = read_dacr();
    shared[52] = mmu_desc_for(0x04200000UL);  /* blob */
    shared[53] = mmu_desc_for(0x04300000UL);  /* shared */
    shared[54] = mmu_desc_for(0x04500000UL);  /* WAD */
    shared[55] = mmu_desc_for(0x05200000UL);  /* zone */
    shared[56] = mmu_desc_for(0x05A00000UL);  /* heap */
    /* framebuffer desc filled after we read g_fb_arm below */
    DIAG(0xA1A2UL);

    DIAG(0xA002UL);   /* bss clear done (already) */
    DIAG(0xA003UL);   /* heap init done */

    /* Read params */
    g_fb_arm   = shared[SH_FB_ADDR];
    g_fb_pitch = shared[SH_FB_PITCH];
    g_fb_w     = shared[SH_FB_WIDTH];
    g_fb_h     = shared[SH_FB_HEIGHT];
    g_wad_ptr  = (void*)(u32)shared[SH_WAD_ADDR];
    g_wad_size = (unsigned int)shared[SH_WAD_SIZE];
    if(!g_fb_arm)   g_fb_arm=0x05100000UL;
    if(!g_fb_w)     g_fb_w=320;
    if(!g_fb_h)     g_fb_h=200;
    if(!g_fb_pitch) g_fb_pitch=1280;
    shared[58] = g_fb_arm;
    shared[59] = g_fb_pitch;

    /* Phase B: enable MMU - maps DDR Normal WB/WA, fixes alignment + speed */
    mmu_init_juliadoom(g_fb_arm);
    DIAG(0xA1A3UL);   /* MMU enabled */

    zz_audio_init();   /* Etape 0: init PCM ring + publish to shared */

    /* Re-dump descriptors AFTER MMU on to verify */
    shared[48] = read_sctlr();
    shared[49] = read_ttbr0();
    shared[52] = mmu_desc_for(0x04200000UL);
    shared[53] = mmu_desc_for(0x04300000UL);
    shared[54] = mmu_desc_for(0x04500000UL);
    shared[55] = mmu_desc_for(0x05200000UL);
    shared[56] = mmu_desc_for(0x05A00000UL);
    shared[57] = mmu_desc_for(g_fb_arm & 0xFFF00000UL);
    DIAG(0xA004UL);   /* shared read */

    shared[SH_STATUS]=ST_RUNNING;

    DIAG(0xA005UL);   /* before doomgeneric_Create */
    doomgeneric_Create(3, g_argv);

    /* D_DoomMain should not return (attract mode loops). If it does, idle. */
    shared[5]=0xA0FFUL;   /* doomgeneric_Create returned (unexpected) */
    shared[SH_STATUS]=ST_STOPPED;
    while(1){ __asm__ volatile("wfe"); }
}
