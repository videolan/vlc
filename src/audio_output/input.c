/*****************************************************************************
 * input.c : internal management of input streams for the audio output
 *****************************************************************************
 * Copyright (C) 2002-2004 VideoLAN
 * $Id: input.c,v 1.43 2004/01/06 12:02:05 zorglub Exp $
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
    char * psz_filters;
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
        var_Create( p_aout, "visual", VLC_VAR_STRING | VLC_VAR_HASCHOICE );
        text.psz_string = _("Visualizations");
        var_Change( p_aout, "visual", VLC_VAR_SETTEXT, &text, NULL );
        val.psz_string = ""; text.psz_string = _("Disable");
        var_Change( p_aout, "visual", VLC_VAR_ADDCHOICE, &val, &text );
        val.psz_string = "random"; text.psz_string = _("Random");
        var_Change( p_aout, "visual", VLC_VAR_ADDCHOICE, &val, &text );
        val.psz_string = "scope"; text.psz_string = _("Scope");
        var_Change( p_aout, "visual", VLC_VAR_ADDCHOICE, &val, &text );
        val.psz_string = "spectrum"; text.psz_string = _("Spectrum");
        var_Change( p_aout, "visual", VLC_VAR_ADDCHOICE, &val, &text );
        /* val.psz_string = "goom"; text.psz_string = _("Goom");
        var_Change( p_aout, "visual", VLC_VAR_ADDCHOICE, &val, &text );*/
        if( var_Get( p_aout, "effect-list", &val ) == VLC_SUCCESS )
        {
            var_Set( p_aout, "visual", val );
            if( val.psz_string ) free( val.psz_string );
        }
        var_AddCallback( p_aout, "visual", VisualizationCallback, NULL );
    }

    if( var_Type( p_aout, "audio-filter" ) == 0 )
    {
        var_Create( p_aout, "audio-filter",
                    VLC_VAR_STRING | VLC_VAR_DOINHERIT );
        text.psz_string = _("Audio filters");
        var_Change( p_aout, "audio-filter", VLC_VAR_SETTEXT, &text, NULL );
    }

    var_Get( p_aout, "audio-filter", &val );
    psz_filters = val.psz_string;
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

            while( *psz_parser == ' ' && *psz_parser == ',' )
            {
                psz_parser++;
            }
            if( ( psz_next = strchr( psz_parser , ','  ) ) )
            {
                *psz_next++ = '\0';
            }
            if( *psz_parser =='\0' )
            {
                break;
            }

            msg_Dbg( p_aout, "user filter %s", psz_parser );

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
                module_Need( p_filter,"audio filter", psz_parser );

            if( p_filter->p_module== NULL )
            {
                msg_Err( p_aout, "cannot add user filter %s (skipped)",
                         psz_parser );

                vlc_object_detach( p_filter );
                vlc_object_destroy( p_filter );
                psz_parser = psz_next;
                continue;

            }
            p_filter->b_continuity = VLC_FALSE;

            p_input->pp_filters[p_input->i_nb_filters++] = p_filter;

            /* next filter if any */
            psz_parser = psz_next;
        }
    }
    if( psz_filters ) free( psz_filters );

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

    p_input->b_error = 0;

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
            p_input->pp_resamplers[0]->input.i_rate += 10; /* Hz */
        }
        else
        {
            p_input->pp_resamplers[0]->input.i_rate -= 10; /* Hz */
        }

        /* Check if everything is back to normal, in which case we can stop the
         * resampling */
        if( p_input->pp_resamplers[0]->input.i_rate ==
              p_input->input.i_rate )
        {
            p_input->i_resampling_type = AOUT_RESAMPLING_NONE;
            msg_Warn( p_aout, "resampling stopped after "I64Fi" usec",
                      mdate() - p_input->i_resamp_start_date );
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

static int VisualizationCallback( vlc_object_t *p_this, char const *psz_cmd,
                       vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    aout_instance_t *p_aout = (aout_instance_t *)p_this;
    vlc_value_t val;
    char *psz_mode = newval.psz_string;
    char *psz_filter;

    var_Get( p_aout, "audio-filter", &val );
    psz_filter = val.psz_string;

    if( !psz_mode || !*psz_mode )
    {
        val.psz_string = "";
        var_Set( p_aout, "audio-filter", val );
    }
    else
    {
        if( !psz_filter || !*psz_filter )
        {
            val.psz_string = "visual";
            var_Set( p_aout, "audio-filter", val );
        }
        else
        {
            if( strstr( psz_filter, "visual" ) == NULL )
            {
                psz_filter = realloc( psz_filter, strlen( psz_filter ) + 20 );
                strcat( psz_filter, ",visual" );
            }
            val.psz_string = psz_filter;
            var_Set( p_aout, "audio-filter", val );
        }
    }

    if( psz_mode && *psz_mode )
    {
        vlc_value_t val;
        val.psz_string = psz_mode;
        var_Create( p_aout, "effect-list", VLC_VAR_STRING );
        var_Set( p_aout, "effect-list", val);
    }

    if( psz_filter ) free( psz_filter );

    aout_Restart( p_aout );

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
                    module_Need( p_channel_mixer,"audio filter", psz_name );
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
