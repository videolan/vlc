/*****************************************************************************
 * mixer.c : audio output mixing operations
 *****************************************************************************
 * Copyright (C) 2002 VideoLAN
 * $Id: mixer.c,v 1.4 2002/08/14 00:23:59 massiot Exp $
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
#include <stdlib.h>                            /* calloc(), malloc(), free() */
#include <string.h>

#include <vlc/vlc.h>

#ifdef HAVE_ALLOCA_H
#   include <alloca.h> 
#endif

#include "audio_output.h"
#include "aout_internal.h"

/*****************************************************************************
 * aout_MixerNew: prepare a mixer plug-in
 *****************************************************************************/
int aout_MixerNew( aout_instance_t * p_aout )
{
    p_aout->mixer.p_module = module_Need( p_aout, "audio mixer", NULL );
    if ( p_aout->mixer.p_module == NULL )
    {
        msg_Err( p_aout, "no suitable aout mixer" );
        return -1;
    }
    return 0;
}

/*****************************************************************************
 * aout_MixerDelete: delete the mixer
 *****************************************************************************/
void aout_MixerDelete( aout_instance_t * p_aout )
{
    module_Unneed( p_aout, p_aout->mixer.p_module );
}

/*****************************************************************************
 * aout_MixerRun: entry point for the mixer & post-filters processing
 *****************************************************************************/
void aout_MixerRun( aout_instance_t * p_aout )
{
    int             i;
    aout_buffer_t * p_output_buffer;

    /* See if we have enough data to prepare a new buffer for the audio
     * output. */
    mtime_t wanted_date = 0, first_date = 0;

    for ( i = 0; i < p_aout->i_nb_inputs; i++ )
    {
        aout_fifo_t * p_fifo = &p_aout->pp_inputs[i]->fifo;
        aout_buffer_t * p_buffer;
        vlc_mutex_lock( &p_fifo->lock );
        for ( p_buffer = p_fifo->p_first; p_buffer != NULL;
              p_buffer = p_buffer->p_next )
        {
            if ( !wanted_date )
            {
                if ( !p_aout->output.last_date )
                {
                    first_date = p_buffer->start_date;
                    wanted_date = p_buffer->start_date
                        + (mtime_t)p_aout->output.i_nb_samples * 1000000
                            / p_aout->output.output.i_rate;
                }
                else
                {
                    first_date = p_aout->output.last_date;
                    wanted_date = p_aout->output.last_date
                        + (mtime_t)p_aout->output.i_nb_samples * 1000000
                           / p_aout->output.output.i_rate;
                }
            }

            if ( p_buffer->end_date >= wanted_date ) break;
        }
        vlc_mutex_unlock( &p_fifo->lock );
        if ( p_buffer == NULL ) break;
    }

    if ( i < p_aout->i_nb_inputs )
    {
        /* Interrupted before the end... We can't run. */
        return;
    }

    /* Run the mixer. */
    aout_BufferAlloc( &p_aout->mixer.output_alloc,
                      ((u64)p_aout->output.i_nb_samples * 1000000)
                        / p_aout->output.output.i_rate,
                      /* This is a bit kludgy, but is actually only used
                       * for the S/PDIF dummy mixer : */
                      p_aout->pp_inputs[0]->fifo.p_first,
                      p_output_buffer );
    if ( p_output_buffer == NULL )
    {
        msg_Err( p_aout, "out of memory" );
        return;
    }
    p_output_buffer->i_nb_samples = p_aout->output.i_nb_samples;
    p_output_buffer->i_nb_bytes = (wanted_date - first_date)
                              * aout_FormatToByterate( &p_aout->mixer.output )
                              / 1000000;
    p_output_buffer->start_date = first_date;
    p_output_buffer->end_date = wanted_date;
    p_aout->output.last_date = wanted_date;

    p_aout->mixer.pf_do_work( p_aout, p_output_buffer );

    aout_OutputPlay( p_aout, p_output_buffer );
}

