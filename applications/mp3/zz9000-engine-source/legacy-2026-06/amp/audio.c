
/* this file is a part of amp software, (C) tomislav uzelac 1996,1997
*/

/* audio.c   MPEG audio support
 *
 * Created by: Tomislav Uzelac  Mar 1996
 * merged with amp, May 19 1997
 *
 * Adapted by: Thomas Wenzel Sept 2021
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "amp.h"
#include "audio.h"
#include "getbits.h"
#include "huffman.h"
#include "layer2.h"
#include "layer3.h"
#include "transform.h"
#include "misc2.h"

#include "../MyStdlib.h"

const int t_sampling_frequency[2][2][3] = {{
	{ 22050 , 24000 , 16000},
	{ 44100 , 48000 , 32000}
},{
	{ 11025 , 12000 ,  8000},
	{ 11025 , 12000 ,  8000}
}};

const short t_bitrate[2][3][15] = {{
	{0,32,48,56,64,80,96,112,128,144,160,176,192,224,256},
	{0,8,16,24,32,40,48,56,64,80,96,112,128,144,160},
	{0,8,16,24,32,40,48,56,64,80,96,112,128,144,160}
},{
	{0,32,64,96,128,160,192,224,256,288,320,352,384,416,448},
	{0,32,48,56,64,80,96,112,128,160,192,224,256,320,384},
	{0,32,40,48,56,64,80,96,112,128,160,192,224,256,320}
}};

/* the last sfb is given implicitly on pg.28. of the standard. scalefactors 
 * for that one are 0, pretab also 
 */
/* leftmost index denotes ID, so first three tables are for MPEG2 (header->ID==0)
 * and the other three are for MPEG1 (header->ID==1)
 */
/* 22.05, 24, 16 */
const int t_b8_l[2][3][22]={{ /* table B.8b ISO/IEC 11172-3 */
	{5,11,17,23,29,35,43,53,65,79,95,115,139,167,199,237,283,335,395,463,521,575},
	{5,11,17,23,29,35,43,53,65,79,95,113,135,161,193,231,277,331,393,463,539,575},
	{5,11,17,23,29,35,43,53,65,79,95,115,139,167,199,237,283,335,395,463,521,575}
},{
	{3,7,11,15,19,23,29,35,43,51,61,73,89,109,133,161,195,237,287,341,417,575},
	{3,7,11,15,19,23,29,35,41,49,59,71,87,105,127,155,189,229,275,329,383,575},
	{3,7,11,15,19,23,29,35,43,53,65,81,101,125,155,193,239,295,363,447,549,575}
}};   
const int t_b8_s[2][3][13]={{ /* table B.8b ISO/IEC 11172-3 */
	{3,7,11,17,23,31,41,55,73,99,131,173,191},
	{3,7,11,17,25,35,47,61,79,103,135,179,191},
	{3,7,11,17,25,35,47,61,79,103,133,173,191}
},{
	{3,7,11,15,21,29,39,51,65,83,105,135,191},
	{3,7,11,15,21,27,37,49,63,79,99,125,191},
	{3,7,11,15,21,29,41,57,77,103,137,179,191}
}};

/* call this once at the beginning */
void initialise_decoder(void) {
	premultiply();
	imdct_init();
	calculate_t43();
}

/* call this before each file is played */
void initialise_globals(struct Amp *itsAmp) {
	itsAmp->append       = 0;
	itsAmp->data         = 0;
	itsAmp->nch          = 0;
	itsAmp->f_bdirty     = 1;
	itsAmp->bclean_bytes = 0;

	my_memset(itsAmp->s,0,sizeof(itsAmp->s));
	my_memset(itsAmp->res,0,sizeof(itsAmp->res));
	my_memset(itsAmp->u_start,0,sizeof(itsAmp->u_start));
	my_memset(itsAmp->u_div,0,sizeof(itsAmp->u_div));
}

void report_header_error(int err) {
#if 0
	switch (err) {
		case GETHDR_ERR: fprintf(stderr, "error reading mpeg bitstream. exiting.\n");
		                 break;
		case GETHDR_NS:  fprintf(stderr, "this is a file in MPEG 2.5 format, which is not defined\n");
		                 fprintf(stderr, "by ISO/MPEG. It is \"a special Fraunhofer format\".\n");
		                 fprintf(stderr, "amp does not support this format. sorry.\n");
		                 break;
		case GETHDR_FL1: fprintf(stderr, "ISO/MPEG layer 1 is not supported by amp (yet).\n");
		                 break;
		case GETHDR_FF:  fprintf(stderr, "free format bitstreams are not supported. sorry.\n");
		                 break;	
		case GETHDR_SYN: fprintf(stderr, "oops, we're out of sync.\n");
                     break;
		case GETHDR_EOF: fprintf(stderr, "EOF!\n");
		                 break;
	}
#endif
}

void die(char *str, ...) {
}
