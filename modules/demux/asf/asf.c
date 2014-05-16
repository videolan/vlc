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
#include <vlc_input.h>
#include <vlc_vout.h>

#include <limits.h>

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
static void FlushRemainingPackets( demux_t *p_demux );

#define MAX_ASF_TRACKS 128
#define ASF_PREROLL_FROM_CURRENT -1

typedef struct
{
    int i_cat;

    es_out_id_t     *p_es;
    es_format_t     *p_fmt; /* format backup for video changes */
    bool             b_selected;

    asf_object_stream_properties_t *p_sp;
    asf_object_extended_stream_properties_t *p_esp;

    mtime_t          i_time; /* track time*/

    block_t         *p_frame; /* use to gather complete frame */
} asf_track_t;

struct demux_sys_t
{
    mtime_t             i_time;     /* s */
    mtime_t             i_length;   /* length of file file */
    uint64_t            i_bitrate;  /* global file bitrate */

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

    mtime_t             i_preroll_start;

    vlc_meta_t          *meta;
};

static mtime_t  GetMoviePTS( demux_sys_t * );
static int      DemuxInit( demux_t * );
static void     DemuxEnd( demux_t * );
static int      DemuxPacket( demux_t * );

/*****************************************************************************
 * Open: check file and initializes ASF structures
 *****************************************************************************/
static int Open( vlc_object_t * p_this )
{
    demux_t     *p_demux = (demux_t *)p_this;
    demux_sys_t *p_sys;
    guid_t      guid;
    const uint8_t     *p_peek;

    /* A little test to see if it could be a asf stream */
    if( stream_Peek( p_demux->s, &p_peek, 16 ) < 16 ) return VLC_EGENERIC;

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
    return VLC_SUCCESS;
}

/*****************************************************************************
 * Demux: read packet and send them to decoders
 *****************************************************************************/
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

    for( ;; )
    {
        const uint8_t *p_peek;
        mtime_t i_length;
        mtime_t i_time_begin = GetMoviePTS( p_sys );
        int i_result;

        if( !vlc_object_alive (p_demux) )
            break;
#if 0
        /* FIXME: returns EOF too early for some mms streams */
        if( p_sys->i_data_end >= 0 &&
                stream_Tell( p_demux->s ) >= p_sys->i_data_end )
            return 0; /* EOF */
#endif

        /* Check if we have concatenated files */
        if( stream_Peek( p_demux->s, &p_peek, 16 ) == 16 )
        {
            guid_t guid;

            ASF_GetGUID( &guid, p_peek );
            if( guidcmp( &guid, &asf_object_header_guid ) )
            {
                msg_Warn( p_demux, "found a new ASF header" );
                /* We end this stream */
                DemuxEnd( p_demux );

                /* And we prepare to read the next one */
                if( DemuxInit( p_demux ) )
                {
                    msg_Err( p_demux, "failed to load the new header" );
                    dialog_Fatal( p_demux, _("Could not demux ASF stream"), "%s",
                                    _("VLC failed to load the ASF header.") );
                    return 0;
                }
                es_out_Control( p_demux->out, ES_OUT_RESET_PCR );
                continue;
            }
        }

        /* Read and demux a packet */
        if( ( i_result = DemuxPacket( p_demux ) ) <= 0 )
        {
            FlushRemainingPackets( p_demux );
            return i_result;
        }
        if( i_time_begin == -1 )
        {
            i_time_begin = GetMoviePTS( p_sys );
        }
        else
        {
            i_length = GetMoviePTS( p_sys ) - i_time_begin;
            if( i_length < 0 || i_length >= 40 * 1000 ) break;
        }
    }

    /* Set the PCR */
    /* WARN: Don't move it before the end of the whole chunk */
    p_sys->i_time = GetMoviePTS( p_sys );
    if( p_sys->i_time >= 0 )
    {
#ifdef ASF_DEBUG
        msg_Dbg( p_demux, "Demux Loop Setting PCR to %"PRId64, VLC_TS_0 + p_sys->i_time );
#endif
        es_out_Control( p_demux->out, ES_OUT_SET_PCR, VLC_TS_0 + p_sys->i_time );
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
            if ( tk && tk->p_sp && tk->i_cat == VIDEO_ES && tk->b_selected )
            {
                p_sys->i_seek_track = tk->p_sp->i_stream_number;
                break;
            }
        }
    }

    if ( p_sys->i_seek_track )
    {
        /* Skip forward at least 1 min */
        asf_track_t *tk = p_sys->track[p_sys->i_seek_track];
        if ( tk->p_esp && tk->p_esp->i_average_time_per_frame )
        {
            /* 1 min if fastseek, otherwise 5 sec */
            /* That's a guess for bandwidth */
            uint64_t i_maxwaittime = ( p_sys->b_canfastseek ) ? 600000000 : 50000000;
            i_maxwaittime /= tk->p_esp->i_average_time_per_frame;
            p_sys->i_wait_keyframe = __MIN( i_maxwaittime, UINT_MAX );
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

static int SeekIndex( demux_t *p_demux, mtime_t i_date, float f_pos )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    asf_object_index_t *p_index;

    msg_Dbg( p_demux, "seek with index: %i seconds, position %f",
             i_date >= 0 ? (int)(i_date/1000000) : -1, f_pos );

    if( i_date < 0 )
        i_date = p_sys->i_length * f_pos;

    p_sys->i_preroll_start = i_date - (int64_t) p_sys->p_fp->i_preroll;
    if ( p_sys->i_preroll_start < 0 ) p_sys->i_preroll_start = 0;

    p_index = ASF_FindObject( p_sys->p_root, &asf_object_simple_index_guid, 0 );

    uint64_t i_entry = p_sys->i_preroll_start * 10 / p_index->i_index_entry_time_interval;
    if( i_entry >= p_index->i_index_entry_count )
    {
        msg_Warn( p_demux, "Incomplete index" );
        return VLC_EGENERIC;
    }

    WaitKeyframe( p_demux );

    uint64_t i_offset = (uint64_t)p_index->index_entry[i_entry].i_packet_number *
                        p_sys->p_fp->i_min_data_packet_size;

    if ( stream_Seek( p_demux->s, i_offset + p_sys->i_data_begin ) == VLC_SUCCESS )
    {
        es_out_Control( p_demux->out, ES_OUT_SET_NEXT_DISPLAY_TIME, VLC_TS_0 + i_date );
        return VLC_SUCCESS;
    }
    else return VLC_EGENERIC;
}

static void SeekPrepare( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    p_sys->i_time = VLC_TS_INVALID;
    p_sys->i_preroll_start = ASF_PREROLL_FROM_CURRENT;
    for( int i = 0; i < MAX_ASF_TRACKS ; i++ )
    {
        asf_track_t *tk = p_sys->track[i];
        if( !tk )
            continue;

        tk->i_time = -1;
        if( tk->p_frame )
            block_ChainRelease( tk->p_frame );
        tk->p_frame = NULL;
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
    int64_t     i64, *pi64;
    int         i;
    double      f, *pf;

    switch( i_query )
    {
    case DEMUX_GET_LENGTH:
        pi64 = (int64_t*)va_arg( args, int64_t * );
        *pi64 = p_sys->i_length;
        return VLC_SUCCESS;

    case DEMUX_GET_TIME:
        pi64 = (int64_t*)va_arg( args, int64_t * );
        if( p_sys->i_time < 0 ) return VLC_EGENERIC;
        *pi64 = p_sys->i_time;
        return VLC_SUCCESS;

    case DEMUX_SET_TIME:
        if ( p_sys->p_fp &&
             ! ( p_sys->p_fp->i_flags & ASF_FILE_PROPERTIES_SEEKABLE ) )
            return VLC_EGENERIC;

        SeekPrepare( p_demux );

        if( p_sys->b_index && p_sys->i_length > 0 )
        {
            va_list acpy;
            va_copy( acpy, args );
            i64 = (int64_t)va_arg( acpy, int64_t );
            va_end( acpy );

            if( !SeekIndex( p_demux, i64, -1 ) )
                return VLC_SUCCESS;
        }
        return SeekPercent( p_demux, i_query, args );

    case DEMUX_SET_ES:
    {
        i = (int)va_arg( args, int );
        int i_ret;
        if ( i >= 0 )
        {
            i++; /* video/audio-es variable starts 0 */
            msg_Dbg( p_demux, "Requesting access to enable stream %d", i );
            i_ret = stream_Control( p_demux->s, STREAM_SET_PRIVATE_ID_STATE, i, true );
        }
        else
        {  /* i contains -1 * es_category */
            msg_Dbg( p_demux, "Requesting access to disable stream %d", i );
            i_ret = stream_Control( p_demux->s, STREAM_SET_PRIVATE_ID_STATE, i, false );
        }

        if ( i_ret == VLC_SUCCESS )
        {
            SeekPrepare( p_demux );
            p_sys->i_seek_track = 0;
            WaitKeyframe( p_demux );
        }
        return i_ret;
    }

    case DEMUX_GET_POSITION:
        if( p_sys->i_time < 0 ) return VLC_EGENERIC;
        if( p_sys->i_length > 0 )
        {
            pf = (double*)va_arg( args, double * );
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
        if ( p_sys->p_fp &&
             ! ( p_sys->p_fp->i_flags & ASF_FILE_PROPERTIES_SEEKABLE ) )
            return VLC_EGENERIC;

        SeekPrepare( p_demux );

        if( p_sys->b_index && p_sys->i_length > 0 )
        {
            va_list acpy;
            va_copy( acpy, args );
            f = (double)va_arg( acpy, double );
            va_end( acpy );

            if( !SeekIndex( p_demux, -1, f ) )
                return VLC_SUCCESS;
        }
        return SeekPercent( p_demux, i_query, args );

    case DEMUX_GET_META:
        p_meta = (vlc_meta_t*)va_arg( args, vlc_meta_t* );
        vlc_meta_Merge( p_meta, p_sys->meta );
        return VLC_SUCCESS;

    case DEMUX_CAN_SEEK:
        if ( p_sys->p_fp &&
             ! ( p_sys->p_fp->i_flags & ASF_FILE_PROPERTIES_SEEKABLE ) )
        {
            bool *pb_bool = (bool*)va_arg( args, bool * );
            *pb_bool = false;
            return VLC_SUCCESS;
        }
        // ft

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
static mtime_t GetMoviePTS( demux_sys_t *p_sys )
{
    mtime_t i_time = -1;
    int     i;
    /* As some tracks might have been deselected by access, the PCR might
     * stop updating */
    for( i = 0; i < MAX_ASF_TRACKS ; i++ )
    {
        const asf_track_t *tk = p_sys->track[i];

        if( tk && tk->p_es && tk->b_selected )
        {
            /* Skip discrete tracks */
            if ( tk->i_cat != VIDEO_ES && tk->i_cat != AUDIO_ES )
                continue;

            /* We need to have all ES seen once, as they might have lower DTS */
            if ( tk->i_time + (int64_t)p_sys->p_fp->i_preroll * 1000 < 0 )
            {
                /* early fail */
                return -1;
            }
            else if ( tk->i_time > -1 && ( i_time == -1 || i_time > tk->i_time ) )
            {
                i_time = tk->i_time;
            }
        }
    }

    return i_time;
}

static inline int GetValue2b(uint32_t *var, const uint8_t *p, unsigned int *skip, int left, int bits)
{
    switch(bits&0x03)
    {
    case 1:
        if (left < 1)
            return -1;
        *var = p[*skip]; *skip += 1;
        return 0;
    case 2:
        if (left < 2)
            return -1;
        *var = GetWLE(&p[*skip]); *skip += 2;
        return 0;
    case 3:
        if (left < 4)
            return -1;
        *var = GetDWLE(&p[*skip]); *skip += 4;
        return 0;
    case 0:
    default:
        return 0;
    }
}

struct asf_packet_t
{
    uint32_t property;
    uint32_t length;
    uint32_t padding_length;
    uint32_t send_time;
    bool multiple;
    int length_type;

    /* buffer handling for this ASF packet */
    uint32_t i_skip;
    const uint8_t *p_peek;
    uint32_t left;
};

static void SendPacket(demux_t *p_demux, asf_track_t *tk)
{
    demux_sys_t *p_sys = p_demux->p_sys;

    block_t *p_gather = block_ChainGather( tk->p_frame );

    if( p_sys->i_time < VLC_TS_0 && tk->i_time > VLC_TS_INVALID )
    {
        p_sys->i_time = tk->i_time;
        es_out_Control( p_demux->out, ES_OUT_SET_PCR, VLC_TS_0 + p_sys->i_time );
#ifdef ASF_DEBUG
        msg_Dbg( p_demux, "    setting PCR to %"PRId64, VLC_TS_0 + p_sys->i_time );
#endif
    }

#ifdef ASF_DEBUG
    msg_Dbg( p_demux, "    sending packet dts %"PRId64" pts %"PRId64" pcr %"PRId64, p_gather->i_dts, p_gather->i_pts, p_sys->i_time );
#endif
    es_out_Send( p_demux->out, tk->p_es, p_gather );
    tk->p_frame = NULL;
}

static int DemuxSubPayload(demux_t *p_demux, asf_track_t *tk,
        uint32_t i_sub_payload_data_length, mtime_t i_pts, mtime_t i_dts,
        uint32_t i_media_object_offset, bool b_keyframe )
{
    /* FIXME I don't use i_media_object_number, sould I ? */
    if( tk->p_frame && i_media_object_offset == 0 )
        SendPacket(p_demux, tk);

    block_t *p_frag = stream_Block( p_demux->s, i_sub_payload_data_length );
    if( p_frag == NULL ) {
        msg_Warn( p_demux, "cannot read data" );
        return -1;
    }

    p_frag->i_pts = VLC_TS_0 + i_pts;
    p_frag->i_dts = VLC_TS_0 + i_dts;
    if ( b_keyframe )
        p_frag->i_flags |= BLOCK_FLAG_TYPE_I;

    block_ChainAppend( &tk->p_frame, p_frag );

    return 0;
}

static uint32_t SkipBytes( stream_t *s, uint32_t i_bytes )
{
    int i_read;
    int i_to_read = __MIN(i_bytes, INT_MAX);
    uint32_t i_bytes_read = 0;

    while( i_bytes )
    {
        i_read = stream_Read( s, NULL, i_to_read );
        i_bytes -= i_read;
        i_bytes_read += i_read;
        if ( i_read < i_to_read || i_bytes == 0 )
        {
            /* end of stream */
            return i_bytes_read;
        }
        i_to_read = __MIN(i_bytes, INT_MAX);
    }

    return i_bytes_read;
}

static void ParsePayloadExtensions(demux_t *p_demux, asf_track_t *tk,
                                   const struct asf_packet_t *pkt,
                                   uint32_t i_length, bool *b_keyframe )
{
    if ( !tk || !tk->p_esp || !tk->p_esp->p_ext ) return;
    const uint8_t *p_data = pkt->p_peek + pkt->i_skip + 8;
    i_length -= 8;
    uint16_t i_payload_extensions_size;
    asf_payload_extension_system_t *p_ext = NULL;

    /* Extensions always come in the declared order */
    for ( int i=0; i< tk->p_esp->i_payload_extension_system_count; i++ )
    {
        p_ext = &tk->p_esp->p_ext[i];
        if ( p_ext->i_data_size == 0xFFFF ) /* Variable length extension data */
        {
            if ( i_length < 2 ) return;
            i_payload_extensions_size = GetWLE( p_data );
            p_data += 2;
            i_length -= 2;
            i_payload_extensions_size = 0;
        }
        else
        {
            i_payload_extensions_size = p_ext->i_data_size;
        }

        if ( i_length < i_payload_extensions_size ) return;

        if ( guidcmp( &p_ext->i_extension_id, &mfasf_sampleextension_outputcleanpoint_guid ) )
        {
            if ( i_payload_extensions_size != sizeof(uint8_t) ) goto sizeerror;
            *b_keyframe |= *p_data;
        }
        else if ( guidcmp( &p_ext->i_extension_id, &asf_dvr_sampleextension_videoframe_guid ) )
        {
            if ( i_payload_extensions_size != sizeof(uint32_t) ) goto sizeerror;

            uint32_t i_val = GetDWLE( p_data );
            /* Valid keyframe must be a split frame start fragment */
            *b_keyframe = i_val & ASF_EXTENSION_VIDEOFRAME_NEWFRAME;
            if ( *b_keyframe )
            {
                /* And flagged as IFRAME */
                *b_keyframe |= ( ( i_val & ASF_EXTENSION_VIDEOFRAME_TYPE_MASK )
                                 == ASF_EXTENSION_VIDEOFRAME_IFRAME );
            }
        }
        else if ( guidcmp( &p_ext->i_extension_id, &mfasf_sampleextension_pixelaspectratio_guid ) )
        {
            if ( i_payload_extensions_size != sizeof(uint16_t) ) goto sizeerror;

            uint8_t i_ratio_x = *p_data;
            uint8_t i_ratio_y = *(p_data + 1);
            if ( tk->p_fmt->video.i_sar_num != i_ratio_x || tk->p_fmt->video.i_sar_den != i_ratio_y )
            {
                /* Only apply if origin pixel size >= 1x1, due to broken yacast */
                if ( tk->p_fmt->video.i_height * i_ratio_x > tk->p_fmt->video.i_width * i_ratio_y )
                {
                    vout_thread_t *p_vout = input_GetVout( p_demux->p_input );
                    if ( p_vout )
                    {
                        msg_Info( p_demux, "Changing aspect ratio to %i/%i", i_ratio_x, i_ratio_y );
                        vout_ChangeAspectRatio( p_vout, i_ratio_x, i_ratio_y );
                        vlc_object_release( p_vout );
                    }
                }
                tk->p_fmt->video.i_sar_num = i_ratio_x;
                tk->p_fmt->video.i_sar_den = i_ratio_y;
            }
        }
        i_length -= i_payload_extensions_size;
        p_data += i_payload_extensions_size;
    }

    return;

sizeerror:
    msg_Warn( p_demux, "Unknown extension " GUID_FMT " data size of %u",
              GUID_PRINT( p_ext->i_extension_id ), i_payload_extensions_size );
}

static int DemuxPayload(demux_t *p_demux, struct asf_packet_t *pkt, int i_payload)
{
#ifndef ASF_DEBUG
    VLC_UNUSED( i_payload );
#endif
    demux_sys_t *p_sys = p_demux->p_sys;

    if( ! pkt->left || pkt->i_skip >= pkt->left )
        return -1;

    bool b_packet_keyframe = pkt->p_peek[pkt->i_skip] >> 7;
    uint8_t i_stream_number = pkt->p_peek[pkt->i_skip++] & 0x7f;

    uint32_t i_media_object_number = 0;
    if (GetValue2b(&i_media_object_number, pkt->p_peek, &pkt->i_skip, pkt->left - pkt->i_skip, pkt->property >> 4) < 0)
        return -1;
    uint32_t i_media_object_offset = 0;
    if (GetValue2b(&i_media_object_offset, pkt->p_peek, &pkt->i_skip, pkt->left - pkt->i_skip, pkt->property >> 2) < 0)
        return -1;
    uint32_t i_replicated_data_length = 0;
    if (GetValue2b(&i_replicated_data_length, pkt->p_peek, &pkt->i_skip, pkt->left - pkt->i_skip, pkt->property) < 0)
        return -1;

    mtime_t i_base_pts;
    uint8_t i_pts_delta = 0;
    uint32_t i_payload_data_length = 0;
    uint32_t i_temp_payload_length = 0;
    p_sys->p_fp->i_preroll = __MIN( p_sys->p_fp->i_preroll, INT64_MAX );

    /* First packet, in case we do not have index to guess preroll start time */
    if ( p_sys->i_preroll_start == ASF_PREROLL_FROM_CURRENT )
        p_sys->i_preroll_start = pkt->send_time * 1000;

    /* Non compressed */
    if( i_replicated_data_length > 7 ) // should be at least 8 bytes
    {
        /* Followed by 2 optional DWORDS, offset in media and presentation time */
        i_base_pts = (mtime_t)GetDWLE( pkt->p_peek + pkt->i_skip + 4 );

        /* Parsing extensions, See 7.3.1 */
        ParsePayloadExtensions( p_demux, p_sys->track[i_stream_number], pkt,
                                i_replicated_data_length, &b_packet_keyframe );
        i_base_pts -= p_sys->p_fp->i_preroll;
        pkt->i_skip += i_replicated_data_length;

        if( ! pkt->left || pkt->i_skip >= pkt->left )
            return -1;
    }
    else if ( i_replicated_data_length == 0 )
    {
        /* optional DWORDS missing */
        i_base_pts = (mtime_t)pkt->send_time;
        i_base_pts -= p_sys->p_fp->i_preroll;
    }
    /* Compressed payload */
    else if( i_replicated_data_length == 1 )
    {
        /* i_media_object_offset is presentation time */
        /* Next byte is Presentation Time Delta */
        i_pts_delta = pkt->p_peek[pkt->i_skip];
        i_base_pts = (mtime_t)i_media_object_offset;
        i_base_pts -= p_sys->p_fp->i_preroll;
        pkt->i_skip++;
        i_media_object_offset = 0;
    }
    else
    {
        /* >1 && <8 Invalid replicated length ! */
        msg_Warn( p_demux, "Invalid replicated data length detected." );
        i_payload_data_length = pkt->length - pkt->padding_length - pkt->i_skip;
        goto skip;
    }

    bool b_preroll_done = ( pkt->send_time > (p_sys->i_preroll_start/1000 + p_sys->p_fp->i_preroll) );

    if (i_base_pts < 0) i_base_pts = 0; // FIXME?
    i_base_pts *= 1000;

    if( pkt->multiple ) {
        if (GetValue2b(&i_temp_payload_length, pkt->p_peek, &pkt->i_skip, pkt->left - pkt->i_skip, pkt->length_type) < 0)
            return -1;
    } else
        i_temp_payload_length = pkt->length - pkt->padding_length - pkt->i_skip;

    i_payload_data_length = i_temp_payload_length;

#ifdef ASF_DEBUG
     msg_Dbg( p_demux,
              "payload(%d) stream_number:%"PRIu8" media_object_number:%d media_object_offset:%"PRIu32" replicated_data_length:%"PRIu32" payload_data_length %"PRIu32,
              i_payload + 1, i_stream_number, i_media_object_number,
              i_media_object_offset, i_replicated_data_length, i_payload_data_length );
     msg_Dbg( p_demux,
              "   pts=%"PRId64" st=%"PRIu32, i_base_pts, pkt->send_time );
#endif

     if( ! i_payload_data_length || i_payload_data_length > pkt->left )
     {
         msg_Dbg( p_demux, "  payload length problem %d %"PRIu32" %"PRIu32, pkt->multiple, i_payload_data_length, pkt->left );
         return -1;
     }

    asf_track_t *tk = p_sys->track[i_stream_number];
    if( tk == NULL )
    {
        msg_Warn( p_demux, "undeclared stream[Id 0x%x]", i_stream_number );
        goto skip;
    }

    if( p_sys->i_wait_keyframe )
    {
        if ( i_stream_number == p_sys->i_seek_track )
        {
            if ( !b_packet_keyframe )
            {
                p_sys->i_wait_keyframe--;
                goto skip;
            }
            else
                p_sys->i_wait_keyframe = 0;
        }
        else
            goto skip;
    }

    if( !tk->p_es )
        goto skip;

    bool b_hugedelay = ( p_sys->p_fp->i_preroll * 1000 > CLOCK_FREQ * 3 );

    if ( b_preroll_done || b_hugedelay )
    {
        if ( !b_hugedelay )
        {
            tk->i_time = INT64_C(1000) * pkt->send_time;
            tk->i_time -= p_sys->p_fp->i_preroll * 1000;
            tk->i_time -= tk->p_sp->i_time_offset * 10;
        }
        else
            tk->i_time = i_base_pts;
    }

    uint32_t i_subpayload_count = 0;
    while (i_payload_data_length)
    {
        uint32_t i_sub_payload_data_length = i_payload_data_length;
        if( i_replicated_data_length == 1 )
        {
            i_sub_payload_data_length = pkt->p_peek[pkt->i_skip++];
            i_payload_data_length--;
        }

        SkipBytes( p_demux->s, pkt->i_skip );

        mtime_t i_payload_pts = i_base_pts + (mtime_t)i_pts_delta * i_subpayload_count * 1000;
        i_payload_pts -= tk->p_sp->i_time_offset * 10;
        mtime_t i_payload_dts = INT64_C(1000) * pkt->send_time;
        i_payload_dts -= p_sys->p_fp->i_preroll * 1000;
        i_payload_dts -= tk->p_sp->i_time_offset * 10;

        if ( b_hugedelay )
            i_payload_dts = i_base_pts;

        if ( i_sub_payload_data_length &&
             DemuxSubPayload(p_demux, tk, i_sub_payload_data_length,
                        i_payload_pts, i_payload_dts, i_media_object_offset,
                        b_packet_keyframe ) < 0)
            return -1;

        if ( pkt->left > pkt->i_skip + i_sub_payload_data_length )
            pkt->left -= pkt->i_skip + i_sub_payload_data_length;
        else
            pkt->left = 0;
        pkt->i_skip = 0;
        if( pkt->left > 0 )
        {
            int i_return = stream_Peek( p_demux->s, &pkt->p_peek, __MIN(pkt->left, INT_MAX) );
            if ( i_return <= 0 || (unsigned int) i_return < __MIN(pkt->left, INT_MAX) )
            {
            msg_Warn( p_demux, "cannot peek, EOF ?" );
            return -1;
            }
        }

        if ( i_sub_payload_data_length <= i_payload_data_length )
            i_payload_data_length -= i_sub_payload_data_length;
        else
            i_payload_data_length = 0;

        i_subpayload_count++;
    }

    return 0;

skip:
    pkt->i_skip += i_payload_data_length;
    return 0;
}

static int DemuxPacket( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    uint32_t i_data_packet_min = p_sys->p_fp->i_min_data_packet_size;

    const uint8_t *p_peek;
    int i_return = stream_Peek( p_demux->s, &p_peek,i_data_packet_min );
    if( i_return <= 0 || ((unsigned int) i_return) < i_data_packet_min )
    {
        msg_Warn( p_demux, "cannot peek while getting new packet, EOF ?" );
        return 0;
    }
    unsigned int i_skip = 0;

    /* *** parse error correction if present *** */
    if( p_peek[0]&0x80 )
    {
        unsigned int i_error_correction_data_length = p_peek[0] & 0x0f;
        unsigned int i_opaque_data_present = ( p_peek[0] >> 4 )& 0x01;
        unsigned int i_error_correction_length_type = ( p_peek[0] >> 5 ) & 0x03;
        i_skip += 1; // skip error correction flags

        if( i_error_correction_length_type != 0x00 ||
            i_opaque_data_present != 0 ||
            i_error_correction_data_length != 0x02 )
        {
            goto loop_error_recovery;
        }

        i_skip += i_error_correction_data_length;
    }
    else
        msg_Warn( p_demux, "no error correction" );

    /* sanity check */
    if( i_skip + 2 >= i_data_packet_min )
        goto loop_error_recovery;

    struct asf_packet_t pkt;
    int i_packet_flags = p_peek[i_skip]; i_skip++;
    pkt.property = p_peek[i_skip]; i_skip++;
    pkt.multiple = !!(i_packet_flags&0x01);

    pkt.length = i_data_packet_min;
    pkt.padding_length = 0;

    if (GetValue2b(&pkt.length, p_peek, &i_skip, i_data_packet_min - i_skip, i_packet_flags >> 5) < 0)
        goto loop_error_recovery;
    uint32_t i_packet_sequence;
    if (GetValue2b(&i_packet_sequence, p_peek, &i_skip, i_data_packet_min - i_skip, i_packet_flags >> 1) < 0)
        goto loop_error_recovery;
    if (GetValue2b(&pkt.padding_length, p_peek, &i_skip, i_data_packet_min - i_skip, i_packet_flags >> 3) < 0)
        goto loop_error_recovery;

    if( pkt.padding_length > pkt.length )
    {
        msg_Warn( p_demux, "Too large padding: %"PRIu32, pkt.padding_length );
        goto loop_error_recovery;
    }

    if( pkt.length < i_data_packet_min )
    {
        /* if packet length too short, there is extra padding */
        pkt.padding_length += i_data_packet_min - pkt.length;
        pkt.length = i_data_packet_min;
    }

    pkt.send_time = GetDWLE( p_peek + i_skip ); i_skip += 4;
    /* uint16_t i_packet_duration = GetWLE( p_peek + i_skip ); */ i_skip += 2;

    i_return = stream_Peek( p_demux->s, &p_peek, pkt.length );
    if( i_return <= 0 || pkt.length == 0 || (unsigned int)i_return < pkt.length )
    {
        msg_Warn( p_demux, "cannot peek, EOF ?" );
        return 0;
    }

    int i_payload_count = 1;
    pkt.length_type = 0x02; //unused
    if( pkt.multiple )
    {
        i_payload_count = p_peek[i_skip] & 0x3f;
        pkt.length_type = ( p_peek[i_skip] >> 6 )&0x03;
        i_skip++;
    }

#ifdef ASF_DEBUG
    msg_Dbg(p_demux, "%d payloads", i_payload_count);
#endif

    pkt.i_skip = i_skip;
    pkt.p_peek = p_peek;
    pkt.left = pkt.length;

    for( int i_payload = 0; i_payload < i_payload_count ; i_payload++ )
        if (DemuxPayload(p_demux, &pkt, i_payload) < 0)
        {
            msg_Warn( p_demux, "payload err %d / %d", i_payload + 1, i_payload_count );
            return 0;
        }

    if( pkt.left > 0 )
    {
#ifdef ASF_DEBUG
        if( pkt.left > pkt.padding_length )
            msg_Warn( p_demux, "Didn't read %"PRIu32" bytes in the packet",
                            pkt.left - pkt.padding_length );
        else if( pkt.left < pkt.padding_length )
            msg_Warn( p_demux, "Read %"PRIu32" too much bytes in the packet",
                            pkt.padding_length - pkt.left );
#endif
        int i_return = stream_Read( p_demux->s, NULL, pkt.left );
        if( i_return < 0 || (unsigned int) i_return < pkt.left )
        {
            msg_Err( p_demux, "cannot skip data, EOF ?" );
            return 0;
        }
    }

    return 1;

loop_error_recovery:
    msg_Warn( p_demux, "unsupported packet header" );
    if( p_sys->p_fp->i_min_data_packet_size != p_sys->p_fp->i_max_data_packet_size )
    {
        msg_Err( p_demux, "unsupported packet header, fatal error" );
        return -1;
    }
    i_return = stream_Read( p_demux->s, NULL, i_data_packet_min );
    if( i_return <= 0 || (unsigned int) i_return != i_data_packet_min )
    {
        msg_Warn( p_demux, "cannot skip data, EOF ?" );
        return 0;
    }

    return 1;
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
    p_prios->pi_stream_numbers = malloc( (size_t)p_sys->i_track * sizeof(uint16_t) );
    if ( !p_prios->pi_stream_numbers ) return;

    if ( p_mutex->i_stream_number_count )
    {
        /* Just set highest prio on highest in the group */
        for ( uint16_t i = 1; i < p_mutex->i_stream_number_count; i++ )
        {
            if ( p_prios->i_count > p_sys->i_track ) break;
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
    p_prios->pi_stream_numbers = malloc( (size_t)p_sys->i_track * sizeof( uint16_t ) );
    if ( !p_prios->pi_stream_numbers ) return;

    if ( p_bitrate_mutex->i_stream_number_count )
    {
        /* Just remove < highest */
        for ( uint16_t i = 1; i < p_bitrate_mutex->i_stream_number_count; i++ )
        {
            if ( p_prios->i_count > p_sys->i_track ) break;
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
    p_sys->i_time   = -1;
    p_sys->i_length = 0;
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
    p_sys->meta         = NULL;

    /* Now load all object ( except raw data ) */
    stream_Control( p_demux->s, STREAM_CAN_FASTSEEK, &p_sys->b_canfastseek );
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
        dialog_Fatal( p_demux, _("Could not demux ASF stream"), "%s",
                        _("DRM protected streams are not supported.") );
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

        tk = p_sys->track[p_sp->i_stream_number] = malloc( sizeof( asf_track_t ) );
        memset( tk, 0, sizeof( asf_track_t ) );

        tk->i_time = -1;
        tk->p_sp = p_sp;
        tk->p_es = NULL;
        tk->p_esp = NULL;
        tk->p_frame = NULL;

        if ( strncmp( p_demux->psz_access, "mms", 3 ) )
        {
            /* Check (not mms) if this track is selected (ie will receive data) */
            if( !stream_Control( p_demux->s, STREAM_GET_PRIVATE_ID_STATE,
                                 (int) p_sp->i_stream_number, &b_access_selected ) &&
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
                    tk->p_esp = p_esp;
                    break;
                }
            }
        }

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

            msg_Dbg( p_demux, "added new audio stream(codec:0x%x,ID:%d)",
                    GetWLE( p_data ), p_sp->i_stream_number );
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
                fmt.i_codec = VLC_FOURCC( 'm','p','g','2' ) ;
                fmt.b_packetized = false;
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

            msg_Dbg( p_demux, "added new video stream(ID:%d)",
                     p_sp->i_stream_number );
        }
        else if( guidcmp( &p_sp->i_stream_type, &asf_object_extended_stream_header ) &&
            p_sp->i_type_specific_data_length >= 64 )
        {
            /* Now follows a 64 byte header of which we don't know much */
            guid_t  *p_ref  = (guid_t *)p_sp->p_type_specific_data;
            uint8_t *p_data = p_sp->p_type_specific_data + 64;
            unsigned int i_data = p_sp->i_type_specific_data_length - 64;

            msg_Dbg( p_demux, "Ext stream header detected. datasize = %d", p_sp->i_type_specific_data_length );
            if( guidcmp( p_ref, &asf_object_extended_stream_type_audio ) &&
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
                fmt.b_packetized = true;

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

                msg_Dbg( p_demux, "added new audio stream (codec:0x%x,ID:%d)",
                    i_format, p_sp->i_stream_number );
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

        tk->i_cat = fmt.i_cat;
        if( fmt.i_cat != UNKNOWN_ES )
        {
            if( p_esp && p_languages &&
                p_esp->i_language_index < p_languages->i_language )
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

            tk->p_es = es_out_Add( p_demux->out, &fmt );

            if( !stream_Control( p_demux->s, STREAM_GET_PRIVATE_ID_STATE,
                                 (int) p_sp->i_stream_number, &b_access_selected ) &&
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
    stream_Seek( p_demux->s, p_sys->i_data_begin );

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
        p_sys->i_length = (mtime_t)p_sys->p_fp->i_play_duration / 10 *
                   (mtime_t)i_count /
                   (mtime_t)p_sys->p_fp->i_data_packets_count - p_sys->p_fp->i_preroll * 1000;
        if( p_sys->i_length < 0 )
            p_sys->i_length = 0;

        if( p_sys->i_length > 0 )
        {
            p_sys->i_bitrate = 8 * i_size * 1000000 / p_sys->i_length;
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
    return VLC_SUCCESS;

error:
    ASF_FreeObjectRoot( p_demux->s, p_sys->p_root );
    return VLC_EGENERIC;
}

/*****************************************************************************
 * FlushRemainingPackets: flushes tail packets
 *****************************************************************************/

static void FlushRemainingPackets( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    for ( unsigned int i = 0; i < MAX_ASF_TRACKS; i++ )
    {
        asf_track_t *tk = p_sys->track[i];
        if( tk && tk->p_frame )
            SendPacket( p_demux, tk );
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
    }
    if( p_sys->meta )
    {
        vlc_meta_Delete( p_sys->meta );
        p_sys->meta = NULL;
    }

    for( int i = 0; i < MAX_ASF_TRACKS; i++ )
    {
        asf_track_t *tk = p_sys->track[i];

        if( tk )
        {
            if( tk->p_frame )
                block_ChainRelease( tk->p_frame );

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

