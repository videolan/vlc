/*****************************************************************************
 * ac3_decoder.c: core ac3 decoder
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 * $Id: ac3_decoder.c,v 1.30 2001/04/30 21:04:20 reno Exp $
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

#include "defs.h"

#include "config.h"
#include "common.h"
#include "threads.h"
#include "mtime.h"

#include "intf_msg.h"                        /* intf_DbgMsg(), intf_ErrMsg() */

#include "stream_control.h"
#include "input_ext-dec.h"
#include "audio_output.h"


#include "ac3_decoder.h"
#include "ac3_decoder_thread.h"

#include "ac3_internal.h"

#include <stdio.h>

void imdct_init (imdct_t * p_imdct);
void downmix_init (downmix_t * p_downmix);

static float cmixlev_lut[4] = { 0.707, 0.595, 0.500, 0.707 };
static float smixlev_lut[4] = { 0.707, 0.500, 0.0  , 0.500 };

int ac3_init (ac3dec_t * p_ac3dec)
{
//    p_ac3dec->bit_stream.buffer = 0;
//    p_ac3dec->bit_stream.i_available = 0;
    p_ac3dec->mantissa.lfsr_state = 1;       /* dither_gen initialization */
    imdct_init(&p_ac3dec->imdct);
    downmix_init(&p_ac3dec->downmix);
    
    return 0;
}

int ac3_decode_frame (ac3dec_t * p_ac3dec, s16 * buffer)
{
    int i;
    ac3dec_thread_t * p_ac3dec_t = (ac3dec_thread_t *) p_ac3dec->bit_stream.p_callback_arg;
    
    if (parse_bsi (p_ac3dec))
    {
        intf_WarnMsg (1,"Error during ac3parsing");
        parse_auxdata (p_ac3dec);
        return 1;
    }
    
	/* compute downmix parameters
	 * downmix to tow channels for now */
	p_ac3dec->dm_par.clev = 0.0;
    p_ac3dec->dm_par.slev = 0.0; 
    p_ac3dec->dm_par.unit = 1.0;
	if (p_ac3dec->bsi.acmod & 0x1)	/* have center */
	    p_ac3dec->dm_par.clev = cmixlev_lut[p_ac3dec->bsi.cmixlev];

	if (p_ac3dec->bsi.acmod & 0x4)	/* have surround channels */
		p_ac3dec->dm_par.slev = smixlev_lut[p_ac3dec->bsi.surmixlev];

    p_ac3dec->dm_par.unit /= 1.0 + p_ac3dec->dm_par.clev + p_ac3dec->dm_par.slev;
	p_ac3dec->dm_par.clev *= p_ac3dec->dm_par.unit;
	p_ac3dec->dm_par.slev *= p_ac3dec->dm_par.unit;

    for (i = 0; i < 6; i++) {
        /* Initialize freq/time sample storage */
        memset(p_ac3dec->samples, 0, sizeof(float) * 256 * 
                (p_ac3dec->bsi.nfchans + p_ac3dec->bsi.lfeon));


        if ((p_ac3dec_t->p_fifo->b_die) && (p_ac3dec_t->p_fifo->b_error))
        {        
            return 1;
        }
 
        if (parse_audblk (p_ac3dec, i))
        {
            intf_WarnMsg (1,"Error during ac3audioblock");
            parse_auxdata (p_ac3dec);
            return 1;
        }

        if ((p_ac3dec_t->p_fifo->b_die) && (p_ac3dec_t->p_fifo->b_error))
        {        
            return 1;
        }

        if (exponent_unpack (p_ac3dec))
        {
            intf_WarnMsg (1,"Error during ac3unpack");
            parse_auxdata (p_ac3dec);
            return 1;
        }
        bit_allocate (p_ac3dec);
        mantissa_unpack (p_ac3dec);

        if ((p_ac3dec_t->p_fifo->b_die) && (p_ac3dec_t->p_fifo->b_error))
        {        
            return 1;
        }
        
        if  (p_ac3dec->bsi.acmod == 0x2)
            rematrix (p_ac3dec);
        imdct (p_ac3dec, buffer);

        buffer += 2*256;
    }

    parse_auxdata (p_ac3dec);

    return 0;
}
