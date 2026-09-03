
/* amp.h   main interface to decoding functions
 *
 * Created by: Thomas Wenzel  Sept 2021
 */


#ifndef _AMP_H_
#define _AMP_H_

#include "ampconfig.h"
#include "engine/engine.h"

struct Amp {
	void *itsFifo;
	/* buffer, AUX is used in case of input buffer "overflow", and its contents
	 * are copied to the beginning of the buffer */
	unsigned char buffer[BUFFER_SIZE+BUFFER_AUX];
	/* bit reservoir stuff. f_bdirty must be set to TRUE when starting play!
	 */
	int f_bdirty,bclean_bytes;
	/* the maximum value of is_pos. for short blocks is_max[sfb=0] == is_max[6],
	* it's sloppy but i'm sick of waisting storage. blaah...
	*/
	int is_max[21];
	int intensity_scale;
	int append;
	int data;
	int nch;
	int non_zero[2]; /* this is 2*bigvalues+4*count1, i guess...*/
	int no_of_imdcts[2];

	unsigned long MP2TotalDelay;
	unsigned long MP3TotalDelay;
	unsigned long EncoderPadding;
	short *DstPtr; // Pointer to destination data in network byte order
	float *EqualizerL;
	float *EqualizerR;
	int DecodeEOF;
	long DecodedBytes;

	short sample_buffer[18][32][2];	
	float s[2][32][18];	
	float res[32][18];
	int scalefac_l[2][2][22];
	int scalefac_s[2][2][13][3];	
	int is[2][578];
	float xr[2][32][18];
	float subband_sample[2][32][36];
	const int *t_l;
	const int *t_s;

	float u[2][2][17][16]; /* no v[][], it's redundant */
	int u_start[2]; /* first element of u[][] */
	int u_div[2];
};

unsigned long amp_start(void);
unsigned long amp_stop(void);
unsigned long amp_init(struct InitAmpParameters *ip);
unsigned long amp_exit(struct ExitAmpParameters *ep);
unsigned long amp_decode(struct DecodeParameters *dp);

#endif

