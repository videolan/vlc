/*****************************************************************************
 * ac3_decoder.c: core ac3 decoder
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

#include "int_types.h"
#include "ac3_decoder.h"
#include "ac3_internal.h"

int ac3_init (ac3dec_t * p_ac3dec)
{
    //p_ac3dec->bit_stream.buffer = 0;
    //p_ac3dec->bit_stream.i_available = 0;

    return 0;
}

int ac3_decode_frame (ac3dec_t * p_ac3dec, s16 * buffer)
{
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

    return 0;
}
