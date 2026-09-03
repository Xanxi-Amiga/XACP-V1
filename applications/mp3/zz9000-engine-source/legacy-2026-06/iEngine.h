
#ifndef _IAMIGA_H_
#define _IAMIGA_H_

// Both sides of the interface need to include engine/engine.h
#include "engine/engine.h"

char* iEngine_get_name(void);
char* iEngine_get_capability(unsigned long capability);

// Shared memory getters
unsigned long iEngine_getNumFrames(void);
unsigned char* iEngine_getOutBuffer(void);
float* iEngine_getEqualizerL(void);
float* iEngine_getEqualizerR(void);
unsigned short* iEngine_getSpecRawL(void);
unsigned short* iEngine_getSpecRawR(void);

// fifo access
unsigned long iEngine_fifo_init(struct InitFifoParameters *ifp);
unsigned long iEngine_fifo_exit(struct ExitFifoParameters *efp);
unsigned long iEngine_fifo_clear(struct ClearFifoParameters *cfp);
unsigned long iEngine_fifo_fill(struct FillFifoParameters *ffp);

// equalizer access
unsigned long iEngine_equalizer_init(struct InitEqualizerParameters *iep);
unsigned long iEngine_equalizer_exit(struct ExitEqualizerParameters *eep);
unsigned long iEngine_equalizer_config(struct ConfigEqualizerParameters *cep);
unsigned long iEngine_equalizer_set(struct SetEqualizerParameters *sep);
unsigned long iEngine_equalizer_run(struct RunEqualizerParameters *rep);

// amp access
unsigned long iEngine_amp_start(void);
unsigned long iEngine_amp_stop(void);
unsigned long iEngine_amp_init(struct InitAmpParameters *ip);
unsigned long iEngine_amp_exit(struct ExitAmpParameters *ep);
unsigned long iEngine_amp_decode(struct DecodeParameters *dp);

#endif


