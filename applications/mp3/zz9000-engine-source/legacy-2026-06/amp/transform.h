/* this file is a part of amp software, (C) tomislav uzelac 1996,1997
*/
 
/* transform.h  tables galore
 *
 * Created by: tomislav uzelac  May 1996
 * Last modified by: tomislav uzelac  Mar  1 97
 */
void imdct_init(void);
void imdct(struct Amp *itsAmp, int win_type,int sb,int ch);
void poly(struct Amp *itsAmp,int ch,int i);
void premultiply(void);

