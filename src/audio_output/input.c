/*****************************************************************************
 * input.c : internal management of input streams for the audio output
 *****************************************************************************
 * Copyright (C) 2002-2004 the VideoLAN team
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <stdlib.h>                            /* calloc(), malloc(), free() */
#include <string.h>

#include <vlc/vlc.h>
#include <vlc/input.h>                 /* for input_thread_t and i_pts_delay */

#ifdef HAVE_ALLOCA_H
#   include <alloca.h>
#endif

#include "audio_output.h"
#include "aout_internal.h"

static int VisualizationCallback( vlc_object_t *, char const *,
                                  vlc_value_t, vlc_value_t, void * );
static int EqualizerCallback( vlc_object_t *, char const *,
                              vlc_value_t, vlc_value_t, void * );
static aout_filter_t * allocateUserChannelMixer( aout_instance_t *,
                                                 audio_sample_format_t *,
                                                 audio_sample_format_t * );

/*****************************************************************************
 * aout_InputNew : allocate a new input and rework the filter pipeline
 *****************************************************************************/
int aout_InputNew( aout_instance_t * p_aout, aout_input_t * p_input )
{
    audio_sample_format_t user_filter_format;
    audio_sample_format_t intermediate_format;/* input of resampler */
    vlc_value_t val, text;
    char * psz_filters, *psz_visual;
    aout_filter_t * p_user_channel_mixer;

    aout_FormatPrint( p_aout, "input", &p_input->input );

    /* Prepare FIFO. */
    aout_FifoInit( p_aout, &p_input->fifo, p_aout->mixer.mixer.i_rate );
    p_input->p_first_byte_to_mix = NULL;

    /* Prepare format structure */
    memcpy( &intermediate_format, &p_aout->mixer.mixer,
            sizeof(audio_sample_format_t) );
    intermediate_format.i_rate = p_input->input.i_rate;

    /* Try to use the channel mixer chosen by the user */
    memcpy ( &user_filter_format, &intermediate_format,
             sizeof(audio_sample_format_t) );
    user_filter_format.i_physical_channels = p_input->input.i_physical_channels;
    user_filter_format.i_original_channels = p_input->input.i_original_channels;
    user_filter_format.i_bytes_per_frame = user_filter_format.i_bytes_per_frame
                              * aout_FormatNbChannels( &user_filter_format )
                              / aout_FormatNbChannels( &intermediate_format );
    p_user_channel_mixer = allocateUserChannelMixer( p_aout, &user_filter_format,
                                                   &intermediate_format );
    /* If it failed, let the main pipeline do channel mixing */
    if ( ! p_user_channel_mixer )
    {
        memcpy ( &user_filter_format, &intermediate_format,
                 sizeof(audio_sample_format_t) );
    }

    /* Create filters. */
    if ( aout_FiltersCreatePipeline( p_aout, p_input->pp_filters,
                                     &p_input->i_nb_filters,
                                     &p_input->input,
                                     &user_filter_format
                                     ) < 0 )
    {
        msg_Err( p_aout, "couldn't set an input pipeline" );

        aout_FifoDestroy( p_aout, &p_input->fifo );
        p_input->b_error = 1;
        return -1;
    }

    /* Now add user filters */
    if( var_Type( p_aout, "visual" ) == 0 )
    {
        module_t *p_module;
        var_Create( p_aout, "visual", VLC_VAR_STRING | VLC_VAR_HASCHOICE );
        text.psz_string = _("Visualizations");
        var_Change( p_aout, "visual", VLC_VAR_SETTEXT, &text, NULL );
        val.psz_string = ""; text.psz_string = _("Disable");
        var_Change( p_aout, "visual", VLC_VAR_ADDCHOICE, &val, &text );
        val.psz_string = "spectrometer"; text.psz_string = _("Spectrometer");
        var_Change( p_aout, "visual", VLC_VAR_ADDCHOICE, &val, &text );
        val.psz_string = "scope"; text.psz_string = _("Scope");
        var_Change( p_aout, "visual", VLC_VAR_ADDCHOICE, &val, &text );
        val.psz_string = "spectrum"; text.psz_string = _("Spectrum");
        var_Change( p_aout, "visual", VLC_VAR_ADDCHOICE, &val, &text );

        /* Look for goom plugin */
        p_module = config_FindModule( VLC_OBJECT(p_aout), "goom" );
        if( p_module )
        {
            val.psz_string = "goom"; text.psz_string = "Goom";
            var_Change( p_aout, "visual", VLC_VAR_ADDCHOICE, &val, &text );
        }

        /* Look for galaktos plugin */
        p_module = config_FindModule( VLC_OBJECT(p_aout), "galaktos" );
        if( p_module )
        {
            val.psz_string = "galaktos"; text.psz_string = "GaLaktos";
            var_Change( p_aout, "visual", VLC_VAR_ADDCHOICE, &val, &text );
        }

        if( var_Get( p_aout, "effect-list", &val ) == VLC_SUCCESS )
        {
            var_Set( p_aout, "visual", val );
            if( val.psz_string ) free( val.psz_string );
        }
        var_AddCallback( p_aout, "visual", VisualizationCallback, NULL );
    }

    if( var_Type( p_aout, "equalizer" ) == 0 )
    {
        module_config_t *p_config;
        int i;

        p_config = config_FindConfig( VLC_OBJECT(p_aout), "equalizer-preset" );
        if( p_config && p_config->i_list )
        {
               var_Create( p_aout, "equalizer",
                           VLC_VAR_STRING | VLC_VAR_HASCHOICE );
            text.psz_string = _("Equalizer");
            var_Change( p_aout, "equalizer", VLC_VAR_SETTEXT, &text, NULL );

            val.psz_string = ""; text.psz_string = _("Disable");
            var_Change( p_aout, "equalizer", VLC_VAR_ADDCHOICE, &val, &text );

            for( i = 0; i < p_config->i_list; i++ )
            {
                val.psz_string = p_config->ppsz_list[i];
                text.psz_string = p_config->ppsz_list_text[i];
                var_Change( p_aout, "equalizer", VLC_VAR_ADDCHOICE,
                            &val, &text );
            }

            var_AddCallback( p_aout, "equalizer", EqualizerCallback, NULL );
        }
    }

    if( var_Type( p_aout, "audio-filter" ) == 0 )
    {
        var_Create( p_aout, "audio-filter",
                    VLC_VAR_STRING | VLC_VAR_DOINHERIT );
        text.psz_string = _("Audio filters");
        var_Change( p_aout, "audio-filter", VLC_VAR_SETTEXT, &text, NULL );
    }
    if( var_Type( p_aout, "audio-visual" ) == 0 )
    {
        var_Create( p_aout, "audio-visual",
                    VLC_VAR_STRING | VLC_VAR_DOINHERIT );
        text.psz_string = _("Audio visualizations");
        var_Change( p_aout, "audio-visual", VLC_VAR_SETTEXT, &text, NULL );
    }

    var_Get( p_aout, "audio-filter", &val );
    psz_filters = val.psz_string;
    var_Get( p_aout, "audio-visual", &val );
    psz_visual = val.psz_string;

    if( psz_filters && *psz_filters && psz_visual && *psz_visual )
    {
        psz_filters = (char *)realloc( psz_filters, strlen( psz_filters ) +
                                                    strlen( psz_visual )  + 1);
        sprintf( psz_filters, "%s:%s", psz_filters, psz_visual );
    }
    else if(  psz_visual && *psz_visual )
    {
        if( psz_filters ) free( psz_filters );
        psz_filters = strdup( psz_visual );
    }

    if( psz_filters && *psz_filters )
    {
        char *psz_parser = psz_filters;
        char *psz_next;
        while( psz_parser && *psz_parser )
        {
            aout_filter_t * p_filter;

            if( p_input->i_nb_filters >= AOUT_MAX_FILTERS )
            {
                msg_Dbg( p_aout, "max filter reached (%d)", AOUT_MAX_FILTERS );
                break;
            }

            while( *psz_parser == ' ' && *psz_parser == ':' )
            {
                psz_parser++;
            }
            if( ( psz_next = strchr( psz_parser , ':'  ) ) )
            {
                *psz_next++ = '\0';
            }
            if( *psz_parser =='\0' )
            {
                break;
            }

            msg_Dbg( p_aout, "user filter \"%s\"", psz_parser );

            /* Create a VLC object */
            p_filter = vlc_object_create( p_aout, sizeof(aout_filter_t) );
            if( p_filter == NULL )
            {
                msg_Err( p_aout, "cannot add user filter %s (skipped)",
                         psz_parser );
                psz_parser = psz_next;
                continue;
            }

            vlc_object_attach( p_filter , p_aout );
            memcpy( &p_filter->input, &user_filter_format,
                    sizeof(audio_sample_format_t) );
            memcpy( &p_filter->output, &user_filter_format,
                    sizeof(audio_sample_format_t) );

            p_filter->p_module =
                module_Need( p_filter,"audio filter", psz_parser, VLC_FALSE );

            if( p_filter->p_module== NULL )
            {
                p_filter->p_module =
                    module_Need( p_filter,"visualization", psz_parser,
                                                           VLC_FALSE );
                if( p_filter->p_module == NULL )
                {
                    msg_Err( p_aout, "cannot add user filter %s (skipped)",
                             psz_parser );

                    vlc_object_detach( p_filter );
                    vlc_object_destroy( p_filter );
                    psz_parser = psz_next;
                    continue;
                }
            }
            p_filter->b_continuity = VLC_FALSE;

            p_input->pp_filters[p_input->i_nb_filters++] = p_filter;

            /* next filter if any */
            psz_parser = psz_next;
        }
    }
    if( psz_filters ) free( psz_filters );
    if( psz_visual ) free( psz_visual );

    /* Attach the user channel mixer */
    if ( p_user_channel_mixer )
    {
        p_input->pp_filters[p_input->i_nb_filters++] = p_user_channel_mixer;
    }

    /* Prepare hints for the buffer allocator. */
    p_input->input_alloc.i_alloc_type = AOUT_ALLOC_HEAP;
    p_input->input_alloc.i_bytes_per_sec = -1;

    if ( AOUT_FMT_NON_LINEAR( &p_aout->mixer.mixer ) )
    {
        p_input->i_nb_resamplers = 0;
    }
    else
    {
        /* Create resamplers. */
        intermediate_format.i_rate = (__MAX(p_input->input.i_rate,
                                            p_aout->mixer.mixer.i_rate)
                                 * (100 + AOUT_MAX_RESAMPLING)) / 100;
        if ( intermediate_format.i_rate == p_aout->mixer.mixer.i_rate )
        {
            /* Just in case... */
            intermediate_format.i_rate++;
        }
        if ( aout_FiltersCreatePipeline( p_aout, p_input->pp_resamplers,
                                         &p_input->i_nb_resamplers,
                                         &intermediate_format,
                                         &p_aout->mixer.mixer ) < 0 )
        {
            msg_Err( p_aout, "couldn't set a resampler pipeline" );

            aout_FiltersDestroyPipeline( p_aout, p_input->pp_filters,
                                         p_input->i_nb_filters );
            aout_FifoDestroy( p_aout, &p_input->fifo );
            var_Destroy( p_aout, "visual" );
            p_input->b_error = 1;

            return -1;
        }

        aout_FiltersHintBuffers( p_aout, p_input->pp_resamplers,
                                 p_input->i_nb_resamplers,
                                 &p_input->input_alloc );

        /* Setup the initial rate of the resampler */
        p_input->pp_resamplers[0]->input.i_rate = p_input->input.i_rate;
    }
    p_input->i_resampling_type = AOUT_RESAMPLING_NONE;

    p_input->input_alloc.i_alloc_type = AOUT_ALLOC_HEAP;
    aout_FiltersHintBuffers( p_aout, p_input->pp_filters,
                             p_input->i_nb_filters,
                             &p_input->input_alloc );

    /* i_bytes_per_sec is still == -1 if no filters */
    p_input->input_alloc.i_bytes_per_sec = __MAX(
                                    p_input->input_alloc.i_bytes_per_sec,
                                    (int)(p_input->input.i_bytes_per_frame
                                     * p_input->input.i_rate
                                     / p_input->input.i_frame_length) );
    /* Allocate in the heap, it is more convenient for the decoder. */
    p_input->input_alloc.i_alloc_type = AOUT_ALLOC_HEAP;

    p_input->b_error = VLC_FALSE;
    p_input->b_restart = VLC_FALSE;

    return 0;
}

/*****************************************************************************
 * aout_InputDelete : delete an input
 *****************************************************************************
 * This function must be entered with the mixer lock.
 *****************************************************************************/
int aout_InputDelete( aout_instance_t * p_aout, aout_input_t * p_input )
{
    if ( p_input->b_error ) return 0;

    aout_FiltersDestroyPipeline( p_aout, p_input->pp_filters,
                                 p_input->i_nb_filters );
    aout_FiltersDestroyPipeline( p_aout, p_input->pp_resamplers,
                                 p_input->i_nb_resamplers );
    aout_FifoDestroy( p_aout, &p_input->fifo );

    return 0;
}

/*****************************************************************************
 * aout_InputPlay : play a buffer
 *****************************************************************************
 * This function must be entered with the input lock.
 *****************************************************************************/
int aout_InputPlay( aout_instance_t * p_aout, aout_input_t * p_input,
                    aout_buffer_t * p_buffer )
{
    mtime_t start_date;

    if( p_input->b_restart )
    {
        aout_fifo_t fifo, dummy_fifo;
        byte_t      *p_first_byte_to_mix;

        vlc_mutex_lock( &p_aout->mixer_lock );

        /* A little trick to avoid loosing our input fifo */
        aout_FifoInit( p_aout, &dummy_fifo, p_aout->mixer.mixer.i_rate );
        p_first_byte_to_mix = p_input->p_first_byte_to_mix;
        fifo = p_input->fifo;
        p_input->fifo = dummy_fifo;
        aout_InputDelete( p_aout, p_input );
        aout_InputNew( p_aout, p_input );
        p_input->p_first_byte_to_mix = p_first_byte_to_mix;
        p_input->fifo = fifo;

        vlc_mutex_unlock( &p_aout->mixer_lock );
    }

    /* We don't care if someone changes the start date behind our back after
     * this. We'll deal with that when pushing the buffer, and compensate
     * with the next incoming buffer. */
    vlc_mutex_lock( &p_aout->input_fifos_lock );
    start_date = aout_FifoNextStart( p_aout, &p_input->fifo );
    vlc_mutex_unlock( &p_aout->input_fifos_lock );

    if ( start_date != 0 && start_date < mdate() )
    {
        /* The decoder is _very_ late. This can only happen if the user
         * pauses the stream (or if the decoder is buggy, which cannot
         * happen :). */
        msg_Warn( p_aout, "computed PTS is out of range ("I64Fd"), "
                  "clearing out", mdate() - start_date );
        vlc_mutex_lock( &p_aout->input_fifos_lock );
        aout_FifoSet( p_aout, &p_input->fifo, 0 );
        p_input->p_first_byte_to_mix = NULL;
        vlc_mutex_unlock( &p_aout->input_fifos_lock );
        if ( p_input->i_resampling_type != AOUT_RESAMPLING_NONE )
            msg_Warn( p_aout, "timing screwed, stopping resampling" );
        p_input->i_resampling_type = AOUT_RESAMPLING_NONE;
        if ( p_input->i_nb_resamplers != 0 )
        {
            p_input->pp_resamplers[0]->input.i_rate = p_input->input.i_rate;
            p_input->pp_resamplers[0]->b_continuity = VLC_FALSE;
        }
        start_date = 0;
    }

    if ( p_buffer->start_date < mdate() + AOUT_MIN_PREPARE_TIME )
    {
        /* The decoder gives us f*cked up PTS. It's its business, but we
         * can't present it anyway, so drop the buffer. */
        msg_Warn( p_aout, "PTS is out of range ("I64Fd"), dropping buffer",
                  mdate() - p_buffer->start_date );
        aout_BufferFree( p_buffer );
        p_input->i_resampling_type = AOUT_RESAMPLING_NONE;
        if ( p_input->i_nb_resamplers != 0 )
        {
            p_input->pp_resamplers[0]->input.i_rate = p_input->input.i_rate;
            p_input->pp_resamplers[0]->b_continuity = VLC_FALSE;
        }
        return 0;
    }

    /* If the audio drift is too big then it's not worth trying to resample
     * the audio. */
    if ( start_date != 0 &&
         ( start_date < p_buffer->start_date - 3 * AOUT_PTS_TOLERANCE ) )
    {
        msg_Warn( p_aout, "audio drift is too big ("I64Fd"), clearing out",
                  start_date - p_buffer->start_date );
        vlc_mutex_lock( &p_aout->input_fifos_lock );
        aout_FifoSet( p_aout, &p_input->fifo, 0 );
        p_input->p_first_byte_to_mix = NULL;
        vlc_mutex_unlock( &p_aout->input_fifos_lock );
        if ( p_input->i_resampling_type != AOUT_RESAMPLING_NONE )
            msg_Warn( p_aout, "timing screwed, stopping resampling" );
        p_input->i_resampling_type = AOUT_RESAMPLING_NONE;
        if ( p_input->i_nb_resamplers != 0 )
        {
            p_input->pp_resamplers[0]->input.i_rate = p_input->input.i_rate;
            p_input->pp_resamplers[0]->b_continuity = VLC_FALSE;
        }
        start_date = 0;
    }
    else if ( start_date != 0 &&
              ( start_date > p_buffer->start_date + 3 * AOUT_PTS_TOLERANCE ) )
    {
        msg_Warn( p_aout, "audio drift is too big ("I64Fd"), dropping buffer",
                  start_date - p_buffer->start_date );
        aout_BufferFree( p_buffer );
        return 0;
    }

    if ( start_date == 0 ) start_date = p_buffer->start_date;

    /* Run pre-filters. */

    aout_FiltersPlay( p_aout, p_input->pp_filters, p_input->i_nb_filters,
                      &p_buffer );

    /* Run the resampler if needed.
     * We first need to calculate the output rate of this resampler. */
    if ( ( p_input->i_resampling_type == AOUT_RESAMPLING_NONE ) &&
         ( start_date < p_buffer->start_date - AOUT_PTS_TOLERANCE
           || start_date > p_buffer->start_date + AOUT_PTS_TOLERANCE ) &&
         p_input->i_nb_resamplers > 0 )
    {
        /* Can happen in several circumstances :
         * 1. A problem at the input (clock drift)
         * 2. A small pause triggered by the user
         * 3. Some delay in the output stage, causing a loss of lip
         *    synchronization
         * Solution : resample the buffer to avoid a scratch.
         */
        mtime_t drift = p_buffer->start_date - start_date;

        p_input->i_resamp_start_date = mdate();
        p_input->i_resamp_start_drift = (int)drift;

        if ( drift > 0 )
            p_input->i_resampling_type = AOUT_RESAMPLING_DOWN;
        else
            p_input->i_resampling_type = AOUT_RESAMPLING_UP;

        msg_Warn( p_aout, "buffer is "I64Fd" %s, triggering %ssampling",
                          drift > 0 ? drift : -drift,
                          drift > 0 ? "in advance" : "late",
                          drift > 0 ? "down" : "up");
    }

    if ( p_input->i_resampling_type != AOUT_RESAMPLING_NONE )
    {
        /* Resampling has been triggered previously (because of dates
         * mismatch). We want the resampling to happen progressively so
         * it isn't too audible to the listener. */

        if( p_input->i_resampling_type == AOUT_RESAMPLING_UP )
        {
            p_input->pp_resamplers[0]->input.i_rate += 2; /* Hz */
        }
        else
        {
            p_input->pp_resamplers[0]->input.i_rate -= 2; /* Hz */
        }

        /* Check if everything is back to normal, in which case we can stop the
         * resampling */
        if( p_input->pp_resamplers[0]->input.i_rate ==
              p_input->input.i_rate )
        {
            p_input->i_resampling_type = AOUT_RESAMPLING_NONE;
            msg_Warn( p_aout, "resampling stopped after "I64Fi" usec "
                      "(drift: "I64Fi")",
                      mdate() - p_input->i_resamp_start_date,
                      p_buffer->start_date - start_date);
        }
        else if( abs( (int)(p_buffer->start_date - start_date) ) <
                 abs( p_input->i_resamp_start_drift ) / 2 )
        {
            /* if we reduced the drift from half, then it is time to switch
             * back the resampling direction. */
            if( p_input->i_resampling_type == AOUT_RESAMPLING_UP )
                p_input->i_resampling_type = AOUT_RESAMPLING_DOWN;
            else
                p_input->i_resampling_type = AOUT_RESAMPLING_UP;
            p_input->i_resamp_start_drift = 0;
        }
        else if( p_input->i_resamp_start_drift &&
                 ( abs( (int)(p_buffer->start_date - start_date) ) >
                   abs( p_input->i_resamp_start_drift ) * 3 / 2 ) )
        {
            /* If the drift is increasing and not decreasing, than something
             * is bad. We'd better stop the resampling right now. */
            msg_Warn( p_aout, "timing screwed, stopping resampling" );
            p_input->i_resampling_type = AOUT_RESAMPLING_NONE;
            p_input->pp_resamplers[0]->input.i_rate = p_input->input.i_rate;
        }
    }

    /* Adding the start date will be managed by aout_FifoPush(). */
    p_buffer->end_date = start_date +
        (p_buffer->end_date - p_buffer->start_date);
    p_buffer->start_date = start_date;

    /* Actually run the resampler now. */
    if ( p_input->i_nb_resamplers > 0 )
    {
        aout_FiltersPlay( p_aout, p_input->pp_resamplers,
                          p_input->i_nb_resamplers,
                          &p_buffer );
    }

    vlc_mutex_lock( &p_aout->input_fifos_lock );
    aout_FifoPush( p_aout, &p_input->fifo, p_buffer );
    vlc_mutex_unlock( &p_aout->input_fifos_lock );

    return 0;
}

static int ChangeFiltersString( aout_instance_t * p_aout,
                                 char *psz_name, vlc_bool_t b_add )
{
    vlc_value_t val;
    char *psz_parser;

    var_Get( p_aout, "audio-filter", &val );

    if( !val.psz_string ) val.psz_string = strdup("");

    psz_parser = strstr( val.psz_string, psz_name );

    if( b_add )
    {
        if( !psz_parser )
        {
            psz_parser = val.psz_string;
            asprintf( &val.psz_string, (*val.psz_string) ? "%s:%s" : "%s%s",
                      val.psz_string, psz_name );
            free( psz_parser );
        }
        else
        {
            return 0;
        }
    }
    else
    {
        if( psz_parser )
        {
            memmove( psz_parser, psz_parser + strlen(psz_name) +
                     (*(psz_parser + strlen(psz_name)) == ':' ? 1 : 0 ),
                     strlen(psz_parser + strlen(psz_name)) + 1 );
        }
        else
        {
            free( val.psz_string );
            return 0;
        }
    }

    var_Set( p_aout, "audio-filter", val );
    free( val.psz_string );
    return 1;
}

static int VisualizationCallback( vlc_object_t *p_this, char const *psz_cmd,
                       vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    aout_instance_t *p_aout = (aout_instance_t *)p_this;
    char *psz_mode = newval.psz_string;
    vlc_value_t val;
    int i;

    if( !psz_mode || !*psz_mode )
    {
        ChangeFiltersString( p_aout, "goom", VLC_FALSE );
        ChangeFiltersString( p_aout, "visual", VLC_FALSE );
        ChangeFiltersString( p_aout, "galaktos", VLC_FALSE );
    }
    else
    {
        if( !strcmp( "goom", psz_mode ) )
        {
            ChangeFiltersString( p_aout, "visual", VLC_FALSE );
            ChangeFiltersString( p_aout, "goom", VLC_TRUE );
            ChangeFiltersString( p_aout, "galaktos", VLC_FALSE );
        }
        else if( !strcmp( "galaktos", psz_mode ) )
        {
            ChangeFiltersString( p_aout, "visual", VLC_FALSE );
            ChangeFiltersString( p_aout, "goom", VLC_FALSE );
            ChangeFiltersString( p_aout, "galaktos", VLC_TRUE );
        }
        else
        {
            val.psz_string = psz_mode;
            var_Create( p_aout, "effect-list", VLC_VAR_STRING );
            var_Set( p_aout, "effect-list", val );

            ChangeFiltersString( p_aout, "goom", VLC_FALSE );
            ChangeFiltersString( p_aout, "visual", VLC_TRUE );
            ChangeFiltersString( p_aout, "galaktos", VLC_FALSE );
        }
    }

    /* That sucks */
    for( i = 0; i < p_aout->i_nb_inputs; i++ )
    {
        p_aout->pp_inputs[i]->b_restart = VLC_TRUE;
    }

    return VLC_SUCCESS;
}

static int EqualizerCallback( vlc_object_t *p_this, char const *psz_cmd,
                       vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    aout_instance_t *p_aout = (aout_instance_t *)p_this;
    char *psz_mode = newval.psz_string;
    vlc_value_t val;
    int i;
    int i_ret;

    if( !psz_mode || !*psz_mode )
    {
        i_ret = ChangeFiltersString( p_aout, "equalizer", VLC_FALSE );
    }
    else
    {
        val.psz_string = psz_mode;
        var_Create( p_aout, "equalizer-preset", VLC_VAR_STRING );
        var_Set( p_aout, "equalizer-preset", val );
        i_ret = ChangeFiltersString( p_aout, "equalizer", VLC_TRUE );

    }

    /* That sucks */
    if( i_ret == 1 )
    {
        for( i = 0; i < p_aout->i_nb_inputs; i++ )
        {
            p_aout->pp_inputs[i]->b_restart = VLC_TRUE;
        }
    }

    return VLC_SUCCESS;
}

static aout_filter_t * allocateUserChannelMixer( aout_instance_t * p_aout,
                                     audio_sample_format_t * p_input_format,
                                     audio_sample_format_t * p_output_format )
{
    aout_filter_t * p_channel_mixer;

    /* Retreive user preferred channel mixer */
    char * psz_name = config_GetPsz( p_aout, "audio-channel-mixer" );

    /* Not specified => let the main pipeline do the mixing */
    if ( ! psz_name ) return NULL;

    /* Debug information */
    aout_FormatsPrint( p_aout, "channel mixer", p_input_format,
                       p_output_format );

    /* Create a VLC object */
    p_channel_mixer = vlc_object_create( p_aout, sizeof(aout_filter_t) );
    if( p_channel_mixer == NULL )
    {
        msg_Err( p_aout, "cannot add user channel mixer %s", psz_name );
        return NULL;
    }
    vlc_object_attach( p_channel_mixer , p_aout );

    /* Attach the suitable module */
    memcpy( &p_channel_mixer->input, p_input_format,
                    sizeof(audio_sample_format_t) );
    memcpy( &p_channel_mixer->output, p_output_format,
                    sizeof(audio_sample_format_t) );
    p_channel_mixer->p_module =
        module_Need( p_channel_mixer,"audio filter", psz_name, VLC_TRUE );
    if( p_channel_mixer->p_module== NULL )
    {
        msg_Err( p_aout, "cannot add user channel mixer %s", psz_name );
        vlc_object_detach( p_channel_mixer );
        vlc_object_destroy( p_channel_mixer );
        return NULL;
    }
    p_channel_mixer->b_continuity = VLC_FALSE;

    /* Ok */
    return p_channel_mixer;
}
