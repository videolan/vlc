/*****************************************************************************
 * ac3_imdct_c.c: ac3 DCT in C
 *****************************************************************************
 * Copyright (C) 1999-2001 VideoLAN
 * $Id: ac3_imdct_c.c,v 1.5 2001/12/30 07:09:55 sam Exp $
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <string.h>                                              /* memcpy() */

#include <math.h>
#include <stdio.h>

#include <videolan/vlc.h>

#include "ac3_imdct.h"
#include "ac3_imdct_common.h"
#include "ac3_retables.h"

#ifndef M_PI
#   define M_PI 3.14159265358979323846
#endif

void _M( fft_64p )  ( complex_t *x );
void _M( fft_128p ) ( complex_t *x );

void _M( imdct_init ) (imdct_t * p_imdct)
{
    int i;
    float scale = 181.019;

    /* Twiddle factors to turn IFFT into IMDCT */
    for (i=0; i < 128; i++) {
        p_imdct->xcos1[i] = cos(2.0f * M_PI * (8*i+1)/(8*N)) * scale; 
        p_imdct->xsin1[i] = sin(2.0f * M_PI * (8*i+1)/(8*N)) * scale;
    }
}

void _M( imdct_do_512 ) (imdct_t * p_imdct, float data[], float delay[])
{
    int i, j;
    float tmp_a_r, tmp_a_i;
    float *data_ptr;
    float *delay_ptr;
    float *window_ptr;

    /* 512 IMDCT with source and dest data in 'data'
     * Pre IFFT complex multiply plus IFFT complex conjugate */

    for( i=0; i < 128; i++) {
        j = pm128[i];
        /* a = (data[256-2*j-1] - data[2*j]) * (xcos1[j] + xsin1[j]);
         * c = data[2*j] * xcos1[j];
         * b = data[256-2*j-1] * xsin1[j];
         * buf1[i].real = a - b + c;
         * buf1[i].imag = b + c; */
        p_imdct->buf[i].real = (data[256-2*j-1] * p_imdct->xcos1[j]) - (data[2*j] * p_imdct->xsin1[j]);
        p_imdct->buf[i].imag = -1.0 * (data[2*j] * p_imdct->xcos1[j] + data[256-2*j-1] * p_imdct->xsin1[j]);
    }

    _M( fft_128p ) ( &p_imdct->buf[0] );

    /* Post IFFT complex multiply  plus IFFT complex conjugate */
    for (i=0; i < 128; i++) {
        tmp_a_r = p_imdct->buf[i].real;
        tmp_a_i = p_imdct->buf[i].imag;
        /* a = (tmp_a_r - tmp_a_i) * (xcos1[j] + xsin1[j]);
         * b = tmp_a_r * xsin1[j];
         * c = tmp_a_i * xcos1[j];
         * buf[j].real = a - b + c;
         * buf[j].imag = b + c; */
        p_imdct->buf[i].real =(tmp_a_r * p_imdct->xcos1[i])  +  (tmp_a_i  * p_imdct->xsin1[i]);
        p_imdct->buf[i].imag =(tmp_a_r * p_imdct->xsin1[i])  -  (tmp_a_i  * p_imdct->xcos1[i]);
    }

    data_ptr = data;
    delay_ptr = delay;
    window_ptr = window;

    /* Window and convert to real valued signal */
    for (i=0; i< 64; i++) {
        *data_ptr++ = -p_imdct->buf[64+i].imag  * *window_ptr++ + *delay_ptr++;
        *data_ptr++ = p_imdct->buf[64-i-1].real * *window_ptr++ + *delay_ptr++;
    }

    for(i=0; i< 64; i++) {
        *data_ptr++ = -p_imdct->buf[i].real      * *window_ptr++ + *delay_ptr++;
        *data_ptr++ = p_imdct->buf[128-i-1].imag * *window_ptr++ + *delay_ptr++;
    }

    /* The trailing edge of the window goes into the delay line */
    delay_ptr = delay;

    for(i=0; i< 64; i++) {
        *delay_ptr++ = -p_imdct->buf[64+i].real   * *--window_ptr;
        *delay_ptr++ =  p_imdct->buf[64-i-1].imag * *--window_ptr;
    }

    for(i=0; i<64; i++) {
        *delay_ptr++ =  p_imdct->buf[i].imag       * *--window_ptr;
        *delay_ptr++ = -p_imdct->buf[128-i-1].real * *--window_ptr;
    }
}


void _M( imdct_do_512_nol ) (imdct_t * p_imdct, float data[], float delay[])
{
    int i, j;

    float tmp_a_i;
    float tmp_a_r;

    float *data_ptr;
    float *delay_ptr;
    float *window_ptr;

    /* 512 IMDCT with source and dest data in 'data'
     * Pre IFFT complex multiply plus IFFT cmplx conjugate */

    for( i=0; i < 128; i++) {
        /* z[i] = (X[256-2*i-1] + j * X[2*i]) * (xcos1[i] + j * xsin1[i]) */
        j = pm128[i];
        /* a = (data[256-2*j-1] - data[2*j]) * (xcos1[j] + xsin1[j]);
         * c = data[2*j] * xcos1[j];
         * b = data[256-2*j-1] * xsin1[j];
         * buf1[i].real = a - b + c;
         * buf1[i].imag = b + c; */
        p_imdct->buf[i].real = (data[256-2*j-1] * p_imdct->xcos1[j]) - (data[2*j] * p_imdct->xsin1[j]);
        p_imdct->buf[i].imag = -1.0 * (data[2*j] * p_imdct->xcos1[j] + data[256-2*j-1] * p_imdct->xsin1[j]);
    }
       
    _M( fft_128p ) ( &p_imdct->buf[0] );

    /* Post IFFT complex multiply  plus IFFT complex conjugate*/
    for (i=0; i < 128; i++) {
        /* y[n] = z[n] * (xcos1[n] + j * xsin1[n]) ;
         * int j1 = i; */
        tmp_a_r = p_imdct->buf[i].real;
        tmp_a_i = p_imdct->buf[i].imag;
        /* a = (tmp_a_r - tmp_a_i) * (xcos1[j] + xsin1[j]);
         * b = tmp_a_r * xsin1[j];
         * c = tmp_a_i * xcos1[j];
         * buf[j].real = a - b + c;
         * buf[j].imag = b + c; */
        p_imdct->buf[i].real =(tmp_a_r * p_imdct->xcos1[i]) + (tmp_a_i  * p_imdct->xsin1[i]);
        p_imdct->buf[i].imag =(tmp_a_r * p_imdct->xsin1[i]) - (tmp_a_i  * p_imdct->xcos1[i]);
    }
       
    data_ptr = data;
    delay_ptr = delay;
    window_ptr = window;

    /* Window and convert to real valued signal, no overlap here*/
    for (i=0; i< 64; i++) { 
        *data_ptr++ = -p_imdct->buf[64+i].imag  * *window_ptr++; 
        *data_ptr++ = p_imdct->buf[64-i-1].real * *window_ptr++; 
    }

    for(i=0; i< 64; i++) { 
        *data_ptr++ = -p_imdct->buf[i].real      * *window_ptr++; 
        *data_ptr++ = p_imdct->buf[128-i-1].imag * *window_ptr++; 
    }
       
    /* The trailing edge of the window goes into the delay line */
    delay_ptr = delay;

    for(i=0; i< 64; i++) { 
        *delay_ptr++ = -p_imdct->buf[64+i].real   * *--window_ptr; 
        *delay_ptr++ =  p_imdct->buf[64-i-1].imag * *--window_ptr; 
    }

    for(i=0; i<64; i++) {
        *delay_ptr++ =  p_imdct->buf[i].imag       * *--window_ptr; 
        *delay_ptr++ = -p_imdct->buf[128-i-1].real * *--window_ptr; 
    }
}

