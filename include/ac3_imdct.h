/*****************************************************************************
 * ac3_imdct.h : AC3 IMDCT types
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 * $Id: ac3_imdct.h,v 1.6 2001/10/30 19:34:53 reno Exp $
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

    /* Delay buffer for time domain interleaving */
    float * delay;
    float * delay1;

    /* Twiddle factors for IMDCT */
    float * xcos1;
    float * xsin1;
    float * xcos2;
    float * xsin2;
    float * xcos_sin_sse;
   
    /* Twiddle factor LUT */
    complex_t * w_2;
    complex_t * w_4;
    complex_t * w_8;
    complex_t * w_16;
    complex_t * w_32;
    complex_t * w_64;
    complex_t * w_1;
    
    /* Module used and shortcuts */
    struct module_s * p_module;
    void (*pf_imdct_init) (struct imdct_s *);
    //void (*pf_fft_64p) (complex_t *a);
    void (*pf_imdct_256)(struct imdct_s *, float data[], float delay[]);
    void (*pf_imdct_256_nol)(struct imdct_s *, float data[], float delay[]);
    void (*pf_imdct_512)(struct imdct_s *, float data[], float delay[]);
    void (*pf_imdct_512_nol)(struct imdct_s *, float data[], float delay[]);

} imdct_t;

