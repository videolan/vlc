/*****************************************************************************
 * pcr.c: PCR management
 * Manages structures containing PCR information.
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include "defs.h"

#include <sys/types.h>                        /* on BSD, uio.h needs types.h */
#include <sys/uio.h>                                            /* "input.h" */
#include <stdlib.h>                              /* atoi(), malloc(), free() */
#include <netinet/in.h>                                           /* ntohl() */

#include "config.h"
#include "common.h"
#include "threads.h"
#include "mtime.h"
#include "debug.h"
#include "input.h"
#include "intf_msg.h"
#include "input_pcr.h"

/* Note:
 *
 *   SYNCHRONIZATION METHOD
 *
 *   We compute an average for the pcr because we want to eliminate the
 *   network jitter and keep the low frequency variations. The average is
 *   in fact a low pass filter and the jitter is a high frequency signal
 *   that is why it is eliminated by the filter/average.
 *
 *   The low frequency variations enable us to synchronize the client clock
 *   with the server clock because they represent the time variation between
 *   the 2 clocks. Those variations (ie the filtered pcr) are used to compute
 *   the presentation dates for the audio and video frames. With those dates
 *   we can decoding (or trashing) the MPEG2 stream at "exactly" the same rate
 *   as it is sent by the server and so we keep the synchronization between
 *   the server and the client.
 *
 *   It is a very important matter if you want to avoid underflow or overflow
 *   in all the FIFOs, but it may be not enough.
 *
 */

/*****************************************************************************
 * input_PcrReInit : Reinitialize the pcr_descriptor
 *****************************************************************************/
void input_PcrReInit( input_thread_t *p_input )
{
    ASSERT( p_input );

    p_input->p_pcr->delta_pcr       = 0;
    p_input->p_pcr->last_pcr        = 0;
    p_input->p_pcr->c_average_count = 0;
}

/*****************************************************************************
 * input_PcrInit : Initialize PCR decoder
 *****************************************************************************/
int input_PcrInit( input_thread_t *p_input )
{
    ASSERT( p_input );

    if( (p_input->p_pcr = malloc(sizeof(pcr_descriptor_t))) == NULL )
    {
        return( -1 );
    }
    input_PcrReInit(p_input);
    p_input->p_pcr->i_synchro_state = SYNCHRO_NOT_STARTED;

    return( 0 );
}

/*****************************************************************************
 * input_PcrDecode : Decode a PCR frame
 *****************************************************************************/
void input_PcrDecode( input_thread_t *p_input, es_descriptor_t *p_es,
                      u8* p_pcr_data )
{
    mtime_t pcr_time, sys_time, delta_pcr;
    pcr_descriptor_t *p_pcr;

    ASSERT( p_pcr_data );
    ASSERT( p_input );
    ASSERT( p_es );

    p_pcr = p_input->p_pcr;

    /* Convert the PCR in microseconde
     * WARNING: do not remove the casts in the following calculation ! */
    pcr_time  = ( (( (mtime_t)U32_AT((u32*)p_pcr_data) << 1 ) | ( p_pcr_data[4] >> 7 )) * 300 ) / 27;
    sys_time  = mdate();
    delta_pcr = sys_time - pcr_time;

    if( p_es->b_discontinuity ||
        ( p_pcr->last_pcr != 0 &&
              (    (p_pcr->last_pcr - pcr_time) > PCR_MAX_GAP
                || (p_pcr->last_pcr - pcr_time) < - PCR_MAX_GAP ) ) )
    {
        intf_DbgMsg("input debug: input_PcrReInit()\n");
        input_PcrReInit(p_input);
        p_pcr->i_synchro_state = SYNCHRO_REINIT;
        p_es->b_discontinuity = 0;
    }
    p_pcr->last_pcr = pcr_time;

    if( p_pcr->c_average_count == PCR_MAX_AVERAGE_COUNTER )
    {
        p_pcr->delta_pcr =
            ( delta_pcr + (p_pcr->delta_pcr * (PCR_MAX_AVERAGE_COUNTER-1)) )
            / PCR_MAX_AVERAGE_COUNTER;
    }
    else
    {
        p_pcr->delta_pcr =
            ( delta_pcr + (p_pcr->delta_pcr * p_pcr->c_average_count) )
            / ( p_pcr->c_average_count + 1 );
        p_pcr->c_average_count++;
    }

    if( p_pcr->i_synchro_state == SYNCHRO_NOT_STARTED )
    {
        p_pcr->i_synchro_state = SYNCHRO_START;
    }
}

/*****************************************************************************
 * input_PcrEnd : Clean PCR structures before dying
 *****************************************************************************/
void input_PcrEnd( input_thread_t *p_input )
{
    ASSERT( p_input );

    free( p_input->p_pcr );
}
