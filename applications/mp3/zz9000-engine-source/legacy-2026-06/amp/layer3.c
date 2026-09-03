/* this file is a part of amp software, (C) tomislav uzelac 1996,1997
*/

/* layer3.c  layer3 audio decoding
 *
 * Created by: tomislav uzelac  Mar  1 97
 *
 * Adapted by: Thomas Wenzel Sept 2021
 */
#include "config.h"
#include "amp.h"
#include "audio.h"
#include "getbits.h"
#include "getdata.h"
#include "huffman.h"
#include "misc2.h"
#include "transform.h"
#include "callbacks.h"
#include "layer3.h"

/* this function decodes one layer3 audio frame, except for the header decoding */
/* which is done in main() [audio.c]. returns 0 if everything is ok.            */
int layer3_frame(struct Amp *itsAmp, struct AUDIO_HEADER *header,int cnt) {
	static struct SIDE_INFO info;

	int gr,ch,sb,i;
	int mean_frame_size,bitrate,fs,hsize,ssize;

	/* we need these later, hsize is the size of header+side_info */
	if(header->ID) {
		if(header->mode==3) {
			itsAmp->nch=1;
			hsize=21;
		} else {
			itsAmp->nch=2;
			hsize=36;
		}
	}
	else {
		if(header->mode==3) {
			itsAmp->nch=1;
			hsize=13;
		} else {
			itsAmp->nch=2;
			hsize=21;
		}
	}

	/* crc increases hsize by 2 */
	if (header->protection_bit==0) hsize+=2;

	/* read layer3 specific side_info */
	getinfo(itsAmp, header,&info);

	/* MPEG2 only has one granule */
	bitrate=t_bitrate[header->ID][3-header->layer][header->bitrate_index];

	fs=t_sampling_frequency[header->mpeg25][header->ID][header->sampling_frequency];
	if (header->ID) mean_frame_size=144000*bitrate/fs;
	else mean_frame_size=72000*bitrate/fs;


	/* check if mdb is too big for the first few frames. this means that          */
	/* a part of the stream could be missing. We must still fill the buffer       */
	/*                                                                            */
	/* don't forget to (re)initialise bclean_bytes to 0, and f_bdirty to FALSE!!! */
	if (itsAmp->f_bdirty) {
		if (info.main_data_begin > itsAmp->bclean_bytes) {
			fillbfr(itsAmp, mean_frame_size + header->padding_bit - hsize);
			itsAmp->bclean_bytes+=mean_frame_size + header->padding_bit - hsize;
			/* warn(" frame %d discarded, incomplete main_data\n",cnt); */
			return 0;
		} else {
			/* re-initialise */
			itsAmp->f_bdirty=0;
			itsAmp->bclean_bytes=0;
		}
	}

	/* now update the data 'pointer' (counting in bits) according to  */
	/* the main_data_begin information                                */
	itsAmp->data = 8 * ((itsAmp->append - info.main_data_begin) & (BUFFER_SIZE-1));

	/* read into the buffer all bytes up to the start of next header */
	fillbfr(itsAmp, mean_frame_size + header->padding_bit - hsize);

	/* these two should go away */
	itsAmp->t_l=&t_b8_l[header->ID][header->sampling_frequency][0];
	itsAmp->t_s=&t_b8_s[header->ID][header->sampling_frequency][0];

	/* decode the scalefactors and huffman data            */
	/* this part needs to be enhanced for error robustness */
	for (gr=0;gr < ((header->ID) ? 2 : 1);gr++) {
		for (ch=0;ch<itsAmp->nch;ch++) {
			ssize=decode_scalefactors(itsAmp, &info,header,gr,ch);
			decode_huffman_data(itsAmp, &info,gr,ch,ssize);
		}
		
		/* requantization, stereo processing, reordering(shortbl) */
		if (header->mode!=1 || (header->mode==1 && header->mode_extension==0)) {
			for (ch=0;ch<itsAmp->nch;ch++) requantize_mono(itsAmp,gr,ch,&info,header);
		}
		else {
			requantize_ms(itsAmp, gr,&info,header);
		}

		/* just which window? */

		for (ch=0; ch < itsAmp->nch; ch++) {
			int win_type; /* same as in the standard, long=0, start=1 ,.... */
			int window_switching_flag = info.window_switching_flag[gr][ch];
			int block_type = info.block_type[gr][ch];
			int mixed_block_flag = info.mixed_block_flag[gr][ch];

			/* antialiasing butterflies */
			if (!(window_switching_flag && block_type==2)) alias_reduction(itsAmp,ch);

			if (window_switching_flag && block_type==2 && mixed_block_flag) win_type=0;
			else if (!window_switching_flag) win_type=0;
			else win_type=block_type;

			/* Equalizer ... */
			amp_equalize_callback(itsAmp, 3);

			/* imdct ... */
			for (sb=0;sb<2;sb++) imdct(itsAmp, win_type,sb,ch);

			if(window_switching_flag && block_type==2 && mixed_block_flag) win_type=2;

			/* no_of_imdcts tells us how many subbands from the top are all zero */
			/* it is set by the requantize functions in misc2.c                  */
			for(sb=2;sb<itsAmp->no_of_imdcts[ch];sb++) {
				imdct(itsAmp, win_type,sb,ch);
			}
			for(;sb<32;sb++) {
				for(i=0;i<18;i++) {
					itsAmp->res[sb][i]=itsAmp->s[ch][sb][i];
					itsAmp->s[ch][sb][i]=0.0f;
				}
			}

			/* polyphase filterbank */
			for(i=0;i<18;i++) poly(itsAmp,ch,i);
		}
		amp_output_callback(itsAmp, 3);
	}
	/* return status: 0 for ok, errors will be added */
	return 0;
} 
