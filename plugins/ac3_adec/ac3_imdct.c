/*****************************************************************************
 * ac3_imdct.c: ac3 DCT
 *****************************************************************************
 * Copyright (C) 1999-2001 VideoLAN
 * $Id: ac3_imdct.c,v 1.6 2001/12/10 04:53:10 sam Exp $
 *
 * Authors: Michel Kaempf <maxx@via.ecp.fr>
 *          Aaron Holtzman <aholtzma@engr.uvic.ca>
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include "defs.h"

#include <string.h>                                              /* memcpy() */

#include <math.h>
#include <stdio.h>

#include "common.h"
#include "intf_msg.h"
#include "threads.h"
#include "mtime.h"

#include "stream_control.h"
#include "input_ext-dec.h"

#include "modules.h"
#include "modules_export.h"

#include "ac3_imdct.h"
#include "ac3_downmix.h"
#include "ac3_decoder.h"

#ifndef M_PI
#   define M_PI 3.14159265358979323846
#endif

void imdct_init(imdct_t * p_imdct)
{
    int i;
    float scale = 181.019;

    p_imdct->pf_imdct_init( p_imdct );

    /* More twiddle factors to turn IFFT into IMDCT */
    for (i=0; i < 64; i++) {
        p_imdct->xcos2[i] = cos(2.0f * M_PI * (8*i+1)/(4*N)) * scale;
        p_imdct->xsin2[i] = sin(2.0f * M_PI * (8*i+1)/(4*N)) * scale;
    }
}

void imdct (ac3dec_t * p_ac3dec, s16 * buffer)
{
    int   i;
    int   doable = 0;
    float *center=NULL, *left, *right, *left_sur, *right_sur;
    float *delay_left, *delay_right;
    float *delay1_left, *delay1_right, *delay1_center, *delay1_sr, *delay1_sl;
    float right_tmp, left_tmp;
    void (*do_imdct)(imdct_t * p_imdct, float data[], float delay[]);

    /* test if dm in frequency is doable */
    if (!(doable = p_ac3dec->audblk.blksw[0]))
    {
        do_imdct = p_ac3dec->imdct->pf_imdct_512;
    }
    else
    {
        do_imdct = p_ac3dec->imdct->pf_imdct_256;
    }

    /* downmix in the frequency domain if all the channels
     * use the same imdct */
    for (i=0; i < p_ac3dec->bsi.nfchans; i++)
    {
        if (doable != p_ac3dec->audblk.blksw[i])
        {
            do_imdct = NULL;
            break;
        }
    }

    if (do_imdct)
    {
        /* dowmix first and imdct */
        switch(p_ac3dec->bsi.acmod)
        {
            case 7:        /* 3/2 */
                p_ac3dec->downmix.pf_downmix_3f_2r_to_2ch (p_ac3dec->samples, &p_ac3dec->dm_par);
                break;
            case 6:        /* 2/2 */
                p_ac3dec->downmix.pf_downmix_2f_2r_to_2ch (p_ac3dec->samples, &p_ac3dec->dm_par);
                break;
            case 5:        /* 3/1 */
                p_ac3dec->downmix.pf_downmix_3f_1r_to_2ch (p_ac3dec->samples, &p_ac3dec->dm_par);
                break;
            case 4:        /* 2/1 */
                p_ac3dec->downmix.pf_downmix_2f_1r_to_2ch (p_ac3dec->samples, &p_ac3dec->dm_par);
                break;
            case 3:        /* 3/0 */
                p_ac3dec->downmix.pf_downmix_3f_0r_to_2ch (p_ac3dec->samples, &p_ac3dec->dm_par);
                break;
            case 2:
                break;
            default:    /* 1/0 */
//                if (p_ac3dec->bsi.acmod == 1)
                    center = p_ac3dec->samples;
//                else if (p_ac3dec->bsi.acmod == 0)
//                  center = samples[ac3_config.dual_mono_ch_sel];
                do_imdct(p_ac3dec->imdct, center, p_ac3dec->imdct->delay); /* no downmix*/
    
                p_ac3dec->downmix.pf_stream_sample_1ch_to_s16 (buffer, center);

                return;
                break;
        }

        do_imdct (p_ac3dec->imdct, p_ac3dec->samples, p_ac3dec->imdct->delay);
        do_imdct (p_ac3dec->imdct, p_ac3dec->samples+256, p_ac3dec->imdct->delay+256);
        p_ac3dec->downmix.pf_stream_sample_2ch_to_s16(buffer, p_ac3dec->samples, p_ac3dec->samples+256);

    } else {
        /* imdct and then downmix
         * delay and samples should be saved and mixed
         * fprintf(stderr, "time domain downmix\n"); */
        for (i=0; i<p_ac3dec->bsi.nfchans; i++)
        {
            if (p_ac3dec->audblk.blksw[i])
            {
                /* There is only a C function */
                p_ac3dec->imdct->pf_imdct_256_nol( p_ac3dec->imdct,
                     p_ac3dec->samples+256*i, p_ac3dec->imdct->delay1+256*i );
            }
            else
            {
                p_ac3dec->imdct->pf_imdct_512_nol( p_ac3dec->imdct,
                     p_ac3dec->samples+256*i, p_ac3dec->imdct->delay1+256*i );
            }
        }

        /* mix the sample, overlap */
        switch(p_ac3dec->bsi.acmod)
        {
            case 7:        /* 3/2 */
                left = p_ac3dec->samples;
                center = p_ac3dec->samples+256;
                right = p_ac3dec->samples+2*256;
                left_sur = p_ac3dec->samples+3*256;
                right_sur = p_ac3dec->samples+4*256;
                delay_left = p_ac3dec->imdct->delay;
                delay_right = p_ac3dec->imdct->delay+256;
                delay1_left = p_ac3dec->imdct->delay1;
                delay1_center = p_ac3dec->imdct->delay1+256;
                delay1_right = p_ac3dec->imdct->delay1+2*256;
                delay1_sl = p_ac3dec->imdct->delay1+3*256;
                delay1_sr = p_ac3dec->imdct->delay1+4*256;
    
                for (i = 0; i < 256; i++) {
                    left_tmp = p_ac3dec->dm_par.unit * *left++  + p_ac3dec->dm_par.clev * *center  + p_ac3dec->dm_par.slev * *left_sur++;
                    right_tmp= p_ac3dec->dm_par.unit * *right++ + p_ac3dec->dm_par.clev * *center++ + p_ac3dec->dm_par.slev * *right_sur++;
                    *buffer++ = (s16)(left_tmp + *delay_left);
                    *buffer++ = (s16)(right_tmp + *delay_right);
                    *delay_left++ = p_ac3dec->dm_par.unit * *delay1_left++  + p_ac3dec->dm_par.clev * *delay1_center  + p_ac3dec->dm_par.slev * *delay1_sl++;
                    *delay_right++ = p_ac3dec->dm_par.unit * *delay1_right++ + p_ac3dec->dm_par.clev * *center++ + p_ac3dec->dm_par.slev * *delay1_sr++;
                }
                break;
            case 6:        /* 2/2 */
                left = p_ac3dec->samples;
                right = p_ac3dec->samples+256;
                left_sur = p_ac3dec->samples+2*256;
                right_sur = p_ac3dec->samples+3*256;
                delay_left = p_ac3dec->imdct->delay;
                delay_right = p_ac3dec->imdct->delay+256;
                delay1_left = p_ac3dec->imdct->delay1;
                delay1_right = p_ac3dec->imdct->delay1+256;
                delay1_sl = p_ac3dec->imdct->delay1+2*256;
                delay1_sr = p_ac3dec->imdct->delay1+3*256;
    
                for (i = 0; i < 256; i++) {
                    left_tmp = p_ac3dec->dm_par.unit * *left++  + p_ac3dec->dm_par.slev * *left_sur++;
                    right_tmp= p_ac3dec->dm_par.unit * *right++ + p_ac3dec->dm_par.slev * *right_sur++;
                    *buffer++ = (s16)(left_tmp + *delay_left);
                    *buffer++ = (s16)(right_tmp + *delay_right);
                    *delay_left++ = p_ac3dec->dm_par.unit * *delay1_left++  + p_ac3dec->dm_par.slev * *delay1_sl++;
                    *delay_right++ = p_ac3dec->dm_par.unit * *delay1_right++ + p_ac3dec->dm_par.slev * *delay1_sr++;
                }
                break;
            case 5:        /* 3/1 */
                left = p_ac3dec->samples;
                center = p_ac3dec->samples+256;
                right = p_ac3dec->samples+2*256;
                right_sur = p_ac3dec->samples+3*256;
                delay_left = p_ac3dec->imdct->delay;
                delay_right = p_ac3dec->imdct->delay+256;
                delay1_left = p_ac3dec->imdct->delay1;
                delay1_center = p_ac3dec->imdct->delay1+256;
                delay1_right = p_ac3dec->imdct->delay1+2*256;
                delay1_sl = p_ac3dec->imdct->delay1+3*256;
    
                for (i = 0; i < 256; i++) {
                    left_tmp = p_ac3dec->dm_par.unit * *left++  + p_ac3dec->dm_par.clev * *center  - p_ac3dec->dm_par.slev * *right_sur;
                    right_tmp= p_ac3dec->dm_par.unit * *right++ + p_ac3dec->dm_par.clev * *center++ + p_ac3dec->dm_par.slev * *right_sur++;
                    *buffer++ = (s16)(left_tmp + *delay_left);
                    *buffer++ = (s16)(right_tmp + *delay_right);
                    *delay_left++ = p_ac3dec->dm_par.unit * *delay1_left++  + p_ac3dec->dm_par.clev * *delay1_center  + p_ac3dec->dm_par.slev * *delay1_sl;
                    *delay_right++ = p_ac3dec->dm_par.unit * *delay1_right++ + p_ac3dec->dm_par.clev * *center++ + p_ac3dec->dm_par.slev * *delay1_sl++;
                }
                break;
            case 4:        /* 2/1 */
                left = p_ac3dec->samples;
                right = p_ac3dec->samples+256;
                right_sur = p_ac3dec->samples+2*256;
                delay_left = p_ac3dec->imdct->delay;
                delay_right = p_ac3dec->imdct->delay+256;
                delay1_left = p_ac3dec->imdct->delay1;
                delay1_right = p_ac3dec->imdct->delay1+256;
                delay1_sl = p_ac3dec->imdct->delay1+2*256;
    
                for (i = 0; i < 256; i++) {
                    left_tmp = p_ac3dec->dm_par.unit * *left++ - p_ac3dec->dm_par.slev * *right_sur;
                    right_tmp= p_ac3dec->dm_par.unit * *right++ + p_ac3dec->dm_par.slev * *right_sur++;
                    *buffer++ = (s16)(left_tmp + *delay_left);
                    *buffer++ = (s16)(right_tmp + *delay_right);
                    *delay_left++ = p_ac3dec->dm_par.unit * *delay1_left++ + p_ac3dec->dm_par.slev * *delay1_sl;
                    *delay_right++ = p_ac3dec->dm_par.unit * *delay1_right++ + p_ac3dec->dm_par.slev * *delay1_sl++;
                }
                break;
            case 3:        /* 3/0 */
                left = p_ac3dec->samples;
                center = p_ac3dec->samples+256;
                right = p_ac3dec->samples+2*256;
                delay_left = p_ac3dec->imdct->delay;
                delay_right = p_ac3dec->imdct->delay+256;
                delay1_left = p_ac3dec->imdct->delay1;
                delay1_center = p_ac3dec->imdct->delay1+256;
                delay1_right = p_ac3dec->imdct->delay1+2*256;

                for (i = 0; i < 256; i++) {
                    left_tmp = p_ac3dec->dm_par.unit * *left++  + p_ac3dec->dm_par.clev * *center;
                    right_tmp= p_ac3dec->dm_par.unit * *right++ + p_ac3dec->dm_par.clev * *center++;
                    *buffer++ = (s16)(left_tmp + *delay_left);
                    *buffer++ = (s16)(right_tmp + *delay_right);
                    *delay_left++ = p_ac3dec->dm_par.unit * *delay1_left++  + p_ac3dec->dm_par.clev * *delay1_center;
                    *delay_right++ = p_ac3dec->dm_par.unit * *delay1_right++ + p_ac3dec->dm_par.clev * *center++;
                }
                break;
            case 2:        /* copy to output */
                for (i = 0; i < 256; i++) {
                    *buffer++ = (s16) *(p_ac3dec->samples+i);
                    *buffer++ = (s16) *(p_ac3dec->samples+256+i);
                }
                break;
        }
    }
}
