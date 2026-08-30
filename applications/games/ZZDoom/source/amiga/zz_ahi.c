/*
 * zz_ahi.c - ZZDoom AHI double-buffer, event-driven via reply port.
 * AHI_BUF_SAMPLES=512 (23ms per buf, 46ms in flight).
 * The reply port fires when a buffer finishes -> immediate refill.
 * No polling latency. Signal exposed as zz_ahi_signal for Wait() in main loop.
 * ASCII only.
 */
#include <exec/types.h>
#include <exec/memory.h>
#include <devices/ahi.h>
#include <proto/exec.h>
#include <stdio.h>

#define SH_PCM_SIZE_O      (23*4)
#define SH_PCM_WRITE_POS_O (24*4)
#define SH_PCM_READ_POS_O  (25*4)
#define SH_PCM_RATE_O      (26*4)
#define SH_PCM_UNDERRUNS_O (28*4)

#define AHI_BUF_SAMPLES 512    /* 23ms per buf, 46ms in flight */

static struct MsgPort    *ahi_mp     = NULL;
static struct AHIRequest *ahi_req[2] = {NULL, NULL};
static BYTE   ahi_dev_open           = -1;
static WORD  *ahi_buf[2]             = {NULL, NULL};

/* Exposed signal mask for Wait() in main loop */
ULONG zz_ahi_signal = 0;

static volatile UBYTE *au_shared   = NULL;
static volatile UBYTE *au_ringbase = NULL;
static ULONG  au_ring_size = 0;
static ULONG  au_read_pos  = 0;
static ULONG  au_rate      = 22050;

/* Diagnostics */
static ULONG ahi_refill_count = 0;
static ULONG ahi_underrun_arm = 0;

static void pull_samples(WORD *buf)
{
    ULONG wr = rd32(au_shared, SH_PCM_WRITE_POS_O);
    ULONG avail;
    int n = 0, k;

    for(k = 0; k < AHI_BUF_SAMPLES; k++) buf[k] = 0;

    if(wr >= au_read_pos) avail = wr - au_read_pos;
    else                  avail = au_ring_size - au_read_pos + wr;

    while(n < AHI_BUF_SAMPLES && avail >= 2){
        UBYTE lo = au_ringbase[au_read_pos];
        UBYTE hi = au_ringbase[au_read_pos + 1];
        buf[n++] = (WORD)((hi << 8) | lo);
        au_read_pos += 2;
        if(au_read_pos >= au_ring_size) au_read_pos = 0;
        avail -= 2;
    }
    wr32(au_shared, SH_PCM_READ_POS_O, au_read_pos);

    if(n < AHI_BUF_SAMPLES){
        ULONG u = rd32(au_shared, SH_PCM_UNDERRUNS_O);
        wr32(au_shared, SH_PCM_UNDERRUNS_O, u + 1);
        ahi_underrun_arm++;
    }
    ahi_refill_count++;
}

static void prepare_and_send(int idx, struct AHIRequest *link)
{
    pull_samples(ahi_buf[idx]);
    ahi_req[idx]->ahir_Std.io_Flags   = 0;
    ahi_req[idx]->ahir_Std.io_Command = CMD_WRITE;
    ahi_req[idx]->ahir_Std.io_Data    = ahi_buf[idx];
    ahi_req[idx]->ahir_Std.io_Length  = AHI_BUF_SAMPLES * 2;
    ahi_req[idx]->ahir_Std.io_Offset  = 0;
    ahi_req[idx]->ahir_Frequency      = au_rate;
    ahi_req[idx]->ahir_Type           = AHIST_M16S;
    ahi_req[idx]->ahir_Volume         = 0x10000;
    ahi_req[idx]->ahir_Position       = 0x8000;
    ahi_req[idx]->ahir_Link           = link;
    SendIO((struct IORequest*)ahi_req[idx]);
}

int zz_ahi_init(volatile UBYTE *shared, volatile UBYTE *ring_cpu)
{
    int i;
    au_shared    = shared;
    au_ringbase  = ring_cpu;
    au_ring_size = rd32(shared, SH_PCM_SIZE_O);
    au_rate      = rd32(shared, SH_PCM_RATE_O);
    au_read_pos  = 0;
    if(au_rate == 0) au_rate = 22050;

    /* Single reply port - fires when any buffer completes */
    ahi_mp = (struct MsgPort*)CreateMsgPort();
    if(!ahi_mp){ printf("AHI: no msgport\n"); return 0; }

    zz_ahi_signal = 1UL << ahi_mp->mp_SigBit;

    ahi_req[0] = (struct AHIRequest*)CreateIORequest(
                     ahi_mp, sizeof(struct AHIRequest));
    if(!ahi_req[0]){ printf("AHI: no ioreq\n"); return 0; }
    ahi_req[0]->ahir_Version = 4;
    ahi_dev_open = OpenDevice(AHINAME, AHI_DEFAULT_UNIT,
                              (struct IORequest*)ahi_req[0], 0);
    if(ahi_dev_open){ printf("AHI: OpenDevice failed\n"); return 0; }

    ahi_req[1] = (struct AHIRequest*)AllocMem(
                     sizeof(struct AHIRequest), MEMF_PUBLIC|MEMF_CLEAR);
    if(!ahi_req[1]){ printf("AHI: no req[1]\n"); return 0; }
    *ahi_req[1] = *ahi_req[0];

    for(i = 0; i < 2; i++){
        ahi_buf[i] = (WORD*)AllocMem(AHI_BUF_SAMPLES * 2,
                                     MEMF_PUBLIC|MEMF_CLEAR);
        if(!ahi_buf[i]){ printf("AHI: no buf[%d]\n", i); return 0; }
    }

    printf("AHI: event-driven double-buf, %d samples/buf, sig=0x%08lx\n",
           AHI_BUF_SAMPLES, zz_ahi_signal);
    return 1;
}

void zz_ahi_start(void)
{
    prepare_and_send(0, NULL);
    prepare_and_send(1, ahi_req[0]);
}

/* Called when reply port signals (buffer finished).
   Drain all completed requests and immediately refill+resend. */
void zz_ahi_service(void)
{
    struct AHIRequest *done;
    while((done = (struct AHIRequest*)GetMsg(ahi_mp))){
        /* Find which request finished */
        if(done == ahi_req[0]){
            /* refill buf0, chain behind req1 (which is still in flight) */
            prepare_and_send(0, ahi_req[1]);
        } else {
            /* refill buf1, chain behind req0 */
            prepare_and_send(1, ahi_req[0]);
        }
    }
}

/* Legacy non-blocking poll (fallback, called each loop iteration) */
void zz_ahi_poll(void)
{
    zz_ahi_service();
}

void zz_ahi_stop(void)
{
    int i;
    printf("AHI: refills=%lu underruns=%lu\n",
           ahi_refill_count, ahi_underrun_arm);
    for(i = 0; i < 2; i++){
        if(ahi_req[i]){
            AbortIO((struct IORequest*)ahi_req[i]);
            WaitIO((struct IORequest*)ahi_req[i]);
        }
    }
    if(ahi_dev_open == 0){
        CloseDevice((struct IORequest*)ahi_req[0]);
        ahi_dev_open = -1;
    }
    if(ahi_req[0]) DeleteIORequest((struct IORequest*)ahi_req[0]);
    if(ahi_req[1]) FreeMem(ahi_req[1], sizeof(struct AHIRequest));
    for(i = 0; i < 2; i++)
        if(ahi_buf[i]) FreeMem(ahi_buf[i], AHI_BUF_SAMPLES * 2);
    if(ahi_mp) DeleteMsgPort(ahi_mp);
}
