/*****************************************************************************
 * ac3_imdct_common.c: common ac3 DCT functions
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 * $Id: ac3_imdct_common.c,v 1.4 2001/11/28 15:08:05 massiot Exp $
 *
 * Authors: Renaud Dartus <reno@videolan.org>
 *          Aaron Holtzman <aholtzma@engr.uvic.ca>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

/* MODULE_NAME defined in Makefile together with -DBUILTIN */
#ifdef BUILTIN
#   include "modules_inner.h"
#else
#   define _M( foo ) foo
#endif

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include "defs.h"

#include <string.h>                                              /* memcpy() */

#include <math.h>
#include <stdio.h>

#include "config.h"
#include "common.h"

#include "ac3_imdct.h"
#include "ac3_retables.h"

#ifndef M_PI
#   define M_PI 3.14159265358979323846
#endif

void _M( fft_64p )  ( complex_t *x );

void _M( imdct_do_256 ) (imdct_t * p_imdct, float data[],float delay[])
{
    int i, j, k;
    int p, q;

    float tmp_a_i;
    float tmp_a_r;

    float *data_ptr;
    float *delay_ptr;
    float *window_ptr;

    complex_t *buf1, *buf2;

    buf1 = &p_imdct->buf[0];
    buf2 = &p_imdct->buf[64];

    /* Pre IFFT complex multiply plus IFFT complex conjugate */
    for (k=0; k<64; k++) { 
        /* X1[k] = X[2*k]
         * X2[k] = X[2*k+1]    */

        j = pm64[k];
        p = 2 * (128-2*j-1);
        q = 2 * (2 * j);

        /* Z1[k] = (X1[128-2*k-1] + j * X1[2*k]) * (xcos2[k] + j * xsin2[k]); */
        buf1[k].real =        data[p] * p_imdct->xcos2[j] - data[q] * p_imdct->xsin2[j];
        buf1[k].imag = -1.0f*(data[q] * p_imdct->xcos2[j] + data[p] * p_imdct->xsin2[j]);
        /* Z2[k] = (X2[128-2*k-1] + j * X2[2*k]) * (xcos2[k] + j * xsin2[k]); */
        buf2[k].real =        data[p + 1] * p_imdct->xcos2[j] - data[q + 1] * p_imdct->xsin2[j];
        buf2[k].imag = -1.0f*(data[q + 1] * p_imdct->xcos2[j] + data[p + 1] * p_imdct->xsin2[j]);
    }

    _M( fft_64p ) ( &buf1[0] );
    _M( fft_64p ) ( &buf2[0] );

    /* Post IFFT complex multiply */
    for( i=0; i < 64; i++) {
        tmp_a_r =  buf1[i].real;
        tmp_a_i = -buf1[i].imag;
        buf1[i].real = (tmp_a_r * p_imdct->xcos2[i]) - (tmp_a_i * p_imdct->xsin2[i]);
        buf1[i].imag = (tmp_a_r * p_imdct->xsin2[i]) + (tmp_a_i * p_imdct->xcos2[i]);
        tmp_a_r =  buf2[i].real;
        tmp_a_i = -buf2[i].imag;
        buf2[i].real = (tmp_a_r * p_imdct->xcos2[i]) - (tmp_a_i * p_imdct->xsin2[i]);
        buf2[i].imag = (tmp_a_r * p_imdct->xsin2[i]) + (tmp_a_i * p_imdct->xcos2[i]);
    }
    
    data_ptr = data;
    delay_ptr = delay;
    window_ptr = window;

    /* Window and convert to real valued signal */
    for(i=0; i< 64; i++) { 
        *data_ptr++ = -buf1[i].imag     * *window_ptr++ + *delay_ptr++;
        *data_ptr++ = buf1[64-i-1].real * *window_ptr++ + *delay_ptr++;
    }

    for(i=0; i< 64; i++) {
        *data_ptr++ = -buf1[i].real     * *window_ptr++ + *delay_ptr++;
        *data_ptr++ = buf1[64-i-1].imag * *window_ptr++ + *delay_ptr++;
    }
    
    delay_ptr = delay;

    for(i=0; i< 64; i++) {
        *delay_ptr++ = -buf2[i].real      * *--window_ptr;
        *delay_ptr++ =  buf2[64-i-1].imag * *--window_ptr;
    }

    for(i=0; i< 64; i++) {
        *delay_ptr++ =  buf2[i].imag      * *--window_ptr;
        *delay_ptr++ = -buf2[64-i-1].real * *--window_ptr;
    }
}


void _M( imdct_do_256_nol ) (imdct_t * p_imdct, float data[], float delay[])
{
    int i, j, k;
    int p, q;

    float tmp_a_i;
    float tmp_a_r;

    float *data_ptr;
    float *delay_ptr;
    float *window_ptr;

    complex_t *buf1, *buf2;

    buf1 = &p_imdct->buf[0];
    buf2 = &p_imdct->buf[64];

    /* Pre IFFT complex multiply plus IFFT cmplx conjugate */
    for(k=0; k<64; k++) {
        /* X1[k] = X[2*k]
        * X2[k] = X[2*k+1] */
        j = pm64[k];
        p = 2 * (128-2*j-1);
        q = 2 * (2 * j);

        /* Z1[k] = (X1[128-2*k-1] + j * X1[2*k]) * (xcos2[k] + j * xsin2[k]); */
        buf1[k].real =        data[p] * p_imdct->xcos2[j] - data[q] * p_imdct->xsin2[j];
        buf1[k].imag = -1.0f*(data[q] * p_imdct->xcos2[j] + data[p] * p_imdct->xsin2[j]);
        /* Z2[k] = (X2[128-2*k-1] + j * X2[2*k]) * (xcos2[k] + j * xsin2[k]); */
        buf2[k].real =        data[p + 1] * p_imdct->xcos2[j] - data[q + 1] * p_imdct->xsin2[j];
        buf2[k].imag = -1.0f*(data[q + 1] * p_imdct->xcos2[j] + data[p + 1] * p_imdct->xsin2[j]);
    }

    _M( fft_64p ) ( &buf1[0] );
    _M( fft_64p ) ( &buf2[0] );

    /* Post IFFT complex multiply */
    for( i=0; i < 64; i++) {
        /* y1[n] = z1[n] * (xcos2[n] + j * xs in2[n]) ; */
        tmp_a_r =  buf1[i].real;
        tmp_a_i = -buf1[i].imag;
        buf1[i].real =(tmp_a_r * p_imdct->xcos2[i])  -  (tmp_a_i  * p_imdct->xsin2[i]);
        buf1[i].imag =(tmp_a_r * p_imdct->xsin2[i])  +  (tmp_a_i  * p_imdct->xcos2[i]);
        /* y2[n] = z2[n] * (xcos2[n] + j * xsin2[n]) ; */
        tmp_a_r =  buf2[i].real;
        tmp_a_i = -buf2[i].imag;
        buf2[i].real =(tmp_a_r * p_imdct->xcos2[i])  -  (tmp_a_i  * p_imdct->xsin2[i]);
        buf2[i].imag =(tmp_a_r * p_imdct->xsin2[i])  +  (tmp_a_i  * p_imdct->xcos2[i]);
    }
      
    data_ptr = data;
    delay_ptr = delay;
    window_ptr = window;

    /* Window and convert to real valued signal, no overlap */
    for(i=0; i< 64; i++) {
        *data_ptr++ = -buf1[i].imag     * *window_ptr++;
        *data_ptr++ = buf1[64-i-1].real * *window_ptr++;
    }

    for(i=0; i< 64; i++) {
        *data_ptr++ = -buf1[i].real     * *window_ptr++ + *delay_ptr++;
        *data_ptr++ = buf1[64-i-1].imag * *window_ptr++ + *delay_ptr++;
    }

    delay_ptr = delay;

    for(i=0; i< 64; i++) {
        *delay_ptr++ = -buf2[i].real      * *--window_ptr;
        *delay_ptr++ =  buf2[64-i-1].imag * *--window_ptr;
    }

    for(i=0; i< 64; i++) {
        *delay_ptr++ =  buf2[i].imag      * *--window_ptr;
        *delay_ptr++ = -buf2[64-i-1].real * *--window_ptr;
    }
}
