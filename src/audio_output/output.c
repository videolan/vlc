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

#include <vlc_common.h>
#include <vlc_aout.h>
#include <vlc_cpu.h>

#include "libvlc.h"
#include "aout_internal.h"

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

    if (aout->start (aout, &fmt))
    {
        msg_Err (aout, "module not functional");
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
        aout_OutputDelete (aout);
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

    var_DelCallback (aout, "stereo-mode", aout_ChannelsRestart, NULL);
    if (aout->stop != NULL)
        aout->stop (aout);
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

    aout->play (aout, block, &drift);
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
