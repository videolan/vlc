/*****************************************************************************
 * ac3_rematrix.c: ac3 audio rematrixing
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 *
 * Authors:
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *****************************************************************************/
#include "defs.h"

#include "int_types.h"
#include "ac3_decoder.h"
#include "ac3_internal.h"

struct rematrix_band_s {
    u32 start;
    u32 end;
};

static struct rematrix_band_s rematrix_band[] = { {13,24}, {25,36}, {37 ,60}, {61,252}};

static __inline__ u32 min (u32 a, u32 b)
{
    return (a < b ? a : b);
}

/* This routine simply does stereo rematixing for the 2 channel
 * stereo mode */
void rematrix (ac3dec_t * p_ac3dec)
{
    u32 num_bands;
    u32 start;
    u32 end;
    u32 i,j;
    float left,right;

    if (p_ac3dec->audblk.cplinu || p_ac3dec->audblk.cplbegf > 2)
        num_bands = 4;
    else if (p_ac3dec->audblk.cplbegf > 0)
        num_bands = 3;
    else
        num_bands = 2;

    for (i=0;i < num_bands; i++) {
        if (!p_ac3dec->audblk.rematflg[i])
            continue;

        start = rematrix_band[i].start;
        end = min(rematrix_band[i].end ,12 * p_ac3dec->audblk.cplbegf + 36);

        for (j=start;j < end; j++) {
            left  = 0.5f * (p_ac3dec->coeffs.fbw[0][j] + p_ac3dec->coeffs.fbw[1][j]);
            right = 0.5f * (p_ac3dec->coeffs.fbw[0][j] - p_ac3dec->coeffs.fbw[1][j]);
            p_ac3dec->coeffs.fbw[0][j] = left;
            p_ac3dec->coeffs.fbw[1][j] = right;
        }
    }
}
