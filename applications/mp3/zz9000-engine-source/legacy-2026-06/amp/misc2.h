

/* this file is a part of amp software, (C) tomislav uzelac 1996,1997
*/
 
/* misc2.h  
 *
 * Created by: tomislav uzelac  May 1996
 * Last modified by: tomislav uzelac Jan  8 1997
 */

void requantize_mono(struct Amp *itsAmp, int gr, int ch, struct SIDE_INFO *info, struct AUDIO_HEADER *header);
void requantize_ms(struct Amp *itsAmp, int gr,struct SIDE_INFO *info,struct AUDIO_HEADER *header);
void alias_reduction(struct Amp *itsAmp, int ch);
void calculate_t43(void);

