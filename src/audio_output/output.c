/*****************************************************************************
 * output.c : internal management of output streams for the audio output
 *****************************************************************************
 * Copyright (C) 2002 VideoLAN
 * $Id: output.c,v 1.3 2002/08/11 22:36:35 massiot Exp $
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

#include "audio_output.h"
#include "aout_internal.h"

/*****************************************************************************
 * aout_OutputNew : allocate a new output and rework the filter pipeline
 *****************************************************************************/
int aout_OutputNew( aout_instance_t * p_aout,
                    audio_sample_format_t * p_format )
{
    char * psz_name = config_GetPsz( p_aout, "aout" );
    int i_rate = config_GetInt( p_aout, "aout-rate" );
    int i_channels = config_GetInt( p_aout, "aout-channels" );

    /* Prepare FIFO. */
    vlc_mutex_init( p_aout, &p_aout->output.fifo.lock );
    p_aout->output.fifo.p_first = NULL;
    p_aout->output.fifo.pp_last = &p_aout->output.fifo.p_first;
    p_aout->output.last_date = 0;

    p_aout->output.p_module = module_Need( p_aout, "audio output",
                                           psz_name );
    if ( psz_name != NULL ) free( psz_name );
    if ( p_aout->output.p_module == NULL )
    {
        msg_Err( p_aout, "no suitable aout module" );
        return -1;
    }

    /* Retrieve user defaults. */
    memcpy( &p_aout->output.output, p_format, sizeof(audio_sample_format_t) );
    if ( i_rate != -1 ) p_aout->output.output.i_rate = i_rate;
    if ( i_channels != -1 ) p_aout->output.output.i_channels = i_channels;
    if ( AOUT_FMT_IS_SPDIF(&p_aout->output.output) )
    {
        p_aout->output.output.i_format = AOUT_FMT_SPDIF;
    }
    else
    {
        /* Non-S/PDIF mixer only deals with float32 or fixed32. */
        p_aout->output.output.i_format
                     = (p_aout->p_vlc->i_cpu & CPU_CAPABILITY_FPU) ?
                        AOUT_FMT_FLOAT32 : AOUT_FMT_FIXED32;
    }

    /* Find the best output format. */
    if ( p_aout->output.pf_setformat( p_aout ) != 0 )
    {
        msg_Err( p_aout, "couldn't set an output format" );
        module_Unneed( p_aout, p_aout->output.p_module );
        return -1;
    }

    msg_Dbg( p_aout, "output format=%d rate=%d channels=%d",
             p_aout->output.output.i_format, p_aout->output.output.i_rate,
             p_aout->output.output.i_channels );

    /* Calculate the resulting mixer output format. */
    p_aout->mixer.output.i_channels = p_aout->output.output.i_channels;
    p_aout->mixer.output.i_rate = p_aout->output.output.i_rate;
    if ( AOUT_FMT_IS_SPDIF(&p_aout->output.output) )
    {
        p_aout->mixer.output.i_format = AOUT_FMT_SPDIF;
    }
    else
    {
        /* Non-S/PDIF mixer only deals with float32 or fixed32. */
        p_aout->mixer.output.i_format
                     = (p_aout->p_vlc->i_cpu & CPU_CAPABILITY_FPU) ?
                        AOUT_FMT_FLOAT32 : AOUT_FMT_FIXED32;
    }

    /* Calculate the resulting mixer input format. */
    p_aout->mixer.input.i_channels = -1; /* unchanged */
    p_aout->mixer.input.i_rate = p_aout->mixer.output.i_rate;
    p_aout->mixer.input.i_format = p_aout->mixer.output.i_format;

    /* Create filters. */
    if ( aout_FiltersCreatePipeline( p_aout, p_aout->output.pp_filters,
                                     &p_aout->output.i_nb_filters,
                                     &p_aout->mixer.output,
                                     &p_aout->output.output ) < 0 )
    {
        msg_Err( p_aout, "couldn't set an output pipeline" );
        module_Unneed( p_aout, p_aout->output.p_module );
        return -1;
    }

    /* Prepare hints for the buffer allocator. */
    p_aout->mixer.output_alloc.i_alloc_type = AOUT_ALLOC_HEAP;
    p_aout->mixer.output_alloc.i_bytes_per_sec
         = aout_FormatToByterate( &p_aout->output.output,
                                  p_aout->output.output.i_rate );

    aout_FiltersHintBuffers( p_aout, p_aout->output.pp_filters,
                             p_aout->output.i_nb_filters,
                             &p_aout->mixer.output_alloc );

    return 0;
}

/*****************************************************************************
 * aout_OutputDelete : delete the output
 *****************************************************************************/
void aout_OutputDelete( aout_instance_t * p_aout )
{
    module_Unneed( p_aout, p_aout->output.p_module );

    aout_FiltersDestroyPipeline( p_aout, p_aout->output.pp_filters,
                                 p_aout->output.i_nb_filters );
    aout_FifoDestroy( p_aout, &p_aout->output.fifo );
}

/*****************************************************************************
 * aout_OutputPlay : play a buffer
 *****************************************************************************/
void aout_OutputPlay( aout_instance_t * p_aout, aout_buffer_t * p_buffer )
{
    aout_FiltersPlay( p_aout, p_aout->output.pp_filters,
                      p_aout->output.i_nb_filters,
                      &p_buffer );

    p_aout->output.pf_play( p_aout, p_buffer );
}

/*****************************************************************************
 * aout_OutputNextBuffer : give the audio output plug-in the right buffer
 *****************************************************************************/
aout_buffer_t * aout_OutputNextBuffer( aout_instance_t * p_aout,
                                       mtime_t start_date )
{
    aout_buffer_t * p_buffer;

    vlc_mutex_lock( &p_aout->output.fifo.lock );
    p_buffer = p_aout->output.fifo.p_first;

    while ( p_buffer != NULL && p_buffer->end_date < start_date )
    {
        msg_Dbg( p_aout, "audio output is too slow (%lld)",
                 start_date - p_buffer->end_date );
        p_buffer = p_buffer->p_next;
    }

    p_aout->output.fifo.p_first = p_buffer;
    if ( p_buffer == NULL )
    {
        p_aout->output.fifo.pp_last = &p_aout->output.fifo.p_first;
        vlc_mutex_unlock( &p_aout->output.fifo.lock );
        msg_Dbg( p_aout, "audio output is starving" );
        return NULL;
    }

    if ( p_buffer->start_date > start_date
                                 + (mtime_t)p_aout->output.i_nb_samples
                                 * 1000000 / p_aout->output.output.i_rate )
    {
        vlc_mutex_unlock( &p_aout->output.fifo.lock );
        msg_Dbg( p_aout, "audio output is starving (%lld)",
                 p_buffer->start_date - start_date );
        return NULL;
    }

    /* FIXME : there we should handle the case where start_date is not
     * completely equal to p_buffer->start_date. */

    p_aout->output.fifo.p_first = p_buffer->p_next;
    if ( p_buffer->p_next == NULL )
    {
        p_aout->output.fifo.pp_last = &p_aout->output.fifo.p_first;
    }

    vlc_mutex_unlock( &p_aout->output.fifo.lock );
    return p_buffer;
}
