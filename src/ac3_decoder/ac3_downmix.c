/*****************************************************************************
 * ac3_downmix.c: ac3 downmix functions
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 * $Id: ac3_downmix.c,v 1.19 2001/04/06 09:15:47 sam Exp $
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

#include "int_types.h"
#include "ac3_decoder.h"
#include "ac3_internal.h"

#include "ac3_downmix.h"

/* Pre-scaled downmix coefficients */
static const float cmixlev_lut[4] = { 0.2928, 0.2468, 0.2071, 0.2468 };
static const float smixlev_lut[4] = { 0.2928, 0.2071, 0.0   , 0.2071 };

/* Downmix into _two_ channels...other downmix modes aren't implemented
 * to reduce complexity. Realistically, there aren't many machines around
 * with > 2 channel output anyways */

int __inline__ downmix (ac3dec_t * p_ac3dec, float * channel, s16 * out_buf)
{

    dm_par_t    dm_par;
    
    dm_par.clev = 0.0;
    dm_par.slev = 0.0;
    dm_par.unit = 1.0;

    if (p_ac3dec->bsi.acmod & 0x1) /* have center */
        dm_par.clev = cmixlev_lut[p_ac3dec->bsi.cmixlev];

    if (p_ac3dec->bsi.acmod & 0x4) /* have surround channels */
        dm_par.slev = smixlev_lut[p_ac3dec->bsi.surmixlev];

    dm_par.unit /= 1.0 + dm_par.clev + dm_par.slev;
    dm_par.clev *= dm_par.unit;
    dm_par.slev *= dm_par.unit;


    /*
    if (p_ac3dec->bsi.acmod > 7)
        intf_ErrMsg( "ac3dec: (downmix) invalid acmod number" );
    */
    
    switch(p_ac3dec->bsi.acmod)
    {
        case 7: // 3/2
            downmix_3f_2r_to_2ch_c (channel, &dm_par);
            break;
        case 6: // 2/2
            downmix_2f_2r_to_2ch_c (channel, &dm_par);
            break;
        case 5: // 3/1
            downmix_3f_1r_to_2ch_c (channel, &dm_par);
            break;
        case 4: // 2/1
            downmix_2f_1r_to_2ch_c (channel, &dm_par);
            break;
        case 3: // 3/0
            downmix_3f_0r_to_2ch_c (channel, &dm_par);
            break;
        case 2:
            break;
        default: // 1/0
            /* FIXME
            if (p_ac3dec->bsi.acmod == 1)
                center = p_ac3dec->samples.channel[0];
            else if (p_ac3dec->bsi.acmod == 0)
                center = p_ac3dec->samples.channel[0]; */
            return 1;
    }
    return 0;
}

