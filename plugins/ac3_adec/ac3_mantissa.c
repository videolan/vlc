/*****************************************************************************
 * ac3_mantissa.c: ac3 mantissa computation
 *****************************************************************************
 * Copyright (C) 1999-2001 VideoLAN
 * $Id: ac3_mantissa.c,v 1.3 2001/11/28 15:08:04 massiot Exp $
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

#include "config.h"
#include "common.h"
#include "intf_msg.h"
#include "threads.h"
#include "mtime.h"

#include "audio_output.h"

#include "modules.h"
#include "modules_export.h"

#include "stream_control.h"
#include "input_ext-dec.h"


#include "ac3_imdct.h"
#include "ac3_downmix.h"
#include "ac3_decoder.h"

#include "ac3_mantissa.h"

void mantissa_unpack (ac3dec_t * p_ac3dec)
{
    int i, j;
    u32 done_cpl = 0;

    p_ac3dec->mantissa.q_1_pointer = -1;
    p_ac3dec->mantissa.q_2_pointer = -1;
    p_ac3dec->mantissa.q_4_pointer = -1;

    for (i=0; i< p_ac3dec->bsi.nfchans; i++) {
        for (j=0; j < p_ac3dec->audblk.endmant[i]; j++)
            *(p_ac3dec->samples+i*256+j) = coeff_get_float(p_ac3dec, p_ac3dec->audblk.fbw_bap[i][j],
                    p_ac3dec->audblk.dithflag[i], p_ac3dec->audblk.fbw_exp[i][j]);

        if (p_ac3dec->audblk.cplinu && p_ac3dec->audblk.chincpl[i] && !(done_cpl)) {
        /* ncplmant is equal to 12 * ncplsubnd
         * Don't dither coupling channel until channel
         * separation so that interchannel noise is uncorrelated 
         */
            for (j=p_ac3dec->audblk.cplstrtmant; j < p_ac3dec->audblk.cplendmant; j++)
                p_ac3dec->audblk.cpl_flt[j] = coeff_get_float(p_ac3dec, p_ac3dec->audblk.cpl_bap[j],
                        0, p_ac3dec->audblk.cpl_exp[j]);
            done_cpl = 1;
        }
    }
    
    /* uncouple the channel if necessary */
    if (p_ac3dec->audblk.cplinu) {
        for (i=0; i< p_ac3dec->bsi.nfchans; i++) {
            if (p_ac3dec->audblk.chincpl[i])
                uncouple_channel(p_ac3dec, i);
        }
    }

    if (p_ac3dec->bsi.lfeon) {
        /* There are always 7 mantissas for lfe, no dither for lfe */
        for (j=0; j < 7 ; j++)
            *(p_ac3dec->samples+5*256+j) = coeff_get_float(p_ac3dec, p_ac3dec->audblk.lfe_bap[j],
                    0, p_ac3dec->audblk.lfe_exp[j]);
    }
}

