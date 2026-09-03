/*
 *	Equalizer filter, implementation of a 10 band time domain graphic equalizer
 *	using IIR filters.	The IIR filters are implemented using a Direct Form II
 *	approach, modified (b1 == 0 always) to save computation.
 *
 *	This software has been released under the terms of the GNU General Public
 *	license.  See http://www.gnu.org/copyleft/gpl.html for details.
 *
 *	Copyright 2001 Anders Johansson <ajh@atri.curtin.edu.au>
 *
 *	Adapted for Audacious by John Lindgren, 2010
 *
 *	Adapted for EngineLibrary by Thomas Wenzel, 2021
 */

#include <math.h>
#include <string.h>

#include "hton.h"
#include "equalizer.h"

#include "../MyStdlib.h"



/* Q value for band-pass filters 1.2247 = (3/2)^(1/2)
 * Gives 4 dB suppression at Fc*2 and Fc/2 */
#define Q 1.2247449

/* Center frequencies for band-pass filters (Hz) */
/* These are not the historical WinAmp frequencies, because the IIR filters used
 * here are designed for each frequency to be twice the previous.  Using WinAmp
 * frequencies leads to too much gain in some bands and too little in others. */
static const float CF[EQ_BANDS] = {31.25, 62.5, 125, 250, 500, 1000, 2000, 4000, 8000, 16000};

/* 2nd order band-pass filter design */
static void bp2 (float * a, float * b, float fc, float q) {
	double th = 2 * 3.14159265 * fc;
	double C = (1 - tan (th * q / 2)) / (1 + tan (th * q / 2));

	a[0] = (1 + C) * cos (th);
	a[1] = -C;
	b[0] = (1 - C) / 2;
	b[1] = -1.005;
}

static void eq_set_channel_bands (struct Equalizer *itsEqualizer, int channel, float * bands) {
	int k;

	for(k = 0; k < EQ_BANDS; k ++) {
		itsEqualizer->gv[channel][k] = pow (10, bands[k] / 20) - 1;
	}
}

unsigned long equalizer_init(struct InitEqualizerParameters *iep) {
	struct Equalizer *itsEqualizer;

	if(itsEqualizer = my_malloc(sizeof(struct Equalizer))) {
		return (unsigned long)itsEqualizer;
	}
	return 0;
}

unsigned long equalizer_exit(struct ExitEqualizerParameters *eep) {
	struct Equalizer *itsEqualizer = eep->itsEqualizer;

	if(itsEqualizer) {
		my_free(itsEqualizer);
	}
	return 0;
}

unsigned long equalizer_config(struct ConfigEqualizerParameters *ecp) {
	struct Equalizer *itsEqualizer = ecp->itsEqualizer;
	int k;

	itsEqualizer->channels = NTOHL(ecp->new_channels);
	itsEqualizer->rate     = NTOHL(ecp->new_rate);

	/* Calculate number of active filters */
	itsEqualizer->K = EQ_BANDS;

	while(CF[itsEqualizer->K - 1] > (float) itsEqualizer->rate / 2.2) itsEqualizer->K --;

	/* Generate filter taps */
	for(k = 0; k < itsEqualizer->K; k ++) {
		bp2 (itsEqualizer->a[k], itsEqualizer->b[k], CF[k] / (float) itsEqualizer->rate, Q);
	}

	/* Reset state */
	my_memset(itsEqualizer->wqv[0][0], 0, sizeof(float)*MAX_CHANNELS*EQ_BANDS*2);

	return HTONL(0);
}

unsigned long equalizer_set(struct SetEqualizerParameters *esp) {
	struct Equalizer *itsEqualizer = esp->itsEqualizer;
	float *bands = (float*)NTOHL(esp->bands);
	int i;

	for(i=0; i<MAX_CHANNELS;i ++) {
		eq_set_channel_bands(itsEqualizer, i, bands);
	}

	return HTONL(0);
}

unsigned long equalizer_run(struct RunEqualizerParameters *rep) {
	struct Equalizer *itsEqualizer = rep->itsEqualizer;
	short*        data    = (short*)NTOHL(rep->data);
	unsigned long samples = NTOHL(rep->samples);
	int channel;
	int i;

	for(channel=0; channel<itsEqualizer->channels; channel++) {
		int sample;
		float *g = itsEqualizer->gv[channel]; // Gain factor

		for(sample=0; sample<samples; sample++) {
			int k; // Frequency band index */
			short yt; // Current input sample (short)
			float ytf; // Current input sample (float)

			yt = NTOHS(rep->data[itsEqualizer->channels * sample + channel]);
			ytf = yt;
			// Run IIR filter.
			for (k = 0; k < itsEqualizer->K; k ++) {
				// Pointer to circular buffer wq
				float * wq = itsEqualizer->wqv[channel][k];
				// Calculate output from AR part of current filter
				float w = ytf * itsEqualizer->b[k][0] + wq[0] * itsEqualizer->a[k][0] + wq[1] * itsEqualizer->a[k][1];
				// Calculate output from MA part of current filter
				ytf += (w + wq[1] * itsEqualizer->b[k][1]) * g[k];
				// Update circular buffer
				wq[1] = wq[0];
				wq[0] = w;
			}

			// Calculate output (with clipping)
			if(ytf < -32768.0) ytf = -32768.0;
			if(ytf >  32767.0) ytf =  32767.0;

			yt = ytf;
			rep->data[itsEqualizer->channels * sample + channel] = HTONS(yt);
		}
	}

	return HTONL(0);
}

