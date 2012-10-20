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
 * Supply or update the current custom ("hardware") volume.
 * @note This only makes sense after calling aout_VolumeHardInit().
 * @param volume current custom volume
 *
 * @warning The caller (i.e. the audio output plug-in) is responsible for
 * interlocking and synchronizing call to this function and to the
 * audio_output_t.volume_set callback. This ensures that VLC gets correct
 * volume information (possibly with a latency).
 */
static void aout_OutputVolumeReport (audio_output_t *aout, float volume)
{
    var_SetFloat (aout, "volume", volume);
}

static void aout_OutputMuteReport (audio_output_t *aout, bool mute)
{
    var_SetBool (aout, "mute", mute);
}

static void aout_OutputPolicyReport (audio_output_t *aout, bool cork)
{
    (cork ? var_IncInteger : var_DecInteger) (aout->p_parent, "corks");
}

static int aout_OutputGainRequest (audio_output_t *aout, float gain)
{
    aout_owner_t *owner = aout_owner (aout);

    aout_assert_locked (aout);
    aout_volume_SetVolume (owner->volume, gain);
    /* XXX: ideally, return -1 if format cannot be amplified */
    return 0;
}

/*****************************************************************************
 * aout_OutputNew : allocate a new output and rework the filter pipeline
 *****************************************************************************
 * This function is entered with the mixer lock.
 *****************************************************************************/
int aout_OutputNew (audio_output_t *aout, const audio_sample_format_t *fmtp)
{
    aout_owner_t *owner = aout_owner (aout);

    audio_sample_format_t fmt = *fmtp;
    aout_FormatPrepare (&fmt);

    aout_assert_locked (aout);

    aout->event.volume_report = aout_OutputVolumeReport;
    aout->event.mute_report = aout_OutputMuteReport;
    aout->event.policy_report = aout_OutputPolicyReport;
    aout->event.gain_request = aout_OutputGainRequest;

    /* Find the best output plug-in. */
    owner->module = module_need (aout, "audio output", "$aout", false);
    if (owner->module == NULL)
    {
        msg_Err (aout, "no suitable audio output module");
        return -1;
    }

    if (aout->start (aout, &fmt))
    {
        msg_Err (aout, "module not functional");
        module_unneed (aout, owner->module);
        owner->module = NULL;
        return -1;
    }

    if (!var_Type (aout, "stereo-mode"))
        var_Create (aout, "stereo-mode",
                    VLC_VAR_INTEGER | VLC_VAR_HASCHOICE | VLC_VAR_DOINHERIT);

    /* The user may have selected a different channels configuration. */
    var_AddCallback (aout, "stereo-mode", aout_ChannelsRestart, NULL);
    switch (var_GetInteger (aout, "stereo-mode"))
    {
        case AOUT_VAR_CHAN_RSTEREO:
            fmt.i_original_channels |= AOUT_CHAN_REVERSESTEREO;
             break;
        case AOUT_VAR_CHAN_STEREO:
            fmt.i_original_channels = AOUT_CHANS_STEREO;
            break;
        case AOUT_VAR_CHAN_LEFT:
            fmt.i_original_channels = AOUT_CHAN_LEFT;
            break;
        case AOUT_VAR_CHAN_RIGHT:
            fmt.i_original_channels = AOUT_CHAN_RIGHT;
            break;
        case AOUT_VAR_CHAN_DOLBYS:
            fmt.i_original_channels = AOUT_CHANS_STEREO|AOUT_CHAN_DOLBYSTEREO;
            break;
        default:
        {
            if ((fmt.i_original_channels & AOUT_CHAN_PHYSMASK)
                                                         != AOUT_CHANS_STEREO)
                 break;

            vlc_value_t val, txt;
            val.i_int = 0;
            var_Change (aout, "stereo-mode", VLC_VAR_DELCHOICE, &val, NULL);
            txt.psz_string = _("Stereo audio mode");
            var_Change (aout, "stereo-mode", VLC_VAR_SETTEXT, &txt, NULL);
            if (fmt.i_original_channels & AOUT_CHAN_DOLBYSTEREO)
            {
                val.i_int = AOUT_VAR_CHAN_DOLBYS;
                txt.psz_string = _("Dolby Surround");
            }
            else
            {
                val.i_int = AOUT_VAR_CHAN_STEREO;
                txt.psz_string = _("Stereo");
            }
            var_Change (aout, "stereo-mode", VLC_VAR_ADDCHOICE, &val, &txt);
            var_Change (aout, "stereo-mode", VLC_VAR_SETVALUE, &val, NULL);
            val.i_int = AOUT_VAR_CHAN_LEFT;
            txt.psz_string = _("Left");
            var_Change (aout, "stereo-mode", VLC_VAR_ADDCHOICE, &val, &txt);
            if (fmt.i_original_channels & AOUT_CHAN_DUALMONO)
            {   /* Go directly to the left channel. */
                fmt.i_original_channels = AOUT_CHAN_LEFT;
                var_Change (aout, "stereo-mode", VLC_VAR_SETVALUE, &val, NULL);
            }
            val.i_int = AOUT_VAR_CHAN_RIGHT;
            txt.psz_string = _("Right");
            var_Change (aout, "stereo-mode", VLC_VAR_ADDCHOICE, &val, &txt);
            val.i_int = AOUT_VAR_CHAN_RSTEREO;
            txt.psz_string = _("Reverse stereo");
            var_Change (aout, "stereo-mode", VLC_VAR_ADDCHOICE, &val, &txt);
        }
    }

    aout_FormatPrepare (&fmt);
    aout_FormatPrint (aout, "output", &fmt );

    /* Choose the mixer format. */
    owner->mixer_format = fmt;
    if (!AOUT_FMT_LINEAR(&fmt))
        owner->mixer_format.i_format = fmtp->i_format;
    else
    /* Most audio filters can only deal with single-precision,
     * so lets always use that when hardware supports floating point. */
    if( HAVE_FPU )
        owner->mixer_format.i_format = VLC_CODEC_FL32;
    else
    /* Fallback to 16-bits. This avoids pointless conversion to and from
     * 32-bits samples for the sole purpose of software mixing. */
        owner->mixer_format.i_format = VLC_CODEC_S16N;

    aout_FormatPrepare (&owner->mixer_format);
    aout_FormatPrint (aout, "mixer", &owner->mixer_format);

    /* Create filters. */
    owner->nb_filters = 0;
    if (aout_FiltersCreatePipeline (aout, owner->filters, &owner->nb_filters,
                                    &owner->mixer_format, &fmt) < 0)
    {
        msg_Err (aout, "couldn't create audio output pipeline");
        module_unneed (aout, owner->module);
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

    var_DelCallback (aout, "stereo-mode", aout_ChannelsRestart, NULL);
    if (aout->stop != NULL)
        aout->stop (aout);
    module_unneed (aout, owner->module);
    aout->volume_set = NULL;
    aout->mute_set = NULL;
    owner->module = NULL;
    aout_FiltersDestroyPipeline (owner->filters, owner->nb_filters);
}

/**
 * Plays a decoded audio buffer.
 */
void aout_OutputPlay (audio_output_t *aout, block_t *block)
{
    aout_owner_t *owner = aout_owner (aout);
    mtime_t drift = 0;

    aout_assert_locked (aout);

    aout_FiltersPlay (owner->filters, owner->nb_filters, &block);
    if (block == NULL)
        return;
    if (block->i_buffer == 0)
    {
        block_Release (block);
        return;
    }

    if (likely(owner->module != NULL))
        aout->play (aout, block, &drift);
    else
        block_Release (block);
/**
 * Notifies the audio input of the drift from the requested audio
 * playback timestamp (@ref block_t.i_pts) to the anticipated playback time
 * as reported by the audio output hardware.
 * Depending on the drift amplitude, the input core may ignore the drift
 * trigger upsampling or downsampling, or even discard samples.
 * Future VLC versions may instead adjust the input decoding speed.
 *
 * The audio output plugin is responsible for estimating the drift. A negative
 * value means playback is ahead of the intended time and a positive value
 * means playback is late from the intended time. In most cases, the audio
 * output can estimate the delay until playback of the next sample to be
 * queued. Then, before the block is queued:
 *    drift = mdate() + delay - block->i_pts
 * where mdate() + delay is the estimated time when the sample will be rendered
 * and block->i_pts is the intended time.
 */
    if (drift < -AOUT_MAX_PTS_ADVANCE || +AOUT_MAX_PTS_DELAY < drift)
    {
        msg_Warn (aout, "not synchronized (%"PRId64" us), resampling",
                  drift);
        if (date_Get (&owner->sync.date) != VLC_TS_INVALID)
            date_Move (&owner->sync.date, drift);
    }
}

/**
 * Notifies the audio output (if any) of pause/resume events.
 * This enables the output to expedite pause, instead of waiting for its
 * buffers to drain.
 */
void aout_OutputPause( audio_output_t *aout, bool pause, mtime_t date )
{
    aout_assert_locked( aout );
    if( aout->pause != NULL )
        aout->pause( aout, pause, date );
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

    if( aout->flush != NULL )
        aout->flush( aout, wait );
}
