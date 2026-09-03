/* this file is a part of amp software, (C) tomislav uzelac 1996,1997
*/
 
/* getbits.h
 *
 * Created by: tomislav uzelac  Apr 1996
 */

/* gethdr() error codes
*/
#define GETHDR_ERR 0x1
#define GETHDR_NS  0x2
#define GETHDR_FL1 0x4
#define GETHDR_FL2 0x8
#define GETHDR_FF  0x10
#define GETHDR_SYN 0x20
#define GETHDR_EOF 0x30
 
/* exports
*/
int fillbfr(struct Amp *itsAmp, unsigned int advance);
unsigned int getbits(struct Amp *itsAmp, int n);
int gethdr(struct Amp *itsAmp, struct AUDIO_HEADER *header);
void getcrc(struct Amp *itsAmp);
void getinfo(struct Amp *itsAmp, struct AUDIO_HEADER *header,struct SIDE_INFO *info);
int rewind_stream(int nbytes);

