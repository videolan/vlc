/*****************************************************************************
 * input_clock.c: Clock/System date conversions, stream management
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 * $Id: input_clock.c,v 1.1 2001/01/24 19:05:55 massiot Exp $
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
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

#include "config.h"
#include "common.h"
#include "threads.h"
#include "mtime.h"
#include "intf_msg.h"

#include "stream_control.h"
#include "input_ext-intf.h"
#include "input_ext-dec.h"

#include "input.h"

/*****************************************************************************
 * input_ClockToSysdate: converts a movie clock to system date
 *****************************************************************************/
mtime_t input_ClockToSysdate( input_thread_t * p_input,
                              pgrm_descriptor_t * p_pgrm, mtime_t i_clock )
{
    mtime_t     i_sysdate = 0;

    if( p_pgrm->i_synchro_state == SYNCHRO_OK )
    {
        i_sysdate = (i_clock - p_pgrm->cr_ref) 
                        * p_input->stream.control.i_rate
                        * 300
                        / 27
                        / DEFAULT_RATE
                        + p_pgrm->sysdate_ref;
    }

    return( i_sysdate );
}

/*****************************************************************************
 * input_ClockNewRef: writes a new clock reference
 *****************************************************************************/
void input_ClockNewRef( input_thread_t * p_input, pgrm_descriptor_t * p_pgrm,
                        mtime_t i_clock )
{
    p_pgrm->cr_ref = i_clock;
    p_pgrm->sysdate_ref = mdate();
}

