/*****************************************************************************
 * float32.c : precise float32 audio mixer implementation
 *****************************************************************************
 * Copyright (C) 2002 the VideoLAN team
 * $Id$
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stddef.h>
#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_aout.h>

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Create    ( vlc_object_t * );

static void DoWork    ( aout_mixer_t *, aout_buffer_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin ()
    set_category( CAT_AUDIO )
    set_subcategory( SUBCAT_AUDIO_MISC )
    set_description( N_("Float32 audio mixer") )
    set_capability( "audio mixer", 10 )
    set_callbacks( Create, NULL )
vlc_module_end ()

/*****************************************************************************
 * Create: allocate mixer
 *****************************************************************************/
static int Create( vlc_object_t *p_this )
{
    aout_mixer_t * p_mixer = (aout_mixer_t *)p_this;

    if ( p_mixer->fmt.i_format != VLC_CODEC_FL32 )
        return -1;

    /* Use the trivial mixer when we can */
    if ( p_mixer->input_count == 1 && p_mixer->multiplier == 1.0 )
    {
        int i;
        for( i = 0; i < p_mixer->input_count; i++ )
        {
            if( p_mixer->input[i]->multiplier != 1.0 )
                break;
        }
        if( i >= p_mixer->input_count )
            return -1;
    }

    p_mixer->mix = DoWork;
    return 0;
}

/*****************************************************************************
 * ScaleWords: prepare input words for averaging
 *****************************************************************************/
static void ScaleWords( float * p_out, const float * p_in, size_t i_nb_words,
                        int i_nb_inputs, float f_multiplier )
{
    int i;
    f_multiplier /= i_nb_inputs;

    for ( i = i_nb_words; i--; )
    {
        *p_out++ = *p_in++ * f_multiplier;
    }
}

/*****************************************************************************
 * MeanWords: average input words
 *****************************************************************************/
static void MeanWords( float * p_out, const float * p_in, size_t i_nb_words,
                       int i_nb_inputs, float f_multiplier )
{
    int i;
    f_multiplier /= i_nb_inputs;

    for ( i = i_nb_words; i--; )
    {
        *p_out++ += *p_in++ * f_multiplier;
    }
}

/*****************************************************************************
 * DoWork: mix a new output buffer
 *****************************************************************************
 * Terminology : in this function a word designates a single float32, eg.
 * a stereo sample is consituted of two words.
 *****************************************************************************/
static void DoWork( aout_mixer_t * p_mixer, aout_buffer_t * p_buffer )
{
    const int i_nb_inputs = p_mixer->input_count;
    const float f_multiplier_global = p_mixer->multiplier;
    const int i_nb_channels = aout_FormatNbChannels( &p_mixer->fmt );
    int i_input;

    for ( i_input = 0; i_input < i_nb_inputs; i_input++ )
    {
        int i_nb_words = p_buffer->i_nb_samples * i_nb_channels;
        aout_mixer_input_t * p_input = p_mixer->input[i_input];
        float f_multiplier = f_multiplier_global * p_input->multiplier;

        float * p_out = (float *)p_buffer->p_buffer;
        float * p_in = (float *)p_input->begin;

        if ( p_input->is_invalid )
            continue;

        for ( ; ; )
        {
            ptrdiff_t i_available_words = (
                 (float *)p_input->fifo.p_first->p_buffer - p_in)
                                   + p_input->fifo.p_first->i_nb_samples
                                   * i_nb_channels;

            if ( i_available_words < i_nb_words )
            {
                aout_buffer_t * p_old_buffer;

                if ( i_available_words > 0 )
                {
                    if ( !i_input )
                    {
                        ScaleWords( p_out, p_in, i_available_words,
                                    i_nb_inputs, f_multiplier );
                    }
                    else
                    {
                        MeanWords( p_out, p_in, i_available_words,
                                   i_nb_inputs, f_multiplier );
                    }
                }

                i_nb_words -= i_available_words;
                p_out += i_available_words;

                /* Next buffer */
                p_old_buffer = aout_FifoPop( NULL, &p_input->fifo );
                aout_BufferFree( p_old_buffer );
                if ( p_input->fifo.p_first == NULL )
                {
                    msg_Err( p_mixer, "internal amix error" );
                    return;
                }
                p_in = (float *)p_input->fifo.p_first->p_buffer;
            }
            else
            {
                if ( i_nb_words > 0 )
                {
                    if ( !i_input )
                    {
                        ScaleWords( p_out, p_in, i_nb_words, i_nb_inputs,
                                    f_multiplier );
                    }
                    else
                    {
                        MeanWords( p_out, p_in, i_nb_words, i_nb_inputs,
                                   f_multiplier );
                    }
                }
                p_input->begin = (void *)(p_in + i_nb_words);
                break;
            }
        }
    }
}

