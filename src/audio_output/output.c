/*****************************************************************************
 * output.c : internal management of output streams for the audio output
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <assert.h>
#include <vlc_common.h>
#include <vlc_aout.h>
#include <vlc_aout_intf.h>
#include <vlc_cpu.h>
#include <vlc_modules.h>

#include "libvlc.h"
#include "aout_internal.h"

/*****************************************************************************
 * aout_OutputNew : allocate a new output and rework the filter pipeline
 *****************************************************************************
 * This function is entered with the mixer lock.
 *****************************************************************************/
int aout_OutputNew( audio_output_t * p_aout,
                    const audio_sample_format_t * p_format )
{
    vlc_assert_locked( &p_aout->lock );
    p_aout->format = *p_format;

    /* Retrieve user defaults. */
    int i_rate = var_InheritInteger( p_aout, "aout-rate" );
    if ( i_rate != 0 )
        p_aout->format.i_rate = i_rate;
    aout_FormatPrepare( &p_aout->format );

    /* Find the best output plug-in. */
    p_aout->module = module_need( p_aout, "audio output", "$aout", false );
    if ( p_aout->module == NULL )
    {
        msg_Err( p_aout, "no suitable audio output module" );
        return -1;
    }

    if ( var_Type( p_aout, "audio-channels" ) ==
             (VLC_VAR_INTEGER | VLC_VAR_HASCHOICE) )
    {
        /* The user may have selected a different channels configuration. */
        switch( var_InheritInteger( p_aout, "audio-channels" ) )
        {
            case AOUT_VAR_CHAN_RSTEREO:
                p_aout->format.i_original_channels |= AOUT_CHAN_REVERSESTEREO;
                break;
            case AOUT_VAR_CHAN_STEREO:
                p_aout->format.i_original_channels =
                                              AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT;
                break;
            case AOUT_VAR_CHAN_LEFT:
                p_aout->format.i_original_channels = AOUT_CHAN_LEFT;
                break;
            case AOUT_VAR_CHAN_RIGHT:
                p_aout->format.i_original_channels = AOUT_CHAN_RIGHT;
                break;
            case AOUT_VAR_CHAN_DOLBYS:
                p_aout->format.i_original_channels =
                      AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_DOLBYSTEREO;
                break;
        }
    }
    else if ( p_aout->format.i_physical_channels == AOUT_CHAN_CENTER
              && (p_aout->format.i_original_channels
                   & AOUT_CHAN_PHYSMASK) == (AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT) )
    {
        vlc_value_t val, text;

        /* Mono - create the audio-channels variable. */
        var_Create( p_aout, "audio-channels",
                    VLC_VAR_INTEGER | VLC_VAR_HASCHOICE );
        text.psz_string = _("Audio Channels");
        var_Change( p_aout, "audio-channels", VLC_VAR_SETTEXT, &text, NULL );

        val.i_int = AOUT_VAR_CHAN_STEREO; text.psz_string = _("Stereo");
        var_Change( p_aout, "audio-channels", VLC_VAR_ADDCHOICE, &val, &text );
        val.i_int = AOUT_VAR_CHAN_LEFT; text.psz_string = _("Left");
        var_Change( p_aout, "audio-channels", VLC_VAR_ADDCHOICE, &val, &text );
        val.i_int = AOUT_VAR_CHAN_RIGHT; text.psz_string = _("Right");
        var_Change( p_aout, "audio-channels", VLC_VAR_ADDCHOICE, &val, &text );
        if ( p_aout->format.i_original_channels & AOUT_CHAN_DUALMONO )
        {
            /* Go directly to the left channel. */
            p_aout->format.i_original_channels = AOUT_CHAN_LEFT;
            var_SetInteger( p_aout, "audio-channels", AOUT_VAR_CHAN_LEFT );
        }
        var_AddCallback( p_aout, "audio-channels", aout_ChannelsRestart,
                         NULL );
    }
    else if ( p_aout->format.i_physical_channels ==
               (AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT)
                && (p_aout->format.i_original_channels &
                     (AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT)) )
    {
        vlc_value_t val, text;

        /* Stereo - create the audio-channels variable. */
        var_Create( p_aout, "audio-channels",
                    VLC_VAR_INTEGER | VLC_VAR_HASCHOICE );
        text.psz_string = _("Audio Channels");
        var_Change( p_aout, "audio-channels", VLC_VAR_SETTEXT, &text, NULL );

        if ( p_aout->format.i_original_channels & AOUT_CHAN_DOLBYSTEREO )
        {
            val.i_int = AOUT_VAR_CHAN_DOLBYS;
            text.psz_string = _("Dolby Surround");
        }
        else
        {
            val.i_int = AOUT_VAR_CHAN_STEREO;
            text.psz_string = _("Stereo");
        }
        var_Change( p_aout, "audio-channels", VLC_VAR_ADDCHOICE, &val, &text );
        val.i_int = AOUT_VAR_CHAN_LEFT; text.psz_string = _("Left");
        var_Change( p_aout, "audio-channels", VLC_VAR_ADDCHOICE, &val, &text );
        val.i_int = AOUT_VAR_CHAN_RIGHT; text.psz_string = _("Right");
        var_Change( p_aout, "audio-channels", VLC_VAR_ADDCHOICE, &val, &text );
        val.i_int = AOUT_VAR_CHAN_RSTEREO; text.psz_string=_("Reverse stereo");
        var_Change( p_aout, "audio-channels", VLC_VAR_ADDCHOICE, &val, &text );
        if ( p_aout->format.i_original_channels & AOUT_CHAN_DUALMONO )
        {
            /* Go directly to the left channel. */
            p_aout->format.i_original_channels = AOUT_CHAN_LEFT;
            var_SetInteger( p_aout, "audio-channels", AOUT_VAR_CHAN_LEFT );
        }
        var_AddCallback( p_aout, "audio-channels", aout_ChannelsRestart,
                         NULL );
    }
    var_TriggerCallback( p_aout, "intf-change" );

    aout_FormatPrepare( &p_aout->format );

    /* Prepare FIFO. */
    aout_FifoInit( p_aout, &p_aout->fifo, p_aout->format.i_rate );
    aout_FormatPrint( p_aout, "output", &p_aout->format );

    /* Choose the mixer format. */
    p_aout->mixer_format = p_aout->format;
    if ( AOUT_FMT_NON_LINEAR(&p_aout->format) )
        p_aout->mixer_format.i_format = p_format->i_format;
    else
    /* Most audio filters can only deal with single-precision,
     * so lets always use that when hardware supports floating point. */
    if( HAVE_FPU )
        p_aout->mixer_format.i_format = VLC_CODEC_FL32;
    else
    /* Otherwise, audio filters will not work. Use fixed-point if the input has
     * more than 16-bits depth. */
    if( p_format->i_bitspersample > 16 )
        p_aout->mixer_format.i_format = VLC_CODEC_FI32;
    else
    /* Fallback to 16-bits. This avoids pointless conversion to and from
     * 32-bits samples for the sole purpose of software mixing. */
        p_aout->mixer_format.i_format = VLC_CODEC_S16N;

    aout_FormatPrepare( &p_aout->mixer_format );
    aout_FormatPrint( p_aout, "mixer", &p_aout->mixer_format );

    /* Create filters. */
    p_aout->i_nb_filters = 0;
    if ( aout_FiltersCreatePipeline( p_aout, p_aout->pp_filters,
                                     &p_aout->i_nb_filters,
                                     &p_aout->mixer_format,
                                     &p_aout->format ) < 0 )
    {
        msg_Err( p_aout, "couldn't create audio output pipeline" );
        module_unneed( p_aout, p_aout->module );
        p_aout->module = NULL;
        return -1;
    }
    return 0;
}

/*****************************************************************************
 * aout_OutputDelete : delete the output
 *****************************************************************************
 * This function is entered with the mixer lock.
 *****************************************************************************/
void aout_OutputDelete( audio_output_t * p_aout )
{
    vlc_assert_locked( &p_aout->lock );

    if( p_aout->module == NULL )
        return;

    module_unneed( p_aout, p_aout->module );
    aout_VolumeNoneInit( p_aout ); /* clear volume callback */
    p_aout->module = NULL;
    aout_FiltersDestroyPipeline( p_aout->pp_filters, p_aout->i_nb_filters );
    aout_FifoDestroy( &p_aout->fifo );
}

static block_t *aout_OutputSlice( audio_output_t *, aout_fifo_t * );

/*****************************************************************************
 * aout_OutputPlay : play a buffer
 *****************************************************************************
 * This function is entered with the mixer lock.
 *****************************************************************************/
void aout_OutputPlay( audio_output_t * p_aout, aout_buffer_t * p_buffer )
{
    vlc_assert_locked( &p_aout->lock );

    aout_FiltersPlay( p_aout->pp_filters, p_aout->i_nb_filters, &p_buffer );
    if( !p_buffer )
        return;
    if( p_buffer->i_buffer == 0 )
    {
        block_Release( p_buffer );
        return;
    }

    aout_fifo_t *fifo = &p_aout->p_input->fifo;
    /* XXX: cleanup */
    aout_FifoPush( fifo, p_buffer );

    while( (p_buffer = aout_OutputSlice( p_aout, fifo ) ) != NULL )
    {
        aout_FifoPush( &p_aout->fifo, p_buffer );
        p_aout->pf_play( p_aout );
    }
}

/**
 * Notifies the audio output (if any) of pause/resume events.
 * This enables the output to expedite pause, instead of waiting for its
 * buffers to drain.
 */
void aout_OutputPause( audio_output_t *aout, bool pause, mtime_t date )
{
    vlc_assert_locked( &aout->lock );

    if( aout->pf_pause != NULL )
        aout->pf_pause( aout, pause, date );
    if( !pause )
    {
        mtime_t duration = date - aout->p_input->i_pause_date;
        /* XXX: ^ onk onk! gruik! ^ */
        aout_FifoMoveDates( &aout->fifo, duration );
    }
}

/**
 * Flushes or drains the audio output buffers.
 * This enables the output to expedite seek and stop.
 * @param wait if true, wait for buffer playback (i.e. drain),
 *             if false, discard the buffers immediately (i.e. flush)
 */
void aout_OutputFlush( audio_output_t *aout, bool wait )
{
    vlc_assert_locked( &aout->lock );

    if( aout->pf_flush != NULL )
        aout->pf_flush( aout, wait );
    aout_FifoReset( &aout->fifo );
}


/*** Volume handling ***/

/**
 * Dummy volume setter. This is the default volume setter.
 */
static int aout_VolumeNoneSet (audio_output_t *aout, float volume, bool mute)
{
    (void)aout; (void)volume; (void)mute;
    return -1;
}

/**
 * Configures the dummy volume setter.
 * @note Audio output plugins for which volume is irrelevant
 * should call this function during activation.
 */
void aout_VolumeNoneInit (audio_output_t *aout)
{
    /* aout_New() -safely- calls this function without the lock, before any
     * other thread knows of this audio output instance.
    vlc_assert_locked (&aout->lock); */
    aout->pf_volume_set = aout_VolumeNoneSet;
}

/**
 * Volume setter for software volume.
 */
static int aout_VolumeSoftSet (audio_output_t *aout, float volume, bool mute)
{
    vlc_assert_locked (&aout->lock);

    /* Cubic mapping from software volume to amplification factor.
     * This provides a good tradeoff between low and high volume ranges.
     *
     * This code is only used for the VLC software mixer. If you change this
     * formula, be sure to update the aout_VolumeHardInit()-based plugins also.
     */
    if (!mute)
        volume = volume * volume * volume;
    else
        volume = 0.;

    aout->mixer_multiplier = volume;
    return 0;
}

/**
 * Configures the volume setter for software mixing
 * and apply the default volume.
 * @note Audio output plugins that cannot apply the volume
 * should call this function during activation.
 */
void aout_VolumeSoftInit (audio_output_t *aout)
{
    audio_volume_t volume = var_InheritInteger (aout, "volume");
    bool mute = var_InheritBool (aout, "mute");

    vlc_assert_locked (&aout->lock);
    aout->pf_volume_set = aout_VolumeSoftSet;
    aout_VolumeSoftSet (aout, volume / (float)AOUT_VOLUME_DEFAULT, mute);
}

/**
 * Configures a custom volume setter. This is used by audio outputs that can
 * control the hardware volume directly and/or emulate it internally.
 * @param setter volume setter callback
 */
void aout_VolumeHardInit (audio_output_t *aout, aout_volume_cb setter)
{
    vlc_assert_locked (&aout->lock);
    aout->pf_volume_set = setter;
}

/**
 * Supply or update the current custom ("hardware") volume.
 * @note This only makes sense after calling aout_VolumeHardInit().
 * @param setter volume setter callback
 * @param volume current custom volume
 * @param mute current mute flag
 * @note Audio output plugins that cannot apply the volume
 * should call this function during activation.
 */
void aout_VolumeHardSet (audio_output_t *aout, float volume, bool mute)
{
#warning FIXME
    /* REVISIT: This is tricky. We cannot acquire the volume lock as this gets
     * called from the audio output (it would cause a lock inversion).
     * We also should not override the input manager volume, but only the
     * volume of the current audio output... FIXME */
    msg_Err (aout, "%s(%f, %u)", __func__, volume, (unsigned)mute);
}


/*** Buffer management ***/

/**
 * Rearranges audio blocks in correct number of samples.
 * @note (FIXME) This is left here for historical reasons. It belongs in the
 * output code. Besides, this operation should be avoided if possible.
 */
static block_t *aout_OutputSlice (audio_output_t *p_aout, aout_fifo_t *p_fifo)
{
    const unsigned samples = p_aout->i_nb_samples;
    /* FIXME: Remove this silly constraint. Just pass buffers as they come to
     * "smart" audio outputs. */
    assert( samples > 0 );

    vlc_assert_locked( &p_aout->lock );

    /* Retrieve the date of the next buffer. */
    date_t exact_start_date = p_aout->fifo.end_date;
    mtime_t start_date = date_Get( &exact_start_date );

    /* See if we have enough data to prepare a new buffer for the audio output. */
    aout_buffer_t *p_buffer = p_fifo->p_first;
    if( p_buffer == NULL )
        return NULL;

    /* Find the earliest start date available. */
    if ( start_date == VLC_TS_INVALID )
    {
        start_date = p_buffer->i_pts;
        date_Set( &exact_start_date, start_date );
    }
    /* Compute the end date for the new buffer. */
    mtime_t end_date = date_Increment( &exact_start_date, samples );

    /* Check that start_date is available. */
    mtime_t prev_date;
    for( ;; )
    {
        /* Check for the continuity of start_date */
        prev_date = p_buffer->i_pts + p_buffer->i_length;
        if( prev_date >= start_date - 1 )
            break;
        /* We authorize a +-1 because rounding errors get compensated
         * regularly. */
        msg_Warn( p_aout, "got a packet in the past (%"PRId64")",
                  start_date - prev_date );
        aout_BufferFree( aout_FifoPop( p_fifo ) );

        p_buffer = p_fifo->p_first;
        if( p_buffer == NULL )
            return NULL;
    }

    /* Check that we have enough samples. */
    while( prev_date < end_date )
    {
        p_buffer = p_buffer->p_next;
        if( p_buffer == NULL )
            return NULL;

        /* Check that all buffers are contiguous. */
        if( prev_date != p_buffer->i_pts )
        {
            msg_Warn( p_aout,
                      "buffer hole, dropping packets (%"PRId64")",
                      p_buffer->i_pts - prev_date );

            aout_buffer_t *p_deleted;
            while( (p_deleted = p_fifo->p_first) != p_buffer )
                aout_BufferFree( aout_FifoPop( p_fifo ) );
        }

        prev_date = p_buffer->i_pts + p_buffer->i_length;
    }

    if( !AOUT_FMT_NON_LINEAR( &p_aout->format ) )
    {
        p_buffer = p_fifo->p_first;

        /* Additionally check that p_first_byte_to_mix is well located. */
        const unsigned framesize = p_aout->format.i_bytes_per_frame;
        ssize_t delta = (start_date - p_buffer->i_pts)
                      * p_aout->format.i_rate / CLOCK_FREQ;
        if( delta != 0 )
            msg_Warn( p_aout, "input start is not output end (%zd)", delta );
        if( delta < 0 )
        {
            /* Is it really the best way to do it ? */
            aout_FifoReset( &p_aout->fifo );
            return NULL;
        }
        if( delta > 0 )
        {
            mtime_t t = delta * CLOCK_FREQ / p_aout->format.i_rate;
            p_buffer->i_nb_samples -= delta;
            p_buffer->i_pts += t;
            p_buffer->i_length -= t;
            delta *= framesize;
            p_buffer->p_buffer += delta;
            p_buffer->i_buffer -= delta;
        }

        /* Build packet with adequate number of samples */
        unsigned needed = samples * framesize;
        p_buffer = block_Alloc( needed );
        if( unlikely(p_buffer == NULL) )
            /* XXX: should free input buffers */
            return NULL;
        p_buffer->i_nb_samples = samples;

        for( uint8_t *p_out = p_buffer->p_buffer; needed > 0; )
        {
            aout_buffer_t *p_inbuf = p_fifo->p_first;
            if( unlikely(p_inbuf == NULL) )
            {
                msg_Err( p_aout, "packetization error" );
                vlc_memset( p_out, 0, needed );
                break;
            }

            const uint8_t *p_in = p_inbuf->p_buffer;
            size_t avail = p_inbuf->i_nb_samples * framesize;
            if( avail > needed )
            {
                vlc_memcpy( p_out, p_in, needed );
                p_fifo->p_first->p_buffer += needed;
                p_fifo->p_first->i_buffer -= needed;
                needed /= framesize;
                p_fifo->p_first->i_nb_samples -= needed;

                mtime_t t = needed * CLOCK_FREQ / p_aout->format.i_rate;
                p_fifo->p_first->i_pts += t;
                p_fifo->p_first->i_length -= t;
                break;
            }

            vlc_memcpy( p_out, p_in, avail );
            needed -= avail;
            p_out += avail;
            /* Next buffer */
            aout_BufferFree( aout_FifoPop( p_fifo ) );
        }
    }
    else
        p_buffer = aout_FifoPop( p_fifo );

    p_buffer->i_pts = start_date;
    p_buffer->i_length = end_date - start_date;

    return p_buffer;
}

/*****************************************************************************
 * aout_OutputNextBuffer : give the audio output plug-in the right buffer
 *****************************************************************************
 * If b_can_sleek is 1, the aout core functions won't try to resample
 * new buffers to catch up - that is we suppose that the output plug-in can
 * compensate it by itself. S/PDIF outputs should always set b_can_sleek = 1.
 * This function is entered with no lock at all :-).
 *****************************************************************************/
aout_buffer_t * aout_OutputNextBuffer( audio_output_t * p_aout,
                                       mtime_t start_date,
                                       bool b_can_sleek )
{
    aout_fifo_t *p_fifo = &p_aout->fifo;
    aout_buffer_t * p_buffer;
    mtime_t now = mdate();

    aout_lock( p_aout );

    /* Drop the audio sample if the audio output is really late.
     * In the case of b_can_sleek, we don't use a resampler so we need to be
     * a lot more severe. */
    while( ((p_buffer = p_fifo->p_first) != NULL)
     && p_buffer->i_pts < (b_can_sleek ? start_date : now) - AOUT_MAX_PTS_DELAY )
    {
        msg_Dbg( p_aout, "audio output is too slow (%"PRId64"), "
                 "trashing %"PRId64"us", now - p_buffer->i_pts,
                 p_buffer->i_length );
        aout_BufferFree( aout_FifoPop( p_fifo ) );
    }

    if( p_buffer == NULL )
    {
#if 0 /* This is bad because the audio output might just be trying to fill
       * in its internal buffers. And anyway, it's up to the audio output
       * to deal with this kind of starvation. */

        /* Set date to 0, to allow the mixer to send a new buffer ASAP */
        aout_FifoReset( &p_aout->fifo );
        if ( !p_aout->b_starving )
            msg_Dbg( p_aout,
                 "audio output is starving (no input), playing silence" );
        p_aout->b_starving = true;
#endif
        goto out;
    }

    mtime_t delta = start_date - p_buffer->i_pts;
    /* Here we suppose that all buffers have the same duration - this is
     * generally true, and anyway if it's wrong it won't be a disaster.
     */
    if ( 0 > delta + p_buffer->i_length )
    {
        if ( !p_aout->b_starving )
            msg_Dbg( p_aout, "audio output is starving (%"PRId64"), "
                     "playing silence", -delta );
        p_aout->b_starving = true;
        p_buffer = NULL;
        goto out;
    }

    p_aout->b_starving = false;
    p_buffer = aout_FifoPop( p_fifo );

    if( !b_can_sleek
     && ( delta > AOUT_MAX_PTS_DELAY || delta < -AOUT_MAX_PTS_ADVANCE ) )
    {
        /* Try to compensate the drift by doing some resampling. */
        msg_Warn( p_aout, "output date isn't PTS date, requesting "
                  "resampling (%"PRId64")", delta );

        aout_FifoMoveDates( &p_aout->p_input->fifo, delta );
        aout_FifoMoveDates( p_fifo, delta );
    }
out:
    aout_unlock( p_aout );
    return p_buffer;
}
