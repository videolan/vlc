/*****************************************************************************
 * ac3_imdct.c: ac3 DCT
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 * $Id: ac3_imdct.c,v 1.17 2001/04/30 21:04:20 reno Exp $
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

#include "defs.h"

#include <math.h>
#include <stdio.h>

#include "config.h"
#include "common.h"
#include "threads.h"
#include "mtime.h"

#include "stream_control.h"
#include "input_ext-dec.h"

#include "ac3_decoder.h"
#include "ac3_internal.h"

#include "ac3_downmix.h"
#include "ac3_imdct_c.h"
#if 0
#include "ac3_imdct_kni.h"
#endif

#include "tests.h"

#ifndef M_PI
#   define M_PI 3.14159265358979323846
#endif


void imdct_init(imdct_t * p_imdct)
{
	int i;
	float scale = 255.99609372;

#if 0
	if ( TestCPU (CPU_CAPABILITY_MMX) )
    {
        imdct_init_kni (p_imdct);
    } else 
#endif
    {
        imdct_init_c (p_imdct);
    }

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
		do_imdct = p_ac3dec->imdct.imdct_do_512;
    }
	else
    {
		do_imdct = imdct_do_256; /* There is only a C function */
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
    		case 7:		/* 3/2 */
    			p_ac3dec->downmix.downmix_3f_2r_to_2ch (p_ac3dec->samples[0], &p_ac3dec->dm_par);
    			break;
    		case 6:		/* 2/2 */
    			p_ac3dec->downmix.downmix_2f_2r_to_2ch (p_ac3dec->samples[0], &p_ac3dec->dm_par);
    			break;
    		case 5:		/* 3/1 */
    			p_ac3dec->downmix.downmix_3f_1r_to_2ch (p_ac3dec->samples[0], &p_ac3dec->dm_par);
    			break;
    		case 4:		/* 2/1 */
    			p_ac3dec->downmix.downmix_2f_1r_to_2ch (p_ac3dec->samples[0], &p_ac3dec->dm_par);
    			break;
	    	case 3:		/* 3/0 */
    			p_ac3dec->downmix.downmix_3f_0r_to_2ch (p_ac3dec->samples[0], &p_ac3dec->dm_par);
    			break;
    		case 2:
    			break;
    		default:	/* 1/0 */
//    			if (p_ac3dec->bsi.acmod == 1)
    				center = p_ac3dec->samples[0];
//    			else if (p_ac3dec->bsi.acmod == 0)
//                  center = samples[ac3_config.dual_mono_ch_sel];
                do_imdct(&p_ac3dec->imdct, center, p_ac3dec->imdct.delay[0]); /* no downmix*/
    
    			p_ac3dec->downmix.stream_sample_1ch_to_s16 (buffer, center);

        	    return;
                break;
        }

		do_imdct (&p_ac3dec->imdct, p_ac3dec->samples[0], p_ac3dec->imdct.delay[0]);
		do_imdct (&p_ac3dec->imdct, p_ac3dec->samples[1], p_ac3dec->imdct.delay[1]);
		p_ac3dec->downmix.stream_sample_2ch_to_s16(buffer, p_ac3dec->samples[0], p_ac3dec->samples[1]);

	} else {
        /* imdct and then downmix
		 * delay and samples should be saved and mixed
		 * fprintf(stderr, "time domain downmix\n"); */
		for (i=0; i<p_ac3dec->bsi.nfchans; i++)
        {
			if (p_ac3dec->audblk.blksw[i])
                /* There is only a C function */
				imdct_do_256_nol (&p_ac3dec->imdct, p_ac3dec->samples[i], p_ac3dec->imdct.delay1[i]);
			else
				p_ac3dec->imdct.imdct_do_512_nol (&p_ac3dec->imdct, p_ac3dec->samples[i], p_ac3dec->imdct.delay1[i]);
		}

		/* mix the sample, overlap */
		switch(p_ac3dec->bsi.acmod)
        {
    		case 7:		/* 3/2 */
    			left = p_ac3dec->samples[0];
    			center = p_ac3dec->samples[1];
    			right = p_ac3dec->samples[2];
    			left_sur = p_ac3dec->samples[3];
    			right_sur = p_ac3dec->samples[4];
    			delay_left = p_ac3dec->imdct.delay[0];
    			delay_right = p_ac3dec->imdct.delay[1];
    			delay1_left = p_ac3dec->imdct.delay1[0];
    			delay1_center = p_ac3dec->imdct.delay1[1];
	    		delay1_right = p_ac3dec->imdct.delay1[2];
            	delay1_sl = p_ac3dec->imdct.delay1[3];
    			delay1_sr = p_ac3dec->imdct.delay1[4];
    
	    		for (i = 0; i < 256; i++) {
    				left_tmp = p_ac3dec->dm_par.unit * *left++  + p_ac3dec->dm_par.clev * *center  + p_ac3dec->dm_par.slev * *left_sur++;
    				right_tmp= p_ac3dec->dm_par.unit * *right++ + p_ac3dec->dm_par.clev * *center++ + p_ac3dec->dm_par.slev * *right_sur++;
    				*buffer++ = (s16)(left_tmp + *delay_left);
	    			*buffer++ = (s16)(right_tmp + *delay_right);
    				*delay_left++ = p_ac3dec->dm_par.unit * *delay1_left++  + p_ac3dec->dm_par.clev * *delay1_center  + p_ac3dec->dm_par.slev * *delay1_sl++;
    				*delay_right++ = p_ac3dec->dm_par.unit * *delay1_right++ + p_ac3dec->dm_par.clev * *center++ + p_ac3dec->dm_par.slev * *delay1_sr++;
    			}
    			break;
    		case 6:		/* 2/2 */
    			left = p_ac3dec->samples[0];
    			right = p_ac3dec->samples[1];
    			left_sur = p_ac3dec->samples[2];
	    		right_sur = p_ac3dec->samples[3];
    			delay_left = p_ac3dec->imdct.delay[0];
    			delay_right = p_ac3dec->imdct.delay[1];
	    		delay1_left = p_ac3dec->imdct.delay1[0];
    			delay1_right = p_ac3dec->imdct.delay1[1];
    			delay1_sl = p_ac3dec->imdct.delay1[2];
    			delay1_sr = p_ac3dec->imdct.delay1[3];
    
    			for (i = 0; i < 256; i++) {
    				left_tmp = p_ac3dec->dm_par.unit * *left++  + p_ac3dec->dm_par.slev * *left_sur++;
    				right_tmp= p_ac3dec->dm_par.unit * *right++ + p_ac3dec->dm_par.slev * *right_sur++;
    				*buffer++ = (s16)(left_tmp + *delay_left);
    				*buffer++ = (s16)(right_tmp + *delay_right);
    				*delay_left++ = p_ac3dec->dm_par.unit * *delay1_left++  + p_ac3dec->dm_par.slev * *delay1_sl++;
    				*delay_right++ = p_ac3dec->dm_par.unit * *delay1_right++ + p_ac3dec->dm_par.slev * *delay1_sr++;
    			}
    			break;
    		case 5:		/* 3/1 */
    			left = p_ac3dec->samples[0];
    			center = p_ac3dec->samples[1];
    			right = p_ac3dec->samples[2];
    			right_sur = p_ac3dec->samples[3];
    			delay_left = p_ac3dec->imdct.delay[0];
    			delay_right = p_ac3dec->imdct.delay[1];
    			delay1_left = p_ac3dec->imdct.delay1[0];
    			delay1_center = p_ac3dec->imdct.delay1[1];
    			delay1_right = p_ac3dec->imdct.delay1[2];
    			delay1_sl = p_ac3dec->imdct.delay1[3];
    
    			for (i = 0; i < 256; i++) {
    				left_tmp = p_ac3dec->dm_par.unit * *left++  + p_ac3dec->dm_par.clev * *center  - p_ac3dec->dm_par.slev * *right_sur;
    				right_tmp= p_ac3dec->dm_par.unit * *right++ + p_ac3dec->dm_par.clev * *center++ + p_ac3dec->dm_par.slev * *right_sur++;
	    			*buffer++ = (s16)(left_tmp + *delay_left);
    				*buffer++ = (s16)(right_tmp + *delay_right);
    				*delay_left++ = p_ac3dec->dm_par.unit * *delay1_left++  + p_ac3dec->dm_par.clev * *delay1_center  + p_ac3dec->dm_par.slev * *delay1_sl;
    				*delay_right++ = p_ac3dec->dm_par.unit * *delay1_right++ + p_ac3dec->dm_par.clev * *center++ + p_ac3dec->dm_par.slev * *delay1_sl++;
	    		}
    			break;
    		case 4:		/* 2/1 */
    			left = p_ac3dec->samples[0];
    			right = p_ac3dec->samples[1];
    			right_sur = p_ac3dec->samples[2];
	    		delay_left = p_ac3dec->imdct.delay[0];
    			delay_right = p_ac3dec->imdct.delay[1];
    			delay1_left = p_ac3dec->imdct.delay1[0];
    			delay1_right = p_ac3dec->imdct.delay1[1];
    			delay1_sl = p_ac3dec->imdct.delay1[2];
    
        		for (i = 0; i < 256; i++) {
    				left_tmp = p_ac3dec->dm_par.unit * *left++ - p_ac3dec->dm_par.slev * *right_sur;
    				right_tmp= p_ac3dec->dm_par.unit * *right++ + p_ac3dec->dm_par.slev * *right_sur++;
	    			*buffer++ = (s16)(left_tmp + *delay_left);
    				*buffer++ = (s16)(right_tmp + *delay_right);
    				*delay_left++ = p_ac3dec->dm_par.unit * *delay1_left++ + p_ac3dec->dm_par.slev * *delay1_sl;
    				*delay_right++ = p_ac3dec->dm_par.unit * *delay1_right++ + p_ac3dec->dm_par.slev * *delay1_sl++;
    			}
    			break;
    		case 3:		/* 3/0 */
    			left = p_ac3dec->samples[0];
    			center = p_ac3dec->samples[1];
    			right = p_ac3dec->samples[2];
    			delay_left = p_ac3dec->imdct.delay[0];
	    		delay_right = p_ac3dec->imdct.delay[1];
    			delay1_left = p_ac3dec->imdct.delay1[0];
    	   		delay1_center = p_ac3dec->imdct.delay1[1];
		    	delay1_right = p_ac3dec->imdct.delay1[2];

    			for (i = 0; i < 256; i++) {
    				left_tmp = p_ac3dec->dm_par.unit * *left++  + p_ac3dec->dm_par.clev * *center;
    				right_tmp= p_ac3dec->dm_par.unit * *right++ + p_ac3dec->dm_par.clev * *center++;
    				*buffer++ = (s16)(left_tmp + *delay_left);
    				*buffer++ = (s16)(right_tmp + *delay_right);
	    			*delay_left++ = p_ac3dec->dm_par.unit * *delay1_left++  + p_ac3dec->dm_par.clev * *delay1_center;
    				*delay_right++ = p_ac3dec->dm_par.unit * *delay1_right++ + p_ac3dec->dm_par.clev * *center++;
    			}
    			break;
    		case 2:		/* copy to output */
    			for (i = 0; i < 256; i++) {
    				*buffer++ = (s16)p_ac3dec->samples[0][i];
	    			*buffer++ = (s16)p_ac3dec->samples[1][i];
    			}
    			break;
		}
	}
}
