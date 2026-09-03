/* this file is a part of amp software, (C) tomislav uzelac 1996,1997
*/

/* audio.h  some global variables
 *
 * Created by: tomislav uzelac Mar/Apr, Jul 96
 */

#include <stdio.h>

struct AUDIO_HEADER {
	int ID;
	int layer;
	int protection_bit;
	int bitrate_index;
	int sampling_frequency;
	int padding_bit;
	int private_bit;
	int mode;
	int mode_extension;
	int copyright;
	int original;
	int emphasis;
	int mpeg25;
};

struct SIDE_INFO {
	int main_data_begin;
	int scfsi[2][4];
	int part2_3_length[2][2];
	int big_values[2][2];
	int global_gain[2][2];
	int scalefac_compress[2][2];
	int window_switching_flag[2][2];
	int block_type[2][2];
	int mixed_block_flag[2][2];
	int table_select[2][2][3];
	int subblock_gain[2][2][3];
	int region0_count[2][2];
	int region1_count[2][2];
	int preflag[2][2];
	int scalefac_scale[2][2];
	int count1table_select[2][2];
};

extern const int t_sampling_frequency[2][2][3];
extern const short t_bitrate[2][3][15];
extern const int t_b8_l[2][3][22];
extern const int t_b8_s[2][3][13];

void statusDisplay(struct AUDIO_HEADER *header, int frameNo);
int decodeMPEG(void);
void initialise_globals(struct Amp *itsAmp);
void initialise_decoder(void);
void report_header_error(int err);

