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

#include <math.h>

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
int aout_OutputNew( audio_output_t *p_aout,
                    const audio_sample_format_t * p_format )
{
    aout_owner_t *owner = aout_owner (p_aout);

    aout_assert_locked( p_aout );
    p_aout->format = *p_format;

    /* Retrieve user defaults. */
    int i_rate = var_InheritInteger( p_aout, "aout-rate" );
    if ( i_rate != 0 )
        p_aout->format.i_rate = i_rate;
    aout_FormatPrepare( &p_aout->format );

    /* Find the best output plug-in. */
    owner->module = module_need (p_aout, "audio output", "$aout", false);
    if (owner->module == NULL)
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
    aout_FormatPrint( p_aout, "output", &p_aout->format );

    /* Choose the mixer format. */
    owner->mixer_format = p_aout->format;
    if (AOUT_FMT_NON_LINEAR(&p_aout->format))
        owner->mixer_format.i_format = p_format->i_format;
    else
    /* Most audio filters can only deal with single-precision,
     * so lets always use that when hardware supports floating point. */
    if( HAVE_FPU )
        owner->mixer_format.i_format = VLC_CODEC_FL32;
    else
    /* Otherwise, audio filters will not work. Use fixed-point if the input has
     * more than 16-bits depth. */
    if( p_format->i_bitspersample > 16 )
        owner->mixer_format.i_format = VLC_CODEC_FI32;
    else
    /* Fallback to 16-bits. This avoids pointless conversion to and from
     * 32-bits samples for the sole purpose of software mixing. */
        owner->mixer_format.i_format = VLC_CODEC_S16N;

    aout_FormatPrepare (&owner->mixer_format);
    aout_FormatPrint (p_aout, "mixer", &owner->mixer_format);

    /* Create filters. */
    owner->nb_filters = 0;
    if (aout_FiltersCreatePipeline (p_aout, owner->filters,
                                    &owner->nb_filters, &owner->mixer_format,
                                    &p_aout->format) < 0)
    {
        msg_Err( p_aout, "couldn't create audio output pipeline" );
        module_unneed (p_aout, owner->module);
        owner->module = NULL;
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
    aout_owner_t *owner = aout_owner (p_aout);

    aout_assert_locked( p_aout );

    if (owner->module == NULL)
        return;

    module_unneed (p_aout, owner->module);
    aout_VolumeNoneInit( p_aout ); /* clear volume callback */
    owner->module = NULL;
    aout_FiltersDestroyPipeline (owner->filters, owner->nb_filters);
}

/*****************************************************************************
 * aout_OutputPlay : play a buffer
 *****************************************************************************
 * This function is entered with the mixer lock.
 *****************************************************************************/
void aout_OutputPlay (audio_output_t *aout, block_t *block)
{
    aout_owner_t *owner = aout_owner (aout);

    aout_assert_locked (aout);

    aout_FiltersPlay (owner->filters, owner->nb_filters, &block);
    if (block == NULL)
        return;
    if (block->i_buffer == 0)
    {
        block_Release (block);
        return;
    }

    aout->pf_play (aout, block);
}

/**
 * Notifies the audio output (if any) of pause/resume events.
 * This enables the output to expedite pause, instead of waiting for its
 * buffers to drain.
 */
void aout_OutputPause( audio_output_t *aout, bool pause, mtime_t date )
{
    aout_assert_locked( aout );
    if( aout->pf_pause != NULL )
        aout->pf_pause( aout, pause, date );
}

/**
 * Flushes or drains the audio output buffers.
 * This enables the output to expedite seek and stop.
 * @param wait if true, wait for buffer playback (i.e. drain),
 *             if false, discard the buffers immediately (i.e. flush)
 */
void aout_OutputFlush( audio_output_t *aout, bool wait )
{
    aout_assert_locked( aout );

    if( aout->pf_flush != NULL )
        aout->pf_flush( aout, wait );
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
    aout_assert_locked (aout); */
    aout->pf_volume_set = aout_VolumeNoneSet;
    var_Destroy (aout, "volume");
    var_Destroy (aout, "mute");
}

/**
 * Volume setter for software volume.
 */
static int aout_VolumeSoftSet (audio_output_t *aout, float volume, bool mute)
{
    aout_owner_t *owner = aout_owner (aout);

    aout_assert_locked (aout);

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

    owner->volume.multiplier = volume;
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

    aout_assert_locked (aout);
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
    aout_assert_locked (aout);
    aout->pf_volume_set = setter;
    var_Create (aout, "volume", VLC_VAR_INTEGER|VLC_VAR_DOINHERIT);
    var_Create (aout, "mute", VLC_VAR_BOOL|VLC_VAR_DOINHERIT);
}

/**
 * Supply or update the current custom ("hardware") volume.
 * @note This only makes sense after calling aout_VolumeHardInit().
 * @param setter volume setter callback
 * @param volume current custom volume
 * @param mute current mute flag
 *
 * @warning The caller (i.e. the audio output plug-in) is responsible for
 * interlocking and synchronizing call to this function and to the
 * audio_output_t.pf_volume_set callback. This ensures that VLC gets correct
 * volume information (possibly with a latency).
 */
void aout_VolumeHardSet (audio_output_t *aout, float volume, bool mute)
{
    audio_volume_t vol = lroundf (volume * (float)AOUT_VOLUME_DEFAULT);

    /* We cannot acquire the volume lock as this gets called from the audio
     * output plug-in (it would cause a lock inversion). */
    var_SetInteger (aout, "volume", vol);
    var_SetBool (aout, "mute", mute);
    var_TriggerCallback (aout, "intf-change");
}


/*** Packet-oriented audio output support ***/

static inline aout_packet_t *aout_packet (audio_output_t *aout)
{
    return (aout_packet_t *)(aout->sys);
}

void aout_PacketInit (audio_output_t *aout, aout_packet_t *p, unsigned samples)
{
    assert (p == aout_packet (aout));

    vlc_mutex_init (&p->lock);
    aout_FifoInit (aout, &p->partial, aout->format.i_rate);
    aout_FifoInit (aout, &p->fifo, aout->format.i_rate);
    p->pause_date = VLC_TS_INVALID;
    p->time_report = VLC_TS_INVALID;
    p->samples = samples;
    p->starving = true;
}

void aout_PacketDestroy (audio_output_t *aout)
{
    aout_packet_t *p = aout_packet (aout);

    aout_FifoDestroy (&p->partial);
    aout_FifoDestroy (&p->fifo);
    vlc_mutex_destroy (&p->lock);
}

static block_t *aout_OutputSlice (audio_output_t *);

void aout_PacketPlay (audio_output_t *aout, block_t *block)
{
    aout_packet_t *p = aout_packet (aout);
    mtime_t time_report;

    vlc_mutex_lock (&p->lock);
    aout_FifoPush (&p->partial, block);
    while ((block = aout_OutputSlice (aout)) != NULL)
        aout_FifoPush (&p->fifo, block);

    time_report = p->time_report;
    p->time_report = VLC_TS_INVALID;
    vlc_mutex_unlock (&p->lock);

    if (time_report != VLC_TS_INVALID)
        aout_TimeReport (aout, mdate () + time_report);
}

void aout_PacketPause (audio_output_t *aout, bool pause, mtime_t date)
{
    aout_packet_t *p = aout_packet (aout);

    if (pause)
    {
        assert (p->pause_date == VLC_TS_INVALID);
        p->pause_date = date;
    }
    else
    {
        assert (p->pause_date != VLC_TS_INVALID);

        mtime_t duration = date - p->pause_date;

        p->pause_date = VLC_TS_INVALID;
        vlc_mutex_lock (&p->lock);
        aout_FifoMoveDates (&p->partial, duration);
        aout_FifoMoveDates (&p->fifo, duration);
        vlc_mutex_unlock (&p->lock);
    }
}

void aout_PacketFlush (audio_output_t *aout, bool drain)
{
    aout_packet_t *p = aout_packet (aout);

    vlc_mutex_lock (&p->lock);
    aout_FifoReset (&p->partial);
    aout_FifoReset (&p->fifo);
    vlc_mutex_unlock (&p->lock);

    (void) drain; /* TODO */
}


/**
 * Rearranges audio blocks in correct number of samples.
 * @note (FIXME) This is left here for historical reasons. It belongs in the
 * output code. Besides, this operation should be avoided if possible.
 */
static block_t *aout_OutputSlice (audio_output_t *p_aout)
{
    aout_packet_t *p = aout_packet (p_aout);
    aout_fifo_t *p_fifo = &p->partial;
    const unsigned samples = p->samples;
    assert( samples > 0 );

    vlc_assert_locked( &p->lock );

    /* Retrieve the date of the next buffer. */
    date_t exact_start_date = p->fifo.end_date;
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
            aout_FifoReset (&p->fifo);
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

/**
 * Dequeues the next audio packet (a.k.a. audio fragment).
 * The audio output plugin must first call aout_PacketPlay() to queue the
 * decoded audio samples. Typically, audio_output_t.pf_play is set to, or calls
 * aout_PacketPlay().
 * @note This function is considered legacy. Please do not use this function in
 * new audio output plugins.
 * @param p_aout audio output instance
 * @param start_date expected PTS of the audio packet
 */
block_t *aout_PacketNext (audio_output_t *p_aout, mtime_t start_date)
{
    aout_packet_t *p = aout_packet (p_aout);
    aout_fifo_t *p_fifo = &p->fifo;
    block_t *p_buffer;
    const bool b_can_sleek = AOUT_FMT_NON_LINEAR (&p_aout->format);
    const mtime_t now = mdate ();
    const mtime_t threshold =
        (b_can_sleek ? start_date : now) - AOUT_MAX_PTS_DELAY;

    vlc_mutex_lock( &p->lock );
    if( p->pause_date != VLC_TS_INVALID )
        goto out; /* paused: do not dequeue buffers */

    for (;;)
    {
        p_buffer = p_fifo->p_first;
        if (p_buffer == NULL)
            goto out; /* nothing to play */

        if (p_buffer->i_pts >= threshold)
            break;

        /* Drop the audio sample if the audio output is really late.
         * In the case of b_can_sleek, we don't use a resampler so we need to
         * be a lot more severe. */
        msg_Dbg (p_aout, "audio output is too slow (%"PRId64" us): "
                 " trashing %"PRId64" us", threshold - p_buffer->i_pts,
                 p_buffer->i_length);
        block_Release (aout_FifoPop (p_fifo));
    }

    mtime_t delta = start_date - p_buffer->i_pts;
    /* This assumes that all buffers have the same duration. This is true
     * since aout_PacketPlay() (aout_OutputSlice()) is used. */
    if (0 >= delta + p_buffer->i_length)
    {
        if (!p->starving)
        {
            msg_Dbg (p_aout, "audio output is starving (%"PRId64"), "
                     "playing silence", delta);
            p->starving = true;
        }
        goto out; /* nothing to play _yet_ */
    }

    p->starving = false;
    p_buffer = aout_FifoPop( p_fifo );

    if (!b_can_sleek
     && (delta < -AOUT_MAX_PTS_ADVANCE || AOUT_MAX_PTS_DELAY < delta))
    {
        msg_Warn (p_aout, "audio output out of sync, "
                          "adjusting dates (%"PRId64" us)", delta);
        aout_FifoMoveDates (&p->partial, delta);
        aout_FifoMoveDates (p_fifo, delta);
        p->time_report = delta;
    }
    vlc_mutex_unlock( &p->lock );
    return p_buffer;
out:
    vlc_mutex_unlock( &p->lock );
    return NULL;
}
