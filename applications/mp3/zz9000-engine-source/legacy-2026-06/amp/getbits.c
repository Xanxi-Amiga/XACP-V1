
/* this file is a part of amp software, (C) tomislav uzelac 1996,1997
*/
 
/* getbits.c  bit level routines, input buffer
 * 
 * Created by: tomislav uzelac  Apr 1996 
 * better synchronization, tomislav uzelac, Apr 23 1997
 *
 * Adapted by: Thomas Wenzel Sept 2021
 */
#include "compiler.h"
#include "config.h"
#include "amp.h"
#include "audio.h"
#include "formats.h"
//#include "rtbuf.h"

#define	GETBITS
#include "getbits.h"

#include "../MyStdlib.h"

/* internal buffer, _bptr holds the position in _bits_
 */
static unsigned char _buffer[32];
static int _bptr;

int get_input(struct Amp *itsAmp, unsigned char* bp, unsigned int size);
long amp_input_callback(struct Amp *itsAmp, void *buffer, long DataToRead);

/* 
 * buffer and bit manipulation functions ***************************************
 */
int _fillbfr(struct Amp *itsAmp, unsigned int size)
{
	_bptr=0;
        return get_input(itsAmp, _buffer, size);
}

int readsync(struct Amp *itsAmp)
{
	_bptr=0;
	_buffer[0]=_buffer[1];
	_buffer[1]=_buffer[2];
	_buffer[2]=_buffer[3];
	return get_input(itsAmp, &_buffer[3],1);
}

unsigned int _getbits(struct Amp *itsAmp, int n)
{
unsigned int pos,ret_value;

        pos = _bptr >> 3;
	ret_value = _buffer[pos] << 24 |
		    _buffer[pos+1] << 16 |
		    _buffer[pos+2] << 8 |
		    _buffer[pos+3];
        ret_value <<= _bptr & 7;
        ret_value >>= 32 - n;
        _bptr += n;
        return ret_value;
}       

int fillbfr(struct Amp *itsAmp, unsigned int advance) {
	int overflow,retval;

	// First sanity check
	if((itsAmp->append) >= (BUFFER_SIZE+BUFFER_AUX)) {
		return GETHDR_ERR;
	}

	// Second sanity check
	if((itsAmp->append+advance) >= (BUFFER_SIZE+BUFFER_AUX)) {
		return GETHDR_ERR;
	}

	retval = get_input(itsAmp, &itsAmp->buffer[itsAmp->append], advance);
	
	if ( itsAmp->append + advance >= BUFFER_SIZE ) {
		overflow = itsAmp->append + advance - BUFFER_SIZE;
		my_memcpy (itsAmp->buffer,&itsAmp->buffer[BUFFER_SIZE], overflow);
		if (overflow < 4) my_memcpy(&itsAmp->buffer[BUFFER_SIZE],itsAmp->buffer,4);
		itsAmp->append = overflow;
	} else {
		if (itsAmp->append==0) my_memcpy(&itsAmp->buffer[BUFFER_SIZE],itsAmp->buffer,4);
		itsAmp->append+=advance;
	}
	return retval;
}

unsigned int getbits(struct Amp *itsAmp, int n)
{
        if (n) {
        unsigned int pos,ret_value;

                pos = itsAmp->data >> 3;
                ret_value = itsAmp->buffer[pos] << 24 |
                        itsAmp->buffer[pos+1] << 16 |
                        itsAmp->buffer[pos+2] << 8 |
                        itsAmp->buffer[pos+3];
                ret_value <<= itsAmp->data & 7;
                ret_value >>= 32 - n;

                itsAmp->data += n;
                itsAmp->data &= (8*BUFFER_SIZE)-1;

                return ret_value;
        } else
                return 0;
}

/*
 * header and side info parsing stuff ******************************************
 */
void parse_header(struct Amp *itsAmp, struct AUDIO_HEADER *header) 
{
        header->ID=_getbits(itsAmp, 1);
        header->layer=_getbits(itsAmp, 2);
        header->protection_bit=_getbits(itsAmp, 1);
        header->bitrate_index=_getbits(itsAmp, 4);
        header->sampling_frequency=_getbits(itsAmp, 2);
        header->padding_bit=_getbits(itsAmp, 1);
        header->private_bit=_getbits(itsAmp, 1);
        header->mode=_getbits(itsAmp, 2);
        header->mode_extension=_getbits(itsAmp, 2);
        if (!header->mode) header->mode_extension=0;
        header->copyright=_getbits(itsAmp, 1);
        header->original=_getbits(itsAmp, 1);
        header->emphasis=_getbits(itsAmp, 2);
}

int header_sanity_check(struct AUDIO_HEADER *header)
{
	if ( 	header->layer==0 ||
		header->bitrate_index==15 ||
		header->sampling_frequency==3) return -1;

	/* an additional check to make shure that stuffing never gets mistaken
 	 * for a syncword. This rules out some legal layer1 streams, but who
 	 * cares about layer1 anyway :-). I must get this right sometime.
	 */
	if ( header->ID==1 && header->layer==3 && header->protection_bit==1) return -1;
	return 0;
}


int gethdr(struct Amp *itsAmp, struct AUDIO_HEADER *header)
{
int s,retval;
struct AUDIO_HEADER tmp;

	/* TODO: add a simple byte counter to check only first, say, 1024
	 * bytes for a new header and then return GETHDR_SYN
	 */
	if ((retval=_fillbfr(itsAmp, 4))!=0) return retval;

/***
	for(;;) {
		while ((s=_getbits(itsAmp, 12)) != 0xfff) {
			if (s==0xffe) {
				tmp.mpeg25=1;
				parse_header(itsAmp, &tmp);
//			if (header_sanity_check(&tmp)==0) return GETHDR_NS;
			}
			if ((retval=readsync(itsAmp))!=0) return retval;
		}
	
		parse_header(itsAmp, &tmp);
		if (header_sanity_check(&tmp)!=0) {
			if ((retval=readsync(itsAmp))!=0) return retval;
		} else break;
	}
***/

	for(;;) {
		s=_getbits(itsAmp, 12);
		while ((s != 0xfff) && (s != 0xffe)) {
			if ((retval=readsync(itsAmp))!=0) return retval;
			s=_getbits(itsAmp, 12);
		}
	
		if (s==0xffe) tmp.mpeg25=1;
		else          tmp.mpeg25=0;
		parse_header(itsAmp, &tmp);
		if (header_sanity_check(&tmp)!=0) {
			if ((retval=readsync(itsAmp))!=0) return retval;
		} else break;
	}

	if (tmp.layer==3) return GETHDR_FL1; // Layer 1 not supported
	/* if (tmp.layer==2) return GETHDR_FL2; */
	if (tmp.bitrate_index==0) return GETHDR_FF;

	my_memcpy(header,&tmp,sizeof(tmp));
	return 0;
}

/* dummy function, to get crc out of the way
*/
void getcrc(struct Amp *itsAmp)
{
	_fillbfr(itsAmp, 2);
	_getbits(itsAmp, 16);
}

/* sizes of side_info:
 * MPEG1   1ch 17    2ch 32
 * MPEG2   1ch  9    2ch 17
 */
void getinfo(struct Amp *itsAmp, struct AUDIO_HEADER *header,struct SIDE_INFO *info)
{
int gr,ch,scfsi_band,region,window;
int nch;	
	if (header->mode==3) {
		nch=1;
		if (header->ID) {
			_fillbfr(itsAmp, 17);
			info->main_data_begin=_getbits(itsAmp, 9);
			_getbits(itsAmp, 5);
		} else {
			_fillbfr(itsAmp, 9);
			info->main_data_begin=_getbits(itsAmp, 8);
			_getbits(itsAmp, 1);
		}
	} else {
		nch=2;
                if (header->ID) {
			_fillbfr(itsAmp, 32);
                        info->main_data_begin=_getbits(itsAmp, 9);
                        _getbits(itsAmp, 3);
                } else {
			_fillbfr(itsAmp, 17);
                        info->main_data_begin=_getbits(itsAmp, 8);
                        _getbits(itsAmp, 2);
                }
	}

	if (header->ID) for (ch=0;ch<nch;ch++)
		for (scfsi_band=0;scfsi_band<4;scfsi_band++)
			info->scfsi[ch][scfsi_band]=_getbits(itsAmp, 1);

	for (gr=0;gr<(header->ID ? 2:1);gr++)
		for (ch=0;ch<nch;ch++) {
			info->part2_3_length[gr][ch]=_getbits(itsAmp, 12);
			info->big_values[gr][ch]=_getbits(itsAmp, 9);
			info->global_gain[gr][ch]=_getbits(itsAmp, 8);
			if (header->ID) info->scalefac_compress[gr][ch]=_getbits(itsAmp, 4);
			else info->scalefac_compress[gr][ch]=_getbits(itsAmp, 9);
			info->window_switching_flag[gr][ch]=_getbits(itsAmp, 1);

			if (info->window_switching_flag[gr][ch]) {
				info->block_type[gr][ch]=_getbits(itsAmp, 2);
				info->mixed_block_flag[gr][ch]=_getbits(itsAmp, 1);

				for (region=0;region<2;region++)
					info->table_select[gr][ch][region]=_getbits(itsAmp, 5);
				info->table_select[gr][ch][2]=0;

				for (window=0;window<3;window++)
					info->subblock_gain[gr][ch][window]=_getbits(itsAmp, 3);
			} else {
				for (region=0;region<3;region++)
					info->table_select[gr][ch][region]=_getbits(itsAmp, 5);

				info->region0_count[gr][ch]=_getbits(itsAmp, 4);
				info->region1_count[gr][ch]=_getbits(itsAmp, 3);
				info->block_type[gr][ch]=0;
			}

			if (header->ID) info->preflag[gr][ch]=_getbits(itsAmp, 1);
			info->scalefac_scale[gr][ch]=_getbits(itsAmp, 1);
			info->count1table_select[gr][ch]=_getbits(itsAmp, 1);
		}
	return;
}

int get_input(struct Amp *itsAmp, unsigned char* bp, unsigned int size) {
	long r;
	r = amp_input_callback(itsAmp, bp, size);
	if(r==-1)   return GETHDR_ERR;
	if(r!=size) return GETHDR_EOF;
	return 0;
}

