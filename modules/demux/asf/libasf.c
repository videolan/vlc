/*****************************************************************************
 * libasf.c :
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: libasf.c,v 1.12 2003/03/14 00:24:08 sigmunau Exp $
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

#include <stdlib.h>                                      /* malloc(), free() */
#include <string.h>                                              /* strdup() */
#include <errno.h>
#include <sys/types.h>

#include <vlc/vlc.h>
#include <vlc/input.h>

#include "libasf.h"

#define ASF_DEBUG 1

#define FREE( p ) \
    if( p ) {free( p ); p = NULL; }

#define GUID_FMT "0x%x-0x%x-0x%x-0x%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x"
#define GUID_PRINT( guid )  \
    (guid).v1,              \
    (guid).v2,              \
    (guid).v3,              \
    (guid).v4[0],(guid).v4[1],(guid).v4[2],(guid).v4[3],    \
    (guid).v4[4],(guid).v4[5],(guid).v4[6],(guid).v4[7]

/* Some functions to manipulate memory */
static uint16_t GetWLE( uint8_t *p_buff )
{
    return( (p_buff[0]) + ( p_buff[1] <<8 ) );
}

static uint32_t GetDWLE( uint8_t *p_buff )
{
    return( p_buff[0] + ( p_buff[1] <<8 ) +
            ( p_buff[2] <<16 ) + ( p_buff[3] <<24 ) );
}

static uint64_t GetQWLE( uint8_t *p_buff )
{
    return( ( (uint64_t)GetDWLE( p_buff ) )|
            ( (uint64_t)GetDWLE( p_buff + 4 ) << 32) );
}

void GetGUID( guid_t *p_guid, uint8_t *p_data )
{
    p_guid->v1 = GetDWLE( p_data );
    p_guid->v2 = GetWLE( p_data + 4);
    p_guid->v3 = GetWLE( p_data + 6);
    memcpy( p_guid->v4, p_data + 8, 8 );
}

int CmpGUID( const guid_t *p_guid1, const guid_t *p_guid2 )
{
    if( (p_guid1->v1 != p_guid2->v1 )||(p_guid1->v2 != p_guid2->v2 )||
        (p_guid1->v3 != p_guid2->v3 )||
        ( memcmp( p_guid1->v4, p_guid2->v4,8 )) )
    {
        return( 0 );
    }
    else
    {
        return( 1 ); /* match */
    }
}
/*****************************************************************************
 * Some basic functions to manipulate stream more easily in vlc
 *
 * ASF_TellAbsolute get file position
 *
 * ASF_SeekAbsolute seek in the file
 *
 * ASF_ReadData read data from the file in a buffer
 *
 *****************************************************************************/
off_t ASF_TellAbsolute( input_thread_t *p_input )
{
    off_t i_pos;

    vlc_mutex_lock( &p_input->stream.stream_lock );

    i_pos= p_input->stream.p_selected_area->i_tell;
//           - ( p_input->p_last_data - p_input->p_current_data  );

    vlc_mutex_unlock( &p_input->stream.stream_lock );

    return( i_pos );
}

int ASF_SeekAbsolute( input_thread_t *p_input,
                      off_t i_pos)
{
    off_t i_filepos;

    i_filepos = ASF_TellAbsolute( p_input );
    if( i_pos == i_filepos )
    {
        return( 1 );
    }

    if( !p_input->stream.b_seekable && i_pos < i_filepos )
    {
        msg_Err( p_input, "cannot seek" );
        return( 0 );
    }

    if( p_input->stream.b_seekable &&
        ( p_input->stream.i_method == INPUT_METHOD_FILE ||
          i_pos < i_filepos ||
          i_pos - i_filepos > 10000 ) )
    {
        input_AccessReinit( p_input );
        p_input->pf_seek( p_input, i_pos );
        return( 1 );
    }
    else if( i_pos > i_filepos )
    {
        uint64_t i_size = i_pos - i_filepos;
        do
        {
            data_packet_t *p_data;
            int i_read;

            i_read =
                input_SplitBuffer(p_input, &p_data, __MIN( i_size, 1024 ) );
            if( i_read <= 0 )
            {
                return( 0 );
            }
            input_DeletePacket( p_input->p_method_data, p_data );
            i_size -= i_read;

        } while( i_size > 0 );
    }
    return( 1 );
}

/* return 1 if success, 0 if fail */
int ASF_ReadData( input_thread_t *p_input, uint8_t *p_buff, int i_size )
{
    data_packet_t *p_data;

    int i_read;


    if( !i_size )
    {
        return( 1 );
    }

    do
    {
        i_read = input_SplitBuffer(p_input, &p_data, __MIN( i_size, 1024 ) );
        if( i_read <= 0 )
        {
            return( 0 );
        }
        memcpy( p_buff, p_data->p_payload_start, i_read );
        input_DeletePacket( p_input->p_method_data, p_data );

        p_buff += i_read;
        i_size -= i_read;

    } while( i_size );

    return( 1 );
}

int  ASF_SkipBytes( input_thread_t *p_input, int i_count )
{
    return( ASF_SeekAbsolute( p_input,
                              ASF_TellAbsolute( p_input ) + i_count ) );
}

/****************************************************************************/
int  ASF_ReadObjectCommon( input_thread_t *p_input,
                           asf_object_t *p_obj )
{
    asf_object_common_t *p_common = (asf_object_common_t*)p_obj;
    uint8_t             *p_peek;

    if( input_Peek( p_input, &p_peek, 24 ) < 24 )
    {
        return( 0 );
    }
    GetGUID( &p_common->i_object_id, p_peek );
    p_common->i_object_size = GetQWLE( p_peek + 16 );
    p_common->i_object_pos = ASF_TellAbsolute( p_input );
    p_common->p_next = NULL;
#ifdef ASF_DEBUG
    msg_Dbg(p_input,
            "Found Object guid: " GUID_FMT " size:"I64Fd,
            GUID_PRINT( p_common->i_object_id ),
            p_common->i_object_size );
#endif

    return( 1 );
}

int ASF_NextObject( input_thread_t *p_input,
                    asf_object_t *p_obj )
{
    asf_object_t obj;
    if( !p_obj )
    {
        if( !ASF_ReadObjectCommon( p_input, &obj ) )
        {
            return( 0 );
        }
        p_obj = &obj;
    }

    if( !p_obj->common.i_object_size )
    {
        return( 0 ); /* failed */
    }
    if( p_obj->common.p_father && p_obj->common.p_father->common.i_object_size != 0 )
    {
        if( p_obj->common.p_father->common.i_object_pos + p_obj->common.p_father->common.i_object_size <
                p_obj->common.i_object_pos + p_obj->common.i_object_size + 24 )
                                /* 24 is min size of an object */
        {
            return( 0 );
        }

    }
    return( ASF_SeekAbsolute( p_input,
                              p_obj->common.i_object_pos + p_obj->common.i_object_size ) );
}

int  ASF_GotoObject( input_thread_t *p_input,
                     asf_object_t *p_obj )
{
    if( !p_obj )
    {
        return( 0 );
    }
    return( ASF_SeekAbsolute( p_input, p_obj->common.i_object_pos ) );
}


void ASF_FreeObject_Null( input_thread_t *p_input,
                            asf_object_t *pp_obj )
{


}

int  ASF_ReadObject_Header( input_thread_t *p_input,
                            asf_object_t *p_obj )
{
    asf_object_header_t *p_hdr = (asf_object_header_t*)p_obj;
    asf_object_t        *p_subobj;
    int                 i_peek;
    uint8_t             *p_peek;

    if( ( i_peek = input_Peek( p_input, &p_peek, 30 ) ) < 30 )
    {
       return( 0 );
    }
    p_hdr->i_sub_object_count = GetDWLE( p_peek + 24 );
    p_hdr->i_reserved1 = p_peek[28];
    p_hdr->i_reserved2 = p_peek[29];
    p_hdr->p_first = NULL;
    p_hdr->p_last  = NULL;
#ifdef ASF_DEBUG
    msg_Dbg(p_input,
            "Read \"Header Object\" subobj:%d, reserved1:%d, reserved2:%d",
            p_hdr->i_sub_object_count,
            p_hdr->i_reserved1,
            p_hdr->i_reserved2 );
#endif
    ASF_SkipBytes( p_input, 30 );
    /* Now load sub object */
    for( ; ; )
    {
        p_subobj  = malloc( sizeof( asf_object_t ) );

        if( !( ASF_ReadObject( p_input, p_subobj, (asf_object_t*)p_hdr ) ) )
        {
            break;
        }
        if( !ASF_NextObject( p_input, p_subobj ) ) /* Go to the next object */
        {
            break;
        }
    }
    return( 1 );
}

int  ASF_ReadObject_Data( input_thread_t *p_input,
                          asf_object_t *p_obj )
{
    asf_object_data_t *p_data = (asf_object_data_t*)p_obj;
    int               i_peek;
    uint8_t           *p_peek;

    if( ( i_peek = input_Peek( p_input, &p_peek, 50 ) ) < 50 )
    {
       return( 0 );
    }
    GetGUID( &p_data->i_file_id, p_peek + 24 );
    p_data->i_total_data_packets = GetQWLE( p_peek + 40 );
    p_data->i_reserved = GetWLE( p_peek + 48 );
#ifdef ASF_DEBUG
    msg_Dbg( p_input,
            "Read \"Data Object\" file_id:" GUID_FMT " total data packet:"
            I64Fd" reserved:%d",
            GUID_PRINT( p_data->i_file_id ),
            p_data->i_total_data_packets,
            p_data->i_reserved );
#endif
    return( 1 );
}

int  ASF_ReadObject_Index( input_thread_t *p_input,
                           asf_object_t *p_obj )
{
    asf_object_index_t *p_index = (asf_object_index_t*)p_obj;
    int                i_peek;
    uint8_t            *p_peek;

    if( ( i_peek = input_Peek( p_input, &p_peek, 56 ) ) < 56 )
    {
       return( 0 );
    }
    GetGUID( &p_index->i_file_id, p_peek + 24 );
    p_index->i_index_entry_time_interval = GetQWLE( p_peek + 40 );
    p_index->i_max_packet_count = GetDWLE( p_peek + 48 );
    p_index->i_index_entry_count = GetDWLE( p_peek + 52 );
    p_index->index_entry = NULL; /* FIXME */

#ifdef ASF_DEBUG
    msg_Dbg( p_input,
            "Read \"Index Object\" file_id:" GUID_FMT
            " index_entry_time_interval:"I64Fd" max_packet_count:%d "
            "index_entry_count:%ld",
            GUID_PRINT( p_index->i_file_id ),
            p_index->i_index_entry_time_interval,
            p_index->i_max_packet_count,
            (long int)p_index->i_index_entry_count );
#endif
    return( 1 );
}
void ASF_FreeObject_Index( input_thread_t *p_input,
                          asf_object_t *p_obj )
{
    asf_object_index_t *p_index = (asf_object_index_t*)p_obj;

    FREE( p_index->index_entry );
}

int  ASF_ReadObject_file_properties( input_thread_t *p_input,
                                     asf_object_t *p_obj )
{
    asf_object_file_properties_t *p_fp = (asf_object_file_properties_t*)p_obj;
    int      i_peek;
    uint8_t  *p_peek;

    if( ( i_peek = input_Peek( p_input, &p_peek,  92) ) < 92 )
    {
       return( 0 );
    }
    GetGUID( &p_fp->i_file_id, p_peek + 24 );
    p_fp->i_file_size = GetQWLE( p_peek + 40 );
    p_fp->i_creation_date = GetQWLE( p_peek + 48 );
    p_fp->i_data_packets_count = GetQWLE( p_peek + 56 );
    p_fp->i_play_duration = GetQWLE( p_peek + 64 );
    p_fp->i_send_duration = GetQWLE( p_peek + 72 );
    p_fp->i_preroll = GetQWLE( p_peek + 80 );
    p_fp->i_flags = GetDWLE( p_peek + 88 );
    p_fp->i_min_data_packet_size = GetDWLE( p_peek + 92 );
    p_fp->i_max_data_packet_size = GetDWLE( p_peek + 96 );
    p_fp->i_max_bitrate = GetDWLE( p_peek + 100 );

#ifdef ASF_DEBUG
    msg_Dbg( p_input,
            "Read \"File Properties Object\" file_id:" GUID_FMT
            " file_size:"I64Fd" creation_date:"I64Fd" data_packets_count:"
            I64Fd" play_duration:"I64Fd" send_duration:"I64Fd" preroll:"
            I64Fd" flags:%d min_data_packet_size:%d max_data_packet_size:%d "
            "max_bitrate:%d",
            GUID_PRINT( p_fp->i_file_id ),
            p_fp->i_file_size,
            p_fp->i_creation_date,
            p_fp->i_data_packets_count,
            p_fp->i_play_duration,
            p_fp->i_send_duration,
            p_fp->i_preroll,
            p_fp->i_flags,
            p_fp->i_min_data_packet_size,
            p_fp->i_max_data_packet_size,
            p_fp->i_max_bitrate );
#endif
    return( 1 );
}

int  ASF_ReadObject_header_extention( input_thread_t *p_input,
                                      asf_object_t *p_obj )
{
    asf_object_header_extention_t *p_he = (asf_object_header_extention_t*)p_obj;
    int     i_peek;
    uint8_t *p_peek;

    if( ( i_peek = input_Peek( p_input, &p_peek, p_he->i_object_size ) ) <  46)
    {
       return( 0 );
    }
    GetGUID( &p_he->i_reserved1, p_peek + 24 );
    p_he->i_reserved2 = GetWLE( p_peek + 40 );
    p_he->i_header_extention_size = GetDWLE( p_peek + 42 );
    if( p_he->i_header_extention_size )
    {
        p_he->p_header_extention_data = malloc( p_he->i_header_extention_size );
        memcpy( p_he->p_header_extention_data,
                p_peek + 46,
                p_he->i_header_extention_size );
    }
    else
    {
        p_he->p_header_extention_data = NULL;
    }
#ifdef ASF_DEBUG
    msg_Dbg( p_input,
            "Read \"Header Extention Object\" reserved1:" GUID_FMT " reserved2:%d header_extention_size:%d",
            GUID_PRINT( p_he->i_reserved1 ),
            p_he->i_reserved2,
            p_he->i_header_extention_size );
#endif
    return( 1 );
}
void ASF_FreeObject_header_extention( input_thread_t *p_input,
                                      asf_object_t *p_obj )
{
    asf_object_header_extention_t *p_he = (asf_object_header_extention_t*)p_obj;

    FREE( p_he->p_header_extention_data );
}

int  ASF_ReadObject_stream_properties( input_thread_t *p_input,
                                       asf_object_t *p_obj )
{
    asf_object_stream_properties_t *p_sp =
                    (asf_object_stream_properties_t*)p_obj;
    int     i_peek;
    uint8_t *p_peek;

    if( ( i_peek = input_Peek( p_input, &p_peek,  p_sp->i_object_size ) ) < 74 )
    {
       return( 0 );
    }
    GetGUID( &p_sp->i_stream_type, p_peek + 24 );
    GetGUID( &p_sp->i_error_correction_type, p_peek + 40 );
    p_sp->i_time_offset = GetQWLE( p_peek + 56 );
    p_sp->i_type_specific_data_length = GetDWLE( p_peek + 64 );
    p_sp->i_error_correction_data_length = GetDWLE( p_peek + 68 );
    p_sp->i_flags = GetWLE( p_peek + 72 );
        p_sp->i_stream_number = p_sp->i_flags&0x07f;
    p_sp->i_reserved = GetDWLE( p_peek + 74 );
    if( p_sp->i_type_specific_data_length )
    {
        p_sp->p_type_specific_data = malloc( p_sp->i_type_specific_data_length );
        memcpy( p_sp->p_type_specific_data,
                p_peek + 78,
                p_sp->i_type_specific_data_length );
    }
    else
    {
        p_sp->p_type_specific_data = NULL;
    }
    if( p_sp->i_error_correction_data_length )
    {
        p_sp->p_error_correction_data = malloc( p_sp->i_error_correction_data_length );
        memcpy( p_sp->p_error_correction_data,
                p_peek + 78 + p_sp->i_type_specific_data_length,
                p_sp->i_error_correction_data_length );
    }
    else
    {
        p_sp->p_error_correction_data = NULL;
    }

#ifdef ASF_DEBUG
    msg_Dbg( p_input,
            "Read \"Stream Properties Object\" stream_type:" GUID_FMT
            " error_correction_type:" GUID_FMT " time_offset:"I64Fd
            " type_specific_data_length:%d error_correction_data_length:%d"
            " flags:0x%x stream_number:%d",
            GUID_PRINT( p_sp->i_stream_type ),
            GUID_PRINT( p_sp->i_error_correction_type ),
            p_sp->i_time_offset,
            p_sp->i_type_specific_data_length,
            p_sp->i_error_correction_data_length,
            p_sp->i_flags,
            p_sp->i_stream_number );

#endif
    return( 1 );
}

void ASF_FreeObject_stream_properties( input_thread_t *p_input,
                                      asf_object_t *p_obj )
{
    asf_object_stream_properties_t *p_sp =
                (asf_object_stream_properties_t*)p_obj;

    FREE( p_sp->p_type_specific_data );
    FREE( p_sp->p_error_correction_data );
}


int  ASF_ReadObject_codec_list( input_thread_t *p_input,
                                asf_object_t *p_obj )
{
    asf_object_codec_list_t *p_cl = (asf_object_codec_list_t*)p_obj;
    int     i_peek;
    uint8_t *p_peek, *p_data;

    unsigned int i_codec;

    if( ( i_peek = input_Peek( p_input, &p_peek, p_cl->i_object_size ) ) < 44 )
    {
       return( 0 );
    }

    GetGUID( &p_cl->i_reserved, p_peek + 24 );
    p_cl->i_codec_entries_count = GetWLE( p_peek + 40 );
    if( p_cl->i_codec_entries_count > 0 )
    {

        p_cl->codec = calloc( p_cl->i_codec_entries_count, sizeof( asf_codec_entry_t ) );
        memset( p_cl->codec, 0, p_cl->i_codec_entries_count * sizeof( asf_codec_entry_t ) );

        p_data = p_peek + 44;
        for( i_codec = 0; i_codec < p_cl->i_codec_entries_count; i_codec++ )
        {
#define codec p_cl->codec[i_codec]
            int i_len, i;

            codec.i_type = GetWLE( p_data ); p_data += 2;
            /* codec name */
            i_len = GetWLE( p_data ); p_data += 2;
            codec.psz_name = calloc( sizeof( char ), i_len + 1);
            for( i = 0; i < i_len; i++ )
            {
                codec.psz_name[i] = GetWLE( p_data + 2*i );
            }
            codec.psz_name[i_len] = '\0';
            p_data += 2 * i_len;

            /* description */
            i_len = GetWLE( p_data ); p_data += 2;
            codec.psz_description = calloc( sizeof( char ), i_len + 1);
            for( i = 0; i < i_len; i++ )
            {
                codec.psz_description[i] = GetWLE( p_data + 2*i );
            }
            codec.psz_description[i_len] = '\0';
            p_data += 2 * i_len;

            /* opaque information */
            codec.i_information_length = GetWLE( p_data ); p_data += 2;
            if( codec.i_information_length > 0 )
            {
                codec.p_information = malloc( codec.i_information_length );
                memcpy( codec.p_information, p_data, codec.i_information_length );
                p_data += codec.i_information_length;
            }
            else
            {
                codec.p_information = NULL;
            }
#undef  codec
        }

    }
    else
    {
        p_cl->codec = NULL;
    }

#ifdef ASF_DEBUG
    msg_Dbg( p_input,
            "Read \"Codec List Object\" reserved_guid:" GUID_FMT " codec_entries_count:%d",
            GUID_PRINT( p_cl->i_reserved ),
            p_cl->i_codec_entries_count );
    for( i_codec = 0; i_codec < p_cl->i_codec_entries_count; i_codec++ )
    {
        char psz_cat[sizeof("Stream ")+10];
        input_info_category_t *p_cat;
        sprintf( psz_cat, "Stream %d", i_codec );
        p_cat = input_InfoCategory( p_input, psz_cat);

#define codec p_cl->codec[i_codec]
        input_AddInfo( p_cat, _("Codec name"), codec.psz_name );
        input_AddInfo( p_cat, _("Codec description"), codec.psz_description );
        msg_Dbg( p_input,
                 "Read \"Codec List Object\" codec[%d] %s name:\"%s\" description:\"%s\" information_length:%d",
                 i_codec,
                 ( codec.i_type == ASF_CODEC_TYPE_VIDEO ) ? "video" : ( ( codec.i_type == ASF_CODEC_TYPE_AUDIO ) ? "audio" : "unknown" ),
                 codec.psz_name,
                 codec.psz_description,
                 codec.i_information_length );

#undef  codec
    }
#endif
    return( 1 );
}
void ASF_FreeObject_codec_list( input_thread_t *p_input,
                                asf_object_t *p_obj )
{
    asf_object_codec_list_t *p_cl = (asf_object_codec_list_t*)p_obj;
    unsigned int i_codec;

    for( i_codec = 0; i_codec < p_cl->i_codec_entries_count; i_codec++ )
    {
#define codec p_cl->codec[i_codec]
        FREE( codec.psz_name );
        FREE( codec.psz_description );
        FREE( codec.p_information );

#undef  codec
    }
    FREE( p_cl->codec );
}

/* Microsoft should qo to hell. This time the length give number of bytes
 * and for the some others object, length give char16 count ... */
int  ASF_ReadObject_content_description( input_thread_t *p_input,
                                         asf_object_t *p_obj )
{
    asf_object_content_description_t *p_cd =
                                    (asf_object_content_description_t*)p_obj;
    int     i_peek;
    uint8_t *p_peek, *p_data;

    int i_len;
    int i_title;
    int i_author;
    int i_copyright;
    int i_description;
    int i_rating;

#define GETSTRINGW( psz_str, i_size ) \
   psz_str = calloc( i_size/2 + 1, sizeof( char ) ); \
   for( i_len = 0; i_len < i_size/2; i_len++ ) \
   { \
       psz_str[i_len] = GetWLE( p_data + 2*i_len ); \
   } \
   psz_str[i_size/2] = '\0'; \
   p_data += i_size;

    if( ( i_peek = input_Peek( p_input, &p_peek, p_cd->i_object_size ) ) < 34 )
    {
       return( 0 );
    }
    p_data = p_peek + 24;

    i_title = GetWLE( p_data ); p_data += 2;
    i_author= GetWLE( p_data ); p_data += 2;
    i_copyright     = GetWLE( p_data ); p_data += 2;
    i_description   = GetWLE( p_data ); p_data += 2;
    i_rating        = GetWLE( p_data ); p_data += 2;

    GETSTRINGW( p_cd->psz_title, i_title );
    GETSTRINGW( p_cd->psz_author, i_author );
    GETSTRINGW( p_cd->psz_copyright, i_copyright );
    GETSTRINGW( p_cd->psz_description, i_description );
    GETSTRINGW( p_cd->psz_rating, i_rating );

#undef  GETSTRINGW

#ifdef ASF_DEBUG
    {
        input_info_category_t *p_cat = input_InfoCategory( p_input, _("Asf") );
        input_AddInfo( p_cat, _("Title"), p_cd->psz_title );
        input_AddInfo( p_cat, _("Author"), p_cd->psz_author );
        input_AddInfo( p_cat, _("Copyright"), p_cd->psz_copyright );
        input_AddInfo( p_cat, _("Description"), p_cd->psz_description );
        input_AddInfo( p_cat, _("Rating"), p_cd->psz_rating );
    }
    msg_Dbg( p_input,
             "Read \"Content Description Object\" title:\"%s\" author:\"%s\" copyright:\"%s\" description:\"%s\" rating:\"%s\"",
             p_cd->psz_title,
             p_cd->psz_author,
             p_cd->psz_copyright,
             p_cd->psz_description,
             p_cd->psz_rating );
#endif
    return( 1 );
}

void ASF_FreeObject_content_description( input_thread_t *p_input,
                                         asf_object_t *p_obj )
{
    asf_object_content_description_t *p_cd = (asf_object_content_description_t*)p_obj;

    FREE( p_cd->psz_title );
    FREE( p_cd->psz_author );
    FREE( p_cd->psz_copyright );
    FREE( p_cd->psz_description );
    FREE( p_cd->psz_rating );
}

static struct
{
    const guid_t  *p_id;
    int     i_type;
    int     (*ASF_ReadObject_function)( input_thread_t *p_input,
                                        asf_object_t *p_obj );
    void    (*ASF_FreeObject_function)( input_thread_t *p_input,
                                        asf_object_t *p_obj );
} ASF_Object_Function [] =
{
    { &asf_object_header_guid,            ASF_OBJECT_TYPE_HEADER,             ASF_ReadObject_Header, ASF_FreeObject_Null },
    { &asf_object_data_guid,              ASF_OBJECT_TYPE_DATA,               ASF_ReadObject_Data,   ASF_FreeObject_Null },
    { &asf_object_index_guid,             ASF_OBJECT_TYPE_INDEX,              ASF_ReadObject_Index,  ASF_FreeObject_Index },
    { &asf_object_file_properties_guid,   ASF_OBJECT_TYPE_FILE_PROPERTIES,    ASF_ReadObject_file_properties,  ASF_FreeObject_Null },
    { &asf_object_stream_properties_guid, ASF_OBJECT_TYPE_STREAM_PROPERTIES,  ASF_ReadObject_stream_properties,ASF_FreeObject_stream_properties },
    { &asf_object_header_extention_guid,  ASF_OBJECT_TYPE_EXTENTION_HEADER,   ASF_ReadObject_header_extention, ASF_FreeObject_header_extention},
    { &asf_object_codec_list_guid,        ASF_OBJECT_TYPE_CODEC_LIST,         ASF_ReadObject_codec_list,       ASF_FreeObject_codec_list },
    { &asf_object_marker_guid,            ASF_OBJECT_TYPE_MARKER,             NULL,                  NULL },
    { &asf_object_content_description_guid, ASF_OBJECT_TYPE_CONTENT_DESCRIPTION, ASF_ReadObject_content_description, ASF_FreeObject_content_description },

    { &asf_object_null_guid,   0,                      NULL,                  NULL }
};

int  ASF_ReadObject( input_thread_t *p_input,
                     asf_object_t *p_obj,
                     asf_object_t *p_father )
{
    int i_result;
    int i_index;

    if( !p_obj )
    {
        return( 0 );
    }
    if( !ASF_ReadObjectCommon( p_input, p_obj ) )
    {
        msg_Warn( p_input, "Cannot read one asf object" );
        return( 0 );
    }
    p_obj->common.p_father = p_father;
    p_obj->common.p_first = NULL;
    p_obj->common.p_next = NULL;
    p_obj->common.p_last = NULL;


    if( p_obj->common.i_object_size < 24 )
    {
        msg_Warn( p_input, "Found a corrupted asf object (size<24)" );
        return( 0 );
    }
    /* find this object */
    for( i_index = 0; ; i_index++ )
    {
        if( CmpGUID( ASF_Object_Function[i_index].p_id,
                     &p_obj->common.i_object_id )||
            CmpGUID( ASF_Object_Function[i_index].p_id,
                     &asf_object_null_guid ) )
        {
            break;
        }
    }
    p_obj->common.i_type = ASF_Object_Function[i_index].i_type;

    /* Now load this object */
    if( ASF_Object_Function[i_index].ASF_ReadObject_function == NULL )
    {
        msg_Warn( p_input, "Unknown asf object (not loaded)" );
        i_result = 1;
    }
    else
    {
        /* XXX ASF_ReadObject_function realloc *pp_obj XXX */
        i_result =
            (ASF_Object_Function[i_index].ASF_ReadObject_function)( p_input,
                                                                    p_obj );
    }

    /* link this object with father */
    if( p_father )
    {
        if( p_father->common.p_first )
        {
            p_father->common.p_last->common.p_next = p_obj;
        }
        else
        {
            p_father->common.p_first = p_obj;
        }
        p_father->common.p_last = p_obj;
    }

    return( i_result );
}

void ASF_FreeObject( input_thread_t *p_input,
                     asf_object_t *p_obj )
{
    int i_index;
    asf_object_t *p_child;

    if( !p_obj )
    {
        return;
    }

    /* Free all child object */
    p_child = p_obj->common.p_first;
    while( p_child )
    {
        asf_object_t *p_next;
        p_next = p_child->common.p_next;
        ASF_FreeObject( p_input, p_child );
        p_child = p_next;
    }

    /* find this object */
    for( i_index = 0; ; i_index++ )
    {
        if( CmpGUID( ASF_Object_Function[i_index].p_id,
                     &p_obj->common.i_object_id )||
            CmpGUID( ASF_Object_Function[i_index].p_id,
                     &asf_object_null_guid ) )
        {
            break;
        }
    }

    /* Now free this object */
    if( ASF_Object_Function[i_index].ASF_FreeObject_function == NULL )
    {
        msg_Warn( p_input,
                  "Unknown asf object " GUID_FMT,
                  GUID_PRINT( p_obj->common.i_object_id ) );
    }
    else
    {
#ifdef ASF_DEBUG
        msg_Dbg( p_input,
                  "Free asf object " GUID_FMT,
                  GUID_PRINT( p_obj->common.i_object_id ) );
#endif
        (ASF_Object_Function[i_index].ASF_FreeObject_function)( p_input,
                                                                p_obj );
    }
    free( p_obj );
    return;
}

/*****************************************************************************
 * ASF_ReadObjetRoot : parse the entire stream/file
 *****************************************************************************/
int ASF_ReadObjectRoot( input_thread_t *p_input,
                        asf_object_root_t *p_root,
                        int b_seekable )
{
    asf_object_t *p_obj;

    p_root->i_type = ASF_OBJECT_TYPE_ROOT;
    memcpy( &p_root->i_object_id, &asf_object_null_guid, sizeof( guid_t ) );
    p_root->i_object_pos = 0;
    p_root->i_object_size = p_input->stream.p_selected_area->i_size;
    p_root->p_first = NULL;
    p_root->p_last = NULL;
    p_root->p_next = NULL;
    p_root->p_hdr = NULL;
    p_root->p_data = NULL;
    p_root->p_index = NULL;

    for( ; ; )
    {
        p_obj  = malloc( sizeof( asf_object_t ) );

        if( !( ASF_ReadObject( p_input, p_obj, (asf_object_t*)p_root ) ) )
        {
            return( 1 );
        }
        switch( p_obj->common.i_type )
        {
            case( ASF_OBJECT_TYPE_HEADER ):
                p_root->p_hdr = (asf_object_header_t*)p_obj;
                break;
            case( ASF_OBJECT_TYPE_DATA ):
                p_root->p_data = (asf_object_data_t*)p_obj;
                break;
            case( ASF_OBJECT_TYPE_INDEX ):
                p_root->p_index = (asf_object_index_t*)p_obj;
                break;
            default:
                msg_Warn( p_input, "Unknow Object found" );
                break;
        }
        if( !b_seekable && ( p_root->p_hdr && p_root->p_data ) )
        {
            /* For unseekable stream it's enouth to play */
            return( 1 );
        }

        if( !ASF_NextObject( p_input, p_obj ) ) /* Go to the next object */
        {
            return( 1 );
        }
    }
}

void ASF_FreeObjectRoot( input_thread_t *p_input,
                         asf_object_root_t *p_root )
{
    asf_object_t *p_obj;

    p_obj = p_root->p_first;
    while( p_obj )
    {
        asf_object_t *p_next;
        p_next = p_obj->common.p_next;
        ASF_FreeObject( p_input, p_obj );
        p_obj = p_next;
    }
    p_root->p_first = NULL;
    p_root->p_last = NULL;
    p_root->p_next = NULL;

    p_root->p_hdr = NULL;
    p_root->p_data = NULL;
    p_root->p_index = NULL;

}

int  __ASF_CountObject( asf_object_t *p_obj, const guid_t *p_guid )
{
    int i_count;
    asf_object_t *p_child;

    if( !p_obj )
    {
        return( 0 );
    }

    i_count = 0;
    p_child = p_obj->common.p_first;
    while( p_child )
    {
        if( CmpGUID( &p_child->common.i_object_id, p_guid ) )
        {
            i_count++;
        }
        p_child = p_child->common.p_next;
    }
    return( i_count );
}

void *__ASF_FindObject( asf_object_t *p_obj, const guid_t *p_guid, int i_number )
{
    asf_object_t *p_child;

    p_child = p_obj->common.p_first;

    while( p_child )
    {
        if( CmpGUID( &p_child->common.i_object_id, p_guid ) )
        {
            if( i_number == 0 )
            {
                /* We found it */
                return( p_child );
            }

            i_number--;
        }
        p_child = p_child->common.p_next;
    }
    return( NULL );
}


