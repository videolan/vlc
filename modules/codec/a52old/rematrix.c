/*****************************************************************************
 * rematrix.c: A52 audio rematrixing
 *****************************************************************************
 * Copyright (C) 1999-2001 VideoLAN
 * $Id: rematrix.c,v 1.1 2002/08/04 17:23:42 sam Exp $
 *
 * Authors: Michel Kaempf <maxx@via.ecp.fr>
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
#include <vlc/vlc.h>
#include <vlc/decoder.h>

#include "imdct.h"
#include "downmix.h"
#include "adec.h"

struct rematrix_band_s {
    u32 start;
    u32 end;
};

static const struct rematrix_band_s rematrix_band[] = { {13,24}, {25,36}, {37 ,60}, {61,252}};

static inline u32 min_value (u32 a, u32 b)
{
    return (a < b ? a : b);
}

/* This routine simply does stereo rematixing for the 2 channel
 * stereo mode */
void rematrix (a52dec_t * p_a52dec)
{
    u32 num_bands;
    u32 start;
    u32 end;
    u32 i,j;
    float left,right;

    if (p_a52dec->audblk.cplinu || p_a52dec->audblk.cplbegf > 2)
        num_bands = 4;
    else if (p_a52dec->audblk.cplbegf > 0)
        num_bands = 3;
    else
        num_bands = 2;

    for (i=0;i < num_bands; i++) {
        if (!p_a52dec->audblk.rematflg[i])
            continue;

        start = rematrix_band[i].start;
        end = min_value(rematrix_band[i].end ,12 * p_a52dec->audblk.cplbegf + 36);

        for (j=start;j < end; j++) {
            left  = 0.5f * ( *(p_a52dec->samples+j) + *(p_a52dec->samples+256+j) );
            right = 0.5f * ( *(p_a52dec->samples+j) - *(p_a52dec->samples+256+j) );
            *(p_a52dec->samples+j) = left;
            *(p_a52dec->samples+256+j) = right;
        }
    }
}

