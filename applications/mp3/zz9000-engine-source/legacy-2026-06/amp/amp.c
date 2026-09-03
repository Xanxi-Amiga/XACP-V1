
/* amp.c   main interface to decoding functions
 *
 * Created by: Thomas Wenzel  Sept 2021
 */

#include <math.h>

#include "hton.h"
#include "amp.h"
#include "fifo/fifo.h"
#include "audio.h"
#include "getbits.h"
#include "fft_new.h"
#include "layer2.h"
#include "layer3.h"
#include "transform.h"
#include "callbacks.h"

#include "../MyStdlib.h"

//#include "printf/printf.h"

#define AMP_MP2_DECODER_DELAY 0
#define AMP_MP3_DECODER_DELAY 528

static float EngineFFTWin[1024];
static float EngineFFTdataL[1024];
static float EngineFFTdataR[1024];

/* call this once at the beginning */
unsigned long amp_start(void) {
	initialise_decoder();
	blackman_harris_74db_window(EngineFFTWin, 1024);
 	return HTONL(1);
}

unsigned long amp_stop(void) {
	return HTONL(1);
}

/* call this before each file is played */
unsigned long amp_init(struct InitAmpParameters *ip) {
	struct Amp *itsAmp;
	if(itsAmp = my_malloc(sizeof(struct Amp))) {
		initialise_globals(itsAmp);
		itsAmp->itsFifo        = ip->itsFifo;
		itsAmp->MP2TotalDelay  = 482; // empirical value
		itsAmp->MP3TotalDelay  = NTOHL(ip->Delay) + AMP_MP3_DECODER_DELAY;
		itsAmp->EncoderPadding = NTOHL(ip->Padding);
		itsAmp->DstPtr         = NULL;
		itsAmp->EqualizerL     = NULL;
		itsAmp->EqualizerR     = NULL;
 		return (unsigned long)itsAmp;
	}
	return 0;
}

unsigned long amp_exit(struct ExitAmpParameters *ep) {
	struct Amp *itsAmp = ep->itsAmp;

	if(itsAmp) {
		my_free(itsAmp);
	}
	return HTONL(0);
}

unsigned long amp_decode(struct DecodeParameters *dp) {
	struct Amp  *itsAmp  = dp->itsAmp;

	unsigned long   NumFrames  = NTOHL(dp->NumFrames);
	unsigned long   VisFrames  = NTOHL(dp->VisFrames);
	unsigned short  Layer      = NTOHS(dp->Layer);
	unsigned short  Channels   = NTOHS(dp->Channels);
	short*          DstBuffer  = NTOHP(dp->DstBuffer);
	float*          EqualizerL = NTOHP(dp->EqualizerL);
	float*          EqualizerR = NTOHP(dp->EqualizerR);
	unsigned short* SpecRawL   = NTOHP(dp->SpecRawL);
	unsigned short* SpecRawR   = NTOHP(dp->SpecRawR);

	struct Fifo *itsFifo = itsAmp->itsFifo;
	int HeaderError, LayerError;
	struct AUDIO_HEADER AudioHeader;
	float *sl;
	float *sr;
	short *OldDstPtr;
	long h,i,j,k,errors;

	static float SpecL[576];
	static float SpecR[576];

	itsAmp->DstPtr        = DstBuffer;
	itsAmp->EqualizerL    = EqualizerL;
	itsAmp->EqualizerR    = EqualizerR;
	itsAmp->DecodeEOF     = 0;
	itsAmp->DecodedBytes  = 0;

	LayerError = 0;
	HeaderError = 0;
	sl = NULL;
	sr = NULL;
	errors = 0;
//	printf("Decode...\n");
	for(h=0; h<VisFrames; h++) {
		for(k=0; k<576; k++) {
			SpecL[k]=0;
			SpecR[k]=0;
		}
		for(i=0; i<NumFrames; i++) {
			HeaderError = gethdr(itsAmp, &AudioHeader);
			if(AudioHeader.protection_bit==0) getcrc(itsAmp);
			if(HeaderError == GETHDR_EOF) {
				itsAmp->DecodeEOF = 1;
				break;
			}

			if(!HeaderError) {
				unsigned short FrameLayer;
				unsigned short FrameChannels;
				FrameLayer	  = 4-AudioHeader.layer;
				FrameChannels = AudioHeader.mode==3 ? 1 : 2;
				// We expect Layer and Channels not to change suddenly within the stream.
				// If it seems to do so then it's probably in fact a bad frame.
				// Attention! Some streams have both Stereo and J-Stereo frames!
				// So we can't just compare modes. We really need to compare channels.
				if(Layer    != FrameLayer)    HeaderError=1;
				if(Channels != FrameChannels) HeaderError=1;
			}

			if(!HeaderError) {
				OldDstPtr = itsAmp->DstPtr;
				if(Layer==3) {
					LayerError=layer3_frame(itsAmp, &AudioHeader, 0);
					sl=&itsAmp->xr[0][0][0];	// Layer3 Spectrum
					sr=&itsAmp->xr[1][0][0];
				}
				else if (Layer==2) {
					LayerError=layer2_frame(itsAmp, &AudioHeader, 0);
					sl=&itsAmp->subband_sample[0][0][0];	// Layer2 Subbands
					sr=&itsAmp->subband_sample[1][0][0];
				}
			}

			if(HeaderError || LayerError) {
				itsAmp->DstPtr = (short*)NTOHL((unsigned long)dp->DstBuffer);
				itsAmp->DecodedBytes = 0;
				i = 0;
				errors++;
				if(errors > 10) {
					itsAmp->DecodeEOF = 1;
					break;
				}
			}

			if(!HeaderError && sl && sr) {
				for(k=0; k<576; k++) {
					if(sl[k] > 0.0) SpecL[k] += sl[k];
					else SpecL[k] -= sl[k];
					if(sr[k] > 0.0) SpecR[k] += sr[k];
					else SpecR[k] -= sr[k];
					if (itsAmp->nch==1) SpecR[k]=SpecL[k];
				}
			}
		}

		if(!HeaderError) {
			for(k=0; k<512; k++) {				
				float vl,vr;
				float ll,lr;
				SpecL[k] /= 16;
				SpecR[k] /= 16;
				SpecL[k] /= NumFrames;
				SpecR[k] /= NumFrames;

				vl=SpecL[k];
				vr=SpecR[k];
				if(vl<0.000000000001) vl=0.000000000001;
				if(vr<0.000000000001) vr=0.000000000001;
				ll=98304.0+12288.0*log(vl);
				lr=98304.0+12288.0*log(vr);

				if(ll<0.0) ll=0.0;
				if(ll>65535.0) ll=65535.0;
				if(lr<0.0) lr=0.0;
				if(lr>65535.0) lr=65535.0;

				if (itsAmp->nch==1) lr=ll;

				SpecRawL[576*h+k] = HTONS((unsigned short)ll);
				SpecRawR[576*h+k] = HTONS((unsigned short)lr);
			}
			for(k=512; k<576; k++) {
				SpecRawL[576*h+k] = 0;
				SpecRawR[576*h+k] = 0;
			}
		}
	}

	if(itsFifo->FillLevel <= 0) itsAmp->DecodeEOF = 1;
	dp->DecodeEOF = HTONL(itsAmp->DecodeEOF);

	if(itsAmp->DecodeEOF) {	
		long FramesToCrop;
		if(Layer == 2) {
			FramesToCrop = AMP_MP2_DECODER_DELAY;
			if(itsAmp->EncoderPadding > AMP_MP2_DECODER_DELAY) {
				FramesToCrop = itsAmp->EncoderPadding - AMP_MP2_DECODER_DELAY;
			}
			itsAmp->DecodedBytes -= itsAmp->nch*sizeof(short)*FramesToCrop;
			if(itsAmp->DecodedBytes < 0) itsAmp->DecodedBytes = 0;
		}
		else {
			FramesToCrop = AMP_MP3_DECODER_DELAY;
			// Padding is usually at least 576 which is always larger than AMP_MP3_DECODER_DELAY.
			if(itsAmp->EncoderPadding > AMP_MP3_DECODER_DELAY) {
				FramesToCrop = itsAmp->EncoderPadding - AMP_MP3_DECODER_DELAY;
			}
			itsAmp->DecodedBytes -= itsAmp->nch*sizeof(short)*FramesToCrop;
			if(itsAmp->DecodedBytes < 0) itsAmp->DecodedBytes = 0;
		}
	}
//	printf("end\n");

	return HTONL(itsAmp->DecodedBytes);
}

long amp_input_callback(struct Amp *itsAmp, void *buffer, long BytesToRead) {
	struct Fifo *itsFifo = itsAmp->itsFifo;
	unsigned long BytesRead;
	unsigned char *dst;

	BytesRead = 0;
	dst = buffer;
	while(BytesToRead) {
		// If FiFo is empty then exit the loop.
		if(itsFifo->FillLevel <= 0) break;
		dst[BytesRead++] = itsFifo->Buffer[itsFifo->ReadIdx++];
		if(itsFifo->ReadIdx >= itsFifo->Size) itsFifo->ReadIdx = 0;
		itsFifo->FillLevel--;
		itsFifo->Space++;
		BytesToRead--;
	}

	return BytesRead;
}

void amp_output_callback(struct Amp *itsAmp, unsigned char Layer) {
	short *SrcPtr = (short*)itsAmp->sample_buffer;
	long FramesToCopy = 576;
	long StartFrame;
	unsigned long BytesToCopy;
	unsigned long ShortsToCopy;
	int i;

	if(Layer == 2) {
		if(itsAmp->MP2TotalDelay >= FramesToCopy) {
			itsAmp->MP2TotalDelay -= FramesToCopy;
			return;
		}
		else {
			FramesToCopy -= itsAmp->MP2TotalDelay;
			itsAmp->MP2TotalDelay = 0;
		}
	}
	else {
		if(itsAmp->MP3TotalDelay >= FramesToCopy) {
			itsAmp->MP3TotalDelay -= FramesToCopy;
			return;
		}
		else {
			FramesToCopy -= itsAmp->MP3TotalDelay;
			itsAmp->MP3TotalDelay = 0;
		}
	}

	BytesToCopy = FramesToCopy*2;
	ShortsToCopy = FramesToCopy;
	StartFrame = 576 - FramesToCopy;
	if(itsAmp->nch==2) {
		BytesToCopy *= 2;
		ShortsToCopy *= 2;
		StartFrame *= 2;
	}

	for(i=0; i<ShortsToCopy; i++) {
		*itsAmp->DstPtr++ = HTONS(SrcPtr[StartFrame+i]);
	}

	itsAmp->DecodedBytes   += BytesToCopy;
}

void amp_equalize_callback(struct Amp *itsAmp, unsigned char Layer) {
	float *sl, *sr;
	unsigned long i,j;

	static const unsigned long BandIndex[32] = {
		  0,  13,  26,	39,  52,  65,  78,	91,
		104, 117, 130, 143, 156, 169, 182, 195,
		208, 221, 234, 247, 260, 273, 286, 299,
		312, 325, 338, 351, 364, 377, 390, 403
	};

	if(itsAmp->EqualizerL == NULL) return;
	if(itsAmp->EqualizerR == NULL) return;

	switch(Layer) {
		case 2:
			for(i=0; i<32; i++) {
				for(j=0; j<36; j++) {
					itsAmp->subband_sample[0][i][j] *= NTOHF(itsAmp->EqualizerL[BandIndex[i]]);
					itsAmp->subband_sample[1][i][j] *= NTOHF(itsAmp->EqualizerR[BandIndex[i]]);
				}
			}
		break;

		case 3:
			sl=&itsAmp->xr[0][0][0];
			sr=&itsAmp->xr[1][0][0];
			for(i=0; i<576; i++) {
				sl[i] *=      NTOHF(itsAmp->EqualizerL[i]);
				sr[i] *= sqrt(NTOHF(itsAmp->EqualizerR[i]));
			}
		break;
	}
}


