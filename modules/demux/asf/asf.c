/*****************************************************************************
 * asf.c : ASFv01 file input module for vlc
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: asf.c,v 1.16 2003/01/20 13:04:03 fenrir Exp $
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
#include <string.h>                                              /* strdup() */
#include <errno.h>
#include <sys/types.h>

#include <vlc/vlc.h>
#include <vlc/input.h>

#include "libasf.h"
#include "asf.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int    Activate   ( vlc_object_t * );
static void   Deactivate ( vlc_object_t * );
static int    Demux      ( input_thread_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_description( "ASF v1.0 demuxer (file only)" );
    set_capability( "demux", 200 );
    set_callbacks( Activate, Deactivate );
    add_shortcut( "asf" );
vlc_module_end();

static uint16_t GetWLE( uint8_t *p_buff )
{
    return( (p_buff[0]) + ( p_buff[1] <<8 ) );
}
static uint32_t GetDWLE( uint8_t *p_buff )
{
    return( p_buff[0] + ( p_buff[1] <<8 ) +
            ( p_buff[2] <<16 ) + ( p_buff[3] <<24 ) );
}

/*****************************************************************************
 * Activate: check file and initializes ASF structures
 *****************************************************************************/
static int Activate( vlc_object_t * p_this )
{
    input_thread_t  *p_input = (input_thread_t *)p_this;
    uint8_t         *p_peek;
    guid_t          guid;

    demux_sys_t     *p_demux;
    int             i_stream;

    /* Initialize access plug-in structures. */
    if( p_input->i_mtu == 0 )
    {
        /* Improve speed. */
        p_input->i_bufsize = INPUT_DEFAULT_BUFSIZE;
    }

    p_input->pf_demux = Demux;

    /* a little test to see if it could be a asf stream */
    if( input_Peek( p_input, &p_peek, 16 ) < 16 )
    {
        msg_Warn( p_input, "ASF v1.0 plugin discarded (cannot peek)" );
        return( -1 );
    }
    GetGUID( &guid, p_peek );
    if( !CmpGUID( &guid, &asf_object_header_guid ) )
    {
        msg_Warn( p_input, "ASF v1.0 plugin discarded (not a valid file)" );
        return( -1 );
    }

    /* create our structure that will contains all data */
    if( !( p_input->p_demux_data =
                p_demux = malloc( sizeof( demux_sys_t ) ) ) )
    {
        msg_Err( p_input, "out of memory" );
        return( -1 );
    }
    memset( p_demux, 0, sizeof( demux_sys_t ) );
    p_demux->i_first_pts = -1;

    /* Now load all object ( except raw data ) */
    if( !ASF_ReadObjectRoot( p_input, &p_demux->root, p_input->stream.b_seekable ) )
    {
        msg_Warn( p_input, "ASF v1.0 plugin discarded (not a valid file)" );
        free( p_demux );
        return( -1 );
    }
    /* Check if we have found all mandatory asf object */
    if( !p_demux->root.p_hdr || !p_demux->root.p_data )
    {
        ASF_FreeObjectRoot( p_input, &p_demux->root );
        free( p_demux );
        msg_Warn( p_input, "ASF v1.0 plugin discarded (not a valid file)" );
        return( -1 );
    }

    if( !( p_demux->p_fp = ASF_FindObject( p_demux->root.p_hdr,
                                    &asf_object_file_properties_guid, 0 ) ) )
    {
        ASF_FreeObjectRoot( p_input, &p_demux->root );
        free( p_demux );
        msg_Warn( p_input, "ASF v1.0 plugin discarded (missing file_properties object)" );
        return( -1 );
    }

    if( p_demux->p_fp->i_min_data_packet_size != p_demux->p_fp->i_max_data_packet_size )
    {
        ASF_FreeObjectRoot( p_input, &p_demux->root );
        free( p_demux );
        msg_Warn( p_input, "ASF v1.0 plugin discarded (invalid file_properties object)" );
        return( -1 );
    }

    p_demux->i_streams = ASF_CountObject( p_demux->root.p_hdr,
                                          &asf_object_stream_properties_guid );
    if( !p_demux->i_streams )
    {
        ASF_FreeObjectRoot( p_input, &p_demux->root );
        free( p_demux );
        msg_Warn( p_input, "ASF plugin discarded (cannot find any stream!)" );
        return( -1 );
    }
    else
    {
        input_info_category_t *p_cat = input_InfoCategory( p_input, "Asf" );
        msg_Dbg( p_input, "found %d streams", p_demux->i_streams );
        input_AddInfo( p_cat, "Number of streams", "%d" , p_demux->i_streams );
    }

    /*  create one program */
    vlc_mutex_lock( &p_input->stream.stream_lock );
    if( input_InitStream( p_input, 0 ) == -1)
    {
        vlc_mutex_unlock( &p_input->stream.stream_lock );
        msg_Err( p_input, "cannot init stream" );
        return( -1 );
    }
    if( input_AddProgram( p_input, 0, 0) == NULL )
    {
        vlc_mutex_unlock( &p_input->stream.stream_lock );
        msg_Err( p_input, "cannot add program" );
        return( -1 );
    }
    p_input->stream.p_selected_program = p_input->stream.pp_programs[0];
    p_input->stream.i_mux_rate = 0 ; /* FIXME */
    vlc_mutex_unlock( &p_input->stream.stream_lock );

    for( i_stream = 0; i_stream < p_demux->i_streams; i_stream ++ )
    {
        asf_stream_t    *p_stream;
        asf_object_stream_properties_t *p_sp;
        char psz_cat[sizeof("Stream ")+10];
        input_info_category_t *p_cat;
        sprintf( psz_cat, "Stream %d", i_stream );
        p_cat = input_InfoCategory( p_input, psz_cat);

        p_sp = ASF_FindObject( p_demux->root.p_hdr,
                               &asf_object_stream_properties_guid,
                               i_stream );

        p_stream =
            p_demux->stream[p_sp->i_stream_number] =
                malloc( sizeof( asf_stream_t ) );
        memset( p_stream, 0, sizeof( asf_stream_t ) );
        p_stream->p_sp = p_sp;

        vlc_mutex_lock( &p_input->stream.stream_lock );
        p_stream->p_es =
            input_AddES( p_input,
                         p_input->stream.p_selected_program,
                         p_sp->i_stream_number,
                         0 );

        vlc_mutex_unlock( &p_input->stream.stream_lock );
        if( CmpGUID( &p_sp->i_stream_type, &asf_object_stream_type_audio ) )
        {
            int i_codec;
            if( p_sp->p_type_specific_data )
            {
                i_codec = GetWLE( p_sp->p_type_specific_data );
            }
            else
            {
                i_codec = -1;
            }

            p_stream->i_cat = AUDIO_ES;
            input_AddInfo( p_cat, "Type", "Audio" );
            msg_Dbg( p_input,
                    "adding new audio stream(codec:0x%x,ID:%d)",
                    i_codec,
                    p_sp->i_stream_number );
            switch( i_codec )
            {
                case( 0x01 ):
                    p_stream->p_es->i_fourcc =
                        VLC_FOURCC( 'a', 'r', 'a', 'w' );
                    break;
                case( 0x50 ):
                case( 0x55 ):
                    p_stream->p_es->i_fourcc =
                        VLC_FOURCC( 'm','p','g','a' );
                    break;
                case( 0x2000 ):
                    p_stream->p_es->i_fourcc =
                        VLC_FOURCC( 'a','5','2',' ' );
                    break;
                case( 0x160 ):
                    p_stream->p_es->i_fourcc =
                        VLC_FOURCC( 'w','m','a','1' );
                    break;
                case( 0x161 ):
                    p_stream->p_es->i_fourcc =
                        VLC_FOURCC( 'w','m','a','2' );
                    break;
                default:
                    p_stream->p_es->i_fourcc =
                        VLC_FOURCC( 'm','s',(i_codec >> 8)&0xff,i_codec&0xff );
            }
            input_AddInfo( p_cat, "Codec", "%.4s", (char*)&p_stream->p_es->i_fourcc );
            if( p_sp->i_type_specific_data_length > 0 )
            {
                WAVEFORMATEX    *p_wf;
                size_t          i_size;
                uint8_t         *p_data;

                i_size = p_sp->i_type_specific_data_length;

                p_wf = malloc( i_size );
                p_stream->p_es->p_waveformatex = (void*)p_wf;
                p_data = p_sp->p_type_specific_data;

                p_wf->wFormatTag        = GetWLE( p_data );
                p_wf->nChannels         = GetWLE( p_data + 2 );
                input_AddInfo( p_cat, "Channels", "%d", p_wf->nChannels );
                p_wf->nSamplesPerSec    = GetDWLE( p_data + 4 );
                input_AddInfo( p_cat, "Sample rate", "%d", p_wf->nSamplesPerSec );
                p_wf->nAvgBytesPerSec   = GetDWLE( p_data + 8 );
                input_AddInfo( p_cat, "Avg. byterate", "%d", p_wf->nAvgBytesPerSec );
                p_wf->nBlockAlign       = GetWLE( p_data + 12 );
                p_wf->wBitsPerSample    = GetWLE( p_data + 14 );
                input_AddInfo( p_cat, "Bits Per Sample", "%d", p_wf->wBitsPerSample );
                p_wf->cbSize            = __MAX( 0,
                                              i_size - sizeof( WAVEFORMATEX ));
                if( i_size > sizeof( WAVEFORMATEX ) )
                {
                    memcpy( (uint8_t*)p_wf + sizeof( WAVEFORMATEX ),
                            p_data + sizeof( WAVEFORMATEX ),
                            i_size - sizeof( WAVEFORMATEX ) );
                }
            }

        }
        else
        if( CmpGUID( &p_sp->i_stream_type, &asf_object_stream_type_video ) )
        {
            p_stream->i_cat = VIDEO_ES;
            input_AddInfo( p_cat, "Type", "Video" );
            msg_Dbg( p_input,
                    "adding new video stream(ID:%d)",
                    p_sp->i_stream_number );
            if( p_sp->p_type_specific_data )
            {
                p_stream->p_es->i_fourcc =
                    VLC_FOURCC( p_sp->p_type_specific_data[27],
                                p_sp->p_type_specific_data[28],
                                p_sp->p_type_specific_data[29],
                                p_sp->p_type_specific_data[30] );
            }
            else
            {
                p_stream->p_es->i_fourcc =
                    VLC_FOURCC( 'u','n','d','f' );
            }
            input_AddInfo( p_cat, "Codec", "%.4s", (char*)&p_stream->p_es->i_fourcc );
            if( p_sp->i_type_specific_data_length > 11 )
            {
                BITMAPINFOHEADER *p_bih;
                size_t      i_size;
                uint8_t     *p_data;

                i_size = p_sp->i_type_specific_data_length - 11;

                p_bih = malloc( i_size );
                p_stream->p_es->p_bitmapinfoheader = (void*)p_bih;
                p_data = p_sp->p_type_specific_data + 11;

                p_bih->biSize       = GetDWLE( p_data );
                input_AddInfo( p_cat, "Size", "%d", p_bih->biSize );
                p_bih->biWidth      = GetDWLE( p_data + 4 );
                p_bih->biHeight     = GetDWLE( p_data + 8 );
                input_AddInfo( p_cat, "Resolution", "%dx%d", p_bih->biWidth, p_bih->biHeight );
                p_bih->biPlanes     = GetDWLE( p_data + 12 );
                input_AddInfo( p_cat, "Planes", "%d", p_bih->biPlanes );
                p_bih->biBitCount   = GetDWLE( p_data + 14 );
                input_AddInfo( p_cat, "Bits per pixel", "%d", p_bih->biBitCount );
                p_bih->biCompression= GetDWLE( p_data + 16 );
                input_AddInfo( p_cat, "Compression Rate", "%d", p_bih->biCompression );
                p_bih->biSizeImage  = GetDWLE( p_data + 20 );
                input_AddInfo( p_cat, "Image Size", "%d", p_bih->biSizeImage );
                p_bih->biXPelsPerMeter = GetDWLE( p_data + 24 );
                input_AddInfo( p_cat, "X pixels per meter", "%d", p_bih->biXPelsPerMeter );
                p_bih->biYPelsPerMeter = GetDWLE( p_data + 28 );
                input_AddInfo( p_cat, "Y pixels per meter", "%d", p_bih->biYPelsPerMeter );
                p_bih->biClrUsed       = GetDWLE( p_data + 32 );
                p_bih->biClrImportant  = GetDWLE( p_data + 36 );

                if( i_size > sizeof( BITMAPINFOHEADER ) )
                {
                    memcpy( (uint8_t*)p_bih + sizeof( BITMAPINFOHEADER ),
                            p_data + sizeof( BITMAPINFOHEADER ),
                            i_size - sizeof( BITMAPINFOHEADER ) );
                }
            }

        }
        else
        {
            p_stream->i_cat = UNKNOWN_ES;
            msg_Dbg( p_input,
                    "ignoring unknown stream(ID:%d)",
                    p_sp->i_stream_number );
            p_stream->p_es->i_fourcc = VLC_FOURCC( 'u','n','d','f' );
        }
        p_stream->p_es->i_cat = p_stream->i_cat;

        vlc_mutex_lock( &p_input->stream.stream_lock );
        if( p_stream->p_es->i_fourcc != VLC_FOURCC( 'u','n','d','f' ) )
        {
            input_SelectES( p_input, p_stream->p_es );
        }
        vlc_mutex_unlock( &p_input->stream.stream_lock );
    }


    p_demux->i_data_begin = p_demux->root.p_data->i_object_pos + 50;
    if( p_demux->root.p_data->i_object_size != 0 )
    { // local file
        p_demux->i_data_end = p_demux->root.p_data->i_object_pos +
                                    p_demux->root.p_data->i_object_size;
    }
    else
    { // live/broacast
        p_demux->i_data_end = -1;
    }


    // go to first packet
    ASF_SeekAbsolute( p_input, p_demux->i_data_begin );

    vlc_mutex_lock( &p_input->stream.stream_lock );
    /* try to calculate movie time */
    if( p_demux->p_fp->i_data_packets_count > 0 )
    {
        int64_t i_count;
        mtime_t i_length;

        /* real number of packets */
        i_count = ( p_input->stream.p_selected_area->i_size -
                       p_demux->i_data_begin ) /
                            p_demux->p_fp->i_min_data_packet_size;
        /* calculate the time duration in s */
        i_length = (mtime_t)p_demux->p_fp->i_play_duration / 10 *
                   (mtime_t)i_count /
                   (mtime_t)p_demux->p_fp->i_data_packets_count /
                   (mtime_t)1000000;
        if( i_length > 0 )
        {
            p_input->stream.i_mux_rate =
                p_input->stream.p_selected_area->i_size / 50 / i_length;
        }
        else
        {
            p_input->stream.i_mux_rate = 0;
        }

    }
    else
    {
        /* cannot known */
        p_input->stream.i_mux_rate = 0;
    }



    p_input->stream.p_selected_program->b_is_ok = 1;
    vlc_mutex_unlock( &p_input->stream.stream_lock );


    return( 0 );
}

/*****************************************************************************
 * Demux: read packet and send them to decoders
 *****************************************************************************/
#define GETVALUE2b( bits, var, def ) \
    switch( (bits)&0x03 ) \
    { \
        case 1: var = p_peek[i_skip]; i_skip++; break; \
        case 2: var = GetWLE( p_peek + i_skip );  i_skip+= 2; break; \
        case 3: var = GetDWLE( p_peek + i_skip ); i_skip+= 4; break; \
        case 0: \
        default: var = def; break;\
    }

static int Demux( input_thread_t *p_input )
{
    demux_sys_t *p_demux = p_input->p_demux_data;
    int i;

    /* catch seek from user */
    if( p_input->stream.p_selected_program->i_synchro_state == SYNCHRO_REINIT )
    {
        off_t i_offset;
        msleep( DEFAULT_PTS_DELAY );
        i_offset = ASF_TellAbsolute( p_input ) - p_demux->i_data_begin;

        if( i_offset  < 0 )
        {
            i_offset = 0;
        }
        /* XXX work only when i_min_data_packet_size == i_max_data_packet_size */
        i_offset += p_demux->p_fp->i_min_data_packet_size -
                        i_offset % p_demux->p_fp->i_min_data_packet_size;
        ASF_SeekAbsolute( p_input, p_demux->i_data_begin + i_offset );

        p_demux->i_time = 0;
        for( i = 0; i < 128 ; i++ )
        {
#define p_stream p_demux->stream[i]
            if( p_stream )
            {
                p_stream->i_time = 0;
            }
#undef p_stream
        }
        p_demux->i_first_pts = -1;
    }


    for( i = 0; i < 10; i++ ) // parse 10 packets
    {
        int     i_data_packet_min = p_demux->p_fp->i_min_data_packet_size;
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


        if( input_Peek( p_input, &p_peek, i_data_packet_min ) < i_data_packet_min )
        {
            // EOF ?
            msg_Warn( p_input, "cannot peek while getting new packet, EOF ?" );
            return( 0 );
        }
        i_skip = 0;

        /* *** parse error correction if present *** */
        if( p_peek[0]&0x80 )
        {
            int i_error_correction_length_type;
            int i_error_correction_data_length;
            int i_opaque_data_present;

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

            i_pts = __MAX( i_pts - p_demux->p_fp->i_preroll * 1000, 0 );
            if( b_packet_multiple_payload )
            {
                GETVALUE2b( i_payload_length_type, i_payload_data_length, 0 );
            }
            else
            {
                i_payload_data_length = i_packet_length -
                                            i_packet_padding_length - i_skip;
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

            if( !( p_stream = p_demux->stream[i_stream_number] ) )
            {
                msg_Warn( p_input,
                          "undeclared stream[Id 0x%x]", i_stream_number );
                i_skip += i_payload_data_length;
                continue;   // over payload
            }

            if( !p_stream->p_es || !p_stream->p_es->p_decoder_fifo )
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
                if( p_stream->p_pes && i_media_object_offset == 0 )                 {
                    /* send complete packet to decoder */
                    if( p_stream->p_pes->i_pes_size > 0 )
                    {
                        input_DecodePES( p_stream->p_es->p_decoder_fifo, p_stream->p_pes );
                        p_stream->p_pes = NULL;
                    }
                }

                if( !p_stream->p_pes )  // add a new PES
                {
                    p_stream->i_time =
                        ( (mtime_t)i_pts + i_payload * (mtime_t)i_pts_delta );

                    if( p_demux->i_first_pts == -1 )
                    {
                        p_demux->i_first_pts = p_stream->i_time;
                    }
                    p_stream->i_time -= p_demux->i_first_pts;

                    p_stream->p_pes = input_NewPES( p_input->p_method_data );
                    p_stream->p_pes->i_dts =
                        p_stream->p_pes->i_pts =
                            input_ClockGetTS( p_input,
                                              p_input->stream.p_selected_program,
                                              ( p_stream->i_time+DEFAULT_PTS_DELAY) * 9 /100 );

                    p_stream->p_pes->p_next = NULL;
                    p_stream->p_pes->i_nb_data = 0;
                    p_stream->p_pes->i_pes_size = 0;
                }

                i_read = i_sub_payload_data_length + i_skip;
                if( input_SplitBuffer( p_input, &p_data, i_read ) < i_read )
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
                    if( input_Peek( p_input, &p_peek, i_packet_size_left ) < i_packet_size_left )
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
            if( !ASF_SkipBytes( p_input, i_packet_size_left ) )
            {
                msg_Warn( p_input, "cannot skip data, EOF ?" );
                return( 0 );
            }
        }

        continue;

loop_error_recovery:
        msg_Warn( p_input, "unsupported packet header" );
        if( p_demux->p_fp->i_min_data_packet_size != p_demux->p_fp->i_max_data_packet_size )
        {
            msg_Err( p_input, "unsupported packet header, fatal error" );
            return( -1 );
        }
        ASF_SkipBytes( p_input, i_data_packet_min );

    }   // loop over packet
    p_demux->i_time = -1;
    for( i = 0; i < 128 ; i++ )
    {
#define p_stream p_demux->stream[i]
        if( p_stream && p_stream->p_es && p_stream->p_es->p_decoder_fifo )
        {
            if( p_demux->i_time < 0 )
            {
                p_demux->i_time = p_stream->i_time;
            }
            else
            {
                p_demux->i_time = __MIN( p_demux->i_time, p_stream->i_time );
            }
        }
#undef p_stream
    }

    if( p_demux->i_time >= 0 )
    {
        /* update pcr XXX in mpeg scale so in 90000 unit/s */
        p_demux->i_pcr =( __MAX( p_demux->i_time /*- DEFAULT_PTS_DELAY*/, 0 ) ) * 9 / 100;

        /* first wait for the good time to read next packets */
        input_ClockManageRef( p_input,
                              p_input->stream.p_selected_program,
                              p_demux->i_pcr );
    }


    return( 1 );
}

/*****************************************************************************
 * MP4End: frees unused data
 *****************************************************************************/
static void Deactivate( vlc_object_t * p_this )
{
#define FREE( p ) \
    if( p ) { free( p ); }

    input_thread_t *  p_input = (input_thread_t *)p_this;
    demux_sys_t *p_demux = p_input->p_demux_data;
    int i_stream;

    msg_Dbg( p_input, "Freeing all memory" );
    ASF_FreeObjectRoot( p_input, &p_demux->root );
    for( i_stream = 0; i_stream < 128; i_stream++ )
    {
#define p_stream p_demux->stream[i_stream]
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

#undef FREE
}

