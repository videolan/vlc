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
    demux_index_entry_t *idx = (demux_index_entry_t *)malloc( sizeof( demux_index_entry_t ) );
    idx->p_next = idx->p_prev = NULL;
    idx->i_pagepos_end = -1;
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






/* Find the last frame for p_stream,
   -1 is returned on failure */

static int64_t find_last_frame (demux_t *p_demux, logical_stream_t *p_stream)
{

    return -1;
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

 
    return -1;
}






/* find upper and lower pagepos for i_tframe; if we find an exact match, we return it */

static demux_index_entry_t *get_bounds_for ( logical_stream_t *p_stream, int64_t i_tframe, 
                                             int64_t *pi_pos_lower, int64_t *pi_pos_upper)
{

    return NULL;
}


/************************************************************************
 * public functions
 *************************************************************************/



/* return highest frame number for p_stream (which must be a theora or dirac video stream) */

int64_t oggseek_get_last_frame ( demux_t *p_demux, logical_stream_t *p_stream )
{

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

    return VLC_EGENERIC;

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


