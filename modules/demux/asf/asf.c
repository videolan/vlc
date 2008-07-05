/*****************************************************************************
 * asf.c : ASF demux module
 *****************************************************************************
 * Copyright (C) 2002-2003 the VideoLAN team
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
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
#include <vlc_plugin.h>
#include <vlc_demux.h>
#include <vlc_interface.h>

#include <vlc_meta.h>
#include <vlc_access.h>                /* GET_PRIVATE_ID_STATE */
#include <vlc_codecs.h>                /* BITMAPINFOHEADER, WAVEFORMATEX */
#include "libasf.h"

/* TODO
 *  - add support for the newly added object: language, bitrate,
 *                                            extended stream properties.
 */

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open  ( vlc_object_t * );
static void Close ( vlc_object_t * );

vlc_module_begin();
    set_category( CAT_INPUT );
    set_subcategory( SUBCAT_INPUT_DEMUX );
    set_description( N_("ASF v1.0 demuxer") );
    set_capability( "demux", 200 );
    set_callbacks( Open, Close );
    add_shortcut( "asf" );
vlc_module_end();


/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int Demux  ( demux_t * );
static int Control( demux_t *, int i_query, va_list args );

typedef struct
{
    int i_cat;

    es_out_id_t     *p_es;

    asf_object_stream_properties_t *p_sp;

    mtime_t i_time;

    block_t         *p_frame; /* use to gather complete frame */

} asf_track_t;

struct demux_sys_t
{
    mtime_t             i_time;     /* s */
    mtime_t             i_length;   /* length of file file */
    int64_t             i_bitrate;  /* global file bitrate */

    asf_object_root_t            *p_root;
    asf_object_file_properties_t *p_fp;

    unsigned int        i_track;
    asf_track_t         *track[128]; /* track number is stored on 7 bits */

    int64_t             i_data_begin;
    int64_t             i_data_end;

    bool          b_index;
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
    if( !ASF_CmpGUID( &guid, &asf_object_header_guid ) ) return VLC_EGENERIC;

    /* Set p_demux fields */
    p_demux->pf_demux = Demux;
    p_demux->pf_control = Control;
    p_demux->p_sys = p_sys = malloc( sizeof( demux_sys_t ) );
    memset( p_sys, 0, sizeof( demux_sys_t ) );

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
            if( ASF_CmpGUID( &guid, &asf_object_header_guid ) )
            {
                msg_Warn( p_demux, "found a new ASF header" );
                /* We end this stream */
                DemuxEnd( p_demux );

                /* And we prepare to read the next one */
                if( DemuxInit( p_demux ) )
                {
                    msg_Err( p_demux, "failed to load the new header" );
                    intf_UserFatal( p_demux, false, _("Could not demux ASF stream"),
                                    _("VLC failed to load the ASF header.") );
                    return 0;
                }
                continue;
            }
        }

        /* Read and demux a packet */
        if( ( i_result = DemuxPacket( p_demux ) ) <= 0 )
        {
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
    p_sys->i_time = GetMoviePTS( p_sys );
    if( p_sys->i_time >= 0 )
    {
        es_out_Control( p_demux->out, ES_OUT_SET_PCR, p_sys->i_time );
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
 * SeekIndex: goto to i_date or i_percent
 *****************************************************************************/
static int SeekIndex( demux_t *p_demux, mtime_t i_date, float f_pos )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    asf_object_index_t *p_index;
    int64_t i_pos;

    msg_Dbg( p_demux, "seek with index: %i seconds, position %f",
             (int)(i_date/1000000), f_pos );

    p_index = ASF_FindObject( p_sys->p_root, &asf_object_index_guid, 0 );

    if( i_date < 0 ) i_date = p_sys->i_length * f_pos;

    i_pos = i_date * 10 / p_index->i_index_entry_time_interval;
    i_pos = p_index->index_entry[i_pos].i_packet_number *
        p_sys->p_fp->i_min_data_packet_size;

    return stream_Seek( p_demux->s, p_sys->i_data_begin + i_pos );
}

/*****************************************************************************
 * Control:
 *****************************************************************************/
static int Control( demux_t *p_demux, int i_query, va_list args )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    vlc_meta_t  *p_meta;
    int64_t     i64, *pi64;
    double      f, *pf;
    int         i;

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
        p_sys->i_time = -1;
        for( i = 0; i < 128 ; i++ )
            if( p_sys->track[i] ) p_sys->track[i]->i_time = -1;

        if( p_sys->b_index && p_sys->i_length > 0 )
        {
            i64 = (int64_t)va_arg( args, int64_t );
            return SeekIndex( p_demux, i64, -1 );
        }
        else
        {
            return demux_vaControlHelper( p_demux->s, p_sys->i_data_begin,
                                           p_sys->i_data_end, p_sys->i_bitrate,
                                           p_sys->p_fp->i_min_data_packet_size,
                                           i_query, args );
        }

    case DEMUX_GET_POSITION:
        if( p_sys->i_time < 0 ) return VLC_EGENERIC;
        if( p_sys->i_length > 0 )
        {
            pf = (double*)va_arg( args, double * );
            *pf = p_sys->i_time / (double)p_sys->i_length;
            return VLC_SUCCESS;
        }
        return demux_vaControlHelper( p_demux->s, p_sys->i_data_begin,
                                       p_sys->i_data_end, p_sys->i_bitrate,
                                       p_sys->p_fp->i_min_data_packet_size,
                                       i_query, args );

    case DEMUX_SET_POSITION:
        p_sys->i_time = -1;
        for( i = 0; i < 128 ; i++ )
            if( p_sys->track[i] ) p_sys->track[i]->i_time = -1;

        if( p_sys->b_index && p_sys->i_length > 0 )
        {
            f = (double)va_arg( args, double );
            return SeekIndex( p_demux, -1, f );
        }
        else
        {
            return demux_vaControlHelper( p_demux->s, p_sys->i_data_begin,
                                           p_sys->i_data_end, p_sys->i_bitrate,
                                           p_sys->p_fp->i_min_data_packet_size,
                                           i_query, args );
        }

    case DEMUX_GET_META:
        p_meta = (vlc_meta_t*)va_arg( args, vlc_meta_t* );
        vlc_meta_Merge( p_meta, p_sys->meta );
        return VLC_SUCCESS;

    default:
        return demux_vaControlHelper( p_demux->s, p_sys->i_data_begin,
                                       p_sys->i_data_end, p_sys->i_bitrate,
                                       p_sys->p_fp->i_min_data_packet_size,
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

    for( i = 0; i < 128 ; i++ )
    {
        asf_track_t *tk = p_sys->track[i];

        if( tk && tk->p_es && tk->i_time > 0)
        {
            if( i_time < 0 ) i_time = tk->i_time;
            else i_time = __MIN( i_time, tk->i_time );
        }
    }

    return i_time;
}

#define GETVALUE2b( bits, var, def ) \
    switch( (bits)&0x03 ) \
    { \
        case 1: var = p_peek[i_skip]; i_skip++; break; \
        case 2: var = GetWLE( p_peek + i_skip );  i_skip+= 2; break; \
        case 3: var = GetDWLE( p_peek + i_skip ); i_skip+= 4; break; \
        case 0: \
        default: var = def; break;\
    }

static int DemuxPacket( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    int         i_data_packet_min = p_sys->p_fp->i_min_data_packet_size;
    const uint8_t *p_peek;
    int         i_skip;

    int         i_packet_size_left;
    int         i_packet_flags;
    int         i_packet_property;

    int         b_packet_multiple_payload;
    int         i_packet_length;
    int         i_packet_sequence;
    int         i_packet_padding_length;

    uint32_t    i_packet_send_time;
    uint16_t    i_packet_duration;
    int         i_payload;
    int         i_payload_count;
    int         i_payload_length_type;


    if( stream_Peek( p_demux->s, &p_peek,i_data_packet_min)<i_data_packet_min )
    {
        msg_Warn( p_demux, "cannot peek while getting new packet, EOF ?" );
        return 0;
    }
    i_skip = 0;

    /* *** parse error correction if present *** */
    if( p_peek[0]&0x80 )
    {
        unsigned int i_error_correction_length_type;
        unsigned int i_error_correction_data_length;
        unsigned int i_opaque_data_present;

        i_error_correction_data_length = p_peek[0] & 0x0f;  // 4bits
        i_opaque_data_present = ( p_peek[0] >> 4 )& 0x01;    // 1bit
        i_error_correction_length_type = ( p_peek[0] >> 5 ) & 0x03; // 2bits
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
    {
        msg_Warn( p_demux, "p_peek[0]&0x80 != 0x80" );
    }

    /* sanity check */
    if( i_skip + 2 >= i_data_packet_min )
    {
        goto loop_error_recovery;
    }

    i_packet_flags = p_peek[i_skip]; i_skip++;
    i_packet_property = p_peek[i_skip]; i_skip++;

    b_packet_multiple_payload = i_packet_flags&0x01;

    /* read some value */
    GETVALUE2b( i_packet_flags >> 5, i_packet_length, i_data_packet_min );
    GETVALUE2b( i_packet_flags >> 1, i_packet_sequence, 0 );
    GETVALUE2b( i_packet_flags >> 3, i_packet_padding_length, 0 );

    if( i_packet_padding_length > i_packet_length )
    {
        msg_Warn( p_demux, "Too large padding: %d", i_packet_padding_length );
        goto loop_error_recovery;
    }

    i_packet_send_time = GetDWLE( p_peek + i_skip ); i_skip += 4;
    i_packet_duration  = GetWLE( p_peek + i_skip ); i_skip += 2;

    /* FIXME I have to do that for some file, I don't known why */
    i_packet_size_left = i_data_packet_min /*i_packet_length*/ ;

    if( b_packet_multiple_payload )
    {
        i_payload_count = p_peek[i_skip] & 0x3f;
        i_payload_length_type = ( p_peek[i_skip] >> 6 )&0x03;
        i_skip++;
    }
    else
    {
        i_payload_count = 1;
        i_payload_length_type = 0x02; // unused
    }

    for( i_payload = 0; i_payload < i_payload_count ; i_payload++ )
    {
        asf_track_t   *tk;

        int i_packet_keyframe;
        int i_stream_number;
        int i_media_object_number;
        int i_media_object_offset;
        int i_replicated_data_length;
        int i_payload_data_length;
        int i_payload_data_pos;
        int i_sub_payload_data_length;
        int i_tmp;

        mtime_t i_pts;
        mtime_t i_pts_delta;

        if( i_skip >= i_packet_size_left )
        {
            /* prevent some segfault with invalid file */
            break;
        }

        i_packet_keyframe = p_peek[i_skip] >> 7;
        i_stream_number = p_peek[i_skip++] & 0x7f;

        GETVALUE2b( i_packet_property >> 4, i_media_object_number, 0 );
        GETVALUE2b( i_packet_property >> 2, i_tmp, 0 );
        GETVALUE2b( i_packet_property, i_replicated_data_length, 0 );

        if( i_replicated_data_length > 1 ) // should be at least 8 bytes
        {
            i_pts = (mtime_t)GetDWLE( p_peek + i_skip + 4 ) * 1000;
            i_skip += i_replicated_data_length;
            i_pts_delta = 0;

            i_media_object_offset = i_tmp;

            if( i_skip >= i_packet_size_left )
            {
                break;
            }
        }
        else if( i_replicated_data_length == 1 )
        {
            /* msg_Dbg( p_demux, "found compressed payload" ); */

            i_pts = (mtime_t)i_tmp * 1000;
            i_pts_delta = (mtime_t)p_peek[i_skip] * 1000; i_skip++;

            i_media_object_offset = 0;
        }
        else
        {
            i_pts = (mtime_t)i_packet_send_time * 1000;
            i_pts_delta = 0;

            i_media_object_offset = i_tmp;
        }

        i_pts = __MAX( i_pts - p_sys->p_fp->i_preroll * 1000, 0 );
        if( b_packet_multiple_payload )
        {
            GETVALUE2b( i_payload_length_type, i_payload_data_length, 0 );
        }
        else
        {
            i_payload_data_length = i_packet_length -
                                    i_packet_padding_length - i_skip;
        }

        if( i_payload_data_length < 0 || i_payload_data_length > i_packet_size_left )
        {
            break;
        }
#if 0
         msg_Dbg( p_demux,
                  "payload(%d/%d) stream_number:%d media_object_number:%d media_object_offset:%d replicated_data_length:%d payload_data_length %d",
                  i_payload + 1, i_payload_count, i_stream_number, i_media_object_number,
                  i_media_object_offset, i_replicated_data_length, i_payload_data_length );
#endif

        if( ( tk = p_sys->track[i_stream_number] ) == NULL )
        {
            msg_Warn( p_demux,
                      "undeclared stream[Id 0x%x]", i_stream_number );
            i_skip += i_payload_data_length;
            continue;   // over payload
        }

        if( !tk->p_es )
        {
            i_skip += i_payload_data_length;
            continue;
        }


        for( i_payload_data_pos = 0;
             i_payload_data_pos < i_payload_data_length &&
                    i_packet_size_left > 0;
             i_payload_data_pos += i_sub_payload_data_length )
        {
            block_t *p_frag;
            int i_read;

            // read sub payload length
            if( i_replicated_data_length == 1 )
            {
                i_sub_payload_data_length = p_peek[i_skip]; i_skip++;
                i_payload_data_pos++;
            }
            else
            {
                i_sub_payload_data_length = i_payload_data_length;
            }

            /* FIXME I don't use i_media_object_number, sould I ? */
            if( tk->p_frame && i_media_object_offset == 0 )
            {
                /* send complete packet to decoder */
                block_t *p_gather = block_ChainGather( tk->p_frame );

                es_out_Send( p_demux->out, tk->p_es, p_gather );

                tk->p_frame = NULL;
            }

            i_read = i_sub_payload_data_length + i_skip;
            if( ( p_frag = stream_Block( p_demux->s, i_read ) ) == NULL )
            {
                msg_Warn( p_demux, "cannot read data" );
                return 0;
            }
            i_packet_size_left -= i_read;

            p_frag->p_buffer += i_skip;
            p_frag->i_buffer -= i_skip;

            if( tk->p_frame == NULL )
            {
                tk->i_time =
                    ( (mtime_t)i_pts + i_payload * (mtime_t)i_pts_delta );

                p_frag->i_pts = tk->i_time;

                if( tk->i_cat != VIDEO_ES )
                    p_frag->i_dts = p_frag->i_pts;
                else
                {
                    p_frag->i_dts = p_frag->i_pts;
                    p_frag->i_pts = 0;
                }
            }

            block_ChainAppend( &tk->p_frame, p_frag );

            i_skip = 0;
            if( i_packet_size_left > 0 )
            {
                if( stream_Peek( p_demux->s, &p_peek, i_packet_size_left )
                                                         < i_packet_size_left )
                {
                    msg_Warn( p_demux, "cannot peek, EOF ?" );
                    return 0;
                }
            }
        }
    }

    if( i_packet_size_left > 0 )
    {
#ifdef ASF_DEBUG
        if( i_packet_size_left > i_packet_padding_length )
            msg_Warn( p_demux, "Didn't read %d bytes in the packet",
                            i_packet_size_left - i_packet_padding_length );
        else if( i_packet_size_left < i_packet_padding_length )
            msg_Warn( p_demux, "Read %d too much bytes in the packet",
                            i_packet_padding_length - i_packet_size_left );
#endif
        if( stream_Read( p_demux->s, NULL, i_packet_size_left )
                                                         < i_packet_size_left )
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
    if( stream_Read( p_demux->s, NULL, i_data_packet_min ) != i_data_packet_min )
    {
        msg_Warn( p_demux, "cannot skip data, EOF ?" );
        return 0;
    }

    return 1;
}

/*****************************************************************************
 *
 *****************************************************************************/
static int DemuxInit( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    bool b_seekable;
    unsigned int i_stream, i;
    asf_object_content_description_t *p_cd;
    asf_object_index_t *p_index;
    bool b_index;

    /* init context */
    p_sys->i_time   = -1;
    p_sys->i_length = 0;
    p_sys->i_bitrate = 0;
    p_sys->p_root   = NULL;
    p_sys->p_fp     = NULL;
    p_sys->b_index  = 0;
    p_sys->i_track  = 0;
    for( i = 0; i < 128; i++ )
    {
        p_sys->track[i] = NULL;
    }
    p_sys->i_data_begin = -1;
    p_sys->i_data_end   = -1;
    p_sys->meta         = NULL;

    /* Now load all object ( except raw data ) */
    stream_Control( p_demux->s, STREAM_CAN_FASTSEEK, &b_seekable );
    if( !(p_sys->p_root = ASF_ReadObjectRoot(p_demux->s, b_seekable)) )
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

    p_sys->i_track = ASF_CountObject( p_sys->p_root->p_hdr,
                                      &asf_object_stream_properties_guid );
    if( p_sys->i_track <= 0 )
    {
        msg_Warn( p_demux, "ASF plugin discarded (cannot find any stream!)" );
        goto error;
    }

    /* check if index is available */
    p_index = ASF_FindObject( p_sys->p_root, &asf_object_index_guid, 0 );
    b_index = p_index && p_index->i_index_entry_count;

    msg_Dbg( p_demux, "found %d streams", p_sys->i_track );

    for( i_stream = 0; i_stream < p_sys->i_track; i_stream ++ )
    {
        asf_track_t    *tk;
        asf_object_stream_properties_t *p_sp;
        bool b_access_selected;

        p_sp = ASF_FindObject( p_sys->p_root->p_hdr,
                               &asf_object_stream_properties_guid,
                               i_stream );

        tk = p_sys->track[p_sp->i_stream_number] = malloc( sizeof( asf_track_t ) );
        memset( tk, 0, sizeof( asf_track_t ) );

        tk->i_time = -1;
        tk->p_sp = p_sp;
        tk->p_es = NULL;
        tk->p_frame = NULL;

        /* Check (in case of mms) if this track is selected (ie will receive data) */
        if( !stream_Control( p_demux->s, STREAM_CONTROL_ACCESS, ACCESS_GET_PRIVATE_ID_STATE,
                             p_sp->i_stream_number, &b_access_selected ) &&
            !b_access_selected )
        {
            tk->i_cat = UNKNOWN_ES;
            msg_Dbg( p_demux, "ignoring not selected stream(ID:%d) (by access)",
                     p_sp->i_stream_number );
            continue;
        }

        if( ASF_CmpGUID( &p_sp->i_stream_type, &asf_object_stream_type_audio ) &&
            p_sp->i_type_specific_data_length >= sizeof( WAVEFORMATEX ) - 2 )
        {
            es_format_t fmt;
            uint8_t *p_data = p_sp->p_type_specific_data;
            int i_format;

            es_format_Init( &fmt, AUDIO_ES, 0 );
            i_format = GetWLE( &p_data[0] );
            wf_tag_to_fourcc( i_format, &fmt.i_codec, NULL );
            fmt.audio.i_channels        = GetWLE(  &p_data[2] );
            fmt.audio.i_rate      = GetDWLE( &p_data[4] );
            fmt.i_bitrate         = GetDWLE( &p_data[8] ) * 8;
            fmt.audio.i_blockalign      = GetWLE(  &p_data[12] );
            fmt.audio.i_bitspersample   = GetWLE(  &p_data[14] );

            if( p_sp->i_type_specific_data_length > sizeof( WAVEFORMATEX ) &&
                i_format != WAVE_FORMAT_MPEGLAYER3 &&
                i_format != WAVE_FORMAT_MPEG )
            {
                fmt.i_extra = __MIN( GetWLE( &p_data[16] ),
                                     p_sp->i_type_specific_data_length -
                                     sizeof( WAVEFORMATEX ) );
                fmt.p_extra = malloc( fmt.i_extra );
                memcpy( fmt.p_extra, &p_data[sizeof( WAVEFORMATEX )],
                        fmt.i_extra );
            }

            tk->i_cat = AUDIO_ES;
            tk->p_es = es_out_Add( p_demux->out, &fmt );
            es_format_Clean( &fmt );

            msg_Dbg( p_demux, "added new audio stream(codec:0x%x,ID:%d)",
                    GetWLE( p_data ), p_sp->i_stream_number );
        }
        else if( ASF_CmpGUID( &p_sp->i_stream_type,
                              &asf_object_stream_type_video ) &&
                 p_sp->i_type_specific_data_length >= 11 +
                 sizeof( BITMAPINFOHEADER ) )
        {
            es_format_t  fmt;
            uint8_t      *p_data = &p_sp->p_type_specific_data[11];

            es_format_Init( &fmt, VIDEO_ES,
                            VLC_FOURCC( p_data[16], p_data[17],
                                        p_data[18], p_data[19] ) );
            fmt.video.i_width = GetDWLE( p_data + 4 );
            fmt.video.i_height= GetDWLE( p_data + 8 );


            if( fmt.i_codec == VLC_FOURCC( 'D','V','R',' ') )
            {
                /* DVR-MS special ASF */
                fmt.i_codec = VLC_FOURCC( 'm','p','g','2' ) ;
                fmt.b_packetized = false;
            }

            if( p_sp->i_type_specific_data_length > 11 +
                sizeof( BITMAPINFOHEADER ) )
            {
                fmt.i_extra = __MIN( GetDWLE( p_data ),
                                     p_sp->i_type_specific_data_length - 11 -
                                     sizeof( BITMAPINFOHEADER ) );
                fmt.p_extra = malloc( fmt.i_extra );
                memcpy( fmt.p_extra, &p_data[sizeof( BITMAPINFOHEADER )],
                        fmt.i_extra );
            }

            /* Look for an aspect ratio */
            if( p_sys->p_root->p_metadata )
            {
                asf_object_metadata_t *p_meta = p_sys->p_root->p_metadata;
                int i_aspect_x = 0, i_aspect_y = 0;
                unsigned int i;

                for( i = 0; i < p_meta->i_record_entries_count; i++ )
                {
                    if( !strcmp( p_meta->record[i].psz_name, "AspectRatioX" ) )
                    {
                        if( (!i_aspect_x && !p_meta->record[i].i_stream) ||
                            p_meta->record[i].i_stream ==
                            p_sp->i_stream_number )
                            i_aspect_x = p_meta->record[i].i_val;
                    }
                    if( !strcmp( p_meta->record[i].psz_name, "AspectRatioY" ) )
                    {
                        if( (!i_aspect_y && !p_meta->record[i].i_stream) ||
                            p_meta->record[i].i_stream ==
                            p_sp->i_stream_number )
                            i_aspect_y = p_meta->record[i].i_val;
                    }
                }

                if( i_aspect_x && i_aspect_y )
                {
                    fmt.video.i_aspect = i_aspect_x *
                        (int64_t)fmt.video.i_width * VOUT_ASPECT_FACTOR /
                        fmt.video.i_height / i_aspect_y;
                }
        }

            tk->i_cat = VIDEO_ES;
            tk->p_es = es_out_Add( p_demux->out, &fmt );
            es_format_Clean( &fmt );

            /* If there is a video track then use the index for seeking */
            p_sys->b_index = b_index;

            msg_Dbg( p_demux, "added new video stream(ID:%d)",
                     p_sp->i_stream_number );
        }
        else if( ASF_CmpGUID( &p_sp->i_stream_type, &asf_object_extended_stream_header ) &&
            p_sp->i_type_specific_data_length >= 64 )
        {
            /* Now follows a 64 byte header of which we don't know much */
            es_format_t fmt;
            guid_t  *p_ref  = (guid_t *)p_sp->p_type_specific_data;
            uint8_t *p_data = p_sp->p_type_specific_data + 64;
            unsigned int i_data = p_sp->i_type_specific_data_length - 64;

            msg_Dbg( p_demux, "Ext stream header detected. datasize = %d", p_sp->i_type_specific_data_length );
            if( ASF_CmpGUID( p_ref, &asf_object_extended_stream_type_audio ) &&
                i_data >= sizeof( WAVEFORMATEX ) - 2)
            {
                int      i_format;
                es_format_Init( &fmt, AUDIO_ES, 0 );
                i_format = GetWLE( &p_data[0] );
                if( i_format == 0 )
                    fmt.i_codec = VLC_FOURCC( 'a','5','2',' ');
                else
                    wf_tag_to_fourcc( i_format, &fmt.i_codec, NULL );
                fmt.audio.i_channels        = GetWLE(  &p_data[2] );
                fmt.audio.i_rate      = GetDWLE( &p_data[4] );
                fmt.i_bitrate         = GetDWLE( &p_data[8] ) * 8;
                fmt.audio.i_blockalign      = GetWLE(  &p_data[12] );
                fmt.audio.i_bitspersample   = GetWLE(  &p_data[14] );
                fmt.b_packetized = true;

                if( p_sp->i_type_specific_data_length > sizeof( WAVEFORMATEX ) &&
                    i_format != WAVE_FORMAT_MPEGLAYER3 &&
                    i_format != WAVE_FORMAT_MPEG )
                {
                    fmt.i_extra = __MIN( GetWLE( &p_data[16] ),
                                         p_sp->i_type_specific_data_length -
                                         sizeof( WAVEFORMATEX ) );
                    fmt.p_extra = malloc( fmt.i_extra );
                    memcpy( fmt.p_extra, &p_data[sizeof( WAVEFORMATEX )],
                        fmt.i_extra );
                }

                tk->i_cat = AUDIO_ES;
                tk->p_es = es_out_Add( p_demux->out, &fmt );
                es_format_Clean( &fmt );

                msg_Dbg( p_demux, "added new audio stream (codec:0x%x,ID:%d)",
                    i_format, p_sp->i_stream_number );
            }
        }
        else
        {
            tk->i_cat = UNKNOWN_ES;
            msg_Dbg( p_demux, "ignoring unknown stream(ID:%d)",
                     p_sp->i_stream_number );
        }
    }

    p_sys->i_data_begin = p_sys->p_root->p_data->i_object_pos + 50;
    if( p_sys->p_root->p_data->i_object_size != 0 )
    { /* local file */
        p_sys->i_data_end = p_sys->p_root->p_data->i_object_pos +
                                    p_sys->p_root->p_data->i_object_size;
    }
    else
    { /* live/broacast */
        p_sys->i_data_end = -1;
    }

    /* go to first packet */
    stream_Seek( p_demux->s, p_sys->i_data_begin );

    /* try to calculate movie time */
    if( p_sys->p_fp->i_data_packets_count > 0 )
    {
        int64_t i_count;
        int64_t i_size = stream_Size( p_demux->s );

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
                   (mtime_t)p_sys->p_fp->i_data_packets_count;

        if( p_sys->i_length > 0 )
        {
            p_sys->i_bitrate = 8 * i_size * (int64_t)1000000 / p_sys->i_length;
        }
    }

    /* Create meta information */
    p_sys->meta = vlc_meta_New();

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
    for( i_stream = 0, i = 0; i < 128; i++ )
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

    es_out_Control( p_demux->out, ES_OUT_RESET_PCR );
    return VLC_SUCCESS;

error:
    ASF_FreeObjectRoot( p_demux->s, p_sys->p_root );
    return VLC_EGENERIC;
}
/*****************************************************************************
 *
 *****************************************************************************/
static void DemuxEnd( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    int         i;

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

    for( i = 0; i < 128; i++ )
    {
        asf_track_t *tk = p_sys->track[i];

        if( tk )
        {
            if( tk->p_frame )
            {
                block_ChainRelease( tk->p_frame );
            }
            if( tk->p_es )
            {
                es_out_Del( p_demux->out, tk->p_es );
            }
            free( tk );
        }
        p_sys->track[i] = 0;
    }
}

