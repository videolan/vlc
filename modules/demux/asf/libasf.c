/*****************************************************************************
 * libasf.c : asf stream demux module for vlc
 *****************************************************************************
 * Copyright (C) 2001-2003 VideoLAN
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Gildas Bazin <gbazin@videolan.org>
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

#include <vlc/vlc.h>
#include <vlc/input.h>

#include "codecs.h"                        /* BITMAPINFOHEADER, WAVEFORMATEX */
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

/****************************************************************************
 *
 ****************************************************************************/
static int ASF_ReadObject( stream_t *, asf_object_t *,  asf_object_t * );

/****************************************************************************
 * GUID functions
 ****************************************************************************/
void ASF_GetGUID( guid_t *p_guid, uint8_t *p_data )
{
    p_guid->v1 = GetDWLE( p_data );
    p_guid->v2 = GetWLE( p_data + 4);
    p_guid->v3 = GetWLE( p_data + 6);
    memcpy( p_guid->v4, p_data + 8, 8 );
}

int ASF_CmpGUID( const guid_t *p_guid1, const guid_t *p_guid2 )
{
    if( (p_guid1->v1 != p_guid2->v1 )||
        (p_guid1->v2 != p_guid2->v2 )||
        (p_guid1->v3 != p_guid2->v3 )||
        ( memcmp( p_guid1->v4, p_guid2->v4,8 )) )
    {
        return( 0 );
    }
    return( 1 ); /* match */
}

/****************************************************************************
 *
 ****************************************************************************/
static int ASF_ReadObjectCommon( stream_t *s, asf_object_t *p_obj )
{
    asf_object_common_t *p_common = (asf_object_common_t*)p_obj;
    uint8_t             *p_peek;

    if( stream_Peek( s, &p_peek, 24 ) < 24 )
    {
        return( VLC_EGENERIC );
    }
    ASF_GetGUID( &p_common->i_object_id, p_peek );
    p_common->i_object_size = GetQWLE( p_peek + 16 );
    p_common->i_object_pos  = stream_Tell( s );
    p_common->p_next = NULL;

#ifdef ASF_DEBUG
    msg_Dbg( (vlc_object_t*)s,
             "found object guid: " GUID_FMT " size:"I64Fd,
             GUID_PRINT( p_common->i_object_id ),
             p_common->i_object_size );
#endif

    return( VLC_SUCCESS );
}

static int ASF_NextObject( stream_t *s, asf_object_t *p_obj )
{
    asf_object_t obj;
    if( p_obj == NULL )
    {
        if( ASF_ReadObjectCommon( s, &obj ) )
        {
            return( VLC_EGENERIC );
        }
        p_obj = &obj;
    }

    if( p_obj->common.i_object_size <= 0 )
    {
        return VLC_EGENERIC;
    }
    if( p_obj->common.p_father &&
        p_obj->common.p_father->common.i_object_size != 0 )
    {
        if( p_obj->common.p_father->common.i_object_pos +
            p_obj->common.p_father->common.i_object_size <
                p_obj->common.i_object_pos + p_obj->common.i_object_size + 24 )
                                /* 24 is min size of an object */
        {
            return VLC_EGENERIC;
        }

    }

    return stream_Seek( s, p_obj->common.i_object_pos +
                        p_obj->common.i_object_size );
}

static void ASF_FreeObject_Null( asf_object_t *pp_obj )
{
    return;
}

static int  ASF_ReadObject_Header( stream_t *s, asf_object_t *p_obj )
{
    asf_object_header_t *p_hdr = (asf_object_header_t*)p_obj;
    asf_object_t        *p_subobj;
    int                 i_peek;
    uint8_t             *p_peek;

    if( ( i_peek = stream_Peek( s, &p_peek, 30 ) ) < 30 )
    {
       return( VLC_EGENERIC );
    }

    p_hdr->i_sub_object_count = GetDWLE( p_peek + 24 );
    p_hdr->i_reserved1 = p_peek[28];
    p_hdr->i_reserved2 = p_peek[29];
    p_hdr->p_first = NULL;
    p_hdr->p_last  = NULL;

#ifdef ASF_DEBUG
    msg_Dbg( (vlc_object_t*)s,
             "read \"header object\" subobj:%d, reserved1:%d, reserved2:%d",
             p_hdr->i_sub_object_count,
             p_hdr->i_reserved1,
             p_hdr->i_reserved2 );
#endif

    /* Cannot fail as peek succeed */
    stream_Read( s, NULL, 30 );

    /* Now load sub object */
    for( ; ; )
    {
        p_subobj = malloc( sizeof( asf_object_t ) );

        if( ASF_ReadObject( s, p_subobj, (asf_object_t*)p_hdr ) )
        {
            free( p_subobj );
            break;
        }
        if( ASF_NextObject( s, p_subobj ) ) /* Go to the next object */
        {
            break;
        }
    }
    return VLC_SUCCESS;
}

static int ASF_ReadObject_Data( stream_t *s, asf_object_t *p_obj )
{
    asf_object_data_t *p_data = (asf_object_data_t*)p_obj;
    int               i_peek;
    uint8_t           *p_peek;

    if( ( i_peek = stream_Peek( s, &p_peek, 50 ) ) < 50 )
    {
       return VLC_EGENERIC;
    }
    ASF_GetGUID( &p_data->i_file_id, p_peek + 24 );
    p_data->i_total_data_packets = GetQWLE( p_peek + 40 );
    p_data->i_reserved = GetWLE( p_peek + 48 );

#ifdef ASF_DEBUG
    msg_Dbg( (vlc_object_t*)s,
             "read \"data object\" file_id:" GUID_FMT " total data packet:"
             I64Fd" reserved:%d",
             GUID_PRINT( p_data->i_file_id ),
             p_data->i_total_data_packets,
             p_data->i_reserved );
#endif

    return VLC_SUCCESS;
}

static int ASF_ReadObject_Index( stream_t *s, asf_object_t *p_obj )
{
    asf_object_index_t *p_index = (asf_object_index_t*)p_obj;
    int                i_peek;
    uint8_t            *p_peek;

    if( ( i_peek = stream_Peek( s, &p_peek, 56 ) ) < 56 )
    {
       return VLC_EGENERIC;
    }
    ASF_GetGUID( &p_index->i_file_id, p_peek + 24 );
    p_index->i_index_entry_time_interval = GetQWLE( p_peek + 40 );
    p_index->i_max_packet_count = GetDWLE( p_peek + 48 );
    p_index->i_index_entry_count = GetDWLE( p_peek + 52 );
    p_index->index_entry = NULL; /* FIXME */

#ifdef ASF_DEBUG
    msg_Dbg( (vlc_object_t*)s,
            "read \"index object\" file_id:" GUID_FMT
            " index_entry_time_interval:"I64Fd" max_packet_count:%d "
            "index_entry_count:%ld",
            GUID_PRINT( p_index->i_file_id ),
            p_index->i_index_entry_time_interval,
            p_index->i_max_packet_count,
            (long int)p_index->i_index_entry_count );
#endif

    return VLC_SUCCESS;
}

static void ASF_FreeObject_Index( asf_object_t *p_obj )
{
    asf_object_index_t *p_index = (asf_object_index_t*)p_obj;

    FREE( p_index->index_entry );
}

static int ASF_ReadObject_file_properties( stream_t *s, asf_object_t *p_obj )
{
    asf_object_file_properties_t *p_fp = (asf_object_file_properties_t*)p_obj;
    int      i_peek;
    uint8_t  *p_peek;

    if( ( i_peek = stream_Peek( s, &p_peek,  92) ) < 92 )
    {
       return VLC_EGENERIC;
    }
    ASF_GetGUID( &p_fp->i_file_id, p_peek + 24 );
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
    msg_Dbg( (vlc_object_t*)s,
            "read \"file properties object\" file_id:" GUID_FMT
            " file_size:"I64Fd" creation_date:"I64Fd" data_packets_count:"
            I64Fd" play_duration:"I64Fd" send_duration:"I64Fd" preroll:"
            I64Fd" flags:%d min_data_packet_size:%d max_data_packet_size:%d "
            "max_bitrate:%d",
            GUID_PRINT( p_fp->i_file_id ), p_fp->i_file_size,
            p_fp->i_creation_date, p_fp->i_data_packets_count,
            p_fp->i_play_duration, p_fp->i_send_duration,
            p_fp->i_preroll, p_fp->i_flags,
            p_fp->i_min_data_packet_size, p_fp->i_max_data_packet_size,
            p_fp->i_max_bitrate );
#endif

    return VLC_SUCCESS;
}

static void ASF_FreeObject_metadata( asf_object_t *p_obj )
{
    asf_object_metadata_t *p_meta =
        (asf_object_metadata_t *)p_obj;
    unsigned int i;

    for( i = 0; i < p_meta->i_record_entries_count; i++ )
    {
        if( p_meta->record[i].psz_name ) free( p_meta->record[i].psz_name );
        if( p_meta->record[i].p_data ) free( p_meta->record[i].p_data );
    }
    if( p_meta->record ) free( p_meta->record );
}

static int ASF_ReadObject_metadata( stream_t *s, asf_object_t *p_obj )
{
    asf_object_metadata_t *p_meta =
        (asf_object_metadata_t *)p_obj;

    int i_peek, i_entries, i;
    uint8_t *p_peek;

    p_meta->i_record_entries_count = 0;
    p_meta->record = 0;

    if( stream_Peek( s, &p_peek, p_meta->i_object_size ) <
        (int)p_meta->i_object_size )
    {
       return VLC_EGENERIC;
    }

    i_peek = 24;
    i_entries = GetWLE( p_peek + i_peek ); i_peek += 2;
    for( i = 0; i < i_entries && i_peek < (int)p_meta->i_object_size -12; i++ )
    {
        asf_metadata_record_t record;
        int i_name, i_data, j;

        if( GetWLE( p_peek + i_peek ) != 0 )
        {
            ASF_FreeObject_metadata( p_obj );
            return VLC_EGENERIC;
        }

        i_peek += 2;
        record.i_stream = GetWLE( p_peek + i_peek ); i_peek += 2;
        i_name = GetWLE( p_peek + i_peek ); i_peek += 2;
        record.i_type = GetWLE( p_peek + i_peek ); i_peek += 2;
        i_data = GetDWLE( p_peek + i_peek ); i_peek += 4;

        if( record.i_type > ASF_METADATA_TYPE_WORD ||
            i_peek + i_data + i_name > (int)p_meta->i_object_size )
        {
            ASF_FreeObject_metadata( p_obj );
            return VLC_EGENERIC;
        }

        record.i_val = 0;
        record.i_data = 0;
        record.p_data = 0;

        /* get name */
        record.psz_name = malloc( i_name/2 + 1 );
        for( j = 0; j < i_name/2; j++ )
        {
            record.psz_name[j] = GetWLE( p_peek + i_peek ); i_peek += 2;
        }
        record.psz_name[j] = 0; /* just to make sure */

        /* get data */
        if( record.i_type == ASF_METADATA_TYPE_STRING )
        {
            record.p_data = malloc( i_data/2 + 1 );
            record.i_data = i_data/2;
            for( j = 0; j < i_data/2; j++ )
            {
                record.p_data[j] = GetWLE( p_peek + i_peek ); i_peek += 2;
            }
            record.p_data[j] = 0; /* just to make sure */

#ifdef ASF_DEBUG
            msg_Dbg( s, "metadata: %s=%s", record.psz_name, record.p_data );
#endif
        }
        else if( record.i_type == ASF_METADATA_TYPE_BYTE )
        {
            record.p_data = malloc( i_data );
            record.i_data = i_data;
            memcpy( record.p_data, p_peek + i_peek, i_data );
            p_peek += i_data;

#ifdef ASF_DEBUG
            msg_Dbg( s, "metadata: %s (%i bytes)", record.psz_name,
                     record.i_data );
#endif
        }
        else
        {
            if( record.i_type == ASF_METADATA_TYPE_QWORD )
            {
                record.i_val = GetQWLE( p_peek + i_peek ); i_peek += 8;
            }
            else if( record.i_type == ASF_METADATA_TYPE_DWORD )
            {
                record.i_val = GetDWLE( p_peek + i_peek ); i_peek += 4;
            }
            else
            {
                record.i_val = GetWLE( p_peek + i_peek ); i_peek += 2;
            }

#ifdef ASF_DEBUG
            msg_Dbg( s, "metadata: %s=%i", record.psz_name, record.i_val );
#endif
        }

        p_meta->i_record_entries_count++;
        p_meta->record =
            realloc( p_meta->record, p_meta->i_record_entries_count *
                     sizeof(asf_metadata_record_t) );
        memcpy( &p_meta->record[p_meta->i_record_entries_count-1],
                &record, sizeof(asf_metadata_record_t) );
    }

    return VLC_SUCCESS;
}

static int ASF_ReadObject_header_extension( stream_t *s, asf_object_t *p_obj )
{
    asf_object_header_extension_t *p_he =
        (asf_object_header_extension_t *)p_obj;
    int     i_peek;
    uint8_t *p_peek;

    if( ( i_peek = stream_Peek( s, &p_peek, p_he->i_object_size ) ) <  46)
    {
       return VLC_EGENERIC;
    }
    ASF_GetGUID( &p_he->i_reserved1, p_peek + 24 );
    p_he->i_reserved2 = GetWLE( p_peek + 40 );
    p_he->i_header_extension_size = GetDWLE( p_peek + 42 );
    if( p_he->i_header_extension_size )
    {
        p_he->p_header_extension_data =
            malloc( p_he->i_header_extension_size );
        memcpy( p_he->p_header_extension_data, p_peek + 46,
                p_he->i_header_extension_size );
    }
    else
    {
        p_he->p_header_extension_data = NULL;
    }

#ifdef ASF_DEBUG
    msg_Dbg( (vlc_object_t*)s,
            "read \"header extension object\" reserved1:" GUID_FMT
            " reserved2:%d header_extension_size:%d",
            GUID_PRINT( p_he->i_reserved1 ), p_he->i_reserved2,
            p_he->i_header_extension_size );
#endif

    if( !p_he->i_header_extension_size ) return VLC_SUCCESS;

    /* Read the extension objects */
    stream_Read( s, NULL, 46 );
    for( ; ; )
    {
        asf_object_t *p_obj = malloc( sizeof( asf_object_t ) );

        if( ASF_ReadObject( s, p_obj, (asf_object_t*)p_he ) )
        {
            free( p_obj );
            break;
        }

        if( ASF_NextObject( s, p_obj ) ) /* Go to the next object */
        {
            break;
        }
    }

    return VLC_SUCCESS;
}

static void ASF_FreeObject_header_extension( asf_object_t *p_obj )
{
    asf_object_header_extension_t *p_he =
        (asf_object_header_extension_t *)p_obj;

    FREE( p_he->p_header_extension_data );
}

static int ASF_ReadObject_stream_properties( stream_t *s, asf_object_t *p_obj )
{
    asf_object_stream_properties_t *p_sp =
                    (asf_object_stream_properties_t*)p_obj;
    int     i_peek;
    uint8_t *p_peek;

    if( ( i_peek = stream_Peek( s, &p_peek,  p_sp->i_object_size ) ) < 74 )
    {
       return VLC_EGENERIC;
    }
    ASF_GetGUID( &p_sp->i_stream_type, p_peek + 24 );
    ASF_GetGUID( &p_sp->i_error_correction_type, p_peek + 40 );
    p_sp->i_time_offset = GetQWLE( p_peek + 56 );
    p_sp->i_type_specific_data_length = GetDWLE( p_peek + 64 );
    p_sp->i_error_correction_data_length = GetDWLE( p_peek + 68 );
    p_sp->i_flags = GetWLE( p_peek + 72 );
        p_sp->i_stream_number = p_sp->i_flags&0x07f;
    p_sp->i_reserved = GetDWLE( p_peek + 74 );
    if( p_sp->i_type_specific_data_length )
    {
        p_sp->p_type_specific_data =
            malloc( p_sp->i_type_specific_data_length );
        memcpy( p_sp->p_type_specific_data, p_peek + 78,
                p_sp->i_type_specific_data_length );
    }
    else
    {
        p_sp->p_type_specific_data = NULL;
    }
    if( p_sp->i_error_correction_data_length )
    {
        p_sp->p_error_correction_data =
            malloc( p_sp->i_error_correction_data_length );
        memcpy( p_sp->p_error_correction_data,
                p_peek + 78 + p_sp->i_type_specific_data_length,
                p_sp->i_error_correction_data_length );
    }
    else
    {
        p_sp->p_error_correction_data = NULL;
    }

#ifdef ASF_DEBUG
    msg_Dbg( (vlc_object_t*)s,
            "read \"stream Properties object\" stream_type:" GUID_FMT
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
    return VLC_SUCCESS;
}

static void ASF_FreeObject_stream_properties( asf_object_t *p_obj )
{
    asf_object_stream_properties_t *p_sp =
                (asf_object_stream_properties_t*)p_obj;

    FREE( p_sp->p_type_specific_data );
    FREE( p_sp->p_error_correction_data );
}


static int ASF_ReadObject_codec_list( stream_t *s, asf_object_t *p_obj )
{
    asf_object_codec_list_t *p_cl = (asf_object_codec_list_t*)p_obj;
    int     i_peek;
    uint8_t *p_peek, *p_data;

    unsigned int i_codec;

    if( ( i_peek = stream_Peek( s, &p_peek, p_cl->i_object_size ) ) < 44 )
    {
       return VLC_EGENERIC;
    }

    ASF_GetGUID( &p_cl->i_reserved, p_peek + 24 );
    p_cl->i_codec_entries_count = GetWLE( p_peek + 40 );
    if( p_cl->i_codec_entries_count > 0 )
    {

        p_cl->codec = calloc( p_cl->i_codec_entries_count,
                              sizeof( asf_codec_entry_t ) );
        memset( p_cl->codec, 0,
                p_cl->i_codec_entries_count * sizeof( asf_codec_entry_t ) );

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
    msg_Dbg( s, "read \"codec list object\" reserved_guid:" GUID_FMT
             " codec_entries_count:%d",
            GUID_PRINT( p_cl->i_reserved ), p_cl->i_codec_entries_count );

    for( i_codec = 0; i_codec < p_cl->i_codec_entries_count; i_codec++ )
    {
#define codec p_cl->codec[i_codec]
        msg_Dbg( s, "read \"codec list object\" codec[%d] %s name:\"%s\" "
                 "description:\"%s\" information_length:%d",
                 i_codec, ( codec.i_type == ASF_CODEC_TYPE_VIDEO ) ?
                 "video" : ( ( codec.i_type == ASF_CODEC_TYPE_AUDIO ) ?
                 "audio" : "unknown" ),
                 codec.psz_name, codec.psz_description,
                 codec.i_information_length );
#undef  codec
    }
#endif

    return VLC_SUCCESS;
}

static void ASF_FreeObject_codec_list( asf_object_t *p_obj )
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

/* Microsoft should go to hell. This time the length give number of bytes
 * and for the some others object, length give char16 count ... */
static int ASF_ReadObject_content_description(stream_t *s, asf_object_t *p_obj)
{
    asf_object_content_description_t *p_cd =
        (asf_object_content_description_t *)p_obj;
    uint8_t *p_peek, *p_data;
    int i_peek;
    int i_len, i_title, i_author, i_copyright, i_description, i_rating;

#define GETSTRINGW( psz_str, i_size ) \
   psz_str = calloc( i_size/2 + 1, sizeof( char ) ); \
   for( i_len = 0; i_len < i_size/2; i_len++ ) \
   { \
       psz_str[i_len] = GetWLE( p_data + 2*i_len ); \
   } \
   psz_str[i_size/2] = '\0'; \
   p_data += i_size;

    if( ( i_peek = stream_Peek( s, &p_peek, p_cd->i_object_size ) ) < 34 )
    {
       return VLC_EGENERIC;
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
    msg_Dbg( (vlc_object_t*)s,
             "Read \"content description object\" title:\"%s\" author:\"%s\" copyright:\"%s\" description:\"%s\" rating:\"%s\"",
             p_cd->psz_title,
             p_cd->psz_author,
             p_cd->psz_copyright,
             p_cd->psz_description,
             p_cd->psz_rating );
#endif
    return VLC_SUCCESS;
}

static void ASF_FreeObject_content_description( asf_object_t *p_obj)
{
    asf_object_content_description_t *p_cd =
        (asf_object_content_description_t *)p_obj;

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
    int     (*ASF_ReadObject_function)( stream_t *, asf_object_t *p_obj );
    void    (*ASF_FreeObject_function)( asf_object_t *p_obj );

} ASF_Object_Function [] =
{
    { &asf_object_header_guid, ASF_OBJECT_TYPE_HEADER,
      ASF_ReadObject_Header, ASF_FreeObject_Null },
    { &asf_object_data_guid, ASF_OBJECT_TYPE_DATA,
      ASF_ReadObject_Data, ASF_FreeObject_Null },
    { &asf_object_index_guid, ASF_OBJECT_TYPE_INDEX,
      ASF_ReadObject_Index, ASF_FreeObject_Index },
    { &asf_object_file_properties_guid, ASF_OBJECT_TYPE_FILE_PROPERTIES,
      ASF_ReadObject_file_properties, ASF_FreeObject_Null },
    { &asf_object_stream_properties_guid, ASF_OBJECT_TYPE_STREAM_PROPERTIES,
      ASF_ReadObject_stream_properties,ASF_FreeObject_stream_properties },
    { &asf_object_header_extension_guid, ASF_OBJECT_TYPE_HEADER_EXTENSION,
      ASF_ReadObject_header_extension, ASF_FreeObject_header_extension},
    { &asf_object_metadata_guid, ASF_OBJECT_TYPE_METADATA,
      ASF_ReadObject_metadata, ASF_FreeObject_metadata},
    { &asf_object_codec_list_guid, ASF_OBJECT_TYPE_CODEC_LIST,
      ASF_ReadObject_codec_list, ASF_FreeObject_codec_list },
    { &asf_object_marker_guid, ASF_OBJECT_TYPE_MARKER,
      NULL, NULL },
    { &asf_object_content_description_guid, ASF_OBJECT_TYPE_CONTENT_DESCRIPTION,
      ASF_ReadObject_content_description, ASF_FreeObject_content_description },

    { &asf_object_null_guid, 0, NULL, NULL }
};

static int ASF_ReadObject( stream_t *s, asf_object_t *p_obj,
                           asf_object_t *p_father )
{
    int i_result;
    int i_index;

    if( !p_obj ) return( 0 );

    if( ASF_ReadObjectCommon( s, p_obj ) )
    {
        msg_Warn( (vlc_object_t*)s, "cannot read one asf object" );
        return VLC_EGENERIC;
    }
    p_obj->common.p_father = p_father;
    p_obj->common.p_first = NULL;
    p_obj->common.p_next = NULL;
    p_obj->common.p_last = NULL;

    if( p_obj->common.i_object_size < 24 )
    {
        msg_Warn( (vlc_object_t*)s, "found a corrupted asf object (size<24)" );
        return VLC_EGENERIC;
    }

    /* find this object */
    for( i_index = 0; ; i_index++ )
    {
        if( ASF_CmpGUID( ASF_Object_Function[i_index].p_id,
                         &p_obj->common.i_object_id ) ||
            ASF_CmpGUID( ASF_Object_Function[i_index].p_id,
                         &asf_object_null_guid ) )
        {
            break;
        }
    }
    p_obj->common.i_type = ASF_Object_Function[i_index].i_type;

    /* Now load this object */
    if( ASF_Object_Function[i_index].ASF_ReadObject_function == NULL )
    {
        msg_Warn( (vlc_object_t*)s, "unknown asf object (not loaded)" );
        i_result = VLC_SUCCESS;
    }
    else
    {
        /* XXX ASF_ReadObject_function realloc *pp_obj XXX */
        i_result =
          (ASF_Object_Function[i_index].ASF_ReadObject_function)( s, p_obj );
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

static void ASF_FreeObject( stream_t *s, asf_object_t *p_obj )
{
    int i_index;
    asf_object_t *p_child;

    if( !p_obj ) return;

    /* Free all child object */
    p_child = p_obj->common.p_first;
    while( p_child )
    {
        asf_object_t *p_next;
        p_next = p_child->common.p_next;
        ASF_FreeObject( s, p_child );
        p_child = p_next;
    }

    /* find this object */
    for( i_index = 0; ; i_index++ )
    {
        if( ASF_CmpGUID( ASF_Object_Function[i_index].p_id,
                     &p_obj->common.i_object_id )||
            ASF_CmpGUID( ASF_Object_Function[i_index].p_id,
                     &asf_object_null_guid ) )
        {
            break;
        }
    }

    /* Now free this object */
    if( ASF_Object_Function[i_index].ASF_FreeObject_function == NULL )
    {
        msg_Warn( (vlc_object_t*)s,
                  "unknown asf object " GUID_FMT,
                  GUID_PRINT( p_obj->common.i_object_id ) );
    }
    else
    {
#ifdef ASF_DEBUG
        msg_Dbg( (vlc_object_t*)s,
                  "free asf object " GUID_FMT,
                  GUID_PRINT( p_obj->common.i_object_id ) );
#endif
        (ASF_Object_Function[i_index].ASF_FreeObject_function)( p_obj );
    }
    free( p_obj );
    return;
}

/*****************************************************************************
 * ASF_ReadObjetRoot : parse the entire stream/file
 *****************************************************************************/
asf_object_root_t *ASF_ReadObjectRoot( stream_t *s, int b_seekable )
{
    asf_object_root_t *p_root = malloc( sizeof( asf_object_root_t ) );
    asf_object_t *p_obj;

    p_root->i_type = ASF_OBJECT_TYPE_ROOT;
    memcpy( &p_root->i_object_id, &asf_object_null_guid, sizeof( guid_t ) );
    p_root->i_object_pos = stream_Tell( s );
    p_root->i_object_size = 0;
    p_root->p_first = NULL;
    p_root->p_last  = NULL;
    p_root->p_next  = NULL;
    p_root->p_hdr   = NULL;
    p_root->p_data  = NULL;
    p_root->p_fp    = NULL;
    p_root->p_index = NULL;
    p_root->p_metadata = NULL;

    for( ; ; )
    {
        p_obj = malloc( sizeof( asf_object_t ) );

        if( ASF_ReadObject( s, p_obj, (asf_object_t*)p_root ) )
        {
            free( p_obj );
            break;
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
                msg_Warn( (vlc_object_t*)s, "unknow object found" );
                break;
        }
        if( p_obj->common.i_type == ASF_OBJECT_TYPE_DATA &&
            p_obj->common.i_object_size <= 50 )
        {
            /* probably a dump of broadcasted asf */
            break;
        }
        if( !b_seekable && p_root->p_hdr && p_root->p_data )
        {
            /* For unseekable stream it's enough to play */
            break;
        }

        if( ASF_NextObject( s, p_obj ) ) /* Go to the next object */
        {
            break;
        }
    }

    if( p_root->p_hdr != NULL && p_root->p_data != NULL )
    {
        p_root->p_fp = ASF_FindObject( p_root->p_hdr,
                                       &asf_object_file_properties_guid, 0 );

        if( p_root->p_fp )
        {
            asf_object_t *p_hdr_ext =
                ASF_FindObject( p_root->p_hdr,
                                &asf_object_header_extension_guid, 0 );
            if( p_hdr_ext )
            {
                p_root->p_metadata =
                    ASF_FindObject( p_hdr_ext,
                                    &asf_object_metadata_guid, 0 );
            }

            return p_root;
        }
        msg_Warn( s, "cannot find file properties object" );
    }

    /* Invalid file */
    ASF_FreeObjectRoot( s, p_root );
    return NULL;
}

void ASF_FreeObjectRoot( stream_t *s, asf_object_root_t *p_root )
{
    asf_object_t *p_obj;

    p_obj = p_root->p_first;
    while( p_obj )
    {
        asf_object_t *p_next;
        p_next = p_obj->common.p_next;
        ASF_FreeObject( s, p_obj );
        p_obj = p_next;
    }
    free( p_root );
}

int  __ASF_CountObject( asf_object_t *p_obj, const guid_t *p_guid )
{
    int i_count;
    asf_object_t *p_child;

    if( !p_obj ) return( 0 );

    i_count = 0;
    p_child = p_obj->common.p_first;
    while( p_child )
    {
        if( ASF_CmpGUID( &p_child->common.i_object_id, p_guid ) )
        {
            i_count++;
        }
        p_child = p_child->common.p_next;
    }
    return( i_count );
}

void *__ASF_FindObject( asf_object_t *p_obj, const guid_t *p_guid,
                        int i_number )
{
    asf_object_t *p_child;

    p_child = p_obj->common.p_first;

    while( p_child )
    {
        if( ASF_CmpGUID( &p_child->common.i_object_id, p_guid ) )
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
