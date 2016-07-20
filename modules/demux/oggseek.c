/*****************************************************************************
 * oggseek.c : ogg seeking functions for ogg demuxer vlc
 *****************************************************************************
 * Copyright (C) 2008 - 2010 Gabriel Finch <salsaman@gmail.com>
 *
 * Authors: Gabriel Finch <salsaman@gmail.com>
 * adapted from: http://lives.svn.sourceforge.net/viewvc/lives/trunk/lives-plugins
 * /plugins/decoders/ogg_theora_decoder.c
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_demux.h>

#include <ogg/ogg.h>
#include <limits.h>

#include <assert.h>

#include "ogg.h"
#include "oggseek.h"

/* Theora spec 7.1 */
#define THEORA_FTYPE_NOTDATA       0x80
#define THEORA_FTYPE_INTERFRAME    0x40

#define SEGMENT_NOT_FOUND -1

#define MAX_PAGE_SIZE 65307
typedef struct packetStartCoordinates
{
    int64_t i_pos;
    int64_t i_pageno;
    int64_t i_skip;
} packetStartCoordinates;

//#define OGG_SEEK_DEBUG 1
#ifdef OGG_SEEK_DEBUG
  #define OggDebug(code) code
  #define OggNoDebug(code)
#else
  #define OggDebug(code)
  #define OggNoDebug(code) code
#endif
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


/* internal function to create a new list member */

static demux_index_entry_t *index_entry_new( void )
{
    demux_index_entry_t *idx = xmalloc( sizeof( demux_index_entry_t ) );
    if ( !idx ) return NULL;
    idx->p_next = idx->p_prev = NULL;
    idx->i_pagepos_end = -1;
    return idx;
}

/* We insert into index, sorting by pagepos (as a page can match multiple
   time stamps) */
const demux_index_entry_t *OggSeek_IndexAdd ( logical_stream_t *p_stream,
                                             int64_t i_timestamp,
                                             int64_t i_pagepos )
{
    demux_index_entry_t *idx;
    demux_index_entry_t *last_idx = NULL;

    if ( p_stream == NULL ) return NULL;

    idx = p_stream->idx;

    if ( i_timestamp < 1 || i_pagepos < 1 ) return NULL;

    if ( idx == NULL )
    {
        demux_index_entry_t *ie = index_entry_new();
        if ( !ie ) return NULL;
        ie->i_value = i_timestamp;
        ie->i_pagepos = i_pagepos;
        p_stream->idx = ie;
        return ie;
    }

    while ( idx != NULL )
    {
        if ( idx->i_pagepos > i_pagepos ) break;
        last_idx = idx;
        idx = idx->p_next;
    }

    /* new entry; insert after last_idx */
    idx = index_entry_new();
    if ( !idx ) return NULL;
    if ( last_idx != NULL )
    {
        idx->p_next = last_idx->p_next;
        last_idx->p_next = idx;
        idx->p_prev = last_idx;
    }
    else
    {
        idx->p_next = p_stream->idx;
        p_stream->idx = idx;
    }

    if ( idx->p_next != NULL )
    {
        idx->p_next->p_prev = idx;
    }

    idx->i_value = i_timestamp;
    idx->i_pagepos = i_pagepos;

    return idx;
}

static bool OggSeekIndexFind ( logical_stream_t *p_stream, int64_t i_timestamp,
                               int64_t *pi_pos_lower, int64_t *pi_pos_upper )
{
    demux_index_entry_t *idx = p_stream->idx;

    while ( idx != NULL )
    {
        if ( idx->i_value <= i_timestamp )
        {
            if ( !idx->p_next ) /* found on last index */
            {
                *pi_pos_lower = idx->i_pagepos;
                return true;
            }
            if ( idx->p_next->i_value > i_timestamp )
            {
                *pi_pos_lower = idx->i_pagepos;
                *pi_pos_upper = idx->p_next->i_pagepos;
                return true;
            }
        }
        idx = idx->p_next;
    }

    return false;
}

/*********************************************************************
 * private functions
 **********************************************************************/

/* seek in ogg file to offset i_pos and update the sync */

static void seek_byte( demux_t *p_demux, int64_t i_pos )
{
    demux_sys_t *p_sys  = p_demux->p_sys;

    if ( ! vlc_stream_Seek( p_demux->s, i_pos ) )
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

    i_bytes_to_read = __MIN( i_bytes_to_read, INT_MAX );

    seek_byte ( p_demux, p_sys->i_input_position );

    buf = ogg_sync_buffer( &p_sys->oy, i_bytes_to_read );

    i_result = vlc_stream_Read( p_demux->s, buf, i_bytes_to_read );

    ogg_sync_wrote( &p_sys->oy, i_result );
    return i_result;
}


void Oggseek_ProbeEnd( demux_t *p_demux )
{
    /* Temporary state */
    ogg_stream_state os;
    ogg_sync_state oy;
    ogg_page page;
    demux_sys_t *p_sys = p_demux->p_sys;
    int64_t i_pos, i_startpos, i_result, i_granule, i_lowerbound;
    int64_t i_length = 0;
    int64_t i_backup_pos = vlc_stream_Tell( p_demux->s );
    int64_t i_upperbound = stream_Size( p_demux->s );
    unsigned int i_backoffset = OGGSEEK_BYTES_TO_READ;
    assert( OGGSEEK_BYTES_TO_READ < UINT_MAX );

    const char *buffer;

    ogg_stream_init( &os, -1 );
    ogg_sync_init( &oy );

    /* Try to lookup last granule from each logical stream */
    i_lowerbound = stream_Size( p_demux->s ) - p_sys->i_streams * MAX_PAGE_SIZE * 2;
    i_lowerbound = __MAX( 0, i_lowerbound );

    i_pos = i_startpos = __MAX( i_lowerbound, i_upperbound - i_backoffset );

    if ( vlc_stream_Seek( p_demux->s, i_pos ) )
    {
        ogg_sync_clear( &oy );
        ogg_stream_clear( &os );
        return;
    }

    while( i_pos >= i_lowerbound )
    {

        while( i_pos < i_upperbound )
        {
            if ( oy.unsynced )
                i_result = ogg_sync_pageseek( &oy, &page );

            buffer = ogg_sync_buffer( &oy, OGGSEEK_BYTES_TO_READ );
            if ( buffer == NULL ) goto clean;
            i_result = vlc_stream_Read( p_demux->s, (void*) buffer, OGGSEEK_BYTES_TO_READ );
            if ( i_result < 1 ) goto clean;
            i_pos += i_result;
            ogg_sync_wrote( &oy, i_result );

            while( ogg_sync_pageout( &oy, &page ) == 1 )
            {
                i_granule = ogg_page_granulepos( &page );
                if ( i_granule == -1 ) continue;

                for ( int i=0; i< p_sys->i_streams; i++ )
                {
                    if ( p_sys->pp_stream[i]->i_serial_no != ogg_page_serialno( &page ) )
                        continue;

                    i_length = Oggseek_GranuleToAbsTimestamp( p_sys->pp_stream[i], i_granule, false );
                    p_sys->i_length = __MAX( p_sys->i_length, i_length / 1000000 );
                    break;
                }
            }
            if ( i_length > 0 ) break;
        }

        /* We found at least a page with valid granule */
        if ( i_length > 0 ) break;

        /* Otherwise increase read size, starting earlier */
        if ( i_backoffset <= ( UINT_MAX >> 1 ) )
        {
            i_backoffset <<= 1;
            i_startpos = i_upperbound - i_backoffset;
        }
        else
        {
            i_startpos -= i_backoffset;
        }
        i_pos = i_startpos;

        if ( vlc_stream_Seek( p_demux->s, i_pos ) )
            break;
    }

clean:
    vlc_stream_Seek( p_demux->s, i_backup_pos );

    ogg_sync_clear( &oy );
    ogg_stream_clear( &os );
}


static int64_t find_first_page_granule( demux_t *p_demux,
                                int64_t i_pos1, int64_t i_pos2,
                                logical_stream_t *p_stream,
                                int64_t *i_granulepos )
{
    int64_t i_result;
    *i_granulepos = -1;
    int64_t i_bytes_to_read = i_pos2 - i_pos1 + 1;
    int64_t i_bytes_read;
    int64_t i_packets_checked;

    demux_sys_t *p_sys  = p_demux->p_sys;

    ogg_packet op;

    seek_byte( p_demux, i_pos1 );

    if ( i_pos1 == p_stream->i_data_start )
        return p_sys->i_input_position;

    if ( i_bytes_to_read > OGGSEEK_BYTES_TO_READ ) i_bytes_to_read = OGGSEEK_BYTES_TO_READ;

    while ( 1 )
    {

        if ( p_sys->i_input_position >= i_pos2 )
        {
            /* we reached the end and found no pages */
            return -1;
        }

        /* read next chunk */
        if ( ! ( i_bytes_read = get_data( p_demux, i_bytes_to_read ) ) )
        {
            /* EOF */
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
            return p_sys->i_input_position;
        }

        p_sys->b_page_waiting = false;

        if ( ! ( i_result = oggseek_read_page( p_demux ) ) )
        {
            /* EOF */
            return p_sys->i_input_position;
        }

        // found a page
        if ( ogg_stream_pagein( &p_stream->os, &p_sys->current_page ) != 0 )
        {
            /* page is not for this stream or incomplete */
            p_sys->i_input_position += i_result;
            continue;
        }

        if ( ogg_page_granulepos( &p_sys->current_page ) <= 0 )
        {
            /* A negative granulepos means that the packet continues on the
             * next page => read the next page */
            p_sys->i_input_position += i_result;
            continue;
        }

        i_packets_checked = 0;

        while ( ogg_stream_packetout( &p_stream->os, &op ) > 0 )
        {
            i_packets_checked++;
        }

        if ( i_packets_checked )
        {
            *i_granulepos = ogg_page_granulepos( &p_sys->current_page );
            return i_pos1;
        }

        /*  -> start of next page */
        p_sys->i_input_position += i_result;
        i_pos1 = p_sys->i_input_position;
    }
}

/* Checks if current packet matches codec keyframe */
bool Ogg_IsKeyFrame( logical_stream_t *p_stream, ogg_packet *p_packet )
{
    if ( p_stream->b_oggds )
    {
        return ( p_packet->bytes > 0 && p_packet->packet[0] & PACKET_IS_SYNCPOINT );
    }
    else switch ( p_stream->fmt.i_codec )
    {
    case VLC_CODEC_THEORA:
    case VLC_CODEC_DAALA: /* Same convention used in daala */
        if ( p_packet->bytes <= 0 || p_packet->packet[0] & THEORA_FTYPE_NOTDATA )
            return false;
        else
            return !( p_packet->packet[0] & THEORA_FTYPE_INTERFRAME );
    case VLC_CODEC_VP8:
        return ( ( ( p_packet->granulepos >> 3 ) & 0x07FFFFFF ) == 0 );
    case VLC_CODEC_DIRAC:
        return ( p_packet->granulepos & 0xFF8000FF );
    default:
        return true;
    }
}

int64_t Ogg_GetKeyframeGranule( logical_stream_t *p_stream, int64_t i_granule )
{
    if ( p_stream->b_oggds )
    {
           return -1; /* We have no way to know */
    }
    else if( p_stream->fmt.i_codec == VLC_CODEC_THEORA ||
             p_stream->fmt.i_codec == VLC_CODEC_DAALA )
    {
        return ( i_granule >> p_stream->i_granule_shift ) << p_stream->i_granule_shift;
    }
    else if( p_stream->fmt.i_codec == VLC_CODEC_DIRAC )
    {
        return ( i_granule >> 31 ) << 31;
    }
    /* No change, that's keyframe or it can't be shifted out (oggds) */
    return i_granule;
}

static bool OggSeekToPacket( demux_t *p_demux, logical_stream_t *p_stream,
            int64_t i_granulepos, packetStartCoordinates *p_lastpacketcoords,
            bool b_exact )
{
    ogg_packet op;
    demux_sys_t *p_sys  = p_demux->p_sys;
    if ( ogg_stream_pagein( &p_stream->os, &p_sys->current_page ) != 0 )
        return false;
    p_sys->b_page_waiting = true;
    int i=0;

    int64_t itarget_frame = Ogg_GetKeyframeGranule( p_stream, i_granulepos );
    int64_t iframe = Ogg_GetKeyframeGranule( p_stream, ogg_page_granulepos( &p_sys->current_page ) );

    if ( ! ogg_page_continued( &p_sys->current_page ) )
    {
        /* Start of frame, not continued page, but no packet. */
        p_lastpacketcoords->i_pageno = ogg_page_pageno( &p_sys->current_page );
        p_lastpacketcoords->i_pos = p_sys->i_input_position;
        p_lastpacketcoords->i_skip = 0;
    }

    if ( b_exact && iframe > itarget_frame )
    {
        while( ogg_stream_packetout( &p_stream->os, &op ) > 0 ) {};
        p_sys->b_page_waiting = false;
        return false;
    }

    while( ogg_stream_packetpeek( &p_stream->os, &op ) > 0 )
    {
        if ( ( !b_exact || itarget_frame == iframe ) && Ogg_IsKeyFrame( p_stream, &op ) )
        {
            OggDebug(
                msg_Dbg(p_demux, "** KEYFRAME **" );
                msg_Dbg(p_demux, "** KEYFRAME PACKET START pageno %"PRId64" OFFSET %"PRId64" skip %"PRId64" **", p_lastpacketcoords->i_pageno, p_lastpacketcoords->i_pos, p_lastpacketcoords->i_skip );
                msg_Dbg(p_demux, "KEYFRAME PACKET IS at pageno %"PRId64" OFFSET %"PRId64" with skip %d packet (%d / %d) ",
                    ogg_page_pageno( &p_sys->current_page ), p_sys->i_input_position, i, i+1, ogg_page_packets( &p_sys->current_page ) );
                DemuxDebug( p_sys->b_seeked = true; )
            );

            if ( i != 0 ) /* Not continued packet */
            {
                /* We need to handle the case when the packet spans onto N
                       previous page(s). packetout() will be valid only when
                       all segments are assembled.
                       Keyframe flag is only available after assembling last part
                       (when packetout() becomes valid). We have no way to guess
                       keyframe at earlier time.
                    */
                p_lastpacketcoords->i_pageno = ogg_page_pageno( &p_sys->current_page );
                p_lastpacketcoords->i_pos = p_sys->i_input_position;
                p_lastpacketcoords->i_skip = i;
            }
            return true;
        }

        p_lastpacketcoords->i_pageno = ogg_page_pageno( &p_sys->current_page );
        p_lastpacketcoords->i_pos = p_sys->i_input_position;
        p_lastpacketcoords->i_skip = i + 1;
        i++;
        /* remove that packet and go sync to next */
        ogg_stream_packetout( &p_stream->os, &op );
    }

    return false;
}

static int64_t OggForwardSeekToFrame( demux_t *p_demux, int64_t i_pos1, int64_t i_pos2,
                logical_stream_t *p_stream, int64_t i_granulepos, bool b_fastseek )
{
    int64_t i_result;
    int64_t i_bytes_to_read;
    int64_t i_bytes_read;

    demux_sys_t *p_sys  = p_demux->p_sys;

    i_bytes_to_read = i_pos2 - i_pos1 + 1;
    seek_byte( p_demux, i_pos1 );
    if ( i_bytes_to_read > OGGSEEK_BYTES_TO_READ ) i_bytes_to_read = OGGSEEK_BYTES_TO_READ;

    OggDebug(
        msg_Dbg( p_demux, "Probing Fwd %"PRId64" %"PRId64" for granule %"PRId64,
        i_pos1, i_pos2, i_granulepos );
    );

    while ( 1 )
    {

        if ( p_sys->i_input_position >= i_pos2 )
            return SEGMENT_NOT_FOUND;

        /* read next chunk */
        if ( ! ( i_bytes_read = get_data( p_demux, i_bytes_to_read ) ) )
            return SEGMENT_NOT_FOUND;

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

    ogg_packet op;
    while( ogg_stream_packetout( &p_stream->os, &op ) > 0 ) {};

    packetStartCoordinates lastpacket = { -1, -1, -1 };

    while( 1 )
    {

        if ( p_sys->i_input_position >= i_pos2 )
        {
            /* reached the end of the search region and nothing was found */
            break;
        }

        p_sys->b_page_waiting = false;

        if ( ! ( i_result = oggseek_read_page( p_demux ) ) )
        {
            /* EOF */
            break;
        }

        // found a page
        if ( p_stream->os.serialno != ogg_page_serialno( &p_sys->current_page ) )
        {
            /* page is not for this stream */
            p_sys->i_input_position += i_result;
            continue;
        }

        if ( OggSeekToPacket( p_demux, p_stream, i_granulepos, &lastpacket, b_fastseek ) )
        {
            p_sys->i_input_position = lastpacket.i_pos;
            p_stream->i_skip_frames = 0;
            return p_sys->i_input_position;
        }

        /*  -> start of next page */
        p_sys->i_input_position += i_result;
    }

    return SEGMENT_NOT_FOUND;
}

static int64_t OggBackwardSeekToFrame( demux_t *p_demux, int64_t i_pos1, int64_t i_pos2,
                               logical_stream_t *p_stream, int64_t i_granulepos )
{
    int64_t i_result;
    int64_t i_offset = __MAX( 1 + ( (i_pos2 - i_pos1) >> 1 ), OGGSEEK_BYTES_TO_READ );

restart:

    OggDebug(
        msg_Dbg( p_demux, "Probing Back %"PRId64" %"PRId64" for granule %"PRId64,
        i_pos1, i_pos2, i_granulepos );
    );

    i_result = OggForwardSeekToFrame( p_demux, i_pos1, i_pos2, p_stream,
                                      i_granulepos, true );

    if ( i_result == SEGMENT_NOT_FOUND && i_pos1 > p_stream->i_data_start )
    {
        i_pos1 = __MAX( p_stream->i_data_start, i_pos1 - i_offset );
        goto restart;
    }

    return i_result;
}

/* Dont use b_presentation with frames granules ! */
int64_t Oggseek_GranuleToAbsTimestamp( logical_stream_t *p_stream,
                                       int64_t i_granule, bool b_presentation )
{
    int64_t i_timestamp = -1;
    if ( i_granule < 1 )
        return -1;

    if ( p_stream->b_oggds )
    {
        if ( b_presentation ) i_granule--;
        i_timestamp = i_granule * CLOCK_FREQ / p_stream->f_rate;
    }
    else  switch( p_stream->fmt.i_codec )
    {
    case VLC_CODEC_THEORA:
    case VLC_CODEC_DAALA:
    case VLC_CODEC_KATE:
    {
        ogg_int64_t iframe = i_granule >> p_stream->i_granule_shift;
        ogg_int64_t pframe = i_granule - ( iframe << p_stream->i_granule_shift );
        /* See Theora A.2.3 */
        if ( b_presentation ) pframe -= p_stream->i_keyframe_offset;
        i_timestamp = ( iframe + pframe ) * CLOCK_FREQ / p_stream->f_rate;
        break;
    }
    case VLC_CODEC_VP8:
    {
        ogg_int64_t frame = i_granule >> p_stream->i_granule_shift;
        if ( b_presentation ) frame--;
        i_timestamp = frame * CLOCK_FREQ / p_stream->f_rate;
        break;
    }
    case VLC_CODEC_DIRAC:
    {
        ogg_int64_t i_dts = i_granule >> 31;
        ogg_int64_t delay = (i_granule >> 9) & 0x1fff;
        /* NB, OggDirac granulepos values are in units of 2*picturerate */
        double f_rate = p_stream->f_rate;
        if ( !p_stream->special.dirac.b_interlaced ) f_rate *= 2;
        if ( b_presentation ) i_dts += delay;
        i_timestamp = i_dts * CLOCK_FREQ / f_rate;
        break;
    }
    case VLC_CODEC_OPUS:
    {
        if ( b_presentation ) return VLC_TS_INVALID;
        i_timestamp = ( i_granule - p_stream->i_pre_skip ) * CLOCK_FREQ / 48000;
        break;
    }
    case VLC_CODEC_VORBIS:
    case VLC_CODEC_FLAC:
    {
        if ( b_presentation ) return VLC_TS_INVALID;
        i_timestamp = i_granule * CLOCK_FREQ / p_stream->f_rate;
        break;
    }
    case VLC_CODEC_SPEEX:
    {
        if ( b_presentation )
            i_granule -= p_stream->special.speex.i_framesize *
                         p_stream->special.speex.i_framesperpacket;
        i_timestamp = i_granule * CLOCK_FREQ / p_stream->f_rate;
        break;
    }
    case VLC_CODEC_OGGSPOTS:
    {
        if ( b_presentation ) return VLC_TS_INVALID;
        i_timestamp = ( i_granule >> p_stream->i_granule_shift )
                * CLOCK_FREQ / p_stream->f_rate;
        break;
    }
    }

    return i_timestamp;
}

/* returns pos */
static int64_t OggBisectSearchByTime( demux_t *p_demux, logical_stream_t *p_stream,
            int64_t i_targettime, int64_t i_pos_lower, int64_t i_pos_upper)
{
    int64_t i_start_pos;
    int64_t i_end_pos;
    int64_t i_segsize;

    struct
    {
        int64_t i_pos;
        int64_t i_timestamp;
        int64_t i_granule;
    } bestlower = { p_stream->i_data_start, -1, -1 },
      current = { -1, -1, -1 },
      lowestupper = { -1, -1, -1 };

    demux_sys_t *p_sys  = p_demux->p_sys;

    i_pos_lower = __MAX( i_pos_lower, p_stream->i_data_start );
    i_pos_upper = __MIN( i_pos_upper, p_sys->i_total_length );
    if ( i_pos_upper < 0 ) i_pos_upper = p_sys->i_total_length;

    i_start_pos = i_pos_lower;
    i_end_pos = i_pos_upper;

    i_segsize = ( i_end_pos - i_start_pos + 1 ) >> 1;
    i_start_pos += i_segsize;

    OggDebug( msg_Dbg(p_demux, "Bisecting for time=%"PRId64" between %"PRId64" and %"PRId64,
            i_targettime, i_pos_lower, i_pos_upper ) );

    do
    {
        /* see if the frame lies in current segment */
        i_start_pos = __MAX( i_start_pos, i_pos_lower );
        i_end_pos = __MIN( i_end_pos, i_pos_upper );

        if ( i_start_pos >= i_end_pos )
        {
            if ( i_start_pos == i_pos_lower)
            {
                return i_start_pos;
            }
            return -1;
        }


        current.i_pos = find_first_page_granule( p_demux,
                                                 i_start_pos, i_end_pos,
                                                 p_stream,
                                                 &current.i_granule );

        current.i_timestamp = Oggseek_GranuleToAbsTimestamp( p_stream,
                                                             current.i_granule, false );

        if ( current.i_timestamp == -1 && current.i_granule > 0 )
        {
            msg_Err( p_demux, "Unmatched granule. New codec ?" );
            return -1;
        }
        else if ( current.i_timestamp < -1 )  /* due to preskip with some codecs */
        {
            current.i_timestamp = 0;
        }

        if ( current.i_pos != -1 && current.i_granule != -1 )
        {
            /* found a page */

            if ( current.i_timestamp <= i_targettime )
            {
                /* set our lower bound */
                if ( current.i_timestamp > bestlower.i_timestamp )
                    bestlower = current;
                i_start_pos = current.i_pos;
            }
            else if ( current.i_timestamp > i_targettime )
            {
                if ( lowestupper.i_timestamp == -1 || current.i_timestamp < lowestupper.i_timestamp )
                    lowestupper = current;
                /* check lower half of segment */
                i_start_pos -= i_segsize;
                i_end_pos -= i_segsize;
            }
        }
        else
        {
            /* no keyframe found, check lower segment */
            i_end_pos -= i_segsize;
            i_start_pos -= i_segsize;
        }

        OggDebug( msg_Dbg(p_demux, "Bisect restart i_segsize=%"PRId64" between %"PRId64
                                   " and %"PRId64 " bl %"PRId64" lu %"PRId64,
                i_segsize, i_start_pos, i_end_pos, bestlower.i_granule, lowestupper.i_granule  ) );

        i_segsize = ( i_end_pos - i_start_pos + 1 ) >> 1;
        i_start_pos += i_segsize;

    } while ( i_segsize > 64 );

    if ( bestlower.i_granule == -1 )
    {
        if ( lowestupper.i_granule == -1 )
            return -1;
        else
            bestlower = lowestupper;
    }

    if ( p_stream->b_oggds )
    {
        int64_t a = OggBackwardSeekToFrame( p_demux,
                __MAX ( bestlower.i_pos - OGGSEEK_BYTES_TO_READ, p_stream->i_data_start ),
                bestlower.i_pos,
                p_stream, bestlower.i_granule /* unused */ );
        return a;
    }
    /* If not each packet is usable as keyframe, query the codec for keyframe */
    else if ( Ogg_GetKeyframeGranule( p_stream, bestlower.i_granule ) != bestlower.i_granule )
    {
        int64_t i_keyframegranule = Ogg_GetKeyframeGranule( p_stream, bestlower.i_granule );

        OggDebug( msg_Dbg( p_demux, "Need to reseek to keyframe (%"PRId64") granule (%"PRId64"!=%"PRId64") to t=%"PRId64,
                           i_keyframegranule >> p_stream->i_granule_shift,
                           bestlower.i_granule,
                           i_pos_upper,
                           Oggseek_GranuleToAbsTimestamp( p_stream, i_keyframegranule, false ) ) );

        OggDebug( msg_Dbg( p_demux, "Seeking back to %"PRId64, __MAX ( bestlower.i_pos - OGGSEEK_BYTES_TO_READ, p_stream->i_data_start ) ) );

        int64_t a = OggBackwardSeekToFrame( p_demux,
            __MAX ( bestlower.i_pos - OGGSEEK_BYTES_TO_READ, p_stream->i_data_start ),
            stream_Size( p_demux->s ), p_stream, i_keyframegranule );
        return a;
    }

    return bestlower.i_pos;
}


/************************************************************************
 * public functions
 *************************************************************************/

int Oggseek_BlindSeektoAbsoluteTime( demux_t *p_demux, logical_stream_t *p_stream,
                                     int64_t i_time, bool b_fastseek )
{
    demux_sys_t *p_sys  = p_demux->p_sys;
    int64_t i_lowerpos = -1;
    int64_t i_upperpos = -1;
    bool b_found = false;

    /* Search in skeleton */
    Ogg_GetBoundsUsingSkeletonIndex( p_stream, i_time, &i_lowerpos, &i_upperpos );
    if ( i_lowerpos != -1 ) b_found = true;

    /* And also search in our own index */
    if ( !b_found && OggSeekIndexFind( p_stream, i_time, &i_lowerpos, &i_upperpos ) )
    {
        b_found = true;
    }

    /* Or try to be smart with audio fixed bitrate streams */
    if ( !b_found && p_stream->fmt.i_cat == AUDIO_ES && p_sys->i_streams == 1
         && p_sys->i_bitrate && Ogg_GetKeyframeGranule( p_stream, 0xFF00FF00 ) == 0xFF00FF00 )
    {
        /* But only if there's no keyframe/preload requirements */
        /* FIXME: add function to get preload time by codec, ex: opus */
        i_lowerpos = i_time * p_sys->i_bitrate / INT64_C(8000000);
        b_found = true;
    }

    /* or search */
    if ( !b_found && b_fastseek )
    {
        i_lowerpos = OggBisectSearchByTime( p_demux, p_stream, i_time,
                                            p_stream->i_data_start, p_sys->i_total_length );
        b_found = ( i_lowerpos != -1 );
    }

    if ( !b_found ) return -1;

    if ( i_lowerpos < p_stream->i_data_start || i_upperpos > p_sys->i_total_length )
        return -1;

    /* And really do seek */
    p_sys->i_input_position = i_lowerpos;
    seek_byte( p_demux, p_sys->i_input_position );
    ogg_stream_reset( &p_stream->os );

    return i_lowerpos;
}

int Oggseek_BlindSeektoPosition( demux_t *p_demux, logical_stream_t *p_stream,
                                 double f, bool b_canfastseek )
{
    OggDebug( msg_Dbg( p_demux, "=================== Seeking To Blind Pos" ) );
    int64_t i_size = stream_Size( p_demux->s );
    int64_t i_granule;
    int64_t i_pagepos;

    i_size = find_first_page_granule( p_demux,
                                             i_size * f, i_size,
                                             p_stream,
                                             &i_granule );

    OggDebug( msg_Dbg( p_demux, "Seek start pos is %"PRId64" granule %"PRId64, i_size, i_granule ) );

    i_granule = Ogg_GetKeyframeGranule( p_stream, i_granule );

    if ( b_canfastseek )
    {
        /* Peek back until we meet a keyframe to start our decoding up to our
         * final seek time */
        i_pagepos = OggBackwardSeekToFrame( p_demux,
                __MAX ( i_size - MAX_PAGE_SIZE, p_stream->i_data_start ),
                __MIN ( i_size + MAX_PAGE_SIZE, p_demux->p_sys->i_total_length ),
                p_stream, i_granule );
    }
    else
    {
        /* Otherwise, we just sync to the next keyframe we meet */
        i_pagepos = OggForwardSeekToFrame( p_demux,
                __MAX ( i_size - MAX_PAGE_SIZE, p_stream->i_data_start ),
                stream_Size( p_demux->s ),
                p_stream, i_granule, false );
    }

    OggDebug( msg_Dbg( p_demux, "=================== Seeked To %"PRId64" granule %"PRId64, i_pagepos, i_granule ) );
    return i_pagepos;
}

int Oggseek_SeektoAbsolutetime( demux_t *p_demux, logical_stream_t *p_stream,
                                int64_t i_time )
{
    demux_sys_t *p_sys  = p_demux->p_sys;

    OggDebug( msg_Dbg( p_demux, "=================== Seeking To Absolute Time %"PRId64, i_time ) );
    int64_t i_offset_lower = -1;
    int64_t i_offset_upper = -1;

    if ( Ogg_GetBoundsUsingSkeletonIndex( p_stream, i_time, &i_offset_lower, &i_offset_upper ) )
    {
        /* Exact match */
        OggDebug( msg_Dbg( p_demux, "Found keyframe at %"PRId64" using skeleton index", i_offset_lower ) );
        if ( i_offset_lower == -1 ) i_offset_lower = p_stream->i_data_start;
        p_sys->i_input_position = i_offset_lower;
        seek_byte( p_demux, p_sys->i_input_position );
        ogg_stream_reset( &p_stream->os );
        return i_offset_lower;
    }
    OggDebug( msg_Dbg( p_demux, "Search bounds set to %"PRId64" %"PRId64" using skeleton index", i_offset_lower, i_offset_upper ) );

    OggNoDebug(
        OggSeekIndexFind( p_stream, i_time, &i_offset_lower, &i_offset_upper )
    );

    i_offset_lower = __MAX( i_offset_lower, p_stream->i_data_start );
    i_offset_upper = __MIN( i_offset_upper, p_sys->i_total_length );

    int64_t i_pagepos = OggBisectSearchByTime( p_demux, p_stream, i_time,
                                       i_offset_lower, i_offset_upper);
    if ( i_pagepos >= 0 )
    {
        /* be sure to clear any state or read+pagein() will fail on same # */
        ogg_stream_reset( &p_stream->os );
        p_sys->i_input_position = i_pagepos;
        seek_byte( p_demux, p_sys->i_input_position );
    }
    /* Insert keyframe position into index */
    OggNoDebug(
    if ( i_pagepos >= p_stream->i_data_start )
        OggSeek_IndexAdd( p_stream, i_time, i_pagepos )
    );

    OggDebug( msg_Dbg( p_demux, "=================== Seeked To %"PRId64" time %"PRId64, i_pagepos, i_time ) );
    return i_pagepos;
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
    int i;
    int64_t i_in_pos;
    int64_t i_result;
    int i_page_size;
    char *buf;

    demux_sys_t *p_sys  = p_demux->p_sys;

    /* store position of this page */
    i_in_pos = p_ogg->i_input_position = vlc_stream_Tell( p_demux->s );

    if ( p_sys->b_page_waiting) {
        msg_Warn( p_demux, "Ogg page already loaded" );
        return 0;
    }

    if ( vlc_stream_Read ( p_demux->s, header, PAGE_HEADER_BYTES ) < PAGE_HEADER_BYTES )
    {
        vlc_stream_Seek( p_demux->s, i_in_pos );
        msg_Dbg ( p_demux, "Reached clean EOF in ogg file" );
        return 0;
    }

    i_nsegs = header[ PAGE_HEADER_BYTES - 1 ];

    if ( vlc_stream_Read ( p_demux->s, header+PAGE_HEADER_BYTES, i_nsegs ) < i_nsegs )
    {
        vlc_stream_Seek( p_demux->s, i_in_pos );
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

    i_result = vlc_stream_Read ( p_demux->s, (uint8_t*)buf + PAGE_HEADER_BYTES + i_nsegs,
                             i_page_size - PAGE_HEADER_BYTES - i_nsegs );

    ogg_sync_wrote( &p_ogg->oy, i_result + PAGE_HEADER_BYTES + i_nsegs );




    if ( ogg_sync_pageout( &p_ogg->oy, &p_ogg->current_page ) != 1 )
    {
        msg_Err( p_demux , "Got invalid packet, read %"PRId64" of %i: %s %"PRId64,
                 i_result, i_page_size, buf, i_in_pos );
        return 0;
    }

    return i_result + PAGE_HEADER_BYTES + i_nsegs;
}

