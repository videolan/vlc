/*****************************************************************************
 * oggseek.c : ogg seeking functions for ogg demuxer vlc
 *****************************************************************************
 * Copyright (C) 2008 - 2010 Gabriel Finch <salsaman@gmail.com>
 *
 * Authors: Gabriel Finch <salsaman@gmail.com>
 * adapted from: http://lives.svn.sourceforge.net/viewvc/lives/trunk/lives-plugins
 * /plugins/decoders/ogg_theora_decoder.c
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
#include <vlc_demux.h>

#include <ogg/ogg.h>

#include "ogg.h"
#include "oggseek.h"


/************************************************************
* index entries
*************************************************************/

/* free all entries in index list */

void oggseek_index_entries_free ( demux_index_entry_t *idx )
{
    demux_index_entry_t *idx_next;

    while ( idx != NULL )
    {
        idx_next = idx->p_next;
        free( idx );
        idx = idx_next;
    }
}


/* unlink and free idx. If idx is head of list, return new head */

static demux_index_entry_t *index_entry_delete( demux_index_entry_t *idx )
{
    demux_index_entry_t *xidx = idx;

    if ( idx->p_prev != NULL ) idx->p_prev->p_next = idx->p_next;
    else xidx = idx->p_next;

    if ( idx->p_next != NULL ) idx->p_next->p_prev = idx->p_prev;
    free( idx );

    return xidx;
}


/* internal function to create a new list member */

static demux_index_entry_t *index_entry_new( void )
{
    demux_index_entry_t *idx = xmalloc( sizeof( demux_index_entry_t ) );
    idx->p_next = idx->p_prev = NULL;
    idx->i_pagepos_end = -1;
    return idx;
}



/* add a theora entry to our list; format is highest granulepos -> page offset of
   keyframe start */

const demux_index_entry_t *oggseek_theora_index_entry_add ( logical_stream_t *p_stream,
                                                            int64_t i_granule,
                                                            int64_t i_pagepos)
{
    /* add or update entry for keyframe */
    demux_index_entry_t *idx;
    demux_index_entry_t *oidx;
    demux_index_entry_t *last_idx = NULL;
    int64_t i_gpos;
    int64_t i_frame;
    int64_t i_kframe;
    int64_t i_tframe;
    int64_t i_tkframe;

    if ( p_stream == NULL ) return NULL;

    oidx = idx = p_stream->idx;

    i_tkframe = i_granule >> p_stream->i_granule_shift;
    i_tframe = i_tkframe + i_granule - ( i_tkframe << p_stream->i_granule_shift );

    if ( i_tkframe < 1 ) return NULL;

    if ( idx == NULL )
    {
        demux_index_entry_t *ie = index_entry_new();
        ie->i_value = i_granule;
        ie->i_pagepos = i_pagepos;
        p_stream->idx = ie;
        return ie;
    }


    while ( idx != NULL )
    {
        i_gpos = idx->i_value;

        i_kframe = i_gpos >> p_stream->i_granule_shift;
        if ( i_kframe > i_tframe ) break;

        if ( i_kframe == i_tkframe )
        {
            /* entry exists, update it if applicable, and return it */
            i_frame = i_kframe + i_gpos - ( i_kframe << p_stream->i_granule_shift );
            if ( i_frame < i_tframe )
            {
                idx->i_value = i_granule;
                idx->i_pagepos = i_pagepos;
            }

            return idx;
        }

        last_idx = idx;
        idx = idx->p_next;
    }


    /* new entry; insert after last_idx */

    idx = index_entry_new();

    if ( last_idx != NULL )
    {
        idx->p_next = last_idx->p_next;
        last_idx->p_next = idx;
        idx->p_prev = last_idx;
    }
    else
    {
        idx->p_next = oidx;
        oidx = idx;
    }

    if ( idx->p_next != NULL )
    {
        idx->p_next->p_prev = idx;
    }

    idx->i_value = i_granule;
    idx->i_pagepos = i_pagepos;

    return idx;
}




/*********************************************************************
 * private functions
 **********************************************************************/

/* seek in ogg file to offset i_pos and update the sync */

static void seek_byte( demux_t *p_demux, int64_t i_pos )
{
    demux_sys_t *p_sys  = p_demux->p_sys;

    if ( ! stream_Seek( p_demux->s, i_pos ) )
    {
        ogg_sync_reset( &p_sys->oy );

        p_sys->i_input_position = i_pos;
        p_sys->b_page_waiting = false;
    }
}



/* read bytes from the ogg file to try to find a page start */

static int64_t get_data( demux_t *p_demux, int64_t i_bytes_to_read )
{
    demux_sys_t *p_sys  = p_demux->p_sys;

    char *buf;
    int64_t i_result;

    if ( p_sys->i_total_length > 0 )
    {
        if ( p_sys->i_input_position + i_bytes_to_read > p_sys->i_total_length )
        {
            i_bytes_to_read = p_sys->i_total_length - p_sys->i_input_position;
            if ( i_bytes_to_read <= 0 ) {
                return 0;
            }
        }
    }

    seek_byte ( p_demux, p_sys->i_input_position );

    buf = ogg_sync_buffer( &p_sys->oy, i_bytes_to_read );

    i_result = stream_Read( p_demux->s, buf, i_bytes_to_read );

    p_sys->b_page_waiting = false;

    ogg_sync_wrote( &p_sys->oy, i_result );
    return i_result;
}





/* Find the first first ogg page for p_stream between offsets i_pos1 and i_pos2,
   return file offset in bytes; -1 is returned on failure */

static int64_t find_first_page( demux_t *p_demux, int64_t i_pos1, int64_t i_pos2,
                                logical_stream_t *p_stream,
                                int64_t *pi_kframe, int64_t *pi_frame )
{
    int64_t i_result;
    int64_t i_granulepos;
    int64_t i_bytes_to_read = i_pos2 - i_pos1 + 1;
    int64_t i_bytes_read;
    int64_t i_pages_checked = 0;
    int64_t i_packets_checked;

    demux_sys_t *p_sys  = p_demux->p_sys;

    ogg_packet op;

    seek_byte( p_demux, i_pos1 );

    if ( i_pos1 == p_stream->i_data_start )
    {
        /* set a dummy granulepos at data_start */
        *pi_kframe = p_stream->i_keyframe_offset;
        *pi_frame = p_stream->i_keyframe_offset;

        p_sys->b_page_waiting = true;
        return p_sys->i_input_position;
    }

    if ( i_bytes_to_read > OGGSEEK_BYTES_TO_READ ) i_bytes_to_read = OGGSEEK_BYTES_TO_READ;

    while ( 1 )
    {

        if ( p_sys->i_input_position >= i_pos2 )
        {
            /* we reached the end and found no pages */
            *pi_frame=-1;
            return -1;
        }

        /* read next chunk */
        if ( ! ( i_bytes_read = get_data( p_demux, i_bytes_to_read ) ) )
        {
            /* EOF */
            *pi_frame = -1;
            return -1;
        }

        i_bytes_to_read = OGGSEEK_BYTES_TO_READ;

        i_result = ogg_sync_pageseek( &p_sys->oy, &p_sys->current_page );

        if ( i_result < 0 )
        {
            /* found a page, sync to page start */
            p_sys->i_input_position -= i_result;
            i_pos1 = p_sys->i_input_position;
            continue;
        }

        if ( i_result > 0 || ( i_result == 0 && p_sys->oy.fill > 3 &&
                               ! strncmp( (char *)p_sys->oy.data, "OggS" , 4 ) ) )
        {
            i_pos1 = p_sys->i_input_position;
            break;
        }

        p_sys->i_input_position += i_bytes_read;

    };

    seek_byte( p_demux, p_sys->i_input_position );
    ogg_stream_reset( &p_stream->os );

    while( 1 )
    {

        if ( p_sys->i_input_position >= i_pos2 )
        {
            /* reached the end of the search region and nothing was found */
            *pi_frame = -1;
            return p_sys->i_input_position;
        }

        p_sys->b_page_waiting = false;

        if ( ! ( i_result = oggseek_read_page( p_demux ) ) )
        {
            /* EOF */
            *pi_frame = -1;
            return p_sys->i_input_position;
        }

        // found a page
        if ( p_stream->os.serialno != ogg_page_serialno( &p_sys->current_page ) )
        {
            /* page is not for this stream */
            p_sys->i_input_position += i_result;
            if ( ! i_pages_checked ) i_pos1 = p_sys->i_input_position;
            continue;
        }


        ogg_stream_pagein( &p_stream->os, &p_sys->current_page );

        i_pages_checked++;
        i_packets_checked = 0;

        if ( ogg_stream_packetout( &p_stream->os, &op ) > 0 )
        {
            i_packets_checked++;
        }

        if ( i_packets_checked )
        {
            i_granulepos = ogg_page_granulepos( &p_sys->current_page );

            oggseek_theora_index_entry_add( p_stream, i_granulepos, i_pos1 );

            *pi_kframe =
                i_granulepos >> p_stream->i_granule_shift;

            *pi_frame = *pi_kframe +
                i_granulepos - ( *pi_kframe << p_stream->i_granule_shift );

            p_sys->b_page_waiting = true;
            return i_pos1;

        }

        /*  -> start of next page */
        p_sys->i_input_position += i_result;
    }
}








/* Find the last frame for p_stream,
   -1 is returned on failure */

static int64_t find_last_frame (demux_t *p_demux, logical_stream_t *p_stream)
{

    int64_t i_page_pos;
    int64_t i_start_pos;
    int64_t i_frame = -1;
    int64_t i_last_frame = -1;
    int64_t i_kframe = 0;
    int64_t i_pos1;
    int64_t i_pos2;
    int64_t i_serialno;

    demux_sys_t *p_sys  = p_demux->p_sys;

    i_pos1 = p_stream->i_data_start;
    i_pos2 = p_sys->i_total_length;
    i_serialno = p_stream->os.serialno;

    i_start_pos = i_pos2 - OGGSEEK_BYTES_TO_READ;


    while( 1 )
    {
        if ( i_start_pos < i_pos1 ) i_start_pos = i_pos1;

        i_page_pos = find_first_page( p_demux, i_start_pos, i_pos2, p_stream, &i_kframe, &i_frame );

        if ( i_frame == -1 )
        {
            /* no pages found in range */
            if ( i_last_frame >= 0 )
            {
                /* No more pages in range -> return last one */
                return i_last_frame;
            }
            if ( i_start_pos <= i_pos1 )
            {
                return -1;
            }

            /* Go back a bit */
            i_pos2 -= i_start_pos;
            i_start_pos -= OGGSEEK_BYTES_TO_READ;
            if ( i_start_pos < i_pos1 ) i_start_pos = i_pos1;
            i_pos2 += i_start_pos;
        }
        else
        {
            /* found a page, see if we can find another one */
            i_last_frame = i_frame;
            i_start_pos = i_page_pos + 1;
        }
    }
    return -1;
}






/* convert a theora frame to a granulepos */

static inline int64_t frame_to_gpos( logical_stream_t *p_stream, int64_t i_kframe,
                                     int64_t i_frame )
{
    if ( p_stream->fmt.i_codec == VLC_CODEC_THEORA )
    {
        return ( i_kframe << p_stream->i_granule_shift ) + ( i_frame - i_kframe );
    }

    return i_kframe;
}





/* seek to a suitable point to begin decoding for i_tframe. We can pre-set bounding positions
   i_pos_lower and i_pos_higher to narrow the search domain. */


static int64_t ogg_seek( demux_t *p_demux, logical_stream_t *p_stream, int64_t i_tframe,
                         int64_t i_pos_lower, int64_t i_pos_upper, int64_t *pi_pagepos,
                         bool b_exact )
{
    /* For theora:
     * We do two passes here, first with b_exact set, then with b_exact unset.
     *
     * If b_exact is set, we find the highest granulepos <= the target granulepos
     * from this we extract an estimate of the keyframe (note that there could be other
     * "hidden" keyframes between the found granulepos and the target).
     *
     * On the second pass we find the highest granulepos < target. This places us just before or
     * at the start of the target keyframe.
     *
     * When we come to decode, we start from this second position, discarding any completed
     * packets on that page, and read pages discarding packets until we get to the target frame.
     *
     * The function returns the granulepos which is found,
     * sets the page offset in pi_pagepos. -1 is returned on error.
     *
     * for dirac:
     *
     * we find the highest sync frame <= target frame, and return the sync_frame number
     * b_exact should be set to true
     *
     *
     * the method used is bi-sections:
     *  - we check the lower keyframe
     * if this is == target we return
     * if > target, or we find no keyframes, we go to the lower segment
     * if < target we divide the segment in two and check the upper half
     *
     * This is then repeated until the segment size is too small to hold a packet,
     * at which point we return our best match
     *
     * Two optimisations are made: - anything we discover about keyframes is added to our index
     * - before calling this function we get approximate bounds from the index
     *
     * therefore, subsequent searches become more rapid.
     *
     */

    int64_t i_start_pos;
    int64_t i_end_pos;
    int64_t i_pagepos;
    int64_t i_segsize;
    int64_t i_frame;
    int64_t i_kframe;

    int64_t i_best_kframe = -1;
    int64_t i_best_frame = -1;
    int64_t i_best_pagepos = -1;

    demux_sys_t *p_sys  = p_demux->p_sys;

    if ( i_tframe < p_stream->i_keyframe_offset )
    {
        *pi_pagepos = p_stream->i_data_start;

        if ( ! b_exact ) {
            seek_byte( p_demux, p_stream->i_data_start );
            return frame_to_gpos( p_stream, p_stream->i_keyframe_offset, 1 );
        }
        return frame_to_gpos( p_stream, p_stream->i_keyframe_offset, 0 );
    }

    if ( i_pos_lower < p_stream->i_data_start )
    {
        i_pos_lower = p_stream->i_data_start;
    }

    if ( i_pos_upper < 0 )
    {
        i_pos_upper = p_sys->i_total_length;
    }

    if ( i_pos_upper > p_sys->i_total_length )
    {
        i_pos_upper = p_sys->i_total_length;
    }

    i_start_pos = i_pos_lower;
    i_end_pos = i_pos_upper;

    i_segsize = ( i_end_pos - i_start_pos + 1 ) >> 1;

    do
    {
        /* see if the frame lies in current segment */
        if ( i_start_pos < i_pos_lower )
        {
            i_start_pos = i_pos_lower;
        }
        if ( i_end_pos > i_pos_upper )
        {
            i_end_pos = i_pos_upper;
        }

        if ( i_start_pos >= i_end_pos )
        {
            if ( i_start_pos == i_pos_lower)
            {
                if ( ! b_exact ) seek_byte( p_demux, i_start_pos );
                *pi_pagepos = i_start_pos;
                return frame_to_gpos( p_stream, p_stream->i_keyframe_offset, 1 );
            }
            break;
        }

        if ( p_stream->fmt.i_codec == VLC_CODEC_THEORA )
        {
            i_pagepos = find_first_page( p_demux, i_start_pos, i_end_pos, p_stream,
                                         &i_kframe, &i_frame );
        }
        else return -1;

        if ( i_pagepos != -1 && i_kframe != -1 )
        {
            /* found a page */

            if ( b_exact && i_frame >= i_tframe && i_kframe <= i_tframe )
            {
                /* got it ! */
                *pi_pagepos = i_start_pos;
                return frame_to_gpos( p_stream, i_kframe, i_frame );
            }

            if ( ( i_kframe < i_tframe || ( b_exact && i_kframe == i_tframe ) )
                 && i_kframe > i_best_kframe )
            {
                i_best_kframe = i_kframe;
                i_best_frame = i_frame;
                i_best_pagepos = i_pagepos;
            }

            if ( i_frame >= i_tframe )
            {
                /* check lower half of segment */
                i_start_pos -= i_segsize;
                i_end_pos -= i_segsize;
            }

            else i_start_pos = i_pagepos;

        }
        else
        {
            /* no keyframe found, check lower segment */
            i_end_pos -= i_segsize;
            i_start_pos -= i_segsize;
        }

        i_segsize = ( i_end_pos - i_start_pos + 1 ) >> 1;
        i_start_pos += i_segsize;

    } while ( i_segsize > 64 );

    if ( i_best_kframe >- 1 )
    {
        if ( !b_exact )
        {
            seek_byte( p_demux, i_best_pagepos );
        }
        *pi_pagepos = i_best_pagepos;
        return frame_to_gpos( p_stream, i_best_kframe, i_best_frame );
    }

    return -1;
}






/* find upper and lower pagepos for i_tframe; if we find an exact match, we return it */

static demux_index_entry_t *get_bounds_for ( logical_stream_t *p_stream, int64_t i_tframe,
                                             int64_t *pi_pos_lower, int64_t *pi_pos_upper)
{
    int64_t i_kframe;
    int64_t i_frame;
    int64_t i_gpos;

    demux_index_entry_t *idx = p_stream->idx;

    *pi_pos_lower = *pi_pos_upper = -1;

    while ( idx != NULL )
    {

        if ( idx-> i_pagepos < 0 )
        {
            /* kframe was found to be invalid */
            idx = idx->p_next;
            continue;
        }

        if ( p_stream->fmt.i_codec == VLC_CODEC_THEORA )
        {
            i_gpos = idx->i_value;
            i_kframe = i_gpos >> p_stream->i_granule_shift;
            i_frame = i_kframe + i_gpos - ( i_kframe << p_stream->i_granule_shift );
        }
        else return NULL;


        if ( i_kframe > i_tframe )
        {
            *pi_pos_upper = idx->i_pagepos;
            return NULL;
        }

        if ( i_frame < i_tframe )
        {
            *pi_pos_lower = idx->i_pagepos;
            idx = idx->p_next;
            continue;
        }

        return idx;
    }

    return NULL;
}


/* get highest frame in theora stream */

static int64_t find_last_theora_frame ( demux_t *p_demux, logical_stream_t *p_stream )
{
    int64_t i_frame;

    i_frame = find_last_frame ( p_demux, p_stream );

    /* We need to reset back to the start here, otherwise packets cannot be decoded.
     * I think this is due to the fact that we seek to the end and then we must reset
     * all logical streams, which causes remaining headers not to be read correctly.
     * Seeking to 0 is the only value which seems to work, and it appears to have no
     * adverse effects. */

    seek_byte( p_demux, 0 );
    /* Reset stream states */
    p_stream->i_serial_no = ogg_page_serialno( &p_demux->p_sys->current_page );
    ogg_stream_init( &p_stream->os, p_stream->i_serial_no );
    ogg_stream_pagein( &p_stream->os, &p_demux->p_sys->current_page );

    return i_frame;
}



/************************************************************************
 * public functions
 *************************************************************************/




/* return highest frame number for p_stream (which must be a theora or dirac video stream) */

int64_t oggseek_get_last_frame ( demux_t *p_demux, logical_stream_t *p_stream )
{
    int64_t i_frame = -1;

    if ( p_stream->fmt.i_codec == VLC_CODEC_THEORA )
    {
        i_frame = find_last_theora_frame ( p_demux, p_stream );

        if ( i_frame < 0 ) return -1;
        return i_frame;
    }

    /* unhandled video format */
    return -1;
}






/* seek to target frame in p_stream; actually we will probably end up just before it
 *   (so we set skip)
 *
 * range for i_tframe is 0 -> p_sys->i_total_frames - 1
 */

int oggseek_find_frame ( demux_t *p_demux, logical_stream_t *p_stream, int64_t i_tframe )
{

    const demux_index_entry_t *fidx;

    /* lower and upper bounds for search domain */
    int64_t i_pos_lower;
    int64_t i_pos_upper;

    int64_t i_granulepos;
    int64_t i_pagepos;

    /* keyframe for i_tframe ( <= i_tframe ) */
    int64_t i_kframe;

    /* keyframe for i_kframe ( <= i_kframe ) */
    int64_t i_xkframe;

    /* next frame to be decoded ( >= i_xkframe ) */
    int64_t i_cframe;

    demux_sys_t *p_sys  = p_demux->p_sys;

    i_tframe += p_stream->i_keyframe_offset;

    /* reduce the search domain */
    fidx = get_bounds_for( p_stream, i_tframe, &i_pos_lower, &i_pos_upper );

    if ( fidx == NULL )
    {
        /* no exact match found; search the domain for highest keyframe <= i_tframe */

        i_granulepos = ogg_seek ( p_demux, p_stream, i_tframe, i_pos_lower, i_pos_upper,
                                  &i_pagepos, true );
        if ( i_granulepos == -1 )
        {
            return VLC_EGENERIC;
        }

    }
    else {
        i_granulepos = fidx->i_value;
    }

    if ( p_stream->fmt.i_codec == VLC_CODEC_THEORA )
    {
        i_kframe = i_granulepos >> p_stream->i_granule_shift;
        if ( i_kframe < p_stream->i_keyframe_offset )
        {
            i_kframe = p_stream->i_keyframe_offset;
        }

        /* we found a keyframe, but we don't know where its packet starts, so search for a
           frame just before it */

        /* reduce search domain */
        get_bounds_for( p_stream, i_kframe-1, &i_pos_lower, &i_pos_upper );

        i_granulepos = ogg_seek( p_demux, p_stream, i_kframe-1, i_pos_lower, i_pos_upper,
                                 &i_pagepos, false );

        /* i_cframe will be the next frame we decode */
        i_xkframe = i_granulepos >> p_stream->i_granule_shift;
        i_cframe = i_xkframe + i_granulepos - ( i_xkframe << p_stream->i_granule_shift) + 1;

        if ( p_sys->i_input_position == p_stream->i_data_start )
        {
            i_cframe = i_kframe = p_stream->i_keyframe_offset;
        }
        else
        {
            oggseek_theora_index_entry_add( p_stream, i_granulepos, p_sys->i_input_position );
        }

    }
    else return VLC_EGENERIC;

    p_stream->i_skip_frames = i_tframe - i_cframe;

    ogg_stream_reset( &p_stream->os );

    return VLC_SUCCESS;
}






/****************************************************************************
 * oggseek_read_page: Read a full Ogg page from the physical bitstream.
 ****************************************************************************
 * Returns number of bytes read. This should always be > 0
 * unless we are at the end of stream.
 *
 ****************************************************************************/
int64_t oggseek_read_page( demux_t *p_demux )
{
    demux_sys_t *p_ogg = p_demux->p_sys  ;
    uint8_t header[PAGE_HEADER_BYTES+255];
    int i_nsegs;
    int i_in_pos;
    int i;
    int64_t i_result;
    int i_page_size;
    char *buf;

    demux_sys_t *p_sys  = p_demux->p_sys;

    /* store position of this page */
    i_in_pos = p_ogg->i_input_position = stream_Tell( p_demux->s );

    if ( p_sys->b_page_waiting) {
        msg_Warn( p_demux, "Ogg page already loaded" );
        return 0;
    }

    if ( stream_Read ( p_demux->s, header, PAGE_HEADER_BYTES ) < PAGE_HEADER_BYTES )
    {
        stream_Seek( p_demux->s, i_in_pos );
        msg_Dbg ( p_demux, "Reached clean EOF in ogg file" );
        return 0;
    }

    i_nsegs = header[ PAGE_HEADER_BYTES - 1 ];

    if ( stream_Read ( p_demux->s, header+PAGE_HEADER_BYTES, i_nsegs ) < i_nsegs )
    {
        stream_Seek( p_demux->s, i_in_pos );
        msg_Warn ( p_demux, "Reached broken EOF in ogg file" );
        return 0;
    }

    i_page_size = PAGE_HEADER_BYTES + i_nsegs;

    for ( i = 0; i < i_nsegs; i++ )
    {
        i_page_size += header[ PAGE_HEADER_BYTES + i ];
    }

    ogg_sync_reset( &p_ogg->oy );

    buf = ogg_sync_buffer( &p_ogg->oy, i_page_size );

    memcpy( buf, header, PAGE_HEADER_BYTES + i_nsegs );

    i_result = stream_Read ( p_demux->s, (uint8_t*)buf + PAGE_HEADER_BYTES + i_nsegs,
                             i_page_size - PAGE_HEADER_BYTES - i_nsegs );

    ogg_sync_wrote( &p_ogg->oy, i_result + PAGE_HEADER_BYTES + i_nsegs );




    if ( ogg_sync_pageout( &p_ogg->oy, &p_ogg->current_page ) != 1 )
    {
        msg_Err( p_demux , "Got invalid packet, read %"PRId64" of %i: %s",i_result,i_page_size,
                 buf );
        return 0;
    }

    p_sys->b_page_waiting = false;

    return i_result + PAGE_HEADER_BYTES + i_nsegs;
}


