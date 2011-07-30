/*****************************************************************************
 * mixer.c : audio output mixing operations
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

#include <stddef.h>
#include <vlc_common.h>
#include <libvlc.h>
#include <vlc_modules.h>

#include <vlc_aout.h>
#include "aout_internal.h"
/*****************************************************************************
 * aout_MixerNew: prepare a mixer plug-in
 *****************************************************************************
 * Please note that you must hold the mixer lock.
 *****************************************************************************/
int aout_MixerNew( audio_output_t * p_aout )
{
    assert( !p_aout->p_mixer );
    vlc_assert_locked( &p_aout->lock );

    aout_mixer_t *p_mixer = vlc_object_create( p_aout, sizeof(*p_mixer) );
    if( !p_mixer )
        return VLC_EGENERIC;

    p_mixer->fmt = p_aout->mixer_format;
    p_mixer->fifo = &p_aout->p_input->fifo;
    p_mixer->mix = NULL;
    p_mixer->sys = NULL;

    p_mixer->module = module_need( p_mixer, "audio mixer", NULL, false );
    if( !p_mixer->module )
    {
        msg_Err( p_mixer, "no suitable audio mixer" );
        vlc_object_release( p_mixer );
        return VLC_EGENERIC;
    }

    /* */
    p_aout->p_mixer = p_mixer;
    return VLC_SUCCESS;
}

/*****************************************************************************
 * aout_MixerDelete: delete the mixer
 *****************************************************************************
 * Please note that you must hold the mixer lock.
 *****************************************************************************/
void aout_MixerDelete( audio_output_t * p_aout )
{
    vlc_assert_locked( &p_aout->lock );

    if( !p_aout->p_mixer )
        return;

    module_unneed( p_aout->p_mixer, p_aout->p_mixer->module );
    vlc_object_release( p_aout->p_mixer );
    p_aout->p_mixer = NULL;
}

/*****************************************************************************
 * MixBuffer: try to prepare one output buffer
 *****************************************************************************
 * Please note that you must hold the mixer lock.
 *****************************************************************************/
static int MixBuffer( audio_output_t * p_aout, float volume )
{
    aout_mixer_t *p_mixer = p_aout->p_mixer;
    aout_fifo_t *p_fifo = p_mixer->fifo;
    mtime_t now = mdate();
    const unsigned samples = p_aout->i_nb_samples;
    /* FIXME: Remove this silly constraint. Just pass buffers as they come to
     * "smart" audio outputs. */
    assert( samples > 0 );

    vlc_assert_locked( &p_aout->lock );

    /* Retrieve the date of the next buffer. */
    date_t exact_start_date = p_aout->fifo.end_date;
    mtime_t start_date = date_Get( &exact_start_date );

    if( start_date != 0 && start_date < now )
    {
        /* The output is _very_ late. This can only happen if the user
         * pauses the stream (or if the decoder is buggy, which cannot
         * happen :). */
        msg_Warn( p_mixer, "output PTS is out of range (%"PRId64"), clearing out",
                  mdate() - start_date );
        aout_FifoSet( &p_aout->fifo, 0 );
        date_Set( &exact_start_date, 0 );
        start_date = 0;
    }

    /* See if we have enough data to prepare a new buffer for the audio output. */
    aout_buffer_t *p_buffer = p_fifo->p_first;
    if( p_buffer == NULL )
        return -1;

    /* Find the earliest start date available. */
    if ( !start_date )
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
        msg_Warn( p_mixer, "the mixer got a packet in the past (%"PRId64")",
                  start_date - prev_date );
        aout_BufferFree( aout_FifoPop( p_fifo ) );

        p_buffer = p_fifo->p_first;
        if( p_buffer == NULL )
            return -1;
    }

    /* Check that we have enough samples. */
    while( prev_date < end_date )
    {
        p_buffer = p_buffer->p_next;
        if( p_buffer == NULL )
            return -1;

        /* Check that all buffers are contiguous. */
        if( prev_date != p_buffer->i_pts )
        {
            msg_Warn( p_mixer,
                      "buffer hole, dropping packets (%"PRId64")",
                      p_buffer->i_pts - prev_date );

            aout_buffer_t *p_deleted;
            while( (p_deleted = p_fifo->p_first) != p_buffer )
                aout_BufferFree( aout_FifoPop( p_fifo ) );
        }

        prev_date = p_buffer->i_pts + p_buffer->i_length;
    }

    if( !AOUT_FMT_NON_LINEAR( &p_mixer->fmt ) )
    {
        p_buffer = p_fifo->p_first;

        /* Additionally check that p_first_byte_to_mix is well located. */
        const unsigned framesize = p_mixer->fmt.i_bytes_per_frame;
        ssize_t delta = (start_date - p_buffer->i_pts)
                      * p_mixer->fmt.i_rate / CLOCK_FREQ;
        if( delta != 0 )
            msg_Warn( p_mixer, "mixer start is not output end (%zd)", delta );
        if( delta < 0 )
        {
            /* Is it really the best way to do it ? */
            aout_FifoSet( &p_aout->fifo, 0 );
            return -1;
        }
        if( delta > 0 )
        {
            p_buffer->i_nb_samples -= delta;
            p_buffer->i_pts += delta * CLOCK_FREQ / p_mixer->fmt.i_rate;
            p_buffer->i_length -= delta * CLOCK_FREQ / p_mixer->fmt.i_rate;
            delta *= framesize;
            p_buffer->p_buffer += delta;
            p_buffer->i_buffer -= delta;
        }

        /* Build packet with adequate number of samples */
        unsigned needed = samples * framesize;
        p_buffer = block_Alloc( needed );
        if( unlikely(p_buffer == NULL) )
            /* XXX: should free input buffers */
            return -1;
        p_buffer->i_nb_samples = samples;

        for( uint8_t *p_out = p_buffer->p_buffer; needed > 0; )
        {
            aout_buffer_t *p_inbuf = p_fifo->p_first;
            if( unlikely(p_inbuf == NULL) )
            {
                msg_Err( p_mixer, "internal amix error" );
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
                p_fifo->p_first->i_pts += needed * CLOCK_FREQ / p_mixer->fmt.i_rate;
                p_fifo->p_first->i_length -= needed * CLOCK_FREQ / p_mixer->fmt.i_rate;
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

    /* Run the mixer. */
    p_mixer->mix( p_mixer, p_buffer, volume );
    aout_OutputPlay( p_aout, p_buffer );
    return 0;
}

/*****************************************************************************
 * aout_MixerRun: entry point for the mixer & post-filters processing
 *****************************************************************************
 * Please note that you must hold the mixer lock.
 *****************************************************************************/
void aout_MixerRun( audio_output_t * p_aout, float volume )
{
    while( MixBuffer( p_aout, volume ) != -1 );
}
