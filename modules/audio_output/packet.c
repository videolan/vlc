/*****************************************************************************
 * packet.c : helper for legacy audio output plugins
 *****************************************************************************
 * Copyright (C) 2002-2012 VLC authors and VideoLAN
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
# include <config.h>
#endif
#include <limits.h>
#include <assert.h>
#include <vlc_common.h>
#include <vlc_aout.h>

/**
 * Initializes the members of a FIFO.
 */
static void aout_FifoInit( aout_fifo_t *p_fifo, uint32_t i_rate )
{
    assert( i_rate != 0);
    p_fifo->p_first = NULL;
    p_fifo->pp_last = &p_fifo->p_first;
    date_Init( &p_fifo->end_date, i_rate, 1 );
    date_Set( &p_fifo->end_date, VLC_TS_INVALID );
}

/**
 * Pushes a packet into the FIFO.
 */
static void aout_FifoPush( aout_fifo_t * p_fifo, block_t * p_buffer )
{
    *p_fifo->pp_last = p_buffer;
    p_fifo->pp_last = &p_buffer->p_next;
    *p_fifo->pp_last = NULL;
    /* Enforce the continuity of the stream. */
    if( date_Get( &p_fifo->end_date ) != VLC_TS_INVALID )
    {
        p_buffer->i_pts = date_Get( &p_fifo->end_date );
        p_buffer->i_length = date_Increment( &p_fifo->end_date,
                                             p_buffer->i_nb_samples );
        p_buffer->i_length -= p_buffer->i_pts;
    }
    else
    {
        date_Set( &p_fifo->end_date, p_buffer->i_pts + p_buffer->i_length );
    }
}

/**
 * Trashes all buffers.
 */
static void aout_FifoReset( aout_fifo_t * p_fifo )
{
    block_t * p_buffer;

    date_Set( &p_fifo->end_date, VLC_TS_INVALID );
    p_buffer = p_fifo->p_first;
    while ( p_buffer != NULL )
    {
        block_t * p_next = p_buffer->p_next;
        block_Release( p_buffer );
        p_buffer = p_next;
    }
    p_fifo->p_first = NULL;
    p_fifo->pp_last = &p_fifo->p_first;
}

/**
 * Move forwards or backwards all dates in the FIFO.
 */
static void aout_FifoMoveDates( aout_fifo_t *fifo, mtime_t difference )
{
    if( date_Get( &fifo->end_date ) == VLC_TS_INVALID )
    {
        assert( fifo->p_first == NULL );
        return;
    }

    date_Move( &fifo->end_date, difference );
    for( block_t *block = fifo->p_first; block != NULL; block = block->p_next )
        block->i_pts += difference;
}

/**
 * Gets the next buffer out of the FIFO
 */
static block_t *aout_FifoPop( aout_fifo_t * p_fifo )
{
    block_t *p_buffer = p_fifo->p_first;
    if( p_buffer != NULL )
    {
        p_fifo->p_first = p_buffer->p_next;
        if( p_fifo->p_first == NULL )
            p_fifo->pp_last = &p_fifo->p_first;
    }
    return p_buffer;
}

/**
 * Destroys a FIFO and its buffers
 */
static void aout_FifoDestroy( aout_fifo_t * p_fifo )
{
    block_t * p_buffer;

    p_buffer = p_fifo->p_first;
    while ( p_buffer != NULL )
    {
        block_t * p_next = p_buffer->p_next;
        block_Release( p_buffer );
        p_buffer = p_next;
    }

    p_fifo->p_first = NULL;
    p_fifo->pp_last = &p_fifo->p_first;
}

static inline aout_packet_t *aout_packet (audio_output_t *aout)
{
    return (aout_packet_t *)(aout->sys);
}

void aout_PacketInit (audio_output_t *aout, aout_packet_t *p, unsigned samples,
                      const audio_sample_format_t *fmt)
{
    assert (p == aout_packet (aout));

    vlc_mutex_init (&p->lock);
    p->format = *fmt;
    aout_FifoInit (&p->partial, p->format.i_rate);
    aout_FifoInit (&p->fifo, p->format.i_rate);
    p->pause_date = VLC_TS_INVALID;
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

int aout_PacketTimeGet (audio_output_t *aout, mtime_t *restrict delay)
{
    aout_packet_t *p = aout_packet (aout);
    mtime_t time_report;

    /* Problem: This measurement is imprecise and prone to jitter.
     * Solution: Do not use aout_Packet...(). */
    vlc_mutex_lock (&p->lock);
    time_report = date_Get (&p->fifo.end_date);
    vlc_mutex_unlock (&p->lock);

    if (time_report == VLC_TS_INVALID)
        return -1;
    *delay = time_report - mdate ();
    return 0;
}

static block_t *aout_OutputSlice (audio_output_t *);

void aout_PacketPlay (audio_output_t *aout, block_t *block)
{
    aout_packet_t *p = aout_packet (aout);

    vlc_mutex_lock (&p->lock);
    aout_FifoPush (&p->partial, block);
    while ((block = aout_OutputSlice (aout)) != NULL)
        aout_FifoPush (&p->fifo, block);
    vlc_mutex_unlock (&p->lock);
}

void aout_PacketFlush (audio_output_t *aout, bool drain)
{
    aout_packet_t *p = aout_packet (aout);

    vlc_mutex_lock (&p->lock);
    if (drain)
    {
        mtime_t pts = date_Get (&p->fifo.end_date);
        vlc_mutex_unlock (&p->lock);
        if (pts != VLC_TS_INVALID)
            mwait (pts);
    }
    else
    {
        aout_FifoReset (&p->partial);
        aout_FifoReset (&p->fifo);
        vlc_mutex_unlock (&p->lock);
    }
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

    /* Retrieve the date of the next buffer. */
    date_t exact_start_date = p->fifo.end_date;
    mtime_t start_date = date_Get( &exact_start_date );

    /* Check if there is enough data to slice a new buffer. */
    block_t *p_buffer = p_fifo->p_first;
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

    /* Check that we have enough samples (TODO merge with next loop). */
    for( unsigned available = 0; available < samples; )
    {
        p_buffer = p_buffer->p_next;
        if( p_buffer == NULL )
            return NULL;

        available += p_buffer->i_nb_samples;
    }

    if( AOUT_FMT_LINEAR( &p->format ) )
    {
        const unsigned framesize = p->format.i_bytes_per_frame;
        /* Build packet with adequate number of samples */
        unsigned needed = samples * framesize;

        p_buffer = block_Alloc( needed );
        if( unlikely(p_buffer == NULL) )
            /* XXX: should free input buffers */
            return NULL;
        p_buffer->i_nb_samples = samples;

        for( uint8_t *p_out = p_buffer->p_buffer; needed > 0; )
        {
            block_t *p_inbuf = p_fifo->p_first;
            if( unlikely(p_inbuf == NULL) )
            {
                msg_Err( p_aout, "packetization error" );
                memset( p_out, 0, needed );
                break;
            }

            const uint8_t *p_in = p_inbuf->p_buffer;
            size_t avail = p_inbuf->i_nb_samples * framesize;
            if( avail > needed )
            {
                memcpy( p_out, p_in, needed );
                p_fifo->p_first->p_buffer += needed;
                p_fifo->p_first->i_buffer -= needed;
                needed /= framesize;
                p_fifo->p_first->i_nb_samples -= needed;

                mtime_t t = needed * CLOCK_FREQ / p->format.i_rate;
                p_fifo->p_first->i_pts += t;
                p_fifo->p_first->i_length -= t;
                break;
            }

            memcpy( p_out, p_in, avail );
            needed -= avail;
            p_out += avail;
            /* Next buffer */
            block_Release( aout_FifoPop( p_fifo ) );
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
 * decoded audio samples. Typically, audio_output_t.play is set to, or calls
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
    const bool b_can_sleek = !AOUT_FMT_LINEAR(&p->format);
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
    }
    vlc_mutex_unlock( &p->lock );
    return p_buffer;
out:
    vlc_mutex_unlock( &p->lock );
    return NULL;
}
