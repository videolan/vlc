/*****************************************************************************
 * ac3_imdct.h : AC3 IMDCT types
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 * $Id: ac3_imdct.h,v 1.7 2002/04/05 01:05:22 gbazin Exp $
 *
 * Authors: Michel Kaempf <maxx@via.ecp.fr>
 *          Renaud Dartus <reno@videolan.org>
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

typedef struct complex_s {
    float real;
    float imag;
} complex_t;

#define N 512

typedef struct imdct_s
{
    complex_t * buf;
    void *      buf_orig;                         /* pointer before memalign */

    /* Delay buffer for time domain interleaving */
    float * delay;
    void *  delay_orig;                           /* pointer before memalign */
    float * delay1;
    void *  delay1_orig;                          /* pointer before memalign */

    /* Twiddle factors for IMDCT */
    float * xcos1;
    void *  xcos1_orig;                           /* pointer before memalign */
    float * xsin1;
    void *  xsin1_orig;                           /* pointer before memalign */
    float * xcos2;
    void *  xcos2_orig;                           /* pointer before memalign */
    float * xsin2;
    void *  xsin2_orig;                           /* pointer before memalign */
    float * xcos_sin_sse;
    void *  xcos_sin_sse_orig;                    /* pointer before memalign */
   
    /* Twiddle factor LUT */
    complex_t * w_2;
    void *      w_2_orig;                         /* pointer before memalign */
    complex_t * w_4;
    void *      w_4_orig;                         /* pointer before memalign */
    complex_t * w_8;
    void *      w_8_orig;                         /* pointer before memalign */
    complex_t * w_16;
    void *      w_16_orig;                        /* pointer before memalign */
    complex_t * w_32;
    void *      w_32_orig;                        /* pointer before memalign */
    complex_t * w_64;
    void *      w_64_orig;                        /* pointer before memalign */
    complex_t * w_1;
    void *      w_1_orig;                         /* pointer before memalign */
    
    /* Module used and shortcuts */
    struct module_s * p_module;
    void (*pf_imdct_init) (struct imdct_s *);
    //void (*pf_fft_64p) (complex_t *a);
    void (*pf_imdct_256)(struct imdct_s *, float data[], float delay[]);
    void (*pf_imdct_256_nol)(struct imdct_s *, float data[], float delay[]);
    void (*pf_imdct_512)(struct imdct_s *, float data[], float delay[]);
    void (*pf_imdct_512_nol)(struct imdct_s *, float data[], float delay[]);

} imdct_t;

