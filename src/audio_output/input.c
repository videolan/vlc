/*****************************************************************************
 * input.c : internal management of input streams for the audio output
 *****************************************************************************
 * Copyright (C) 2002-2007 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>

#include <stdio.h>
#include <string.h>
#include <assert.h>

#include <vlc_input.h>
#include <vlc_vout.h>                  /* for vout_Request */
#include <vlc_modules.h>
#include <vlc_aout.h>
#include <vlc_filter.h>
#include <libvlc.h>

#include "aout_internal.h"

static void inputDrop( aout_input_t *, aout_buffer_t * );
static void inputResamplingStop( aout_input_t *p_input );

static int VisualizationCallback( vlc_object_t *, char const *,
                                  vlc_value_t, vlc_value_t, void * );
static int EqualizerCallback( vlc_object_t *, char const *,
                              vlc_value_t, vlc_value_t, void * );

static vout_thread_t *RequestVout( void *,
                                   vout_thread_t *, video_format_t *, bool );

/*****************************************************************************
 * aout_InputNew : allocate a new input and rework the filter pipeline
 *****************************************************************************/
aout_input_t *aout_InputNew (audio_output_t * p_aout,
                             const audio_sample_format_t *restrict infmt,
                             const audio_sample_format_t *restrict outfmt,
                             const aout_request_vout_t *p_request_vout)
{
    aout_input_t *p_input = malloc (sizeof (*p_input));
    if (unlikely(p_input == NULL))
        return NULL;

    aout_FormatPrint( p_aout, "input", infmt );
    p_input->samplerate = infmt->i_rate;

    p_input->i_nb_resamplers = p_input->i_nb_filters = 0;

    /* */
    if( p_request_vout )
    {
        p_input->request_vout = *p_request_vout;
    }
    else
    {
        p_input->request_vout.pf_request_vout = RequestVout;
        p_input->request_vout.p_private = p_aout;
    }

    /* Prepare format structure */
    audio_sample_format_t chain_input_format = *infmt;
    audio_sample_format_t chain_output_format = *outfmt;

    chain_output_format.i_rate = infmt->i_rate;
    aout_FormatPrepare( &chain_output_format );

    /* Now add user filters */
    var_AddCallback( p_aout, "visual", VisualizationCallback, p_input );
    var_AddCallback( p_aout, "equalizer", EqualizerCallback, p_input );

    char *psz_filters = var_GetString( p_aout, "audio-filter" );
    char *psz_visual = var_GetString( p_aout, "audio-visual");
    char *psz_scaletempo = var_InheritBool( p_aout, "audio-time-stretch" ) ? strdup( "scaletempo" ) : NULL;

    p_input->b_recycle_vout = psz_visual && *psz_visual;

    /* parse user filter lists */
    char *const ppsz_array[] = { psz_scaletempo, psz_filters, psz_visual };
    p_input->p_playback_rate_filter = NULL;

    for (unsigned i_visual = 0;
         i_visual < 3 && AOUT_FMT_LINEAR(&chain_output_format);
         i_visual++)
    {
        char *psz_next = NULL;
        char *psz_parser = ppsz_array[i_visual];

        if( psz_parser == NULL || !*psz_parser )
            continue;

        while( psz_parser && *psz_parser )
        {
            filter_t * p_filter = NULL;

            if( p_input->i_nb_filters >= AOUT_MAX_FILTERS )
            {
                msg_Dbg( p_aout, "max filters reached (%d)", AOUT_MAX_FILTERS );
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

            /* Create a VLC object */
            p_filter = vlc_custom_create( p_aout, sizeof(*p_filter),
                                          "audio filter" );
            if( p_filter == NULL )
            {
                msg_Err( p_aout, "cannot add user filter %s (skipped)",
                         psz_parser );
                psz_parser = psz_next;
                continue;
            }

            p_filter->p_owner = malloc( sizeof(*p_filter->p_owner) );
            p_filter->p_owner->p_aout  = p_aout;
            p_filter->p_owner->p_input = p_input;

            /* request format */
            memcpy( &p_filter->fmt_in.audio, &chain_output_format,
                    sizeof(audio_sample_format_t) );
            p_filter->fmt_in.i_codec = chain_output_format.i_format;
            memcpy( &p_filter->fmt_out.audio, &chain_output_format,
                    sizeof(audio_sample_format_t) );
            p_filter->fmt_out.i_codec = chain_output_format.i_format;

            /* try to find the requested filter */
            if( i_visual == 2 ) /* this can only be a visualization module */
            {
                p_filter->p_module = module_need( p_filter, "visualization2",
                                                  psz_parser, true );
            }
            else /* this can be a audio filter module as well as a visualization module */
            {
                p_filter->p_module = module_need( p_filter, "audio filter",
                                              psz_parser, true );

                if ( p_filter->p_module == NULL )
                {
                    /* if the filter requested a special format, retry */
                    if ( !( AOUT_FMTS_IDENTICAL( &p_filter->fmt_in.audio,
                                                 &chain_input_format )
                            && AOUT_FMTS_IDENTICAL( &p_filter->fmt_out.audio,
                                                    &chain_output_format ) ) )
                    {
                        aout_FormatPrepare( &p_filter->fmt_in.audio );
                        aout_FormatPrepare( &p_filter->fmt_out.audio );
                        p_filter->p_module = module_need( p_filter,
                                                          "audio filter",
                                                          psz_parser, true );
                    }
                    /* try visual filters */
                    else
                    {
                        memcpy( &p_filter->fmt_in.audio, &chain_output_format,
                                sizeof(audio_sample_format_t) );
                        memcpy( &p_filter->fmt_out.audio, &chain_output_format,
                                sizeof(audio_sample_format_t) );
                        p_filter->p_module = module_need( p_filter,
                                                          "visualization2",
                                                          psz_parser, true );
                    }
                }
            }

            /* failure */
            if ( p_filter->p_module == NULL )
            {
                msg_Err( p_aout, "cannot add user filter %s (skipped)",
                         psz_parser );

                free( p_filter->p_owner );
                vlc_object_release( p_filter );

                psz_parser = psz_next;
                continue;
            }

            /* complete the filter chain if necessary */
            if ( aout_FiltersCreatePipeline( p_aout, p_input->pp_filters,
                                             &p_input->i_nb_filters,
                                             &chain_input_format,
                                             &p_filter->fmt_in.audio ) < 0 )
            {
                msg_Err( p_aout, "cannot add user filter %s (skipped)",
                         psz_parser );

                module_unneed( p_filter, p_filter->p_module );
                free( p_filter->p_owner );
                vlc_object_release( p_filter );

                psz_parser = psz_next;
                continue;
            }

            /* success */
            p_input->pp_filters[p_input->i_nb_filters++] = p_filter;
            memcpy( &chain_input_format, &p_filter->fmt_out.audio,
                    sizeof( audio_sample_format_t ) );

            if( i_visual == 0 ) /* scaletempo */
                p_input->p_playback_rate_filter = p_filter;

            /* next filter if any */
            psz_parser = psz_next;
        }
    }
    free( psz_visual );
    free( psz_filters );
    free( psz_scaletempo );

    /* complete the filter chain if necessary */
    if ( aout_FiltersCreatePipeline( p_aout, p_input->pp_filters,
                                     &p_input->i_nb_filters,
                                     &chain_input_format,
                                     &chain_output_format ) < 0 )
    {
        msg_Err (p_aout, "cannot setup filtering pipeline");
        goto error;
    }

    /* Create resamplers. */
    if (AOUT_FMT_LINEAR(outfmt))
    {
        chain_output_format.i_rate = (__MAX(p_input->samplerate,
                                            outfmt->i_rate)
                                 * (100 + AOUT_MAX_RESAMPLING)) / 100;
        if ( chain_output_format.i_rate == outfmt->i_rate )
        {
            /* Just in case... */
            chain_output_format.i_rate++;
        }
        if (aout_FiltersCreatePipeline (p_aout, p_input->pp_resamplers,
                                        &p_input->i_nb_resamplers,
                                        &chain_output_format, outfmt) < 0)
        {
            msg_Err (p_aout, "cannot setup a resampling pipeline");
            goto error;
        }

        /* Setup the initial rate of the resampler */
        p_input->pp_resamplers[0]->fmt_in.audio.i_rate = p_input->samplerate;
    }
    p_input->i_resampling_type = AOUT_RESAMPLING_NONE;

    if( ! p_input->p_playback_rate_filter && p_input->i_nb_resamplers > 0 )
    {
        p_input->p_playback_rate_filter = p_input->pp_resamplers[0];
    }

    /* Success */
    p_input->i_last_input_rate = INPUT_RATE_DEFAULT;
    p_input->i_buffer_lost = 0;
    return p_input;

error:
    aout_FiltersDestroyPipeline( p_input->pp_filters, p_input->i_nb_filters );
    aout_FiltersDestroyPipeline( p_input->pp_resamplers,
                                 p_input->i_nb_resamplers );
    free (p_input);
    return NULL;
}

/*****************************************************************************
 * aout_InputDelete : delete an input
 *****************************************************************************
 * This function must be entered with the mixer lock.
 *****************************************************************************/
int aout_InputDelete( audio_output_t * p_aout, aout_input_t * p_input )
{
    var_DelCallback (p_aout, "equalizer", EqualizerCallback, p_input);
    var_DelCallback (p_aout, "visual", VisualizationCallback, p_input);

    /* XXX We need to update b_recycle_vout before calling aout_FiltersDestroyPipeline.
     * FIXME They can be a race condition if audio-visual is updated between
     * aout_InputDelete and aout_InputNew.
     */
    char *psz_visual = var_GetString( p_aout, "audio-visual");
    p_input->b_recycle_vout = psz_visual && *psz_visual;
    free( psz_visual );

    aout_FiltersDestroyPipeline( p_input->pp_filters, p_input->i_nb_filters );
    p_input->i_nb_filters = 0;
    aout_FiltersDestroyPipeline( p_input->pp_resamplers,
                                 p_input->i_nb_resamplers );
    p_input->i_nb_resamplers = 0;

    return 0;
}

/*****************************************************************************
 * aout_InputPlay : play a buffer
 *****************************************************************************
 * This function must be entered with the input lock.
 *****************************************************************************/
/* XXX Do not activate it !! */
//#define AOUT_PROCESS_BEFORE_CHEKS
block_t *aout_InputPlay(audio_output_t *p_aout, aout_input_t *p_input,
                        block_t *p_buffer, int i_input_rate, date_t *date )
{
    mtime_t start_date;

    aout_assert_locked( p_aout );

    if( i_input_rate != INPUT_RATE_DEFAULT && p_input->p_playback_rate_filter == NULL )
    {
        inputDrop( p_input, p_buffer );
        return NULL;
    }

#ifdef AOUT_PROCESS_BEFORE_CHEKS
    /* Run pre-filters. */
    aout_FiltersPlay( p_aout, p_input->pp_filters, p_input->i_nb_filters,
                      &p_buffer );
    if( !p_buffer )
        return NULL;

    /* Actually run the resampler now. */
    if ( p_input->i_nb_resamplers > 0 )
    {
        const mtime_t i_date = p_buffer->i_pts;
        aout_FiltersPlay( p_aout, p_input->pp_resamplers,
                          p_input->i_nb_resamplers,
                          &p_buffer );
    }

    if( !p_buffer )
        return NULL;
    if( p_buffer->i_nb_samples <= 0 )
    {
        block_Release( p_buffer );
        return NULL;
    }
#endif

    /* Handle input rate change, but keep drift correction */
    if( i_input_rate != p_input->i_last_input_rate )
    {
        unsigned int * const pi_rate = &p_input->p_playback_rate_filter->fmt_in.audio.i_rate;
#define F(r,ir) ( INPUT_RATE_DEFAULT * (r) / (ir) )
        const int i_delta = *pi_rate - F(p_input->samplerate,p_input->i_last_input_rate);
        *pi_rate = F(p_input->samplerate + i_delta, i_input_rate);
#undef F
        p_input->i_last_input_rate = i_input_rate;
    }

    mtime_t now = mdate();

    /* We don't care if someone changes the start date behind our back after
     * this. We'll deal with that when pushing the buffer, and compensate
     * with the next incoming buffer. */
    start_date = date_Get (date);

    if ( start_date != VLC_TS_INVALID && start_date < now )
    {
        /* The decoder is _very_ late. This can only happen if the user
         * pauses the stream (or if the decoder is buggy, which cannot
         * happen :). */
        msg_Warn( p_aout, "computed PTS is out of range (%"PRId64"), "
                  "clearing out", now - start_date );
        aout_OutputFlush( p_aout, false );
        if ( p_input->i_resampling_type != AOUT_RESAMPLING_NONE )
            msg_Warn( p_aout, "timing screwed, stopping resampling" );
        inputResamplingStop( p_input );
        p_buffer->i_flags |= BLOCK_FLAG_DISCONTINUITY;
        start_date = VLC_TS_INVALID;
    }

    if ( p_buffer->i_pts < now + AOUT_MIN_PREPARE_TIME )
    {
        /* The decoder gives us f*cked up PTS. It's its business, but we
         * can't present it anyway, so drop the buffer. */
        msg_Warn( p_aout, "PTS is out of range (%"PRId64"), dropping buffer",
                  now - p_buffer->i_pts );
        inputDrop( p_input, p_buffer );
        inputResamplingStop( p_input );
        return NULL;
    }

    /* If the audio drift is too big then it's not worth trying to resample
     * the audio. */
    if( start_date == VLC_TS_INVALID )
    {
        start_date = p_buffer->i_pts;
        date_Set (date, start_date);
    }

    mtime_t drift = start_date - p_buffer->i_pts;

    if( drift < -i_input_rate * 3 * AOUT_MAX_PTS_ADVANCE / INPUT_RATE_DEFAULT )
    {
        msg_Warn( p_aout, "buffer way too early (%"PRId64"), clearing queue",
                  drift );
        aout_OutputFlush( p_aout, false );
        if ( p_input->i_resampling_type != AOUT_RESAMPLING_NONE )
            msg_Warn( p_aout, "timing screwed, stopping resampling" );
        inputResamplingStop( p_input );
        p_buffer->i_flags |= BLOCK_FLAG_DISCONTINUITY;
        start_date = p_buffer->i_pts;
        date_Set (date, start_date);
        drift = 0;
    }
    else
    if( drift > +i_input_rate * 3 * AOUT_MAX_PTS_DELAY / INPUT_RATE_DEFAULT )
    {
        msg_Warn( p_aout, "buffer way too late (%"PRId64"), dropping buffer",
                  drift );
        inputDrop( p_input, p_buffer );
        return NULL;
    }

#ifndef AOUT_PROCESS_BEFORE_CHEKS
    /* Run pre-filters. */
    aout_FiltersPlay( p_input->pp_filters, p_input->i_nb_filters, &p_buffer );
    if( !p_buffer )
        return NULL;
#endif

    /* Run the resampler if needed.
     * We first need to calculate the output rate of this resampler. */
    if ( ( p_input->i_resampling_type == AOUT_RESAMPLING_NONE ) &&
         ( drift < -AOUT_MAX_PTS_ADVANCE || drift > +AOUT_MAX_PTS_DELAY ) &&
         p_input->i_nb_resamplers > 0 )
    {
        /* Can happen in several circumstances :
         * 1. A problem at the input (clock drift)
         * 2. A small pause triggered by the user
         * 3. Some delay in the output stage, causing a loss of lip
         *    synchronization
         * Solution : resample the buffer to avoid a scratch.
         */
        p_input->i_resamp_start_date = now;
        p_input->i_resamp_start_drift = (int)-drift;
        p_input->i_resampling_type = (drift < 0) ? AOUT_RESAMPLING_DOWN
                                                 : AOUT_RESAMPLING_UP;
        msg_Warn( p_aout, (drift < 0)
                  ? "buffer too early (%"PRId64"), down-sampling"
                  : "buffer too late  (%"PRId64"), up-sampling", drift );
    }

    if ( p_input->i_resampling_type != AOUT_RESAMPLING_NONE )
    {
        /* Resampling has been triggered previously (because of dates
         * mismatch). We want the resampling to happen progressively so
         * it isn't too audible to the listener. */

        if( p_input->i_resampling_type == AOUT_RESAMPLING_UP )
            p_input->pp_resamplers[0]->fmt_in.audio.i_rate += 2; /* Hz */
        else
            p_input->pp_resamplers[0]->fmt_in.audio.i_rate -= 2; /* Hz */

        /* Check if everything is back to normal, in which case we can stop the
         * resampling */
        unsigned int i_nominal_rate =
          (p_input->pp_resamplers[0] == p_input->p_playback_rate_filter)
          ? INPUT_RATE_DEFAULT * p_input->samplerate / i_input_rate
          : p_input->samplerate;
        if( p_input->pp_resamplers[0]->fmt_in.audio.i_rate == i_nominal_rate )
        {
            p_input->i_resampling_type = AOUT_RESAMPLING_NONE;
            msg_Warn( p_aout, "resampling stopped after %"PRIi64" usec "
                      "(drift: %"PRIi64")",
                      now - p_input->i_resamp_start_date,
                      p_buffer->i_pts - start_date);
        }
        else if( abs( (int)(p_buffer->i_pts - start_date) ) <
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
                 ( abs( (int)(p_buffer->i_pts - start_date) ) >
                   abs( p_input->i_resamp_start_drift ) * 3 / 2 ) )
        {
            /* If the drift is increasing and not decreasing, than something
             * is bad. We'd better stop the resampling right now. */
            msg_Warn( p_aout, "timing screwed, stopping resampling" );
            inputResamplingStop( p_input );
            p_buffer->i_flags |= BLOCK_FLAG_DISCONTINUITY;
        }
    }

#ifndef AOUT_PROCESS_BEFORE_CHEKS
    /* Actually run the resampler now. */
    if ( p_input->i_nb_resamplers > 0 )
    {
        aout_FiltersPlay( p_input->pp_resamplers, p_input->i_nb_resamplers,
                          &p_buffer );
    }

    if( !p_buffer )
        return NULL;
    if( p_buffer->i_nb_samples <= 0 )
    {
        block_Release( p_buffer );
        return NULL;
    }
#endif

    p_buffer->i_pts = start_date;
    return p_buffer;
}

/*****************************************************************************
 * static functions
 *****************************************************************************/

static void inputDrop( aout_input_t *p_input, aout_buffer_t *p_buffer )
{
    aout_BufferFree( p_buffer );

    p_input->i_buffer_lost++;
}

static void inputResamplingStop( aout_input_t *p_input )
{
    p_input->i_resampling_type = AOUT_RESAMPLING_NONE;
    if( p_input->i_nb_resamplers != 0 )
    {
        p_input->pp_resamplers[0]->fmt_in.audio.i_rate =
            ( p_input->pp_resamplers[0] == p_input->p_playback_rate_filter )
            ? INPUT_RATE_DEFAULT * p_input->samplerate / p_input->i_last_input_rate
            : p_input->samplerate;
    }
}

static vout_thread_t *RequestVout( void *p_private,
                                   vout_thread_t *p_vout, video_format_t *p_fmt, bool b_recycle )
{
    audio_output_t *p_aout = p_private;
    VLC_UNUSED(b_recycle);
    vout_configuration_t cfg = {
        .vout       = p_vout,
        .input      = NULL,
        .change_fmt = true,
        .fmt        = p_fmt,
        .dpb_size   = 1,
    };
    return vout_Request( p_aout, &cfg );
}

vout_thread_t *aout_filter_RequestVout( filter_t *p_filter,
                                        vout_thread_t *p_vout, video_format_t *p_fmt )
{
    aout_input_t *p_input = p_filter->p_owner->p_input;
    aout_request_vout_t *p_request = &p_input->request_vout;

    /* XXX: this only works from audio input */
    /* If you want to use visualization filters from another place, you will
     * need to add a new pf_aout_request_vout callback or store a pointer
     * to aout_request_vout_t inside filter_t (i.e. a level of indirection). */

    return p_request->pf_request_vout( p_request->p_private,
                                       p_vout, p_fmt, p_input->b_recycle_vout );
}

static inline bool ChangeFiltersString (vlc_object_t *aout, const char *var,
                                        const char *filter, bool add)
{
    return aout_ChangeFilterString (aout, aout, var, filter, add);
}

static int VisualizationCallback (vlc_object_t *obj, char const *var,
                                  vlc_value_t oldval, vlc_value_t newval,
                                  void *data)
{
    const char *mode = newval.psz_string;
    //aout_input_t *input = data;
    (void) data;

    if (!*mode)
    {
        ChangeFiltersString (obj, "audio-visual", "goom", false);
        ChangeFiltersString (obj, "audio-visual", "visual", false);
        ChangeFiltersString (obj, "audio-visual", "projectm", false);
        ChangeFiltersString (obj, "audio-visual", "vsxu", false);
    }
    else if (!strcmp ("goom", mode))
    {
        ChangeFiltersString (obj, "audio-visual", "visual", false );
        ChangeFiltersString (obj, "audio-visual", "goom", true );
        ChangeFiltersString (obj, "audio-visual", "projectm", false );
        ChangeFiltersString (obj, "audio-visual", "vsxu", false);
    }
    else if (!strcmp ("projectm", mode))
    {
        ChangeFiltersString (obj, "audio-visual", "visual", false);
        ChangeFiltersString (obj, "audio-visual", "goom", false);
        ChangeFiltersString (obj, "audio-visual", "projectm", true);
        ChangeFiltersString (obj, "audio-visual", "vsxu", false);
    }
    else if (!strcmp ("vsxu", mode))
    {
        ChangeFiltersString (obj, "audio-visual", "visual", false);
        ChangeFiltersString (obj, "audio-visual", "goom", false);
        ChangeFiltersString (obj, "audio-visual", "projectm", false);
        ChangeFiltersString (obj, "audio-visual", "vsxu", true);
    }
    else
    {
        var_Create (obj, "effect-list", VLC_VAR_STRING);
        var_SetString (obj, "effect-list", mode);

        ChangeFiltersString (obj, "audio-visual", "goom", false);
        ChangeFiltersString (obj, "audio-visual", "visual", true);
        ChangeFiltersString (obj, "audio-visual", "projectm", false);
    }

    /* That sucks FIXME: use "input" instead of cast */
    aout_InputRequestRestart ((audio_output_t *)obj);

    (void) var; (void) oldval;
    return VLC_SUCCESS;
}

static int EqualizerCallback (vlc_object_t *obj, char const *cmd,
                              vlc_value_t oldval, vlc_value_t newval,
                              void *data)
{
    char *mode = newval.psz_string;
    //aout_input_t *input = data;
    bool ret;

    (void) data;
    (void) cmd; (void) oldval;
    if (!*mode)
        ret = ChangeFiltersString (obj, "audio-filter", "equalizer", false);
    else
    {
        var_Create (obj, "equalizer-preset", VLC_VAR_STRING);
        var_SetString (obj, "equalizer-preset", mode);
        ret = ChangeFiltersString (obj, "audio-filter", "equalizer", true);
    }

    /* That sucks */
    if (ret)
        aout_InputRequestRestart ((audio_output_t *)obj);
    return VLC_SUCCESS;
}
