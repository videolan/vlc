/*****************************************************************************
 * ac3_downmix.c: ac3 downmix functions
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 * $Id: ac3_downmix.c,v 1.21 2001/04/30 21:04:20 reno Exp $
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

#include "config.h"
#include "common.h"
#include "threads.h"
#include "mtime.h"

#include "tests.h"

#include "stream_control.h"
#include "input_ext-dec.h"
#include "ac3_decoder.h"
#include "ac3_internal.h"
#include "ac3_downmix.h"

void downmix_init (downmix_t * p_downmix)
{
#if 0
    if ( TestCPU (CPU_CAPABILITY_MMX) )
    {
		fprintf(stderr,"Using MMX for downmix\n");
		p_downmix->downmix_3f_2r_to_2ch = downmix_3f_2r_to_2ch_kni;
		p_downmix->downmix_2f_2r_to_2ch = downmix_2f_2r_to_2ch_kni;
		p_downmix->downmix_3f_1r_to_2ch = downmix_3f_1r_to_2ch_kni;
		p_downmix->downmix_2f_1r_to_2ch = downmix_2f_1r_to_2ch_kni;
		p_downmix->downmix_3f_0r_to_2ch = downmix_3f_0r_to_2ch_kni;
		p_downmix->stream_sample_2ch_to_s16 = stream_sample_2ch_to_s16_kni;
    	p_downmix->stream_sample_1ch_to_s16 = stream_sample_1ch_to_s16_kni;
    } else 
#endif
    {
		p_downmix->downmix_3f_2r_to_2ch = downmix_3f_2r_to_2ch_c;
		p_downmix->downmix_2f_2r_to_2ch = downmix_2f_2r_to_2ch_c;
		p_downmix->downmix_3f_1r_to_2ch = downmix_3f_1r_to_2ch_c;
		p_downmix->downmix_2f_1r_to_2ch = downmix_2f_1r_to_2ch_c;
		p_downmix->downmix_3f_0r_to_2ch = downmix_3f_0r_to_2ch_c;
		p_downmix->stream_sample_2ch_to_s16 = stream_sample_2ch_to_s16_c;
		p_downmix->stream_sample_1ch_to_s16 = stream_sample_1ch_to_s16_c;
    }
}
