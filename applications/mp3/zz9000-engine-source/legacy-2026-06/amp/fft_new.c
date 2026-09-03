
#include <stdlib.h>
#include <math.h>

#include "fft_new.h"

#define SWAP(x,y) {float tmp=x; x=y; y=tmp;}
#define SQRT12 0.70710678118654752440
#ifndef PI
#define PI 3.1415926535
#endif

void blackman_window_n (float *vector, unsigned long data_count, float a0, float a1, float a2, float a3) {
	unsigned long i;
	float pi_term = 2.0F * PI / data_count;

	for (i = 0; i < data_count; i++) {
		vector[i] = a0 - a1 * cos (pi_term * i) +
		  ((a2 != 0.0) ? a2 * cos (pi_term * (2 * i)) : 0.0) -
		  ((a3 != 0.0) ? a3 * cos (pi_term * (3 * i)) : 0.0);
	}
}

void blackman_harris_74db_window (float *vector, unsigned long data_count) {
	blackman_window_n (vector, data_count,0.40217f, 0.49703f, 0.09392f, 0.00183f);
}

/****************************************************************************
*
*    split_radix_real_complex_fft(float x[],unsigned long ldn)
*    a real-valued, in-place, split-radix fft program
*    decimation-in-time, cos/sin in second loop
*    input: float x[0]...x[n-1] (C style)
*    length is n=2^ldn
*    output in x[0]...x[n-1], in order:
*         [re(0), re(1),..., re(n/2), im(n/2-1),..., im(1)]
*
*  imag part is negative wrt. output of fht_real_complex_fft() !
*
*
* original Fortran code by Sorensen; published in H.V. Sorensen, D.L. Jones,
* M.T. Heideman, C.S. Burrus(1987)Real-valued fast fourier transform
* algorithms.  IEEE Trans on Acoustics, Speech, & Signal Processing, 35,
* 849-863.  Adapted to C by Bill Simpson, 1995  wsimpson@uwinnipeg.ca
*
****************************************************************************/
/* 
 length is n=2^ldn

 ordering on output:

 f[0]     = re[0] 
 f[1]     = re[1] 
         ... 
 f[n/2-1] = re[n/2-1] 
 f[n/2]   = re[n/2](==nyquist freq) 

 f[n/2+1] = im[n/2-1] (wrt. compl fft with is=-1)
 f[n/2+2] = im[n/2-2] 
         ... 
 f[n-1]   = im[1]

 corresponding real and imag parts (with the exception of
 zero and nyquist freq) are found in f[i] and f[n-i]

 note that the order of imaginary parts 
 is reversed wrt. fft_real_complex_fft()

 This is a decimation-in-time, split-radix algorithm.

 zp!=0 indicates that higher half of data is zero
*/

void split_radix_real_complex_fft(float *x, long n) {
	scramble(x, n);
	split_radix_real_complex_fft_core(x, n);
}

void split_radix_real_complex_fft_core(float *x, long n) {
	long n4;
	float cc1, ss1, cc3, ss3;
	long i1, i2, i3, i4, i5, i6, i7, i8;
	float a;
	float t1, t2, t3, t4, t5, t6;
	float e;
	long nn = n>>1;
	long is, id;
	long n2, n8, i, j;
	

	x = x-1;  /* FORTRAN like indices */

	/*----length two butterflies---------------------------------------------*/
	is = 1;
	id = 4;
	do {
		for(i2=is;i2<=n;i2+=id) {
			i1 = i2+1;

			e = x[i2];
			x[i2] = e + x[i1];
			x[i1] = e - x[i1];
		}

		is = (id<<1)-1;
		id <<= 2;
	} while(is<n);


	/*---- L shaped butterflies-----------------------------------------------*/
	n2 = 2;
	while(nn>>=1) {
		is = 0;
		n2 <<= 1;
		e=2.0*PI/n2;
		id = n2<<1;
		n4 = n2>>2;
		n8 = n2>>3;

		do {
			for(i=is;i<n;i+=id) {
				i1 = i+1;
				i2 = i1 + n4;
				i3 = i2 + n4;
				i4 = i3 + n4;

				t1 = x[i4]+x[i3];
				x[i4] -= x[i3];

				x[i3] = x[i1] - t1;
				x[i1] += t1;

				if(n4!=1) {
					i1 += n8;
					i2 += n8;
					i3 += n8;
					i4 += n8;

					t1 = (x[i3]+x[i4])*SQRT12;
					t2 = (x[i3]-x[i4])*SQRT12;

					x[i4] = x[i2] - t1;
					x[i3] = -x[i2] - t1;

					x[i2] = x[i1] - t2;
					x[i1] += t2;
				}
			}

			is = (id<<1) - n2;
			id <<= 2;
		} while(is<n);

		a=e;

		for(j=2;j<=n8;j++) {

			#ifdef SINTABLE
			{
				unsigned long aw;
				aw=(unsigned long)(a*TABLEN/PI2);
				cc1=CosTab[aw];
				ss1=SinTab[aw];
			}
			#else
			cc1=cos(a);
			ss1=sin(a);
			#endif

			cc3=4.0*cc1*(cc1*cc1-0.75);
			ss3=4.0*ss1*(0.75-ss1*ss1);
			a=(float)j*e;

			is = 0;
			id = n2<<1;
			do {
				for(i=is;i<n;i+=id) {
					i1 = i+j;
					i2 = i1 + n4;
					i3 = i2 + n4;
					i4 = i3 + n4;

					i5 = i + n4 - j + 2;
					i6 = i5 + n4;
					i7 = i6 + n4;
					i8 = i7 + n4;

					t1 = x[i3]*cc1 + x[i7]*ss1;
					t2 = x[i7]*cc1 - x[i3]*ss1;

					t3 = x[i4]*cc3 + x[i8]*ss3;
					t4 = x[i8]*cc3 - x[i4]*ss3;

					t5 = t1 + t3;
					t6 = t2 + t4;
					t3 = t1 - t3;
					t4 = t2 - t4;

					t2 = x[i6] + t6;
					x[i3] = t6 - x[i6];

					x[i8] = t2;

					t2 = x[i2] - t3;
					x[i7] = -x[i2] - t3;

					x[i4] = t2;

					t1 = x[i1] + t5;
					x[i6] = x[i1] - t5;

					x[i1] = t1;

					t1 = x[i5] + t4;
					x[i5] -= t4;

					x[i2] = t1;
				}

				is = (id<<1) - n2;
				id <<= 2;
			} while(is<n);
		}
	}
}
/* ============== end SPLIT_RADIX_REAL_COMPLEX_FFT ================ */


void scramble(float *fr, long n) {
	unsigned long m,k,j;
	for (m=1,j=0; m<n-1; m++) {
		for(k=n>>1; (!((j^=k)&k)); k>>=1);
		if (j>m)  SWAP(fr[m],fr[j]);
	}
}
