/*****************************************************************************
 * audio_output.c : audio output instance
 *****************************************************************************
 * Copyright (C) 2002 VideoLAN
 * $Id: audio_output.c,v 1.91 2002/08/09 23:47:23 massiot Exp $
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
 * aout_NewInstance: initialize aout structure
 *****************************************************************************/
aout_instance_t * __aout_NewInstance( vlc_object_t * p_parent )
{
    aout_instance_t * p_aout;

    /* Allocate descriptor. */
    p_aout = vlc_object_create( p_parent, VLC_OBJECT_AOUT );
    if( p_aout == NULL )
    {
        return NULL;
    }

    /* Initialize members. */
    vlc_mutex_init( p_parent, &p_aout->input_lock );
    vlc_cond_init( p_parent, &p_aout->input_signal );
    p_aout->i_inputs_active = 0;
    p_aout->b_change_requested = 0;
    p_aout->i_nb_inputs = 0;

    vlc_mutex_init( p_parent, &p_aout->mixer_lock );
    vlc_cond_init( p_parent, &p_aout->mixer_signal );
    p_aout->b_mixer_active = 0;

    vlc_object_attach( p_aout, p_parent->p_vlc );

    return p_aout;
}

/*****************************************************************************
 * aout_DeleteInstance: destroy aout structure
 *****************************************************************************/
void aout_DeleteInstance( aout_instance_t * p_aout )
{
    vlc_mutex_destroy( &p_aout->input_lock );
    vlc_cond_destroy( &p_aout->input_signal );
    vlc_mutex_destroy( &p_aout->mixer_lock );
    vlc_cond_destroy( &p_aout->mixer_signal );

    /* Free structure. */
    vlc_object_detach_all( p_aout );
    vlc_object_destroy( p_aout );
}

/*****************************************************************************
 * aout_BufferNew : ask for a new empty buffer
 *****************************************************************************/
aout_buffer_t * aout_BufferNew( aout_instance_t * p_aout,
                                aout_input_t * p_input,
                                size_t i_nb_samples )
{
    aout_buffer_t * p_buffer;

    /* This necessarily allocates in the heap. */
    aout_BufferAlloc( &p_input->input_alloc, (u64)(1000000 * i_nb_samples)
                                         / p_input->input.i_rate,
                      NULL, p_buffer );
    p_buffer->i_nb_samples = i_nb_samples;

    if ( p_buffer == NULL )
    {
        msg_Err( p_aout, "NULL buffer !" );
    }
    else
    {
        p_buffer->start_date = p_buffer->end_date = 0;
    }

    return p_buffer;
}

/*****************************************************************************
 * aout_BufferDelete : destroy an undecoded buffer
 *****************************************************************************/
void aout_BufferDelete( aout_instance_t * p_aout, aout_input_t * p_input,
                        aout_buffer_t * p_buffer )
{
    aout_BufferFree( p_buffer );
}

/*****************************************************************************
 * aout_BufferPlay : filter & mix the decoded buffer
 *****************************************************************************/
void aout_BufferPlay( aout_instance_t * p_aout, aout_input_t * p_input,
                      aout_buffer_t * p_buffer )
{
    vlc_bool_t b_run_mixer = 0;

    if ( p_buffer->start_date == 0 )
    {
        msg_Warn( p_aout, "non-dated buffer received" );
        aout_BufferFree( p_buffer );
    }
    else
    {
        p_buffer->end_date = p_buffer->start_date
                                + (mtime_t)(p_buffer->i_nb_samples * 1000000)
                                    / p_input->input.i_rate;
    }

    aout_InputPlay( p_aout, p_input, p_buffer );

    /* Run the mixer if it is able to run. */
    vlc_mutex_lock( &p_aout->mixer_lock );
    if ( !p_aout->b_mixer_active )
    {
        p_aout->b_mixer_active = 1;
        b_run_mixer = 1;
    }
    vlc_mutex_unlock( &p_aout->mixer_lock );

    if ( b_run_mixer )
    {
        aout_MixerRun( p_aout );
        vlc_mutex_lock( &p_aout->mixer_lock );
        p_aout->b_mixer_active = 0;
        vlc_cond_broadcast( &p_aout->mixer_signal );
        vlc_mutex_unlock( &p_aout->mixer_lock );
    }
}

/*****************************************************************************
 * aout_FormatTo : compute the number of bytes/sample for format (used for
 * aout_FormatToByterate and aout_FormatToSize)
 *****************************************************************************/
int aout_FormatTo( audio_sample_format_t * p_format, int i_multiplier )
{
    int i_result;

    switch ( p_format->i_format )
    {
    case AOUT_FMT_U8:
    case AOUT_FMT_S8:
        i_result = 1;
        break;

    case AOUT_FMT_U16_LE:
    case AOUT_FMT_U16_BE:
    case AOUT_FMT_S16_LE:
    case AOUT_FMT_S16_BE:
        i_result = 2;
        break;

    case AOUT_FMT_FLOAT32:
    case AOUT_FMT_FIXED32:
        i_result = 4;
        break;

    case AOUT_FMT_SPDIF:
    case AOUT_FMT_A52: /* Actually smaller and variable, but who cares ? */
    case AOUT_FMT_DTS: /* Unimplemented and untested */
        /* Please note that we don't multiply by multiplier, because i_rate
         * and i_nb_samples do not have any sense for S/PDIF (yes, it
         * _is_ kludgy). --Meuuh */
        return AOUT_SPDIF_FRAME;

    default:
        return 0; /* will segfault much sooner... */
    }

    return i_result * p_format->i_channels * i_multiplier;
}

