/*****************************************************************************
 * downmix_c.c: A52 downmix functions in C
 *****************************************************************************
 * Copyright (C) 1999, 2000, 2001 VideoLAN
 * $Id: downmix_c.c,v 1.1 2002/08/04 17:23:42 sam Exp $
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

#include <vlc/vlc.h>

#include "../downmix.h"

void E_( downmix_3f_2r_to_2ch ) (float *samples, dm_par_t *dm_par)
{
    int i;
    float *left, *right, *center, *left_sur, *right_sur;
    float left_tmp, right_tmp;
    
    left      = samples;
    center    = samples + 256;
    right     = samples + 256*2;
    left_sur  = samples + 256*3;
    right_sur = samples + 256*4;

    for (i=0; i < 256; i++) {
        left_tmp = dm_par->unit * *left + dm_par->clev * *center + dm_par->slev * *left_sur++;
        right_tmp = dm_par->unit * *right++ + dm_par->clev * *center + dm_par->slev * *right_sur++;
        *left++ = left_tmp;
        *center++ = right_tmp;
    }
}

void E_( downmix_2f_2r_to_2ch ) (float *samples, dm_par_t *dm_par)
{
    int i;
    float *left, *right, *left_sur, *right_sur;
    float left_tmp, right_tmp;
               
    left = &samples[0];
    right = &samples[256];
    left_sur = &samples[512];
    right_sur = &samples[768];

    for (i = 0; i < 256; i++) {
        left_tmp = dm_par->unit * *left  + dm_par->slev * *left_sur++;
        right_tmp= dm_par->unit * *right + dm_par->slev * *right_sur++;
        *left++ = left_tmp;
        *right++ = right_tmp;
    }
}

void E_( downmix_3f_1r_to_2ch ) (float *samples, dm_par_t *dm_par)
{
    int i;
    float *left, *right, *center, *right_sur;
    float left_tmp, right_tmp;

    left = &samples[0];
    right = &samples[512];
    center = &samples[256];
    right_sur = &samples[768];

    for (i = 0; i < 256; i++) {
        left_tmp = dm_par->unit * *left  + dm_par->clev * *center  - dm_par->slev * *right_sur;
        right_tmp= dm_par->unit * *right++ + dm_par->clev * *center + dm_par->slev * *right_sur++;
        *left++ = left_tmp;
        *center++ = right_tmp;
    }
}


void E_( downmix_2f_1r_to_2ch ) (float *samples, dm_par_t *dm_par)
{
    int i;
    float *left, *right, *right_sur;
    float left_tmp, right_tmp;

    left = &samples[0];
    right = &samples[256];
    right_sur = &samples[512];

    for (i = 0; i < 256; i++) {
        left_tmp = dm_par->unit * *left  - dm_par->slev * *right_sur;
        right_tmp= dm_par->unit * *right + dm_par->slev * *right_sur++;
        *left++ = left_tmp;
        *right++ = right_tmp;
    }
}


void E_( downmix_3f_0r_to_2ch ) (float *samples, dm_par_t *dm_par)
{
    int i;
    float *left, *right, *center;
    float left_tmp, right_tmp;

    left = &samples[0];
    center = &samples[256];
    right = &samples[512];

    for (i = 0; i < 256; i++) {
        left_tmp = dm_par->unit * *left  + dm_par->clev * *center;
        right_tmp= dm_par->unit * *right++ + dm_par->clev * *center;
        *left++ = left_tmp;
        *center++ = right_tmp;
    }
}


void E_( stream_sample_2ch_to_s16 ) (s16 *out_buf, float *left, float *right)
{
    int i;
    for (i=0; i < 256; i++) {
        *out_buf++ = (s16) (*left++);
        *out_buf++ = (s16) (*right++);
    }
}


void E_( stream_sample_1ch_to_s16 ) (s16 *out_buf, float *center)
{
    int i;
    float tmp;

    for (i=0; i < 256; i++) {
        *out_buf++ = tmp = (s16) (0.7071f * *center++);
        *out_buf++ = tmp;
    }
}

