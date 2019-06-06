/*****************************************************************************
 * asf.c: MMS access plug-in
 *****************************************************************************
 * Copyright (C) 2001-2004 VLC authors and VideoLAN
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_rand.h>

#include "asf.h"
#include "buffer.h"
#include "../../demux/asf/libasf_guid.h"

void  GenerateGuid ( vlc_guid_t *p_guid )
{
    p_guid->Data1 = 0xbabac001;
    vlc_rand_bytes(&p_guid->Data2, sizeof(p_guid->Data2));
    vlc_rand_bytes(&p_guid->Data3, sizeof(p_guid->Data3));
    vlc_rand_bytes(p_guid->Data4, sizeof(p_guid->Data4));
}

void  asf_HeaderParse ( asf_header_t *hdr,
                            uint8_t *p_header, int i_header )
{
    var_buffer_t buffer;
    vlc_guid_t  guid;
    uint64_t    i_size;

    hdr->i_file_size = 0;
    hdr->i_data_packets_count = 0;
    hdr->i_min_data_packet_size = 0;
    for( unsigned i = 0; i < 128; i++ )
    {
        hdr->stream[i].i_cat = ASF_CODEC_TYPE_UNKNOWN;
        hdr->stream[i].i_selected = 0;
        hdr->stream[i].i_bitrate = -1;
    }

    var_buffer_initread( &buffer, p_header, i_header );
    var_buffer_getguid( &buffer, &guid );

    if( !guidcmp( &guid, &asf_object_header_guid ) )
    {
        /* ERROR: */
    }
    var_buffer_getmemory( &buffer, NULL, 30 - 16 );

    for( ;; )
    {
        var_buffer_getguid( &buffer, &guid );
        i_size = var_buffer_get64( &buffer );

        if( guidcmp( &guid, &asf_object_file_properties_guid ) )
        {
            var_buffer_getmemory( &buffer, NULL, 16 );
            hdr->i_file_size            = var_buffer_get64( &buffer );
            var_buffer_getmemory( &buffer, NULL, 8 );
            hdr->i_data_packets_count   = var_buffer_get64( &buffer );
            var_buffer_getmemory( &buffer, NULL, 8+8+8+4);
            hdr->i_min_data_packet_size = var_buffer_get32( &buffer );

            var_buffer_getmemory( &buffer, NULL, i_size - 24 - 16 - 8 - 8 - 8 - 8-8-8-4 - 4);
        }
        else if( guidcmp( &guid, &asf_object_header_extension_guid ) )
        {
            /* Enter it */
            var_buffer_getmemory( &buffer, NULL, 46 - 24 );
        }
        else if( guidcmp( &guid, &asf_object_extended_stream_properties_guid ) )
        {
            /* Grrrrrr */
            int16_t i_count1, i_count2;
            int i_subsize;

            var_buffer_getmemory( &buffer, NULL, 84 - 24 );

            i_count1 = var_buffer_get16( &buffer );
            i_count2 = var_buffer_get16( &buffer );

            i_subsize = 88;
            for( int i = 0; i < i_count1; i++ )
            {
                int i_len;

                var_buffer_get16( &buffer );
                i_len = var_buffer_get16( &buffer );
                var_buffer_getmemory( &buffer, NULL, i_len );

                i_subsize += 4 + i_len;
            }

            for( int i = 0; i < i_count2; i++ )
            {
                int i_len;
                var_buffer_getmemory( &buffer, NULL, 16 + 2 );
                i_len = var_buffer_get32( &buffer );
                var_buffer_getmemory( &buffer, NULL, i_len );

                i_subsize += 16 + 6 + i_len;
            }

            if( i_size - i_subsize <= 24 )
            {
                var_buffer_getmemory( &buffer, NULL, i_size - i_subsize );
            }
            /* It's a hack we just skip the first part of the object until
             * the embed stream properties if any (ugly, but whose fault ?) */
        }
        else if( guidcmp( &guid, &asf_object_stream_properties_guid ) )
        {
            int     i_stream_id;
            vlc_guid_t  stream_type;

            var_buffer_getguid( &buffer, &stream_type );
            var_buffer_getmemory( &buffer, NULL, 32 );

            i_stream_id = var_buffer_get8( &buffer ) & 0x7f;
            var_buffer_getmemory( &buffer, NULL, i_size - 24 - 32 - 16 - 1);

            if( guidcmp( &stream_type, &asf_object_stream_type_video ) )
            {
                hdr->stream[i_stream_id].i_cat = ASF_CODEC_TYPE_VIDEO;
            }
            else if( guidcmp( &stream_type, &asf_object_stream_type_audio ) )
            {
                hdr->stream[i_stream_id].i_cat = ASF_CODEC_TYPE_AUDIO;
            }
            else
            {
                hdr->stream[i_stream_id].i_cat = ASF_CODEC_TYPE_UNKNOWN;
            }
        }
        else if ( guidcmp( &guid, &asf_object_stream_bitrate_properties ) )
        {
            int     i_count;
            uint8_t i_stream_id;

            i_count = var_buffer_get16( &buffer );
            i_size -= 2;
            while( i_count > 0 )
            {
                i_stream_id = var_buffer_get16( &buffer )&0x7f;
                hdr->stream[i_stream_id].i_bitrate =  var_buffer_get32( &buffer );
                i_count--;
                i_size -= 6;
            }
            var_buffer_getmemory( &buffer, NULL, i_size - 24 );
        }
        else
        {
            // skip unknown guid
            var_buffer_getmemory( &buffer, NULL, i_size - 24 );
        }

        if( var_buffer_readempty( &buffer ) )
            return;
    }
}

void  asf_StreamSelect  ( asf_header_t *hdr,
                              int i_bitrate_max,
                              bool b_all, bool b_audio, bool b_video )
{
    /* XXX FIXME use mututal eclusion information */
    unsigned i;
    int i_audio, i_video;
    int i_bitrate_total;
#if 0
    char *psz_stream;
#endif

    i_audio = 0;
    i_video = 0;
    i_bitrate_total = 0;
    if( b_all )
    {
        /* select all valid stream */
        for( i = 1; i < 128; i++ )
        {
            if( hdr->stream[i].i_cat != ASF_CODEC_TYPE_UNKNOWN )
            {
                hdr->stream[i].i_selected = 1;
            }
        }
        return;
    }
    else
    {
        for( i = 0; i < 128; i++ )
        {
            /* by default, not selected */
            hdr->stream[i].i_selected = 0;
        }
    }

    /* big test:
     * select a stream if
     *    - no audio nor video stream
     *    - or:
     *         - if i_bitrate_max not set keep the highest bitrate
     *         - if i_bitrate_max is set, keep stream that make we used best
     *           quality regarding i_bitrate_max
     *
     * XXX: little buggy:
     *        - it doesn't use mutual exclusion info..
     *        - when selecting a better stream we could select
     *        something that make i_bitrate_total> i_bitrate_max
     */
    for( i = 1; i < 128; i++ )
    {
        if( hdr->stream[i].i_cat == ASF_CODEC_TYPE_UNKNOWN )
        {
            continue;
        }
        else if( hdr->stream[i].i_cat == ASF_CODEC_TYPE_AUDIO && b_audio &&
                 ( i_audio <= 0 ||
                    ( ( ( hdr->stream[i].i_bitrate > hdr->stream[i_audio].i_bitrate &&
                          ( i_bitrate_total + hdr->stream[i].i_bitrate - hdr->stream[i_audio].i_bitrate
                                            < i_bitrate_max || !i_bitrate_max) ) ||
                        ( hdr->stream[i].i_bitrate < hdr->stream[i_audio].i_bitrate &&
                              i_bitrate_max != 0 && i_bitrate_total > i_bitrate_max )
                      ) )  ) )
        {
            /* unselect old stream */
            if( i_audio > 0 )
            {
                hdr->stream[i_audio].i_selected = 0;
                if( hdr->stream[i_audio].i_bitrate> 0 )
                {
                    i_bitrate_total -= hdr->stream[i_audio].i_bitrate;
                }
            }

            hdr->stream[i].i_selected = 1;
            if( hdr->stream[i].i_bitrate> 0 )
            {
                i_bitrate_total += hdr->stream[i].i_bitrate;
            }
            i_audio = i;
        }
        else if( hdr->stream[i].i_cat == ASF_CODEC_TYPE_VIDEO && b_video &&
                 ( i_video <= 0 ||
                    (
                        ( ( hdr->stream[i].i_bitrate > hdr->stream[i_video].i_bitrate &&
                            ( i_bitrate_total + hdr->stream[i].i_bitrate - hdr->stream[i_video].i_bitrate
                                            < i_bitrate_max || !i_bitrate_max) ) ||
                          ( hdr->stream[i].i_bitrate < hdr->stream[i_video].i_bitrate &&
                            i_bitrate_max != 0 && i_bitrate_total > i_bitrate_max )
                        ) ) )  )
        {
            /* unselect old stream */
            if( i_video > 0 )
            {
                hdr->stream[i_video].i_selected = 0;
                if( hdr->stream[i_video].i_bitrate> 0 )
                {
                    i_bitrate_total -= hdr->stream[i_video].i_bitrate;
                }
            }

            hdr->stream[i].i_selected = 1;
            if( hdr->stream[i].i_bitrate> 0 )
            {
                i_bitrate_total += hdr->stream[i].i_bitrate;
            }
            i_video = i;
        }

    }
}
