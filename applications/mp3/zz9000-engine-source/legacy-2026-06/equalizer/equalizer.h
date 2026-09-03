
#include "engine/engine.h"

#ifndef _EQUALIZER_H_
#define _EQUALIZER_H_

#define EQ_BANDS 10
#define MAX_CHANNELS 2

struct Equalizer {
	unsigned long channels;
	unsigned long rate;
	float a[EQ_BANDS][2]; /* A weights */
	float b[EQ_BANDS][2]; /* B weights */
	float wqv[MAX_CHANNELS][EQ_BANDS][2]; /* Circular buffer for W data */
	float gv[MAX_CHANNELS][EQ_BANDS]; /* Gain factor for each channel and band */
	long K; /* Number of used eq bands */
};

unsigned long equalizer_init(struct InitEqualizerParameters *iep);
unsigned long equalizer_exit(struct ExitEqualizerParameters *eep);
unsigned long equalizer_config(struct ConfigEqualizerParameters *ecp);
unsigned long equalizer_set(struct SetEqualizerParameters *esp);
unsigned long equalizer_run(struct RunEqualizerParameters *rep);

#endif

