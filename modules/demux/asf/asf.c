/*****************************************************************************
 * asf.c : ASF demux module
 *****************************************************************************
 * Copyright © 2002-2004, 2006-2008, 2010 VLC authors and VideoLAN
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
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
#include <vlc_plugin.h>
#include <vlc_demux.h>
#include <vlc_dialog.h>

#include <vlc_meta.h>                  /* vlc_meta_Set*, vlc_meta_New */
#include <vlc_access.h>                /* GET_PRIVATE_ID_STATE */
#include <vlc_codecs.h>                /* VLC_BITMAPINFOHEADER, WAVEFORMATEX */
#include <vlc_vout.h>

#include <limits.h>

#include "asfpacket.h"
#include "libasf.h"
#include "assert.h"

/* TODO
 *  - add support for the newly added object: language, bitrate,
 *                                            extended stream properties.
 */

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open  ( vlc_object_t * );
static void Close ( vlc_object_t * );

vlc_module_begin ()
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_DEMUX )
    set_description( N_("ASF/WMV demuxer") )
    set_capability( "demux", 200 )
    set_callbacks( Open, Close )
    add_shortcut( "asf", "wmv" )
vlc_module_end ()


/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int Demux  ( demux_t * );
static int Control( demux_t *, int i_query, va_list args );

#define MAX_ASF_TRACKS (ASF_MAX_STREAMNUMBER + 1)
#define ASF_PREROLL_FROM_CURRENT -1

/* callbacks for packet parser */
static void Packet_UpdateTime( asf_packet_sys_t *p_packetsys, uint8_t i_stream_number,
                               vlc_tick_t i_time );
static void Packet_SetSendTime( asf_packet_sys_t *p_packetsys, vlc_tick_t i_time);
static bool Block_Dequeue( demux_t *p_demux, vlc_tick_t i_nexttime );
static asf_track_info_t * Packet_GetTrackInfo( asf_packet_sys_t *p_packetsys,
                                               uint8_t i_stream_number );
static bool Packet_DoSkip( asf_packet_sys_t *p_packetsys, uint8_t i_stream_number, bool b_packet_keyframe );
static void Packet_Enqueue( asf_packet_sys_t *p_packetsys, uint8_t i_stream_number, block_t **pp_frame );
static void Packet_SetAR( asf_packet_sys_t *p_packetsys, uint8_t i_stream_number,
                          uint8_t i_ratio_x, uint8_t i_ratio_y );

typedef struct
{
    int i_cat;

    es_out_id_t     *p_es;
    es_format_t     *p_fmt; /* format backup for video changes */
    bool             b_selected;

    vlc_tick_t       i_time; /* track time*/

    asf_track_info_t info;

    struct
    {
        block_t     *p_first;
        block_t    **pp_last;
    } queue;

} asf_track_t;

typedef struct
{
    vlc_tick_t          i_time;     /* s */
    vlc_tick_t          i_sendtime;
    vlc_tick_t          i_length;   /* length of file */
    uint64_t            i_bitrate;  /* global file bitrate */
    bool                b_eos;      /* end of current stream */
    bool                b_eof;      /* end of current media */

    asf_object_root_t            *p_root;
    asf_object_file_properties_t *p_fp;

    unsigned int        i_track;
    asf_track_t         *track[MAX_ASF_TRACKS]; /* track number is stored on 7 bits */

    uint64_t            i_data_begin;
    uint64_t            i_data_end;

    bool                b_index;
    bool                b_canfastseek;
    uint8_t             i_seek_track;
    uint8_t             i_access_selected_track[ES_CATEGORY_COUNT]; /* mms, depends on access algorithm */
    unsigned int        i_wait_keyframe;

    vlc_tick_t          i_preroll_start;

    asf_packet_sys_t    packet_sys;

    vlc_meta_t          *meta;
} demux_sys_t;

static int      DemuxInit( demux_t * );
static void     DemuxEnd( demux_t * );

static void     FlushQueue( asf_track_t * );
static void     FlushQueues( demux_t *p_demux );

/*****************************************************************************
 * Open: check file and initializes ASF structures
 *****************************************************************************/
static int Open( vlc_object_t * p_this )
{
    demux_t     *p_demux = (demux_t *)p_this;
    demux_sys_t *p_sys;
    vlc_guid_t      guid;
    const uint8_t     *p_peek;

    /* A little test to see if it could be a asf stream */
    if( vlc_stream_Peek( p_demux->s, &p_peek, 16 ) < 16 ) return VLC_EGENERIC;

    ASF_GetGUID( &guid, p_peek );
    if( !guidcmp( &guid, &asf_object_header_guid ) ) return VLC_EGENERIC;

    /* Set p_demux fields */
    p_demux->pf_demux = Demux;
    p_demux->pf_control = Control;
    p_demux->p_sys = p_sys = calloc( 1, sizeof( demux_sys_t ) );

    /* Load the headers */
    if( DemuxInit( p_demux ) )
    {
        free( p_sys );
        return VLC_EGENERIC;
    }

    p_sys->packet_sys.p_demux = p_demux;
    p_sys->packet_sys.pf_doskip = Packet_DoSkip;
    p_sys->packet_sys.pf_send = Packet_Enqueue;
    p_sys->packet_sys.pf_gettrackinfo = Packet_GetTrackInfo;
    p_sys->packet_sys.pf_updatetime = Packet_UpdateTime;
    p_sys->packet_sys.pf_updatesendtime = Packet_SetSendTime;
    p_sys->packet_sys.pf_setaspectratio = Packet_SetAR;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Demux: read packet and send them to decoders
 *****************************************************************************/
#define CHUNK VLC_TICK_FROM_MS(100)
static int Demux( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    for( int i=0; i<ES_CATEGORY_COUNT; i++ )
    {
        if ( p_sys->i_access_selected_track[i] > 0 )
        {
            es_out_Control( p_demux->out, ES_OUT_SET_ES_STATE,
                            p_sys->track[p_sys->i_access_selected_track[i]]->p_es, true );
            p_sys->i_access_selected_track[i] = 0;
        }
    }

    /* Get selected tracks, especially for computing PCR */
    for( int i=0; i<MAX_ASF_TRACKS; i++ )
    {
        asf_track_t *tk = p_sys->track[i];
        if ( !tk ) continue;
        if ( tk->p_es )
            es_out_Control( p_demux->out, ES_OUT_GET_ES_STATE, tk->p_es, & tk->b_selected );
        else
            tk->b_selected = false;
    }

    while( !p_sys->b_eos && ( p_sys->i_sendtime - p_sys->i_time - CHUNK < 0 ||
                            ( p_sys->i_sendtime - p_sys->i_time - CHUNK ) <
                                                     p_sys->p_fp->i_preroll ) )
    {
        /* Read and demux a packet */
        if( DemuxASFPacket( &p_sys->packet_sys,
                             p_sys->p_fp->i_min_data_packet_size,
                             p_sys->p_fp->i_max_data_packet_size ) <= 0 )
        {
            p_sys->b_eos = true;
            /* Check if we have concatenated files */
            const uint8_t *p_peek;
            if( vlc_stream_Peek( p_demux->s, &p_peek, 16 ) == 16 )
            {
                vlc_guid_t guid;

                ASF_GetGUID( &guid, p_peek );
                p_sys->b_eof = !guidcmp( &guid, &asf_object_header_guid );
                if( !p_sys->b_eof )
                    msg_Warn( p_demux, "found a new ASF header" );
            }
            else
                p_sys->b_eof = true;
        }

        if ( p_sys->i_time == VLC_TICK_INVALID )
            p_sys->i_time = p_sys->i_sendtime;
    }

    if( p_sys->b_eos || ( p_sys->i_sendtime - p_sys->i_time - CHUNK >= 0 &&
                        ( p_sys->i_sendtime - p_sys->i_time - CHUNK ) >=
                                                     p_sys->p_fp->i_preroll ) )
    {
        bool b_data = Block_Dequeue( p_demux, p_sys->i_time + CHUNK );

        p_sys->i_time += CHUNK;
        es_out_SetPCR( p_demux->out, VLC_TICK_0 + p_sys->i_time );
#ifdef ASF_DEBUG
        msg_Dbg( p_demux, "Demux Loop Setting PCR to %"PRId64, VLC_TICK_0 + p_sys->i_time );
#endif
        if ( !b_data && p_sys->b_eos )
        {
            /* We end this stream */
            if( !p_sys->b_eof )
            {
                DemuxEnd( p_demux );

                /* And we prepare to read the next one */
                if( DemuxInit( p_demux ) )
                {
                    msg_Err( p_demux, "failed to load the new header" );
                    return VLC_DEMUXER_EOF;
                }
                es_out_Control( p_demux->out, ES_OUT_RESET_PCR );
            }
            else
                return VLC_DEMUXER_EOF;
        }
    }

    return 1;
}

/*****************************************************************************
 * Close: frees unused data
 *****************************************************************************/
static void Close( vlc_object_t * p_this )
{
    demux_t     *p_demux = (demux_t *)p_this;

    DemuxEnd( p_demux );

    free( p_demux->p_sys );
}

/*****************************************************************************
 * WaitKeyframe: computes the number of frames to wait for a keyframe
 *****************************************************************************/
static void WaitKeyframe( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    if ( ! p_sys->i_seek_track )
    {
        for ( int i=0; i<MAX_ASF_TRACKS; i++ )
        {
            asf_track_t *tk = p_sys->track[i];
            if ( tk && tk->info.p_sp && tk->i_cat == VIDEO_ES && tk->b_selected )
            {
                p_sys->i_seek_track = tk->info.p_sp->i_stream_number;
                break;
            }
        }
    }

    if ( p_sys->i_seek_track )
    {
        /* Skip forward at least 1 min */
        asf_track_t *tk = p_sys->track[p_sys->i_seek_track];
        if ( tk->info.p_esp && tk->info.p_esp->i_average_time_per_frame )
        {
            /* 1 min if fastseek, otherwise 5 sec */
            /* That's a guess for bandwidth */
            msftime_t i_maxwaittime = MSFTIME_FROM_SEC( p_sys->b_canfastseek ? 60 : 5);
            uint64_t frames = i_maxwaittime / tk->info.p_esp->i_average_time_per_frame;
            p_sys->i_wait_keyframe = __MIN( frames, UINT_MAX );
        }
        else
        {
            p_sys->i_wait_keyframe = ( p_sys->b_canfastseek ) ? 25 * 30 : 25 * 5;
        }
    }
    else
    {
        p_sys->i_wait_keyframe = 0;
    }

}

/*****************************************************************************
 * SeekIndex: goto to i_date or i_percent
 *****************************************************************************/
static int SeekPercent( demux_t *p_demux, int i_query, va_list args )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    WaitKeyframe( p_demux );

    msg_Dbg( p_demux, "seek with percent: waiting %i frames", p_sys->i_wait_keyframe );
    return demux_vaControlHelper( p_demux->s, __MIN( INT64_MAX, p_sys->i_data_begin ),
                                   __MIN( INT64_MAX, p_sys->i_data_end ),
                                   __MIN( INT64_MAX, p_sys->i_bitrate ),
                                   __MIN( INT16_MAX, p_sys->p_fp->i_min_data_packet_size ),
                                   i_query, args );
}

static int SeekIndex( demux_t *p_demux, vlc_tick_t i_date, float f_pos )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    asf_object_index_t *p_index;

    msg_Dbg( p_demux, "seek with index: %i seconds, position %f",
             i_date >= 0 ? (int)SEC_FROM_VLC_TICK(i_date) : -1, f_pos );

    if( i_date < 0 )
        i_date = p_sys->i_length * f_pos;

    p_sys->i_preroll_start = i_date - p_sys->p_fp->i_preroll;
    if ( p_sys->i_preroll_start < 0 ) p_sys->i_preroll_start = 0;

    p_index = ASF_FindObject( p_sys->p_root, &asf_object_simple_index_guid, 0 );

    uint64_t i_entry = MSFTIME_FROM_VLC_TICK(p_sys->i_preroll_start) / p_index->i_index_entry_time_interval;
    if( i_entry >= p_index->i_index_entry_count )
    {
        msg_Warn( p_demux, "Incomplete index" );
        return VLC_EGENERIC;
    }

    WaitKeyframe( p_demux );

    uint64_t i_offset = (uint64_t)p_index->index_entry[i_entry].i_packet_number *
                        p_sys->p_fp->i_min_data_packet_size;

    if ( vlc_stream_Seek( p_demux->s, i_offset + p_sys->i_data_begin ) == VLC_SUCCESS )
    {
        es_out_Control( p_demux->out, ES_OUT_SET_NEXT_DISPLAY_TIME, VLC_TICK_0 + i_date );
        return VLC_SUCCESS;
    }
    else return VLC_EGENERIC;
}

static void SeekPrepare( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    p_sys->b_eof = false;
    p_sys->b_eos = false;
    p_sys->i_time = VLC_TICK_INVALID;
    p_sys->i_sendtime = -1;
    p_sys->i_preroll_start = ASFPACKET_PREROLL_FROM_CURRENT;

    for( int i = 0; i < MAX_ASF_TRACKS ; i++ )
    {
        asf_track_t *tk = p_sys->track[i];
        if( tk )
        {
            FlushQueue( tk );
            tk->i_time = VLC_TICK_INVALID;
        }
    }

    es_out_Control( p_demux->out, ES_OUT_RESET_PCR );
}

/*****************************************************************************
 * Control:
 *****************************************************************************/
static int Control( demux_t *p_demux, int i_query, va_list args )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    vlc_meta_t  *p_meta;
    vlc_tick_t  i64;
    int         i;
    double      f, *pf;

    switch( i_query )
    {
    case DEMUX_GET_LENGTH:
        *va_arg( args, vlc_tick_t * ) = p_sys->i_length;
        return VLC_SUCCESS;

    case DEMUX_GET_TIME:
        if( p_sys->i_time == VLC_TICK_INVALID ) return VLC_EGENERIC;
        *va_arg( args, vlc_tick_t * ) = p_sys->i_time;
        return VLC_SUCCESS;

    case DEMUX_SET_TIME:
        if ( !p_sys->p_fp ||
             ! ( p_sys->p_fp->i_flags & ASF_FILE_PROPERTIES_SEEKABLE ) )
            return VLC_EGENERIC;

        SeekPrepare( p_demux );

        if( p_sys->b_index && p_sys->i_length != 0 )
        {
            va_list acpy;
            va_copy( acpy, args );
            i64 = va_arg( acpy, vlc_tick_t );
            va_end( acpy );

            if( !SeekIndex( p_demux, i64, -1 ) )
                return VLC_SUCCESS;
        }
        return SeekPercent( p_demux, i_query, args );

    case DEMUX_SET_ES:
    {
        i = va_arg( args, int );
        int i_ret;
        if ( i >= 0 )
        {
            msg_Dbg( p_demux, "Requesting access to enable stream %d", i );
            i_ret = vlc_stream_Control( p_demux->s,
                                        STREAM_SET_PRIVATE_ID_STATE, i, true );
        }
        else
        {  /* i contains -1 * es_category */
            msg_Dbg( p_demux, "Requesting access to disable stream %d", i );
            i_ret = vlc_stream_Control( p_demux->s,
                                        STREAM_SET_PRIVATE_ID_STATE, i,
                                        false );
        }

        if ( i_ret == VLC_SUCCESS )
        {
            asf_track_t *tk;
            if( i >= 0 )
            {
                tk = p_sys->track[i];
            }
            else
            {
                for( int j = 0; j < MAX_ASF_TRACKS ; j++ )
                {
                    tk = p_sys->track[j];
                    if( !tk || !tk->p_fmt || tk->i_cat != -1 * i )
                        continue;
                    FlushQueue( tk );
                    tk->i_time = VLC_TICK_INVALID;
                }
            }

            p_sys->i_seek_track = 0;
            if ( ( tk && tk->i_cat == VIDEO_ES ) || i == -1 * VIDEO_ES )
                WaitKeyframe( p_demux );
        }
        return i_ret;
    }

    case DEMUX_SET_ES_LIST:
        return VLC_EGENERIC; /* TODO */

    case DEMUX_GET_POSITION:
        if( p_sys->i_time == VLC_TICK_INVALID ) return VLC_EGENERIC;
        if( p_sys->i_length != 0 )
        {
            pf = va_arg( args, double * );
            *pf = p_sys->i_time / (double)p_sys->i_length;
            return VLC_SUCCESS;
        }
        return demux_vaControlHelper( p_demux->s,
                                       __MIN( INT64_MAX, p_sys->i_data_begin ),
                                       __MIN( INT64_MAX, p_sys->i_data_end ),
                                       __MIN( INT64_MAX, p_sys->i_bitrate ),
                                       __MIN( INT16_MAX, p_sys->p_fp->i_min_data_packet_size ),
                                       i_query, args );

    case DEMUX_SET_POSITION:
        if ( !p_sys->p_fp ||
             ( !( p_sys->p_fp->i_flags & ASF_FILE_PROPERTIES_SEEKABLE ) && !p_sys->b_index ) )
            return VLC_EGENERIC;

        SeekPrepare( p_demux );

        if( p_sys->b_index && p_sys->i_length != 0 )
        {
            va_list acpy;
            va_copy( acpy, args );
            f = va_arg( acpy, double );
            va_end( acpy );

            if( !SeekIndex( p_demux, -1, f ) )
                return VLC_SUCCESS;
        }
        return SeekPercent( p_demux, i_query, args );

    case DEMUX_GET_META:
        p_meta = va_arg( args, vlc_meta_t * );
        vlc_meta_Merge( p_meta, p_sys->meta );
        return VLC_SUCCESS;

    case DEMUX_CAN_SEEK:
        if ( !p_sys->p_fp ||
             ( !( p_sys->p_fp->i_flags & ASF_FILE_PROPERTIES_SEEKABLE ) && !p_sys->b_index ) )
        {
            bool *pb_bool = va_arg( args, bool * );
            *pb_bool = false;
            return VLC_SUCCESS;
        }
        /* fall through */
    default:
        return demux_vaControlHelper( p_demux->s,
                                      __MIN( INT64_MAX, p_sys->i_data_begin ),
                                      __MIN( INT64_MAX, p_sys->i_data_end),
                                      __MIN( INT64_MAX, p_sys->i_bitrate ),
                    ( p_sys->p_fp ) ? __MIN( INT_MAX, p_sys->p_fp->i_min_data_packet_size ) : 1,
                    i_query, args );
    }
}

/*****************************************************************************
 *
 *****************************************************************************/
static void Packet_SetAR( asf_packet_sys_t *p_packetsys, uint8_t i_stream_number,
                          uint8_t i_ratio_x, uint8_t i_ratio_y )
{
    demux_t *p_demux = p_packetsys->p_demux;
    demux_sys_t *p_sys = p_demux->p_sys;
    asf_track_t *tk = p_sys->track[i_stream_number];

    if ( !tk->p_fmt || (tk->p_fmt->video.i_sar_num == i_ratio_x && tk->p_fmt->video.i_sar_den == i_ratio_y ) )
        return;

    tk->p_fmt->video.i_sar_num = i_ratio_x;
    tk->p_fmt->video.i_sar_den = i_ratio_y;
    if( tk->p_es )
        es_out_Control( p_demux->out, ES_OUT_SET_ES_FMT, tk->p_es, tk->p_fmt );
}

static void Packet_SetSendTime( asf_packet_sys_t *p_packetsys, vlc_tick_t i_time )
{
    demux_t *p_demux = p_packetsys->p_demux;
    demux_sys_t *p_sys = p_demux->p_sys;

    p_sys->i_sendtime = i_time;
}

static void Packet_UpdateTime( asf_packet_sys_t *p_packetsys, uint8_t i_stream_number,
                               vlc_tick_t i_time )
{
    demux_t *p_demux = p_packetsys->p_demux;
    demux_sys_t *p_sys = p_demux->p_sys;
    asf_track_t *tk = p_sys->track[i_stream_number];

    if ( tk )
        tk->i_time = i_time;
}

static asf_track_info_t * Packet_GetTrackInfo( asf_packet_sys_t *p_packetsys,
                                               uint8_t i_stream_number )
{
    demux_t *p_demux = p_packetsys->p_demux;
    demux_sys_t *p_sys = p_demux->p_sys;
    asf_track_t *tk = p_sys->track[i_stream_number];

    if (!tk)
        return NULL;
    else
        return & tk->info;
}

static bool Packet_DoSkip( asf_packet_sys_t *p_packetsys, uint8_t i_stream_number, bool b_packet_keyframe )
{
    demux_t *p_demux = p_packetsys->p_demux;
    demux_sys_t *p_sys = p_demux->p_sys;
    const asf_track_t *tk = p_sys->track[i_stream_number];

    if( tk == NULL )
    {
        msg_Warn( p_demux, "undeclared stream[Id 0x%x]", i_stream_number );
        return true;
    }

    if( p_sys->i_wait_keyframe )
    {
        if ( i_stream_number == p_sys->i_seek_track )
        {
            if ( !b_packet_keyframe )
            {
                p_sys->i_wait_keyframe--;
                return true;
            }
            else
                p_sys->i_wait_keyframe = 0;
        }
        else
            return true;
    }

    if( !tk->p_es )
        return true;

    return false;
}

static void Packet_Enqueue( asf_packet_sys_t *p_packetsys, uint8_t i_stream_number, block_t **pp_frame )
{
    demux_t *p_demux = p_packetsys->p_demux;
    demux_sys_t *p_sys = p_demux->p_sys;
    asf_track_t *tk = p_sys->track[i_stream_number];
    if ( !tk )
        return;

    block_t *p_gather = block_ChainGather( *pp_frame );
    if( p_gather )
    {
        block_ChainLastAppend( & tk->queue.pp_last, p_gather );
#ifdef ASF_DEBUG
        msg_Dbg( p_demux, "    enqueue packet dts %"PRId64" pts %"PRId64" pcr %"PRId64, p_gather->i_dts, p_gather->i_pts, p_sys->i_time );
#endif
    }

    *pp_frame = NULL;
}

static bool Block_Dequeue( demux_t *p_demux, vlc_tick_t i_nexttime )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    bool b_tracks_have_data = false;
    for( int i = 0; i < MAX_ASF_TRACKS; i++ )
    {
        asf_track_t *tk = p_sys->track[i];
        if (!tk)
            continue;
        b_tracks_have_data |= (tk->queue.p_first != NULL);
        while( tk->queue.p_first && tk->queue.p_first->i_dts <= i_nexttime )
        {
            block_t *p_block = tk->queue.p_first;
            tk->queue.p_first = p_block->p_next;
            if( tk->queue.p_first == NULL )
                tk->queue.pp_last = &tk->queue.p_first;
            else
                p_block->p_next = NULL;

            if( p_sys->i_time == VLC_TICK_INVALID )
            {
                es_out_SetPCR( p_demux->out, VLC_TICK_0 + p_sys->i_time );
#ifdef ASF_DEBUG
                msg_Dbg( p_demux, "    dequeue setting PCR to %"PRId64, VLC_TICK_0 + p_sys->i_time );
#endif
            }

#ifdef ASF_DEBUG
            msg_Dbg( p_demux, "    sending packet dts %"PRId64" pts %"PRId64" pcr %"PRId64, p_block->i_dts, p_block->i_pts, p_sys->i_time );
#endif
            es_out_Send( p_demux->out, tk->p_es, p_block );
        }
    }
    return b_tracks_have_data;
}

/*****************************************************************************
 *
 *****************************************************************************/
typedef struct asf_es_priorities_t
{
    uint16_t *pi_stream_numbers;
    uint16_t i_count;
} asf_es_priorities_t;

/* Fills up our exclusion list */
static void ASF_fillup_es_priorities_ex( demux_sys_t *p_sys, void *p_hdr,
                                         asf_es_priorities_t *p_prios )
{
    /* Find stream exclusions */
    asf_object_advanced_mutual_exclusion_t *p_mutex =
            ASF_FindObject( p_hdr, &asf_object_advanced_mutual_exclusion, 0 );
    if (! p_mutex ) return;

#if ( UINT_MAX > SIZE_MAX / 2 )
    if ( p_sys->i_track > (size_t)SIZE_MAX / sizeof(uint16_t) )
        return;
#endif
    p_prios->pi_stream_numbers = vlc_alloc( p_sys->i_track, sizeof(uint16_t) );
    if ( !p_prios->pi_stream_numbers ) return;

    if ( p_mutex->i_stream_number_count )
    {
        /* Just set highest prio on highest in the group */
        for ( uint16_t i = 1; i < p_mutex->i_stream_number_count; i++ )
        {
            if ( p_prios->i_count > p_sys->i_track || i > p_sys->i_track ) break;
            p_prios->pi_stream_numbers[ p_prios->i_count++ ] = p_mutex->pi_stream_number[ i ];
        }
    }
}

/* Fills up our bitrate exclusion list */
static void ASF_fillup_es_bitrate_priorities_ex( demux_sys_t *p_sys, void *p_hdr,
                                                 asf_es_priorities_t *p_prios )
{
    /* Find bitrate exclusions */
    asf_object_bitrate_mutual_exclusion_t *p_bitrate_mutex =
            ASF_FindObject( p_hdr, &asf_object_bitrate_mutual_exclusion_guid, 0 );
    if (! p_bitrate_mutex ) return;

#if ( UINT_MAX > SIZE_MAX / 2 )
    if ( p_sys->i_track > (size_t)SIZE_MAX / sizeof(uint16_t) )
        return;
#endif
    p_prios->pi_stream_numbers = vlc_alloc( p_sys->i_track, sizeof( uint16_t ) );
    if ( !p_prios->pi_stream_numbers ) return;

    if ( p_bitrate_mutex->i_stream_number_count )
    {
        /* Just remove < highest */
        for ( uint16_t i = 1; i < p_bitrate_mutex->i_stream_number_count; i++ )
        {
            if ( p_prios->i_count > p_sys->i_track || i > p_sys->i_track ) break;
            p_prios->pi_stream_numbers[ p_prios->i_count++ ] = p_bitrate_mutex->pi_stream_numbers[ i ];
        }
    }

}

#define GET_CHECKED( target, getter, maxtarget, temp ) \
{\
    temp i_temp = getter;\
    if ( i_temp > maxtarget ) {\
        msg_Warn( p_demux, "rejecting stream %u : " #target " overflow", i_stream );\
        es_format_Clean( &fmt );\
        goto error;\
    } else {\
        target = i_temp;\
    }\
}

static int DemuxInit( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    /* init context */
    p_sys->i_time   = VLC_TICK_INVALID;
    p_sys->i_sendtime    = -1;
    p_sys->i_length = 0;
    p_sys->b_eos = false;
    p_sys->b_eof = false;
    p_sys->i_bitrate = 0;
    p_sys->p_root   = NULL;
    p_sys->p_fp     = NULL;
    p_sys->b_index  = 0;
    p_sys->i_track  = 0;
    p_sys->i_seek_track = 0;
    p_sys->i_wait_keyframe = 0;
    for( int i = 0; i < MAX_ASF_TRACKS; i++ )
    {
        p_sys->track[i] = NULL;
    }
    p_sys->i_data_begin = 0;
    p_sys->i_data_end   = 0;
    p_sys->i_preroll_start = 0;
    p_sys->meta         = NULL;

    /* Now load all object ( except raw data ) */
    vlc_stream_Control( p_demux->s, STREAM_CAN_FASTSEEK,
                        &p_sys->b_canfastseek );
    if( !(p_sys->p_root = ASF_ReadObjectRoot(p_demux->s, p_sys->b_canfastseek)) )
    {
        msg_Warn( p_demux, "ASF plugin discarded (not a valid file)" );
        return VLC_EGENERIC;
    }
    p_sys->p_fp = p_sys->p_root->p_fp;

    if( p_sys->p_fp->i_min_data_packet_size != p_sys->p_fp->i_max_data_packet_size )
    {
        msg_Warn( p_demux, "ASF plugin discarded (invalid file_properties object)" );
        goto error;
    }

    if ( ASF_FindObject( p_sys->p_root->p_hdr,
                         &asf_object_content_encryption_guid, 0 ) != NULL
         || ASF_FindObject( p_sys->p_root->p_hdr,
                            &asf_object_extended_content_encryption_guid, 0 ) != NULL
         || ASF_FindObject( p_sys->p_root->p_hdr,
                         &asf_object_advanced_content_encryption_guid, 0 ) != NULL )
    {
        vlc_dialog_display_error( p_demux, _("Could not demux ASF stream"), "%s",
            ("DRM protected streams are not supported.") );
        goto error;
    }

    p_sys->i_track = ASF_CountObject( p_sys->p_root->p_hdr,
                                      &asf_object_stream_properties_guid );
    if( p_sys->i_track == 0 )
    {
        msg_Warn( p_demux, "ASF plugin discarded (cannot find any stream!)" );
        goto error;
    }
    msg_Dbg( p_demux, "found %u streams", p_sys->i_track );

    /* check if index is available */
    asf_object_index_t *p_index = ASF_FindObject( p_sys->p_root,
                                                  &asf_object_simple_index_guid, 0 );
    const bool b_index = p_index && p_index->i_index_entry_count;

    /* Find the extended header if any */
    asf_object_t *p_hdr_ext = ASF_FindObject( p_sys->p_root->p_hdr,
                                              &asf_object_header_extension_guid, 0 );

    asf_object_language_list_t *p_languages = NULL;
    asf_es_priorities_t fmt_priorities_ex = { NULL, 0 };
    asf_es_priorities_t fmt_priorities_bitrate_ex = { NULL, 0 };

    if( p_hdr_ext )
    {
        p_languages = ASF_FindObject( p_hdr_ext, &asf_object_language_list, 0 );

        ASF_fillup_es_priorities_ex( p_sys, p_hdr_ext, &fmt_priorities_ex );
        ASF_fillup_es_bitrate_priorities_ex( p_sys, p_hdr_ext, &fmt_priorities_bitrate_ex );
    }

    const bool b_mms = !strncasecmp( p_demux->psz_url, "mms:", 4 );
    bool b_dvrms = false;

    if( b_mms )
    {
        es_out_Control( p_demux->out, ES_OUT_SET_ES_CAT_POLICY,
                        VIDEO_ES, ES_OUT_ES_POLICY_EXCLUSIVE );
    }

    for( unsigned i_stream = 0; i_stream < p_sys->i_track; i_stream++ )
    {
        asf_track_t    *tk;
        asf_object_stream_properties_t *p_sp;
        asf_object_extended_stream_properties_t *p_esp;
        bool b_access_selected;

        p_sp = ASF_FindObject( p_sys->p_root->p_hdr,
                               &asf_object_stream_properties_guid,
                               i_stream );
        p_esp = NULL;

        /* Ignore duplicated streams numbers */
        if (p_sys->track[p_sp->i_stream_number])
            continue;

        tk = p_sys->track[p_sp->i_stream_number] = malloc( sizeof( asf_track_t ) );
        if (!tk)
            goto error;
        memset( tk, 0, sizeof( asf_track_t ) );

        tk->i_time = VLC_TICK_INVALID;
        tk->info.p_sp = p_sp;
        tk->p_es = NULL;
        tk->info.p_esp = NULL;
        tk->info.p_frame = NULL;
        tk->info.i_cat = UNKNOWN_ES;
        tk->queue.p_first = NULL;
        tk->queue.pp_last = &tk->queue.p_first;

        if ( !b_mms )
        {
            /* Check (not mms) if this track is selected (ie will receive data) */
            if( !vlc_stream_Control( p_demux->s, STREAM_GET_PRIVATE_ID_STATE,
                                     (int) p_sp->i_stream_number,
                                     &b_access_selected ) &&
                !b_access_selected )
            {
                tk->i_cat = UNKNOWN_ES;
                msg_Dbg( p_demux, "ignoring not selected stream(ID:%u) (by access)",
                         p_sp->i_stream_number );
                continue;
            }
        }

        /* Find the associated extended_stream_properties if any */
        if( p_hdr_ext )
        {
            int i_ext_stream = ASF_CountObject( p_hdr_ext,
                                                &asf_object_extended_stream_properties_guid );
            for( int i = 0; i < i_ext_stream; i++ )
            {
                asf_object_t *p_tmp =
                    ASF_FindObject( p_hdr_ext,
                                    &asf_object_extended_stream_properties_guid, i );
                if( p_tmp->ext_stream.i_stream_number == p_sp->i_stream_number )
                {
                    p_esp = &p_tmp->ext_stream;
                    tk->info.p_esp = p_esp;
                    break;
                }
            }
        }

        /* Check for DVR-MS */
        if( p_esp )
            for( uint16_t i=0; i<p_esp->i_payload_extension_system_count && !b_dvrms; i++ )
                b_dvrms = guidcmp( &p_esp->p_ext[i].i_extension_id, &asf_dvr_sampleextension_timing_rep_data_guid );

        es_format_t fmt;

        if( guidcmp( &p_sp->i_stream_type, &asf_object_stream_type_audio ) &&
            p_sp->i_type_specific_data_length >= sizeof( WAVEFORMATEX ) - 2 )
        {
            uint8_t *p_data = p_sp->p_type_specific_data;
            int i_format;

            es_format_Init( &fmt, AUDIO_ES, 0 );
            i_format = GetWLE( &p_data[0] );
            wf_tag_to_fourcc( i_format, &fmt.i_codec, NULL );

            GET_CHECKED( fmt.audio.i_channels,      GetWLE( &p_data[2] ),
                                                        255, uint16_t );
            GET_CHECKED( fmt.audio.i_rate,          GetDWLE( &p_data[4] ),
                                                        UINT_MAX, uint32_t );
            GET_CHECKED( fmt.i_bitrate,             GetDWLE( &p_data[8] ) * 8,
                                                        UINT_MAX, uint32_t );
            fmt.audio.i_blockalign      = GetWLE(  &p_data[12] );
            fmt.audio.i_bitspersample   = GetWLE(  &p_data[14] );

            if( p_sp->i_type_specific_data_length > sizeof( WAVEFORMATEX ) &&
                i_format != WAVE_FORMAT_MPEGLAYER3 &&
                i_format != WAVE_FORMAT_MPEG )
            {
                GET_CHECKED( fmt.i_extra, __MIN( GetWLE( &p_data[16] ),
                                     p_sp->i_type_specific_data_length -
                                     sizeof( WAVEFORMATEX ) ),
                             INT_MAX, uint32_t );
                fmt.p_extra = malloc( fmt.i_extra );
                memcpy( fmt.p_extra, &p_data[sizeof( WAVEFORMATEX )],
                        fmt.i_extra );
            }
            msg_Dbg( p_demux, "added new audio stream (codec:%4.4s(0x%x),ID:%d)",
                (char*)&fmt.i_codec, GetWLE( p_data ), p_sp->i_stream_number );
        }
        else if( guidcmp( &p_sp->i_stream_type,
                              &asf_object_stream_type_video ) &&
                 p_sp->i_type_specific_data_length >= 11 +
                 sizeof( VLC_BITMAPINFOHEADER ) )
        {
            uint8_t      *p_data = &p_sp->p_type_specific_data[11];

            es_format_Init( &fmt, VIDEO_ES,
                            VLC_FOURCC( p_data[16], p_data[17],
                                        p_data[18], p_data[19] ) );

            GET_CHECKED( fmt.video.i_width,      GetDWLE( p_data + 4 ),
                                                     UINT_MAX, uint32_t );
            GET_CHECKED( fmt.video.i_height,     GetDWLE( p_data + 8 ),
                                                     UINT_MAX, uint32_t );
            fmt.video.i_visible_width = fmt.video.i_width;
            fmt.video.i_visible_height = fmt.video.i_height;

            if( p_esp && p_esp->i_average_time_per_frame > 0 )
            {
                fmt.video.i_frame_rate = 10000000;
                GET_CHECKED( fmt.video.i_frame_rate_base,
                             p_esp->i_average_time_per_frame,
                             UINT_MAX, uint64_t );
            }

            if( fmt.i_codec == VLC_FOURCC( 'D','V','R',' ') )
            {
                /* DVR-MS special ASF */
                fmt.i_codec = VLC_CODEC_MPGV;
            }

            if( p_sp->i_type_specific_data_length > 11 +
                sizeof( VLC_BITMAPINFOHEADER ) )
            {
                GET_CHECKED( fmt.i_extra, __MIN( GetDWLE( p_data ),
                                     p_sp->i_type_specific_data_length - 11 -
                                     sizeof( VLC_BITMAPINFOHEADER ) ),
                             UINT_MAX, uint32_t );
                fmt.p_extra = malloc( fmt.i_extra );
                memcpy( fmt.p_extra, &p_data[sizeof( VLC_BITMAPINFOHEADER )],
                        fmt.i_extra );
            }

            /* Look for an aspect ratio */
            if( p_sys->p_root->p_metadata )
            {
                asf_object_metadata_t *p_meta = p_sys->p_root->p_metadata;
                unsigned int i_aspect_x = 0, i_aspect_y = 0;
                uint32_t i;
                for( i = 0; i < p_meta->i_record_entries_count; i++ )
                {
                    if( !p_meta->record[i].psz_name )
                        continue;
                    if( !strcmp( p_meta->record[i].psz_name, "AspectRatioX" ) )
                    {
                        if( (!i_aspect_x && !p_meta->record[i].i_stream) ||
                            p_meta->record[i].i_stream ==
                            p_sp->i_stream_number )
                            GET_CHECKED( i_aspect_x, p_meta->record[i].i_val,
                                         UINT_MAX, uint64_t );
                    }
                    if( !strcmp( p_meta->record[i].psz_name, "AspectRatioY" ) )
                    {
                        if( (!i_aspect_y && !p_meta->record[i].i_stream) ||
                            p_meta->record[i].i_stream ==
                            p_sp->i_stream_number )
                            GET_CHECKED( i_aspect_y, p_meta->record[i].i_val,
                                         UINT_MAX, uint64_t );
                    }
                }

                if( i_aspect_x && i_aspect_y )
                {
                    fmt.video.i_sar_num = i_aspect_x;
                    fmt.video.i_sar_den = i_aspect_y;
                }
            }

            /* If there is a video track then use the index for seeking */
            p_sys->b_index = b_index;

            msg_Dbg( p_demux, "added new video stream(codec:%4.4s,ID:%d)",
                     (char*)&fmt.i_codec, p_sp->i_stream_number );
        }
        else if( guidcmp( &p_sp->i_stream_type, &asf_object_stream_type_binary ) &&
            p_sp->i_type_specific_data_length >= 64 )
        {
            vlc_guid_t i_major_media_type;
            ASF_GetGUID( &i_major_media_type, p_sp->p_type_specific_data );
            msg_Dbg( p_demux, "stream(ID:%d) major type " GUID_FMT, p_sp->i_stream_number,
                     GUID_PRINT(i_major_media_type) );

            vlc_guid_t i_media_subtype;
            ASF_GetGUID( &i_media_subtype, &p_sp->p_type_specific_data[16] );
            msg_Dbg( p_demux, "stream(ID:%d) subtype " GUID_FMT, p_sp->i_stream_number,
                     GUID_PRINT(i_media_subtype) );

            //uint32_t i_fixed_size_samples = GetDWBE( &p_sp->p_type_specific_data[32] );
            //uint32_t i_temporal_compression = GetDWBE( &p_sp->p_type_specific_data[36] );
            //uint32_t i_sample_size = GetDWBE( &p_sp->p_type_specific_data[40] );

            vlc_guid_t i_format_type;
            ASF_GetGUID( &i_format_type, &p_sp->p_type_specific_data[44] );
            msg_Dbg( p_demux, "stream(ID:%d) format type " GUID_FMT, p_sp->i_stream_number,
                     GUID_PRINT(i_format_type) );

            //uint32_t i_format_data_size = GetDWBE( &p_sp->p_type_specific_data[60] );
            uint8_t *p_data = p_sp->p_type_specific_data + 64;
            unsigned int i_data = p_sp->i_type_specific_data_length - 64;

            msg_Dbg( p_demux, "Ext stream header detected. datasize = %d", p_sp->i_type_specific_data_length );
            if( guidcmp( &i_major_media_type, &asf_object_extended_stream_type_audio ) &&
                i_data >= sizeof( WAVEFORMATEX ) - 2)
            {
                uint16_t i_format;
                es_format_Init( &fmt, AUDIO_ES, 0 );

                i_format = GetWLE( &p_data[0] );
                if( i_format == 0 )
                    fmt.i_codec = VLC_CODEC_A52;
                else
                    wf_tag_to_fourcc( i_format, &fmt.i_codec, NULL );

                GET_CHECKED( fmt.audio.i_channels,      GetWLE( &p_data[2] ),
                                                            255, uint16_t );
                GET_CHECKED( fmt.audio.i_rate,          GetDWLE( &p_data[4] ),
                                                            UINT_MAX, uint32_t );
                GET_CHECKED( fmt.i_bitrate,             GetDWLE( &p_data[8] ) * 8,
                                                            UINT_MAX, uint32_t );
                fmt.audio.i_blockalign      = GetWLE(  &p_data[12] );
                fmt.audio.i_bitspersample   = GetWLE(  &p_data[14] );

                if( p_sp->i_type_specific_data_length > sizeof( WAVEFORMATEX ) &&
                    i_format != WAVE_FORMAT_MPEGLAYER3 &&
                    i_format != WAVE_FORMAT_MPEG && i_data >= 19 )
                {
                    GET_CHECKED( fmt.i_extra, __MIN( GetWLE( &p_data[16] ),
                                         p_sp->i_type_specific_data_length -
                                         sizeof( WAVEFORMATEX ) - 64),
                                 INT_MAX, uint32_t );
                    fmt.p_extra = malloc( fmt.i_extra );
                    if ( fmt.p_extra )
                        memcpy( fmt.p_extra, &p_data[sizeof( WAVEFORMATEX )], fmt.i_extra );
                    else
                        fmt.i_extra = 0;
                }

                msg_Dbg( p_demux, "added new audio stream (codec:%4.4s(0x%x),ID:%d)",
                    (char*)&fmt.i_codec, i_format, p_sp->i_stream_number );
            }
            else
            {
                es_format_Init( &fmt, UNKNOWN_ES, 0 );
            }
        }
        else
        {
            es_format_Init( &fmt, UNKNOWN_ES, 0 );
        }

        if( b_dvrms )
        {
            fmt.i_original_fourcc = VLC_FOURCC( 'D','V','R',' ');
            fmt.b_packetized = false;
        }

        if( fmt.i_codec == VLC_CODEC_MP4A )
            fmt.b_packetized = false;

        tk->i_cat = tk->info.i_cat = fmt.i_cat;
        if( fmt.i_cat != UNKNOWN_ES )
        {
            if( p_esp && p_languages &&
                p_esp->i_language_index < p_languages->i_language &&
                p_languages->ppsz_language[p_esp->i_language_index] )
            {
                fmt.psz_language = strdup( p_languages->ppsz_language[p_esp->i_language_index] );
                char *p;
                if( fmt.psz_language && (p = strchr( fmt.psz_language, '-' )) )
                    *p = '\0';
            }

            /* Set our priority so we won't get multiple videos */
            int i_priority = ES_PRIORITY_SELECTABLE_MIN;
            for( uint16_t i = 0; i < fmt_priorities_ex.i_count; i++ )
            {
                if ( fmt_priorities_ex.pi_stream_numbers[i] == p_sp->i_stream_number )
                {
                    i_priority = ES_PRIORITY_NOT_DEFAULTABLE;
                    break;
                }
            }
            for( uint16_t i = 0; i < fmt_priorities_bitrate_ex.i_count; i++ )
            {
                if ( fmt_priorities_bitrate_ex.pi_stream_numbers[i] == p_sp->i_stream_number )
                {
                    i_priority = ES_PRIORITY_NOT_DEFAULTABLE;
                    break;
                }
            }
            fmt.i_priority = i_priority;

            if ( i_stream <= INT_MAX )
                fmt.i_id = i_stream;
            else
                msg_Warn( p_demux, "Can't set fmt.i_id to match stream id %u", i_stream );

            if ( fmt.i_cat == VIDEO_ES )
            {
                /* Backup our video format */
                tk->p_fmt = malloc( sizeof( es_format_t ) );
                if ( tk->p_fmt )
                    es_format_Copy( tk->p_fmt, &fmt );
            }

            fmt.i_id = tk->info.p_sp->i_stream_number;

            tk->p_es = es_out_Add( p_demux->out, &fmt );

            if( !vlc_stream_Control( p_demux->s, STREAM_GET_PRIVATE_ID_STATE,
                                     (int) p_sp->i_stream_number,
                                     &b_access_selected ) &&
                b_access_selected )
            {
                p_sys->i_access_selected_track[fmt.i_cat] = p_sp->i_stream_number;
            }

        }
        else
        {
            msg_Dbg( p_demux, "ignoring unknown stream(ID:%d)",
                     p_sp->i_stream_number );
        }

        es_format_Clean( &fmt );
    }

    free( fmt_priorities_ex.pi_stream_numbers );
    free( fmt_priorities_bitrate_ex.pi_stream_numbers );

    p_sys->i_data_begin = p_sys->p_root->p_data->i_object_pos + 50;
    if( p_sys->p_root->p_data->i_object_size != 0 )
    { /* local file */
        p_sys->i_data_end = p_sys->p_root->p_data->i_object_pos +
                                    p_sys->p_root->p_data->i_object_size;
        p_sys->i_data_end = __MIN( (uint64_t)stream_Size( p_demux->s ), p_sys->i_data_end );
    }
    else
    { /* live/broacast */
        p_sys->i_data_end = 0;
    }

    /* go to first packet */
    if( vlc_stream_Seek( p_demux->s, p_sys->i_data_begin ) != VLC_SUCCESS )
        goto error;

    /* try to calculate movie time */
    if( p_sys->p_fp->i_data_packets_count > 0 )
    {
        uint64_t i_count;
        uint64_t i_size = stream_Size( p_demux->s );

        if( p_sys->i_data_end > 0 && i_size > p_sys->i_data_end )
        {
            i_size = p_sys->i_data_end;
        }

        /* real number of packets */
        i_count = ( i_size - p_sys->i_data_begin ) /
                  p_sys->p_fp->i_min_data_packet_size;

        /* calculate the time duration in micro-s */
        p_sys->i_length = VLC_TICK_FROM_MSFTIME(p_sys->p_fp->i_play_duration) *
                   (vlc_tick_t)i_count /
                   (vlc_tick_t)p_sys->p_fp->i_data_packets_count;
        if( p_sys->i_length <= p_sys->p_fp->i_preroll )
            p_sys->i_length = 0;
        else
        {
            p_sys->i_length  -= p_sys->p_fp->i_preroll;
            p_sys->i_bitrate = 8 * i_size * CLOCK_FREQ / p_sys->i_length;
        }
    }

    /* Create meta information */
    p_sys->meta = vlc_meta_New();

    asf_object_content_description_t *p_cd;
    if( ( p_cd = ASF_FindObject( p_sys->p_root->p_hdr,
                                 &asf_object_content_description_guid, 0 ) ) )
    {
        if( p_cd->psz_title && *p_cd->psz_title )
        {
            vlc_meta_SetTitle( p_sys->meta, p_cd->psz_title );
        }
        if( p_cd->psz_artist && *p_cd->psz_artist )
        {
             vlc_meta_SetArtist( p_sys->meta, p_cd->psz_artist );
        }
        if( p_cd->psz_copyright && *p_cd->psz_copyright )
        {
            vlc_meta_SetCopyright( p_sys->meta, p_cd->psz_copyright );
        }
        if( p_cd->psz_description && *p_cd->psz_description )
        {
            vlc_meta_SetDescription( p_sys->meta, p_cd->psz_description );
        }
        if( p_cd->psz_rating && *p_cd->psz_rating )
        {
            vlc_meta_SetRating( p_sys->meta, p_cd->psz_rating );
        }
    }
    asf_object_extended_content_description_t *p_ecd;
    if( ( p_ecd = ASF_FindObject( p_sys->p_root->p_hdr,
                                 &asf_object_extended_content_description, 0 ) ) )
    {
        for( int i = 0; i < p_ecd->i_count; i++ )
        {

#define set_meta( name, vlc_type ) \
            if( p_ecd->ppsz_name[i] && !strncmp( p_ecd->ppsz_name[i], name, strlen(name) ) ) \
                vlc_meta_Set( p_sys->meta, vlc_type, p_ecd->ppsz_value[i] );

            set_meta( "WM/AlbumTitle",   vlc_meta_Album )
            else set_meta( "WM/TrackNumber",  vlc_meta_TrackNumber )
            else set_meta( "WM/Year",         vlc_meta_Date )
            else set_meta( "WM/Genre",        vlc_meta_Genre )
            else set_meta( "WM/Genre",        vlc_meta_Genre )
            else set_meta( "WM/AlbumArtist",  vlc_meta_AlbumArtist )
            else set_meta( "WM/Publisher",    vlc_meta_Publisher )
            else set_meta( "WM/PartOfSet",    vlc_meta_DiscNumber )
            else if( p_ecd->ppsz_value[i] != NULL && p_ecd->ppsz_name[i] &&
                    *p_ecd->ppsz_value[i] != '\0' && /* no empty value */
                    *p_ecd->ppsz_value[i] != '{'  && /* no guid value */
                    *p_ecd->ppsz_name[i] != '{' )    /* no guid name */
                    vlc_meta_AddExtra( p_sys->meta, p_ecd->ppsz_name[i], p_ecd->ppsz_value[i] );
            /* TODO map WM/Composer, WM/Provider, WM/PartOfSet, PeakValue, AverageLevel  */
#undef set_meta
        }
    }

    /// \tood Fix Child meta for ASF tracks
#if 0
    for( i_stream = 0, i = 0; i < MAX_ASF_TRACKS; i++ )
    {
        asf_object_codec_list_t *p_cl = ASF_FindObject( p_sys->p_root->p_hdr,
                                                        &asf_object_codec_list_guid, 0 );

        if( p_sys->track[i] )
        {
            vlc_meta_t *tk = vlc_meta_New();
            TAB_APPEND( p_sys->meta->i_track, p_sys->meta->track, tk );

            if( p_cl && i_stream < p_cl->i_codec_entries_count )
            {
                if( p_cl->codec[i_stream].psz_name &&
                    *p_cl->codec[i_stream].psz_name )
                {
                    vlc_meta_Add( tk, VLC_META_CODEC_NAME,
                                  p_cl->codec[i_stream].psz_name );
                }
                if( p_cl->codec[i_stream].psz_description &&
                    *p_cl->codec[i_stream].psz_description )
                {
                    vlc_meta_Add( tk, VLC_META_CODEC_DESCRIPTION,
                                  p_cl->codec[i_stream].psz_description );
                }
            }
            i_stream++;
        }
    }
#endif

    p_sys->packet_sys.pi_preroll = &p_sys->p_fp->i_preroll;
    p_sys->packet_sys.pi_preroll_start = &p_sys->i_preroll_start;

    return VLC_SUCCESS;

error:
    DemuxEnd( p_demux );
    return VLC_EGENERIC;
}

/*****************************************************************************
 * FlushQueues: flushes tail packets and send queues
 *****************************************************************************/
static void FlushQueue( asf_track_t *tk )
{
    if( tk->info.p_frame )
    {
        block_ChainRelease( tk->info.p_frame );
        tk->info.p_frame = NULL;
    }
    if( tk->queue.p_first )
    {
        block_ChainRelease( tk->queue.p_first );
        tk->queue.p_first = NULL;
        tk->queue.pp_last = &tk->queue.p_first;
    }
}

static void FlushQueues( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    for ( unsigned int i = 0; i < MAX_ASF_TRACKS; i++ )
    {
        asf_track_t *tk = p_sys->track[i];
        if( !tk )
            continue;
        FlushQueue( tk );
    }
}

/*****************************************************************************
 *
 *****************************************************************************/
static void DemuxEnd( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    if( p_sys->p_root )
    {
        ASF_FreeObjectRoot( p_demux->s, p_sys->p_root );
        p_sys->p_root = NULL;
        p_sys->p_fp = NULL;
    }
    if( p_sys->meta )
    {
        vlc_meta_Delete( p_sys->meta );
        p_sys->meta = NULL;
    }

    FlushQueues( p_demux );

    for( int i = 0; i < MAX_ASF_TRACKS; i++ )
    {
        asf_track_t *tk = p_sys->track[i];

        if( tk )
        {
            if( tk->p_es )
            {
                es_out_Del( p_demux->out, tk->p_es );
            }
            if ( tk->p_fmt )
            {
                es_format_Clean( tk->p_fmt );
                free( tk->p_fmt );
            }
            free( tk );
        }
        p_sys->track[i] = 0;
    }
}

