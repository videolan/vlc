/*****************************************************************************
 * ac3_imdct.h : AC3 IMDCT types
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 * $Id: ac3_imdct.h,v 1.3 2001/05/15 16:19:42 sam Exp $
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
    complex_t buf[N/4];

    /* Delay buffer for time domain interleaving */
    float delay[6][256];
    float delay1[6][256];

    /* Twiddle factors for IMDCT */
    float xcos1[N/4];
    float xsin1[N/4];
    float xcos2[N/8];
    float xsin2[N/8];
   
    /* Twiddle factor LUT */
    complex_t *w[7];
    complex_t w_1[1];
    complex_t w_2[2];
    complex_t w_4[4];
    complex_t w_8[8];
    complex_t w_16[16];
    complex_t w_32[32];
    complex_t w_64[64];

    float xcos_sin_sse[128 * 4] __attribute__((aligned(16)));
    
    /* Module used and shortcuts */
    struct module_s * p_module;
    void (*pf_imdct_init) (struct imdct_s *);
    //void (*pf_fft_64p) (complex_t *a);
    void (*pf_imdct_256)(struct imdct_s *, float data[], float delay[]);
    void (*pf_imdct_256_nol)(struct imdct_s *, float data[], float delay[]);
    void (*pf_imdct_512)(struct imdct_s *, float data[], float delay[]);
    void (*pf_imdct_512_nol)(struct imdct_s *, float data[], float delay[]);

} imdct_t;

