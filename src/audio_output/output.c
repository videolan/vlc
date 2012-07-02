/*****************************************************************************
 * output.c : internal management of output streams for the audio output
 *****************************************************************************
 * Copyright (C) 2002-2004 VLC authors and VideoLAN
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <math.h>

#include <vlc_common.h>
#include <vlc_aout.h>
#include <vlc_aout_intf.h>
#include <vlc_cpu.h>
#include <vlc_modules.h>

#include "libvlc.h"
#include "aout_internal.h"

/**
 * Notifies the audio input of the drift from the requested audio
 * playback timestamp (@ref block_t.i_pts) to the anticipated playback time
 * as reported by the audio output hardware.
 * Depending on the drift amplitude, the input core may ignore the drift
 * trigger upsampling or downsampling, or even discard samples.
 * Future VLC versions may instead adjust the input decoding speed.
 *
 * The audio output plugin is responsible for estimating the ideal current
 * playback time defined as follows:
 *  ideal time = buffer timestamp - (output latency + pending buffer duration)
 *
 * Practically, this is the PTS (block_t.i_pts) of the current buffer minus
 * the latency reported by the output programming interface.
 * Computing the estimated drift directly would probably be more intuitive.
 * However the use of an absolute time value does not introduce extra
 * measurement errors due to the CPU scheduling jitter and clock resolution.
 * Furthermore, the ideal while it is an abstract value, is easy for most
 * audio output plugins to compute.
 * The following definition is equivalent but depends on the clock time:
 *  ideal time = real time + drift

 * @note If aout_LatencyReport() is never called, the core will assume that
 * there is no drift.
 *
 * @param ideal estimated ideal time as defined above.
 */
static void aout_OutputTimeReport (audio_output_t *aout, mtime_t ideal)
{
    mtime_t delta = mdate() - ideal /* = -drift */;

    aout_assert_locked (aout);
    if (delta < -AOUT_MAX_PTS_ADVANCE || +AOUT_MAX_PTS_DELAY < delta)
    {
        aout_owner_t *owner = aout_owner (aout);

        msg_Warn (aout, "not synchronized (%"PRId64" us), resampling",
                  delta);
        if (date_Get (&owner->sync.date) != VLC_TS_INVALID)
            date_Move (&owner->sync.date, delta);
    }
}

/**
 * Supply or update the current custom ("hardware") volume.
 * @note This only makes sense after calling aout_VolumeHardInit().
 * @param volume current custom volume
 *
 * @warning The caller (i.e. the audio output plug-in) is responsible for
 * interlocking and synchronizing call to this function and to the
 * audio_output_t.pf_volume_set callback. This ensures that VLC gets correct
 * volume information (possibly with a latency).
 */
static void aout_OutputVolumeReport (audio_output_t *aout, float volume)
{
    long vol = lroundf (volume * (float)AOUT_VOLUME_DEFAULT);

    /* We cannot acquire the volume lock as this gets called from the audio
     * output plug-in (it would cause a lock inversion). */
    var_SetInteger (aout, "volume", vol);
}

static void aout_OutputMuteReport (audio_output_t *aout, bool mute)
{
    var_SetBool (aout, "mute", mute);
}

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
    aout_FormatPrepare( &p_aout->format );

    p_aout->event.time_report = aout_OutputTimeReport;
    p_aout->event.volume_report = aout_OutputVolumeReport;
    p_aout->event.mute_report = aout_OutputMuteReport;

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

    aout_FormatPrepare( &p_aout->format );
    aout_FormatPrint( p_aout, "output", &p_aout->format );

    /* Choose the mixer format. */
    owner->mixer_format = p_aout->format;
    if (!AOUT_FMT_LINEAR(&p_aout->format))
        owner->mixer_format.i_format = p_format->i_format;
    else
    /* Most audio filters can only deal with single-precision,
     * so lets always use that when hardware supports floating point. */
    if( HAVE_FPU )
        owner->mixer_format.i_format = VLC_CODEC_FL32;
    else
    /* Otherwise, audio filters will not work. Use fixed-point if the input has
     * more than 16-bits depth. */
    if( p_format->i_bitspersample > 16 || !AOUT_FMT_LINEAR(p_format))
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

/**
 * Destroys the audio output plug-in instance.
 */
void aout_OutputDelete (audio_output_t *aout)
{
    aout_owner_t *owner = aout_owner (aout);

    aout_assert_locked (aout);

    if (owner->module == NULL)
        return;

    module_unneed (aout, owner->module);
    /* Clear callbacks */
    aout->pf_play = aout_DecDeleteBuffer; /* gruik */
    aout->pf_pause = NULL;
    aout->pf_flush = NULL;
    aout->volume_set = NULL;
    aout->mute_set = NULL;
    owner->module = NULL;
    owner->volume.amp = 1.f;
    owner->volume.mute = false;
    aout_FiltersDestroyPipeline (owner->filters, owner->nb_filters);
}

/**
 * Plays a decoded audio buffer.
 */
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

static int aout_SoftVolumeSet (audio_output_t *aout, float volume)
{
    aout_owner_t *owner = aout_owner (aout);

    aout_assert_locked (aout);
    /* Cubic mapping from software volume to amplification factor.
     * This provides a good tradeoff between low and high volume ranges.
     *
     * This code is only used for the VLC software mixer. If you change this
     * formula, be sure to update the aout_VolumeHardInit()-based plugins also.
     */
    owner->volume.amp = volume * volume * volume;
    aout_VolumeReport (aout, volume);
    return 0;
}

static int aout_SoftMuteSet (audio_output_t *aout, bool mute)
{
    aout_owner_t *owner = aout_owner (aout);

    aout_assert_locked (aout);
    owner->volume.mute = mute;
    aout_MuteReport (aout, mute);
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
    long volume = var_GetInteger (aout, "volume");
    bool mute = var_GetBool (aout, "mute");

    aout_assert_locked (aout);
    aout->volume_set = aout_SoftVolumeSet;
    aout->mute_set = aout_SoftMuteSet;
    aout_SoftVolumeSet (aout, volume / (float)AOUT_VOLUME_DEFAULT);
    aout_SoftMuteSet (aout, mute);
}
