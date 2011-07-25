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

#include <vlc_common.h>
#include <vlc_aout.h>
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
    p_aout->module = NULL;
    aout_FiltersDestroyPipeline( p_aout->pp_filters, p_aout->i_nb_filters );
    aout_FifoDestroy( &p_aout->fifo );
}

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

    aout_FifoPush( &p_aout->fifo, p_buffer );
    p_aout->pf_play( p_aout );
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
        aout_FifoSet( &p_aout->fifo, 0 );
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

        aout_FifoMoveDates( &p_aout->p_input->mixer.fifo, delta );
        aout_FifoMoveDates( p_fifo, delta );
    }
out:
    aout_unlock( p_aout );
    return p_buffer;
}
