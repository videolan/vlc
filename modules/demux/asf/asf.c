/*****************************************************************************
 * asf.c : ASFv01 file input module for vlc
 *****************************************************************************
 * Copyright (C) 2002-2003 VideoLAN
 * $Id: asf.c,v 1.42 2003/11/16 22:54:12 gbazin Exp $
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <stdlib.h>                                      /* malloc(), free() */

#include <vlc/vlc.h>
#include <vlc/input.h>

#include "codecs.h"                        /* BITMAPINFOHEADER, WAVEFORMATEX */
#include "libasf.h"

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open  ( vlc_object_t * );
static void Close ( vlc_object_t * );

vlc_module_begin();
    set_description( _("ASF v1.0 demuxer") );
    set_capability( "demux", 200 );
    set_callbacks( Open, Close );
    add_shortcut( "asf" );
vlc_module_end();


/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Demux   ( input_thread_t * );

typedef struct asf_stream_s
{
    int i_cat;

    es_out_id_t     *p_es;

    asf_object_stream_properties_t *p_sp;

    mtime_t i_time;

    pes_packet_t    *p_pes;     /* used to keep uncomplete frames */

} asf_stream_t;

struct demux_sys_t
{
    mtime_t             i_time;     /* µs */
    mtime_t             i_length;   /* length of file file */

    asf_object_root_t            *p_root;
    asf_object_file_properties_t *p_fp;

    unsigned int        i_streams;
    asf_stream_t        *stream[128];

    int64_t             i_data_begin;
    int64_t             i_data_end;
};

static mtime_t  GetMoviePTS( demux_sys_t * );
static int      DemuxPacket( input_thread_t *, vlc_bool_t b_play_audio );

/*****************************************************************************
 * Open: check file and initializes ASF structures
 *****************************************************************************/
static int Open( vlc_object_t * p_this )
{
    input_thread_t  *p_input = (input_thread_t *)p_this;
    uint8_t         *p_peek;

    guid_t          guid;

    demux_sys_t     *p_sys;
    unsigned int    i_stream, i;
    asf_object_content_description_t *p_cd;

    vlc_bool_t      b_seekable;

    input_info_category_t *p_cat;

    /* a little test to see if it could be a asf stream */
    if( input_Peek( p_input, &p_peek, 16 ) < 16 )
    {
        msg_Warn( p_input, "ASF plugin discarded (cannot peek)" );
        return VLC_EGENERIC;
    }
    ASF_GetGUID( &guid, p_peek );
    if( !ASF_CmpGUID( &guid, &asf_object_header_guid ) )
    {
        msg_Warn( p_input, "ASF plugin discarded (not a valid file)" );
        return VLC_EGENERIC;
    }

    /* Set p_input field */
    p_input->pf_demux         = Demux;
    p_input->pf_demux_control = demux_vaControlDefault;
    p_input->p_demux_data = p_sys = malloc( sizeof( demux_sys_t ) );
    memset( p_sys, 0, sizeof( demux_sys_t ) );
    p_sys->i_time = -1;
    p_sys->i_length = 0;

    /* Now load all object ( except raw data ) */
    stream_Control( p_input->s, STREAM_CAN_FASTSEEK, &b_seekable );
    if( (p_sys->p_root = ASF_ReadObjectRoot( p_input->s, b_seekable )) == NULL )
    {
        msg_Warn( p_input, "ASF plugin discarded (not a valid file)" );
        free( p_sys );
        return VLC_EGENERIC;
    }
    p_sys->p_fp = p_sys->p_root->p_fp;

    if( p_sys->p_fp->i_min_data_packet_size != p_sys->p_fp->i_max_data_packet_size )
    {
        msg_Warn( p_input,
                  "ASF plugin discarded (invalid file_properties object)" );
        goto error;
    }

    p_sys->i_streams = ASF_CountObject( p_sys->p_root->p_hdr,
                                          &asf_object_stream_properties_guid );
    if( !p_sys->i_streams )
    {
        msg_Warn( p_input, "ASF plugin discarded (cannot find any stream!)" );
        goto error;
    }

    msg_Dbg( p_input, "found %d streams", p_sys->i_streams );

    vlc_mutex_lock( &p_input->stream.stream_lock );
    if( input_InitStream( p_input, 0 ) == -1)
    {
        vlc_mutex_unlock( &p_input->stream.stream_lock );
        msg_Err( p_input, "cannot init stream" );
        goto error;
    }
    p_input->stream.i_mux_rate = 0 ; /* updated later */
    vlc_mutex_unlock( &p_input->stream.stream_lock );

    for( i_stream = 0; i_stream < p_sys->i_streams; i_stream ++ )
    {
        asf_stream_t    *p_stream;
        asf_object_stream_properties_t *p_sp;

        p_sp = ASF_FindObject( p_sys->p_root->p_hdr,
                               &asf_object_stream_properties_guid,
                               i_stream );

        p_stream =
            p_sys->stream[p_sp->i_stream_number] =
                malloc( sizeof( asf_stream_t ) );
        memset( p_stream, 0, sizeof( asf_stream_t ) );

        p_stream->i_time = -1;
        p_stream->p_sp = p_sp;
        p_stream->p_es = NULL;

        if( ASF_CmpGUID( &p_sp->i_stream_type, &asf_object_stream_type_audio ) &&
            p_sp->i_type_specific_data_length >= sizeof( WAVEFORMATEX ) - 2 )
        {
            es_format_t  fmt;
            uint8_t      *p_data = p_sp->p_type_specific_data;

            es_format_Init( &fmt, AUDIO_ES, 0 );
            wf_tag_to_fourcc( GetWLE( &p_data[0] ), &fmt.i_codec, NULL );
            fmt.audio.i_channels        = GetWLE(  &p_data[2] );
            fmt.audio.i_rate      = GetDWLE( &p_data[4] );
            fmt.i_bitrate         = GetDWLE( &p_data[8] ) * 8;
            fmt.audio.i_blockalign      = GetWLE(  &p_data[12] );
            fmt.audio.i_bitspersample   = GetWLE(  &p_data[14] );

            if( p_sp->i_type_specific_data_length > sizeof( WAVEFORMATEX ) )
            {
                fmt.i_extra_type = ES_EXTRA_TYPE_WAVEFORMATEX;
                fmt.i_extra = __MIN( GetWLE( &p_data[16] ),
                                     p_sp->i_type_specific_data_length - sizeof( WAVEFORMATEX ) );
                fmt.p_extra = malloc( fmt.i_extra );
                memcpy( fmt.p_extra, &p_data[sizeof( WAVEFORMATEX )], fmt.i_extra );
            }

            p_stream->i_cat = AUDIO_ES;
            p_stream->p_es = es_out_Add( p_input->p_es_out, &fmt );

            msg_Dbg( p_input,
                    "added new audio stream(codec:0x%x,ID:%d)",
                    GetWLE( p_data ), p_sp->i_stream_number );
        }
        else if( ASF_CmpGUID( &p_sp->i_stream_type, &asf_object_stream_type_video ) &&
                 p_sp->i_type_specific_data_length >= 11 + sizeof( BITMAPINFOHEADER ) )
        {
            es_format_t  fmt;
            uint8_t      *p_data = &p_sp->p_type_specific_data[11];

            es_format_Init( &fmt, VIDEO_ES,
                            VLC_FOURCC( p_data[16], p_data[17], p_data[18], p_data[19] ) );
            fmt.video.i_width = GetDWLE( p_data + 4 );
            fmt.video.i_height= GetDWLE( p_data + 8 );

            if( p_sp->i_type_specific_data_length > 11 + sizeof( BITMAPINFOHEADER ) )
            {
                fmt.i_extra_type = ES_EXTRA_TYPE_BITMAPINFOHEADER;
                fmt.i_extra = __MIN( GetDWLE( p_data ),
                                     p_sp->i_type_specific_data_length - 11 - sizeof( BITMAPINFOHEADER ) );
                fmt.p_extra = malloc( fmt.i_extra );
                memcpy( fmt.p_extra, &p_data[sizeof( BITMAPINFOHEADER )], fmt.i_extra );
            }

            p_stream->i_cat = VIDEO_ES;
            p_stream->p_es = es_out_Add( p_input->p_es_out, &fmt );

            msg_Dbg( p_input, "added new video stream(ID:%d)",
                     p_sp->i_stream_number );
        }
        else
        {
            p_stream->i_cat = UNKNOWN_ES;
            msg_Dbg( p_input, "ignoring unknown stream(ID:%d)",
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
    stream_Seek( p_input->s, p_sys->i_data_begin );

    /* try to calculate movie time */
    if( p_sys->p_fp->i_data_packets_count > 0 )
    {
        int64_t i_count;
        int64_t i_size = stream_Size( p_input->s );

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
            p_input->stream.i_mux_rate =
                i_size / 50 * (int64_t)1000000 / p_sys->i_length;
        }
    }

    /* We add all info about this stream */
    p_cat = input_InfoCategory( p_input, "Asf" );
    if( p_sys->i_length > 0 )
    {
        int64_t i_second = p_sys->i_length / (int64_t)1000000;

        input_AddInfo( p_cat, _("Length"), "%d:%d:%d",
                       (int)(i_second / 36000),
                       (int)(( i_second / 60 ) % 60),
                       (int)(i_second % 60) );
    }
    input_AddInfo( p_cat, _("Number of streams"), "%d" , p_sys->i_streams );

    if( ( p_cd = ASF_FindObject( p_sys->p_root->p_hdr,
                                 &asf_object_content_description_guid, 0 ) ) )
    {
        if( *p_cd->psz_title )
            input_AddInfo( p_cat, _("Title"), p_cd->psz_title );
        if( p_cd->psz_author )
            input_AddInfo( p_cat, _("Author"), p_cd->psz_author );
        if( p_cd->psz_copyright )
            input_AddInfo( p_cat, _("Copyright"), p_cd->psz_copyright );
        if( *p_cd->psz_description )
            input_AddInfo( p_cat, _("Description"), p_cd->psz_description );
        if( *p_cd->psz_rating )
            input_AddInfo( p_cat, _("Rating"), p_cd->psz_rating );
    }

    /* FIXME to port to new way */
    for( i_stream = 0, i = 0; i < 128; i++ )
    {
        asf_object_codec_list_t *p_cl =
            ASF_FindObject( p_sys->p_root->p_hdr,
                            &asf_object_codec_list_guid, 0 );

        if( p_sys->stream[i] )
        {
            char psz_cat[sizeof(_("Stream "))+10];
            sprintf( psz_cat, _("Stream %d"), i_stream );
            p_cat = input_InfoCategory( p_input, psz_cat);

            if( p_cl && i_stream < p_cl->i_codec_entries_count )
            {
                input_AddInfo( p_cat, _("Codec name"),
                               p_cl->codec[i_stream].psz_name );
                input_AddInfo( p_cat, _("Codec description"),
                               p_cl->codec[i_stream].psz_description );
            }
            i_stream++;
        }
    }

    return VLC_SUCCESS;

error:
    ASF_FreeObjectRoot( p_input->s, p_sys->p_root );
    free( p_sys );
    return VLC_EGENERIC;
}


/*****************************************************************************
 * Demux: read packet and send them to decoders
 *****************************************************************************/
static int Demux( input_thread_t *p_input )
{
    demux_sys_t *p_sys = p_input->p_demux_data;
    vlc_bool_t b_play_audio;
    int i;

    /* catch seek from user */
    if( p_input->stream.p_selected_program->i_synchro_state == SYNCHRO_REINIT )
    {
        int64_t i_offset;

        msleep( p_input->i_pts_delay );

        i_offset = stream_Tell( p_input->s ) - p_sys->i_data_begin;
        if( i_offset  < 0 )
        {
            i_offset = 0;
        }
        if( i_offset % p_sys->p_fp->i_min_data_packet_size > 0 )
        {
            i_offset -= i_offset % p_sys->p_fp->i_min_data_packet_size;
        }

        if( stream_Seek( p_input->s, i_offset + p_sys->i_data_begin ) )
        {
            msg_Warn( p_input, "cannot resynch after seek (EOF?)" );
            return -1;
        }

        p_sys->i_time = -1;
        for( i = 0; i < 128 ; i++ )
        {
#define p_stream p_sys->stream[i]
            if( p_stream )
            {
                p_stream->i_time = -1;
            }
#undef p_stream
        }
    }

    /* Check if we need to send the audio data to decoder */
    b_play_audio = !p_input->stream.control.b_mute;

    for( ;; )
    {
        mtime_t i_length;
        mtime_t i_time_begin = GetMoviePTS( p_sys );
        int i_result;

        if( p_input->b_die )
        {
            break;
        }

        if( ( i_result = DemuxPacket( p_input, b_play_audio ) ) <= 0 )
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
            if( i_length < 0 || i_length >= 40 * 1000 )
            {
                break;
            }
        }
    }

    p_sys->i_time = GetMoviePTS( p_sys );
    if( p_sys->i_time >= 0 )
    {
        input_ClockManageRef( p_input,
                              p_input->stream.p_selected_program,
                              p_sys->i_time * 9 / 100 );
    }

    return( 1 );
}

/*****************************************************************************
 * Close: frees unused data
 *****************************************************************************/
static void Close( vlc_object_t * p_this )
{
    input_thread_t *p_input = (input_thread_t *)p_this;
    demux_sys_t    *p_sys = p_input->p_demux_data;
    int i_stream;

    msg_Dbg( p_input, "Freeing all memory" );

    ASF_FreeObjectRoot( p_input->s, p_sys->p_root );
    for( i_stream = 0; i_stream < 128; i_stream++ )
    {
#define p_stream p_sys->stream[i_stream]
        if( p_stream )
        {
            if( p_stream->p_pes )
            {
                input_DeletePES( p_input->p_method_data, p_stream->p_pes );
            }
            free( p_stream );
        }
#undef p_stream
    }
    free( p_sys );
}


/*****************************************************************************
 *
 *****************************************************************************/
static mtime_t GetMoviePTS( demux_sys_t *p_sys )
{
    mtime_t i_time;
    int     i_stream;

    i_time = -1;
    for( i_stream = 0; i_stream < 128 ; i_stream++ )
    {
#define p_stream p_sys->stream[i_stream]
        if( p_stream && p_stream->p_es && p_stream->i_time > 0)
        {
            if( i_time < 0 )
            {
                i_time = p_stream->i_time;
            }
            else
            {
                i_time = __MIN( i_time, p_stream->i_time );
            }
        }
#undef p_stream
    }

    return( i_time );
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

static int DemuxPacket( input_thread_t *p_input, vlc_bool_t b_play_audio )
{
    demux_sys_t *p_sys = p_input->p_demux_data;
    int     i_data_packet_min = p_sys->p_fp->i_min_data_packet_size;
    uint8_t *p_peek;
    int     i_skip;

    int     i_packet_size_left;
    int     i_packet_flags;
    int     i_packet_property;

    int     b_packet_multiple_payload;
    int     i_packet_length;
    int     i_packet_sequence;
    int     i_packet_padding_length;

    uint32_t    i_packet_send_time;
    uint16_t    i_packet_duration;
    int         i_payload;
    int         i_payload_count;
    int         i_payload_length_type;


    if( stream_Peek( p_input->s, &p_peek,i_data_packet_min)<i_data_packet_min )
    {
        // EOF ?
        msg_Warn( p_input, "cannot peek while getting new packet, EOF ?" );
        return( 0 );
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
        msg_Warn( p_input, "p_peek[0]&0x80 != 0x80" );
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

    i_packet_send_time = GetDWLE( p_peek + i_skip ); i_skip += 4;
    i_packet_duration  = GetWLE( p_peek + i_skip ); i_skip += 2;

//        i_packet_size_left = i_packet_length;   // XXX données reellement lu
    /* FIXME I have to do that for some file, I don't known why */
    i_packet_size_left = i_data_packet_min;

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
        asf_stream_t   *p_stream;

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

        i_stream_number = p_peek[i_skip] & 0x7f;
        i_skip++;

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

            msg_Dbg( p_input, "found compressed payload" );

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

        if( i_payload_data_length < 0 || i_skip + i_payload_data_length > i_packet_size_left )
        {
            break;
        }

#if 0
         msg_Dbg( p_input,
                  "payload(%d/%d) stream_number:%d media_object_number:%d media_object_offset:%d replicated_data_length:%d payload_data_length %d",
                  i_payload + 1,
                  i_payload_count,
                  i_stream_number,
                  i_media_object_number,
                  i_media_object_offset,
                  i_replicated_data_length,
                  i_payload_data_length );
#endif

        if( !( p_stream = p_sys->stream[i_stream_number] ) )
        {
            msg_Warn( p_input,
                      "undeclared stream[Id 0x%x]", i_stream_number );
            i_skip += i_payload_data_length;
            continue;   // over payload
        }

        if( !p_stream->p_es )
        {
            i_skip += i_payload_data_length;
            continue;
        }


        for( i_payload_data_pos = 0;
             i_payload_data_pos < i_payload_data_length &&
                    i_packet_size_left > 0;
             i_payload_data_pos += i_sub_payload_data_length )
        {
            data_packet_t  *p_data;
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
            if( p_stream->p_pes && i_media_object_offset == 0 )
            {
                /* send complete packet to decoder */
                if( p_stream->p_pes->i_pes_size > 0 )
                {
                    es_out_Send( p_input->p_es_out, p_stream->p_es, p_stream->p_pes );
                    p_stream->p_pes = NULL;
                }
            }

            if( !p_stream->p_pes )  // add a new PES
            {
                p_stream->i_time =
                    ( (mtime_t)i_pts + i_payload * (mtime_t)i_pts_delta );

                p_stream->p_pes = input_NewPES( p_input->p_method_data );
                p_stream->p_pes->i_dts =
                    p_stream->p_pes->i_pts =
                        input_ClockGetTS( p_input,
                                          p_input->stream.p_selected_program,
                                          p_stream->i_time * 9 /100 );

                //msg_Err( p_input, "stream[0x%2x] pts=%lld", i_stream_number, p_stream->p_pes->i_pts );
                p_stream->p_pes->p_next = NULL;
                p_stream->p_pes->i_nb_data = 0;
                p_stream->p_pes->i_pes_size = 0;
            }

            i_read = i_sub_payload_data_length + i_skip;
            if((p_data = stream_DataPacket( p_input->s,i_read,VLC_TRUE))==NULL)
            {
                msg_Warn( p_input, "cannot read data" );
                return( 0 );
            }
            p_data->p_payload_start += i_skip;
            i_packet_size_left -= i_read;


            if( !p_stream->p_pes->p_first )
            {
                p_stream->p_pes->p_first = p_stream->p_pes->p_last = p_data;
            }
            else
            {
                p_stream->p_pes->p_last->p_next = p_data;
                p_stream->p_pes->p_last = p_data;
            }
            p_stream->p_pes->i_pes_size += i_sub_payload_data_length;
            p_stream->p_pes->i_nb_data++;

            i_skip = 0;
            if( i_packet_size_left > 0 )
            {
                if( stream_Peek( p_input->s, &p_peek, i_packet_size_left )
                                                         < i_packet_size_left )
                {
                    // EOF ?
                    msg_Warn( p_input, "cannot peek, EOF ?" );
                    return( 0 );
                }
            }
        }
    }

    if( i_packet_size_left > 0 )
    {
        if( stream_Read( p_input->s, NULL, i_packet_size_left )
                                                         < i_packet_size_left )
        {
            msg_Warn( p_input, "cannot skip data, EOF ?" );
            return( 0 );
        }
    }

    return( 1 );

loop_error_recovery:
    msg_Warn( p_input, "unsupported packet header" );
    if( p_sys->p_fp->i_min_data_packet_size != p_sys->p_fp->i_max_data_packet_size )
    {
        msg_Err( p_input, "unsupported packet header, fatal error" );
        return( -1 );
    }
    stream_Read( p_input->s, NULL, i_data_packet_min );

    return( 1 );
}


