/*****************************************************************************
 * decoder.c: core A52 decoder
 *****************************************************************************
 * Copyright (C) 1999-2001 VideoLAN
 * $Id: decoder.c,v 1.1 2002/08/04 17:23:42 sam Exp $
 *
 * Authors: Michel Kaempf <maxx@via.ecp.fr>
 *          Michel Lespinasse <walken@zoy.org>
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
#include <vlc/decoder.h>

#include "imdct.h"
#include "downmix.h"
#include "adec.h"                                         /* a52dec_thread_t */

#include "internal.h"

static const float cmixlev_lut[4] = { 0.707, 0.595, 0.500, 0.707 };
static const float smixlev_lut[4] = { 0.707, 0.500, 0.0  , 0.500 };

int E_( a52_init )(a52dec_t * p_a52dec)
{
    p_a52dec->mantissa.lfsr_state = 1;          /* dither_gen initialization */
    E_( imdct_init )(p_a52dec->p_imdct) ;
    
    return 0;
}

int decode_frame (a52dec_t * p_a52dec, s16 * buffer)
{
    int i;
    
    if (parse_bsi (p_a52dec))
    {
        msg_Warn( p_a52dec->p_fifo, "parse error" );
        parse_auxdata (p_a52dec);
        return 1;
    }
    
    /* compute downmix parameters
     * downmix to tow channels for now */
    p_a52dec->dm_par.clev = 0.0;
    p_a52dec->dm_par.slev = 0.0; 
    p_a52dec->dm_par.unit = 1.0;
    if (p_a52dec->bsi.acmod & 0x1)    /* have center */
        p_a52dec->dm_par.clev = cmixlev_lut[p_a52dec->bsi.cmixlev];

    if (p_a52dec->bsi.acmod & 0x4)    /* have surround channels */
        p_a52dec->dm_par.slev = smixlev_lut[p_a52dec->bsi.surmixlev];

    p_a52dec->dm_par.unit /= 1.0 + p_a52dec->dm_par.clev + p_a52dec->dm_par.slev;
    p_a52dec->dm_par.clev *= p_a52dec->dm_par.unit;
    p_a52dec->dm_par.slev *= p_a52dec->dm_par.unit;

    for (i = 0; i < 6; i++) {
        /* Initialize freq/time sample storage */
        memset(p_a52dec->samples, 0, sizeof(float) * 256 * 
                (p_a52dec->bsi.nfchans + p_a52dec->bsi.lfeon));


        if( p_a52dec->p_fifo->b_die || p_a52dec->p_fifo->b_error )
        {        
            return 1;
        }
 
        if( parse_audblk( p_a52dec, i ) )
        {
            msg_Warn( p_a52dec->p_fifo, "audioblock error" );
            parse_auxdata( p_a52dec );
            return 1;
        }

        if( p_a52dec->p_fifo->b_die || p_a52dec->p_fifo->b_error )
        {        
            return 1;
        }

        if( exponent_unpack( p_a52dec ) )
        {
            msg_Warn( p_a52dec->p_fifo, "unpack error" );
            parse_auxdata( p_a52dec );
            return 1;
        }

        bit_allocate (p_a52dec);
        mantissa_unpack (p_a52dec);

        if( p_a52dec->p_fifo->b_die || p_a52dec->p_fifo->b_error )
        {        
            return 1;
        }
        
        if  (p_a52dec->bsi.acmod == 0x2)
        {
            rematrix (p_a52dec);
        }

        imdct (p_a52dec, buffer);

        buffer += 2 * 256;
    }

    parse_auxdata (p_a52dec);

    return 0;
}

