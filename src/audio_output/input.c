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
#include <vlc_input.h>
#include <vlc_aout.h>
#include <vlc_filter.h>

#include <libvlc.h>
#include "aout_internal.h"

static void inputDrop( aout_input_t *, block_t * );
static void inputResamplingStop( audio_output_t *, aout_input_t * );

/*****************************************************************************
 * aout_InputNew : allocate a new input and rework the filter pipeline
 *****************************************************************************/
aout_input_t *aout_InputNew (const audio_sample_format_t *restrict infmt)
{
    aout_input_t *p_input = xmalloc (sizeof (*p_input));

    p_input->samplerate = infmt->i_rate;

    p_input->i_resampling_type = AOUT_RESAMPLING_NONE;
    p_input->i_last_input_rate = INPUT_RATE_DEFAULT;
    p_input->i_buffer_lost = 0;
    return p_input;
}

/*****************************************************************************
 * aout_InputDelete : delete an input
 *****************************************************************************
 * This function must be entered with the mixer lock.
 *****************************************************************************/
void aout_InputDelete (aout_input_t * p_input )
{
    free (p_input);
}

/*****************************************************************************
 * aout_InputPlay : play a buffer
 *****************************************************************************
 * This function must be entered with the input lock.
 *****************************************************************************/
block_t *aout_InputPlay(audio_output_t *p_aout, aout_input_t *p_input,
                        block_t *p_buffer, int i_input_rate, date_t *date )
{
    mtime_t start_date;
    aout_owner_t *owner = aout_owner(p_aout);

    aout_assert_locked( p_aout );

    if( i_input_rate != INPUT_RATE_DEFAULT && owner->rate_filter == NULL )
    {
        inputDrop( p_input, p_buffer );
        return NULL;
    }

    /* Handle input rate change, but keep drift correction */
    if( i_input_rate != p_input->i_last_input_rate )
    {
        unsigned int * const pi_rate = &owner->rate_filter->fmt_in.audio.i_rate;
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
        inputResamplingStop( p_aout, p_input );
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
        inputResamplingStop( p_aout, p_input );
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
        inputResamplingStop( p_aout, p_input );
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

    /* Run pre-filters. */
    p_buffer = aout_FiltersPipelinePlay( owner->filters, owner->nb_filters,
                                         p_buffer );
    if( !p_buffer )
        return NULL;

    /* Run the resampler if needed.
     * We first need to calculate the output rate of this resampler. */
    if ( ( p_input->i_resampling_type == AOUT_RESAMPLING_NONE ) &&
         ( drift < -AOUT_MAX_PTS_ADVANCE || drift > +AOUT_MAX_PTS_DELAY ) &&
         owner->resampler != NULL )
    {
        /* Can happen in several circumstances :
         * 1. A problem at the input (clock drift)
         * 2. A small pause triggered by the user
         * 3. Some delay in the output stage, causing a loss of lip
         *    synchronization
         * Solution : resample the buffer to avoid a scratch.
         */
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
            owner->resampler->fmt_in.audio.i_rate += 2; /* Hz */
        else
            owner->resampler->fmt_in.audio.i_rate -= 2; /* Hz */

        /* Check if everything is back to normal, in which case we can stop the
         * resampling */
        unsigned int i_nominal_rate =
          (owner->resampler == owner->rate_filter)
          ? INPUT_RATE_DEFAULT * p_input->samplerate / i_input_rate
          : p_input->samplerate;
        if( owner->resampler->fmt_in.audio.i_rate == i_nominal_rate )
        {
            p_input->i_resampling_type = AOUT_RESAMPLING_NONE;
            msg_Warn( p_aout, "resampling stopped (drift: %"PRIi64")",
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
            inputResamplingStop( p_aout, p_input );
            p_buffer->i_flags |= BLOCK_FLAG_DISCONTINUITY;
        }
    }

    /* Actually run the resampler now. */
    if ( owner->resampler != NULL )
        p_buffer = aout_FiltersPipelinePlay( &owner->resampler, 1, p_buffer );

    if( !p_buffer )
        return NULL;
    if( p_buffer->i_nb_samples <= 0 )
    {
        block_Release( p_buffer );
        return NULL;
    }

    p_buffer->i_pts = start_date;
    return p_buffer;
}

/*****************************************************************************
 * static functions
 *****************************************************************************/

static void inputDrop( aout_input_t *p_input, block_t *p_buffer )
{
    block_Release( p_buffer );

    p_input->i_buffer_lost++;
}

static void inputResamplingStop( audio_output_t *p_aout, aout_input_t *p_input )
{
    aout_owner_t *owner = aout_owner(p_aout);

    p_input->i_resampling_type = AOUT_RESAMPLING_NONE;
    if( owner->resampler != NULL )
    {
        owner->resampler->fmt_in.audio.i_rate =
            ( owner->resampler == owner->rate_filter )
            ? INPUT_RATE_DEFAULT * p_input->samplerate / p_input->i_last_input_rate
            : p_input->samplerate;
    }
}
