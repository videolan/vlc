/*****************************************************************************
 * lpcm_decoder.c: core lpcm decoder
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/
#include <stdio.h>
#include "defs.h"

#include "int_types.h"
#include "lpcm_decoder.h"

int lpcm_init (lpcmdec_t * p_lpcmdec)
{
    fprintf (stderr, "LPCM Debug: lpmcm init called\n");
    return 0;
}

int lpcm_decode_frame (lpcmdec_t * p_lpcmdec, s16 * buffer)
{
/*
 * XXX was part of ac3dec, is to change
    
    int i;

    if (parse_bsi (p_ac3dec))
        return 1;

    for (i = 0; i < 6; i++) {
        if (parse_audblk (p_ac3dec, i))
            return 1;
        if (exponent_unpack (p_ac3dec))
            return 1;
        bit_allocate (p_ac3dec);
        mantissa_unpack (p_ac3dec);
        if  (p_ac3dec->bsi.acmod == 0x2)
            rematrix (p_ac3dec);
        imdct (p_ac3dec);
        downmix (p_ac3dec, buffer);

        buffer += 2*256;
    }

    parse_auxdata (p_ac3dec);
*/
    
    return 0;
}
