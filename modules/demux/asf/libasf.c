/*****************************************************************************
 * libasf.c : asf stream demux module for vlc
 *****************************************************************************
 * Copyright (C) 2001-2003 the VideoLAN team
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>


#include <vlc_demux.h>

#include <vlc_codecs.h>                   /* BITMAPINFOHEADER, WAVEFORMATEX */
#include "libasf.h"

#define ASF_DEBUG 1

#define GUID_FMT "0x%x-0x%x-0x%x-0x%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x"
#define GUID_PRINT( guid )  \
    (guid).v1,              \
    (guid).v2,              \
    (guid).v3,              \
    (guid).v4[0],(guid).v4[1],(guid).v4[2],(guid).v4[3],    \
    (guid).v4[4],(guid).v4[5],(guid).v4[6],(guid).v4[7]

/* Helpers:
 * They ensure that invalid reads will not create problems.
 * They are expansion safe
 * They make the following assumptions:
 *  const uint8_t *p_peek exists and points to the start of a buffer
 *  int i_peek gives the size of the buffer pointed by p_peek
 *  const uint8_t *p_data exits and points to the data inside p_peek to be read.
 */
/* ASF_HAVE(n):
 *  Check that n bytes can be read */
static inline bool AsfObjectHelperHave( const uint8_t *p_peek, int i_peek, const uint8_t *p_current, int i_wanted )
{
    if( i_wanted < 0 || i_wanted > i_peek )
        return false;
    return &p_current[i_wanted] <= &p_peek[i_peek];
}
#define ASF_HAVE(n) AsfObjectHelperHave( p_peek, i_peek, p_data, n )

/* ASF_SKIP(n)
 *  Skip n bytes if possible */
static inline void AsfObjectHelperSkip( const uint8_t *p_peek, int i_peek, uint8_t **pp_data, int i_wanted )
{
    if( AsfObjectHelperHave( p_peek, i_peek, *pp_data, i_wanted ) )
        *pp_data += i_wanted;
    else
        *pp_data = (uint8_t*)&p_peek[i_peek];
}
#define ASF_SKIP(n) AsfObjectHelperSkip( p_peek, i_peek, (uint8_t**)&p_data, n )

/* ASF_READX()
 *  Read X byte if possible, else return 0 */
#define ASF_FUNCTION_READ_X(type, x, cmd ) \
static inline type AsfObjectHelperRead##x( const uint8_t *p_peek, int i_peek, uint8_t **pp_data ) { \
    uint8_t *p_data = *pp_data; \
    type i_ret = 0;  \
    if( ASF_HAVE(x) )   \
        i_ret = cmd;    \
    ASF_SKIP(x);        \
    *pp_data = p_data;  \
    return i_ret;   }
ASF_FUNCTION_READ_X( uint8_t,  1, *p_data )
ASF_FUNCTION_READ_X( uint16_t, 2, GetWLE(p_data) )
ASF_FUNCTION_READ_X( uint32_t, 4, GetDWLE(p_data) )
ASF_FUNCTION_READ_X( uint64_t, 8, GetQWLE(p_data) )
#define ASF_READ1() AsfObjectHelperRead1( p_peek, i_peek, (uint8_t**)&p_data )
#define ASF_READ2() AsfObjectHelperRead2( p_peek, i_peek, (uint8_t**)&p_data )
#define ASF_READ4() AsfObjectHelperRead4( p_peek, i_peek, (uint8_t**)&p_data )
#define ASF_READ8() AsfObjectHelperRead8( p_peek, i_peek, (uint8_t**)&p_data )

/* ASF_READS(n)
 *  Read a string of n/2 wchar long ie n bytes. Do a stupid conversion (suppose latin1)
 *  Return allocated "" if not possible */
static char *AsfObjectHelperReadString( const uint8_t *p_peek, int i_peek, uint8_t **pp_data, int i_size )
{
    uint8_t *p_data = *pp_data;
    char *psz_string;
    if( ASF_HAVE(i_size) )
    {
        psz_string = calloc( i_size/2 + 1, sizeof( char ) );
        if( psz_string )
        {
            int i;
            for( i = 0; i < i_size/2; i++ )
                psz_string[i] = GetWLE( &p_data[2*i] );
            psz_string[i_size/2] = '\0'; \
        }
    }
    else
    {
        psz_string = strdup("");
    }
    ASF_SKIP(i_size);
    *pp_data = p_data;
    return psz_string;
}
#define ASF_READS(n) AsfObjectHelperReadString( p_peek, i_peek, (uint8_t**)&p_data, n )

/****************************************************************************
 *
 ****************************************************************************/
static int ASF_ReadObject( stream_t *, asf_object_t *,  asf_object_t * );

/****************************************************************************
 * GUID functions
 ****************************************************************************/
void ASF_GetGUID( guid_t *p_guid, const uint8_t *p_data )
{
    p_guid->v1 = GetDWLE( p_data );
    p_guid->v2 = GetWLE( p_data + 4);
    p_guid->v3 = GetWLE( p_data + 6);
    memcpy( p_guid->v4, p_data + 8, 8 );
}

bool ASF_CmpGUID( const guid_t *p_guid1, const guid_t *p_guid2 )
{
    if( (p_guid1->v1 != p_guid2->v1 )||
        (p_guid1->v2 != p_guid2->v2 )||
        (p_guid1->v3 != p_guid2->v3 )||
        ( memcmp( p_guid1->v4, p_guid2->v4,8 )) )
    {
        return false;
    }
    return true;
}

/****************************************************************************
 *
 ****************************************************************************/
static int ASF_ReadObjectCommon( stream_t *s, asf_object_t *p_obj )
{
    asf_object_common_t *p_common = &p_obj->common;
    const uint8_t *p_peek;

    if( stream_Peek( s, &p_peek, 24 ) < 24 )
        return VLC_EGENERIC;

    ASF_GetGUID( &p_common->i_object_id, p_peek );
    p_common->i_object_size = GetQWLE( p_peek + 16 );
    p_common->i_object_pos  = stream_Tell( s );
    p_common->p_next = NULL;

#ifdef ASF_DEBUG
    msg_Dbg( s,
             "found object guid: " GUID_FMT " size:%"PRId64,
             GUID_PRINT( p_common->i_object_id ),
             p_common->i_object_size );
#endif

    return VLC_SUCCESS;
}

static int ASF_NextObject( stream_t *s, asf_object_t *p_obj )
{
    asf_object_t obj;
    if( p_obj == NULL )
    {
        if( ASF_ReadObjectCommon( s, &obj ) )
            return VLC_EGENERIC;

        p_obj = &obj;
    }

    if( p_obj->common.i_object_size <= 0 )
        return VLC_EGENERIC;

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
    VLC_UNUSED(pp_obj);
}

static int  ASF_ReadObject_Header( stream_t *s, asf_object_t *p_obj )
{
    asf_object_header_t *p_hdr = &p_obj->header;
    asf_object_t        *p_subobj;
    int                 i_peek;
    const uint8_t       *p_peek;

    if( ( i_peek = stream_Peek( s, &p_peek, 30 ) ) < 30 )
       return VLC_EGENERIC;

    p_hdr->i_sub_object_count = GetDWLE( p_peek + 24 );
    p_hdr->i_reserved1 = p_peek[28];
    p_hdr->i_reserved2 = p_peek[29];
    p_hdr->p_first = NULL;
    p_hdr->p_last  = NULL;

#ifdef ASF_DEBUG
    msg_Dbg( s,
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

        if( !p_subobj || ASF_ReadObject( s, p_subobj, (asf_object_t*)p_hdr ) )
        {
            free( p_subobj );
            break;
        }
        if( ASF_NextObject( s, p_subobj ) ) /* Go to the next object */
            break;
    }
    return VLC_SUCCESS;
}

static int ASF_ReadObject_Data( stream_t *s, asf_object_t *p_obj )
{
    asf_object_data_t *p_data = &p_obj->data;
    int               i_peek;
    const uint8_t     *p_peek;

    if( ( i_peek = stream_Peek( s, &p_peek, 50 ) ) < 50 )
       return VLC_EGENERIC;

    ASF_GetGUID( &p_data->i_file_id, p_peek + 24 );
    p_data->i_total_data_packets = GetQWLE( p_peek + 40 );
    p_data->i_reserved = GetWLE( p_peek + 48 );

#ifdef ASF_DEBUG
    msg_Dbg( s,
             "read \"data object\" file_id:" GUID_FMT " total data packet:"
             "%"PRId64" reserved:%d",
             GUID_PRINT( p_data->i_file_id ),
             p_data->i_total_data_packets,
             p_data->i_reserved );
#endif

    return VLC_SUCCESS;
}

static int ASF_ReadObject_Index( stream_t *s, asf_object_t *p_obj )
{
    asf_object_index_t *p_index = &p_obj->index;
    const uint8_t      *p_peek;
    unsigned int       i;

    /* We just ignore error on the index */
    if( stream_Peek( s, &p_peek, p_index->i_object_size ) <
        __MAX( (int64_t)p_index->i_object_size, 56 ) )
        return VLC_SUCCESS;

    ASF_GetGUID( &p_index->i_file_id, p_peek + 24 );
    p_index->i_index_entry_time_interval = GetQWLE( p_peek + 40 );
    p_index->i_max_packet_count = GetDWLE( p_peek + 48 );
    p_index->i_index_entry_count = GetDWLE( p_peek + 52 );
    p_index->index_entry = NULL;

#ifdef ASF_DEBUG
    msg_Dbg( s,
            "read \"index object\" file_id:" GUID_FMT
            " index_entry_time_interval:%"PRId64" max_packet_count:%d "
            "index_entry_count:%ld",
            GUID_PRINT( p_index->i_file_id ),
            p_index->i_index_entry_time_interval,
            p_index->i_max_packet_count,
            (long int)p_index->i_index_entry_count );
#endif

    /* Sanity checking */
    if( p_index->i_index_entry_count > (p_index->i_object_size - 56) / 6 )
        p_index->i_index_entry_count = (p_index->i_object_size - 56) / 6;

    p_index->index_entry = calloc( p_index->i_index_entry_count,
                                   sizeof(asf_index_entry_t) );
    if( !p_index->index_entry )
        return VLC_ENOMEM;

    for( i = 0, p_peek += 56; i < p_index->i_index_entry_count; i++, p_peek += 6 )
    {
        p_index->index_entry[i].i_packet_number = GetDWLE( p_peek );
        p_index->index_entry[i].i_packet_count = GetDWLE( p_peek + 4 );
    }

    return VLC_SUCCESS;
}

static void ASF_FreeObject_Index( asf_object_t *p_obj )
{
    asf_object_index_t *p_index = &p_obj->index;

    FREENULL( p_index->index_entry );
}

static int ASF_ReadObject_file_properties( stream_t *s, asf_object_t *p_obj )
{
    asf_object_file_properties_t *p_fp = &p_obj->file_properties;
    int           i_peek;
    const uint8_t *p_peek;

    if( ( i_peek = stream_Peek( s, &p_peek,  104 ) ) < 104 )
       return VLC_EGENERIC;

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
    msg_Dbg( s,
            "read \"file properties object\" file_id:" GUID_FMT
            " file_size:%"PRId64" creation_date:%"PRId64" data_packets_count:"
            "%"PRId64" play_duration:%"PRId64" send_duration:%"PRId64" preroll:%"PRId64
            " flags:%d min_data_packet_size:%d "
            " max_data_packet_size:%d max_bitrate:%d",
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
    asf_object_metadata_t *p_meta = &p_obj->metadata;
    unsigned int i;

    for( i = 0; i < p_meta->i_record_entries_count; i++ )
    {
        free( p_meta->record[i].psz_name );
        free( p_meta->record[i].p_data );
    }
    free( p_meta->record );
}

static int ASF_ReadObject_metadata( stream_t *s, asf_object_t *p_obj )
{
    asf_object_metadata_t *p_meta = &p_obj->metadata;

    int i_peek;
    unsigned int i;
    const uint8_t *p_peek, *p_data;
#ifdef ASF_DEBUG
    unsigned int j;
#endif

    if( ( i_peek = stream_Peek( s, &p_peek, p_meta->i_object_size ) ) <
        __MAX( (int64_t)p_meta->i_object_size, 26 ) )
       return VLC_EGENERIC;

    p_meta->i_record_entries_count = GetWLE( p_peek + 24 );

    p_data = p_peek + 26;

    p_meta->record = calloc( p_meta->i_record_entries_count,
                             sizeof(asf_metadata_record_t) );
    if( !p_meta->record )
        return VLC_ENOMEM;

    for( i = 0; i < p_meta->i_record_entries_count; i++ )
    {
        asf_metadata_record_t *p_record = &p_meta->record[i];
        int i_name;
        int i_data;

        if( !ASF_HAVE( 2+2+2+2+4 ) )
            break;

        if( ASF_READ2() != 0 )
            break;

        p_record->i_stream = ASF_READ2();
        i_name = ASF_READ2();
        p_record->i_type = ASF_READ2();
        i_data = ASF_READ4();

        if( !ASF_HAVE( i_name + i_data ) )
            break;

        /* Read name */
        p_record->psz_name = ASF_READS( i_name );

        /* Read data */
        if( p_record->i_type == ASF_METADATA_TYPE_STRING )
        {
            p_record->p_data = ASF_READS( i_data );
            p_record->i_data = i_data/2; /* FIXME Is that needed ? */
        }
        else if( p_record->i_type == ASF_METADATA_TYPE_BYTE )
        {
            p_record->p_data = malloc( i_data );
            p_record->i_data = i_data;
            if( p_record->p_data && i_data > 0 )
                memcpy( p_record->p_data, p_data, i_data );

            p_data += i_data;
        }
        else if( p_record->i_type == ASF_METADATA_TYPE_QWORD )
        {
            p_record->i_val = ASF_READ8();
        }
        else if( p_record->i_type == ASF_METADATA_TYPE_DWORD )
        {
            p_record->i_val = ASF_READ4();
        }
        else if( p_record->i_type == ASF_METADATA_TYPE_WORD )
        {
            p_record->i_val = ASF_READ2();
        }
        else if( p_record->i_type == ASF_METADATA_TYPE_BOOL )
        {
            p_record->i_val = ASF_READ2();
        }
        else
        {
            /* Unknown */
            p_data += i_data;
        }
    }
    p_meta->i_record_entries_count = i;

#ifdef ASF_DEBUG
    msg_Dbg( s,
            "read \"metadata object\" %d entries",
            p_meta->i_record_entries_count );
    for( j = 0; j < p_meta->i_record_entries_count; j++ )
    {
        asf_metadata_record_t *p_rec = &p_meta->record[j];

        if( p_rec->i_type == ASF_METADATA_TYPE_STRING )
            msg_Dbg( s, "  - %s=%s",
                     p_rec->psz_name, p_rec->p_data );
        else if( p_rec->i_type == ASF_METADATA_TYPE_BYTE )
            msg_Dbg( s, "  - %s (%i bytes)",
                     p_rec->psz_name, p_rec->i_data );
        else
            msg_Dbg( s, "  - %s=%"PRId64,
                     p_rec->psz_name, p_rec->i_val );
    }
#endif

    return VLC_SUCCESS;
}

static int ASF_ReadObject_header_extension( stream_t *s, asf_object_t *p_obj )
{
    asf_object_header_extension_t *p_he = &p_obj->header_extension;
    int     i_peek;
    const uint8_t *p_peek;

    if( ( i_peek = stream_Peek( s, &p_peek, p_he->i_object_size ) ) <  46)
    {
       return VLC_EGENERIC;
    }
    ASF_GetGUID( &p_he->i_reserved1, p_peek + 24 );
    p_he->i_reserved2 = GetWLE( p_peek + 40 );
    p_he->i_header_extension_size = GetDWLE( p_peek + 42 );
    if( p_he->i_header_extension_size )
    {
        if( (unsigned int)(i_peek-46) < p_he->i_header_extension_size )
            return VLC_EGENERIC;

        p_he->p_header_extension_data =
            malloc( p_he->i_header_extension_size );
        if( !p_he->p_header_extension_data )
            return VLC_ENOMEM;

        memcpy( p_he->p_header_extension_data, p_peek + 46,
                p_he->i_header_extension_size );
    }
    else
    {
        p_he->p_header_extension_data = NULL;
    }

#ifdef ASF_DEBUG
    msg_Dbg( s,
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

        if( !p_obj || ASF_ReadObject( s, p_obj, (asf_object_t*)p_he ) )
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
    asf_object_header_extension_t *p_he = &p_obj->header_extension;

    FREENULL( p_he->p_header_extension_data );
}

static int ASF_ReadObject_stream_properties( stream_t *s, asf_object_t *p_obj )
{
    asf_object_stream_properties_t *p_sp = &p_obj->stream_properties;
    size_t        i_peek;
    const uint8_t *p_peek;

    if( ( i_peek = stream_Peek( s, &p_peek,  p_sp->i_object_size ) ) < 78 )
       return VLC_EGENERIC;

    ASF_GetGUID( &p_sp->i_stream_type, p_peek + 24 );
    ASF_GetGUID( &p_sp->i_error_correction_type, p_peek + 40 );
    p_sp->i_time_offset = GetQWLE( p_peek + 56 );
    p_sp->i_type_specific_data_length = GetDWLE( p_peek + 64 );
    p_sp->i_error_correction_data_length = GetDWLE( p_peek + 68 );
    p_sp->i_flags = GetWLE( p_peek + 72 );
        p_sp->i_stream_number = p_sp->i_flags&0x07f;
    p_sp->i_reserved = GetDWLE( p_peek + 74 );
    i_peek -= 78;

    if( p_sp->i_type_specific_data_length )
    {
        if( i_peek < p_sp->i_type_specific_data_length )
            return VLC_EGENERIC;

        p_sp->p_type_specific_data =
            malloc( p_sp->i_type_specific_data_length );
        if( !p_sp->p_type_specific_data )
            return VLC_ENOMEM;

        memcpy( p_sp->p_type_specific_data, p_peek + 78,
                p_sp->i_type_specific_data_length );
        i_peek -= p_sp->i_type_specific_data_length;
    }

    if( p_sp->i_error_correction_data_length )
    {
        if( i_peek < p_sp->i_error_correction_data_length )
        {
            free( p_sp->p_type_specific_data );
            return VLC_EGENERIC;
        }

        p_sp->p_error_correction_data =
            malloc( p_sp->i_error_correction_data_length );
        if( !p_sp->p_error_correction_data )
        {
            free( p_sp->p_type_specific_data );
            return VLC_ENOMEM;
        }
        memcpy( p_sp->p_error_correction_data,
                p_peek + 78 + p_sp->i_type_specific_data_length,
                p_sp->i_error_correction_data_length );
    }

#ifdef ASF_DEBUG
    msg_Dbg( s,
            "read \"stream Properties object\" stream_type:" GUID_FMT
            " error_correction_type:" GUID_FMT " time_offset:%"PRId64
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
    asf_object_stream_properties_t *p_sp = &p_obj->stream_properties;

    FREENULL( p_sp->p_type_specific_data );
    FREENULL( p_sp->p_error_correction_data );
}


static int ASF_ReadObject_codec_list( stream_t *s, asf_object_t *p_obj )
{
    asf_object_codec_list_t *p_cl = &p_obj->codec_list;
    int     i_peek;
    const uint8_t *p_peek, *p_data;

    unsigned int i_codec;

    if( ( i_peek = stream_Peek( s, &p_peek, p_cl->i_object_size ) ) < 44 )
       return VLC_EGENERIC;

    ASF_GetGUID( &p_cl->i_reserved, p_peek + 24 );
    p_cl->i_codec_entries_count = GetWLE( p_peek + 40 );

    p_data = p_peek + 44;

    if( p_cl->i_codec_entries_count > 0 )
    {
        p_cl->codec = calloc( p_cl->i_codec_entries_count,
                              sizeof( asf_codec_entry_t ) );
        if( !p_cl->codec )
            return VLC_ENOMEM;

        for( i_codec = 0; i_codec < p_cl->i_codec_entries_count; i_codec++ )
        {
            asf_codec_entry_t *p_codec = &p_cl->codec[i_codec];

            if( !ASF_HAVE( 2+2+2 ) )
                break;

            /* */
            p_codec->i_type = ASF_READ2();

            /* XXX the length here are the number of *unicode* characters and
             * not of bytes like nearly every elsewhere */

            /* codec name */
            p_codec->psz_name = ASF_READS( 2*ASF_READ2() );

            /* description */
            p_codec->psz_description = ASF_READS( 2*ASF_READ2() );

            /* opaque information */
            p_codec->i_information_length = ASF_READ2();
            if( p_codec->i_information_length > 0 && ASF_HAVE( p_codec->i_information_length ) )
            {
                p_codec->p_information = malloc( p_codec->i_information_length );
                if( p_codec->p_information )
                    memcpy( p_codec->p_information, p_data, p_codec->i_information_length );
                else
                    p_codec->i_information_length = 0;
                p_data += p_codec->i_information_length;
            }
        }
        p_cl->i_codec_entries_count = i_codec;
    }

#ifdef ASF_DEBUG
    msg_Dbg( s, "read \"codec list object\" reserved_guid:" GUID_FMT
             " codec_entries_count:%d",
            GUID_PRINT( p_cl->i_reserved ), p_cl->i_codec_entries_count );

    for( i_codec = 0; i_codec < p_cl->i_codec_entries_count; i_codec++ )
    {
        const asf_codec_entry_t *p_codec = &p_cl->codec[i_codec];

        msg_Dbg( s, "  - codec[%d] %s name:\"%s\" "
                 "description:\"%s\" information_length:%d",
                 i_codec, ( p_codec->i_type == ASF_CODEC_TYPE_VIDEO ) ?
                 "video" : ( ( p_codec->i_type == ASF_CODEC_TYPE_AUDIO ) ?
                 "audio" : "unknown" ),
                 p_codec->psz_name, p_codec->psz_description,
                 p_codec->i_information_length );
    }
#endif

    return VLC_SUCCESS;
}

static void ASF_FreeObject_codec_list( asf_object_t *p_obj )
{
    asf_object_codec_list_t *p_cl = &p_obj->codec_list;
    unsigned int i_codec;

    for( i_codec = 0; i_codec < p_cl->i_codec_entries_count; i_codec++ )
    {
        asf_codec_entry_t *p_codec = &p_cl->codec[i_codec];

        FREENULL( p_codec->psz_name );
        FREENULL( p_codec->psz_description );
        FREENULL( p_codec->p_information );
    }
    FREENULL( p_cl->codec );
}

/* Microsoft should go to hell. This time the length give number of bytes
 * and for the some others object, length give char16 count ... */
static int ASF_ReadObject_content_description(stream_t *s, asf_object_t *p_obj)
{
    asf_object_content_description_t *p_cd = &p_obj->content_description;
    const uint8_t *p_peek, *p_data;
    int i_peek, i_title, i_artist, i_copyright, i_description, i_rating;
    vlc_iconv_t cd = (vlc_iconv_t)-1;
    const char *ib = NULL;
    char *ob = NULL;
    size_t i_ibl, i_obl, i_len;

    if( ( i_peek = stream_Peek( s, &p_peek, p_cd->i_object_size ) ) < 34 )
       return VLC_EGENERIC;

    cd = vlc_iconv_open("UTF-8", "UTF-16LE");
    if( cd == (vlc_iconv_t)-1 )
    {
        msg_Err( s, "vlc_iconv_open failed" );
        return VLC_EGENERIC;
    }

/* FIXME i_size*3 is the worst case. */
#define GETSTRINGW( psz_str, i_size ) do { \
    psz_str = calloc( i_size*3+1, sizeof(char) ); \
    if( psz_str ) { \
        ib = (const char *)p_data; \
        ob = psz_str; \
        i_ibl = i_size; \
        i_obl = i_size*3; \
        i_len = vlc_iconv(cd, &ib, &i_ibl, &ob, &i_obl); \
        p_data += i_size; \
    } } while(0)

    p_data = p_peek + 24;
    
    i_title         = ASF_READ2();
    i_artist        = ASF_READ2();
    i_copyright     = ASF_READ2();
    i_description   = ASF_READ2();
    i_rating        = ASF_READ2();

    if( !ASF_HAVE( i_title+i_artist+i_copyright+i_description+i_rating ) )
    {
        vlc_iconv_close( cd );
        return VLC_EGENERIC;
    }

    GETSTRINGW( p_cd->psz_title, i_title );
    GETSTRINGW( p_cd->psz_artist, i_artist );
    GETSTRINGW( p_cd->psz_copyright, i_copyright );
    GETSTRINGW( p_cd->psz_description, i_description );
    GETSTRINGW( p_cd->psz_rating, i_rating );

#undef  GETSTRINGW

#ifdef ASF_DEBUG
    msg_Dbg( s,
             "read \"content description object\" title:\"%s\" artist:\"%s\" copyright:\"%s\" description:\"%s\" rating:\"%s\"",
             p_cd->psz_title,
             p_cd->psz_artist,
             p_cd->psz_copyright,
             p_cd->psz_description,
             p_cd->psz_rating );
#endif

    vlc_iconv_close(cd);
    return VLC_SUCCESS;
}

static void ASF_FreeObject_content_description( asf_object_t *p_obj)
{
    asf_object_content_description_t *p_cd = &p_obj->content_description;

    FREENULL( p_cd->psz_title );
    FREENULL( p_cd->psz_artist );
    FREENULL( p_cd->psz_copyright );
    FREENULL( p_cd->psz_description );
    FREENULL( p_cd->psz_rating );
}

/* Language list: */
static int ASF_ReadObject_language_list(stream_t *s, asf_object_t *p_obj)
{
    asf_object_language_list_t *p_ll = &p_obj->language_list;
    const uint8_t *p_peek, *p_data;
    int i_peek;
    int i;

    if( ( i_peek = stream_Peek( s, &p_peek, p_ll->i_object_size ) ) < 26 )
       return VLC_EGENERIC;

    p_data = &p_peek[24];

    p_ll->i_language = ASF_READ2();
    if( p_ll->i_language > 0 )
    {
        p_ll->ppsz_language = calloc( p_ll->i_language, sizeof( char *) );
        if( !p_ll->ppsz_language )
            return VLC_ENOMEM;

        for( i = 0; i < p_ll->i_language; i++ )
        {
            if( !ASF_HAVE(1) )
                break;
            p_ll->ppsz_language[i] = ASF_READS( ASF_READ1() );
        }
        p_ll->i_language = i;
    }

#ifdef ASF_DEBUG
    msg_Dbg( s, "read \"language list object\" %d entries",
             p_ll->i_language );
    for( i = 0; i < p_ll->i_language; i++ )
        msg_Dbg( s, "  - '%s'",
                 p_ll->ppsz_language[i] );
#endif
    return VLC_SUCCESS;
}

static void ASF_FreeObject_language_list( asf_object_t *p_obj)
{
    asf_object_language_list_t *p_ll = &p_obj->language_list;
    int i;

    for( i = 0; i < p_ll->i_language; i++ )
        FREENULL( p_ll->ppsz_language[i] );
    FREENULL( p_ll->ppsz_language );
}

/* Stream bitrate properties */
static int ASF_ReadObject_stream_bitrate_properties( stream_t *s,
                                                     asf_object_t *p_obj)
{
    asf_object_stream_bitrate_properties_t *p_sb = &p_obj->stream_bitrate;
    const uint8_t *p_peek, *p_data;
    int i_peek;
    int i;

    if( ( i_peek = stream_Peek( s, &p_peek, p_sb->i_object_size ) ) < 26 )
       return VLC_EGENERIC;

    p_data = &p_peek[24];

    p_sb->i_bitrate = ASF_READ2();
    if( p_sb->i_bitrate > 127 )
        p_sb->i_bitrate = 127;  /* Buggy ? */
    for( i = 0; i < p_sb->i_bitrate; i++ )
    {
        if( !ASF_HAVE(2 + 4) )
            break;
        p_sb->bitrate[i].i_stream_number = ASF_READ2()& 0x7f;
        p_sb->bitrate[i].i_avg_bitrate = ASF_READ4();
    }
    p_sb->i_bitrate = i;

#ifdef ASF_DEBUG
    msg_Dbg( s,"read \"stream bitrate properties object\"" );
    for( i = 0; i < p_sb->i_bitrate; i++ )
    {
        msg_Dbg( s,"  - stream=%d bitrate=%d",
                 p_sb->bitrate[i].i_stream_number,
                 p_sb->bitrate[i].i_avg_bitrate );
    }
#endif
    return VLC_SUCCESS;
}
static void ASF_FreeObject_stream_bitrate_properties( asf_object_t *p_obj)
{
    VLC_UNUSED(p_obj);
}

static int ASF_ReadObject_extended_stream_properties( stream_t *s,
                                                      asf_object_t *p_obj)
{
    asf_object_extended_stream_properties_t *p_esp = &p_obj->ext_stream;
    const uint8_t *p_peek, *p_data;
    int i_peek, i;

    if( ( i_peek = stream_Peek( s, &p_peek, p_esp->i_object_size ) ) < 88 )
       return VLC_EGENERIC;

    p_data = &p_peek[24];

    p_esp->i_start_time = GetQWLE( &p_data[0] );
    p_esp->i_end_time = GetQWLE( &p_data[8] );
    p_esp->i_data_bitrate = GetDWLE( &p_data[16] );
    p_esp->i_buffer_size = GetDWLE( &p_data[20] );
    p_esp->i_initial_buffer_fullness = GetDWLE( &p_data[24] );
    p_esp->i_alternate_data_bitrate = GetDWLE( &p_data[28] );
    p_esp->i_alternate_buffer_size = GetDWLE( &p_data[32] );
    p_esp->i_alternate_initial_buffer_fullness = GetDWLE( &p_data[36] );
    p_esp->i_maximum_object_size = GetDWLE( &p_data[40] );
    p_esp->i_flags = GetDWLE( &p_data[44] );
    p_esp->i_stream_number = GetWLE( &p_data[48] );
    p_esp->i_language_index = GetWLE( &p_data[50] );
    p_esp->i_average_time_per_frame= GetQWLE( &p_data[52] );
    p_esp->i_stream_name_count = GetWLE( &p_data[60] );
    p_esp->i_payload_extension_system_count = GetWLE( &p_data[62] );

    p_data += 64;

    p_esp->pi_stream_name_language = calloc( p_esp->i_stream_name_count,
                                             sizeof(int) );
    p_esp->ppsz_stream_name = calloc( p_esp->i_stream_name_count,
                                      sizeof(char*) );
    if( !p_esp->pi_stream_name_language ||
        !p_esp->ppsz_stream_name )
    {
        free( p_esp->pi_stream_name_language );
        free( p_esp->ppsz_stream_name );
        return VLC_ENOMEM;
    }
    for( i = 0; i < p_esp->i_stream_name_count; i++ )
    {
        if( !ASF_HAVE( 2+2 ) )
            break;
        p_esp->pi_stream_name_language[i] = ASF_READ2();
        p_esp->ppsz_stream_name[i] = ASF_READS( ASF_READ2() );
    }
    p_esp->i_stream_name_count = i;

    for( i = 0; i < p_esp->i_payload_extension_system_count; i++ )
    {
        ASF_SKIP( 16 );   // GUID
        ASF_SKIP( 2 );
        ASF_SKIP( ASF_READ4() );
    }

    p_esp->p_sp = NULL;
    if( p_data < &p_peek[i_peek] )
    {
        asf_object_t *p_sp;
        /* Cannot fail as peek succeed */
        stream_Read( s, NULL, p_data - p_peek );
 
        p_sp = malloc( sizeof( asf_object_t ) );

        if( !p_sp || ASF_ReadObject( s, p_sp, NULL ) )
        {
            free( p_sp );
        }
        else
        {
            /* This p_sp will be inserted by ReadRoot later */
            p_esp->p_sp = (asf_object_stream_properties_t*)p_sp;
        }
    }

#ifdef ASF_DEBUG
    msg_Dbg( s, "read \"extended stream properties object\":" );
    msg_Dbg( s, "  - start=%"PRId64" end=%"PRId64,
             p_esp->i_start_time, p_esp->i_end_time );
    msg_Dbg( s, "  - data bitrate=%d buffer=%d initial fullness=%d",
             p_esp->i_data_bitrate,
             p_esp->i_buffer_size,
             p_esp->i_initial_buffer_fullness );
    msg_Dbg( s, "  - alternate data bitrate=%d buffer=%d initial fullness=%d",
             p_esp->i_alternate_data_bitrate,
             p_esp->i_alternate_buffer_size,
             p_esp->i_alternate_initial_buffer_fullness );
    msg_Dbg( s, "  - maximum object size=%d", p_esp->i_maximum_object_size );
    msg_Dbg( s, "  - flags=0x%x", p_esp->i_flags );
    msg_Dbg( s, "  - stream number=%d language=%d",
             p_esp->i_stream_number, p_esp->i_language_index );
    msg_Dbg( s, "  - average time per frame=%"PRId64,
             p_esp->i_average_time_per_frame );
    msg_Dbg( s, "  - stream name count=%d", p_esp->i_stream_name_count );
    for( i = 0; i < p_esp->i_stream_name_count; i++ )
        msg_Dbg( s, "     - lang id=%d name=%s",
                 p_esp->pi_stream_name_language[i],
                 p_esp->ppsz_stream_name[i] );
    msg_Dbg( s, "  - payload extension system count=%d",
             p_esp->i_payload_extension_system_count );
#endif
    return VLC_SUCCESS;
}
static void ASF_FreeObject_extended_stream_properties( asf_object_t *p_obj)
{
    asf_object_extended_stream_properties_t *p_esp = &p_obj->ext_stream;
    int i;

    for( i = 0; i < p_esp->i_stream_name_count; i++ )
        FREENULL( p_esp->ppsz_stream_name[i] );
    FREENULL( p_esp->pi_stream_name_language );
    FREENULL( p_esp->ppsz_stream_name );
}


static int ASF_ReadObject_advanced_mutual_exclusion( stream_t *s,
                                                     asf_object_t *p_obj)
{
    asf_object_advanced_mutual_exclusion_t *p_ae = &p_obj->advanced_mutual_exclusion;
    const uint8_t *p_peek, *p_data;
    int i_peek;
    int i;

    if( ( i_peek = stream_Peek( s, &p_peek, p_ae->i_object_size ) ) < 42 )
       return VLC_EGENERIC;

    p_data = &p_peek[24];

    ASF_GetGUID( &p_ae->type, &p_data[0] );
    ASF_SKIP( 16 );
    p_ae->i_stream_number_count = ASF_READ2();
    p_ae->pi_stream_number = calloc( p_ae->i_stream_number_count, sizeof(int) );

    for( i = 0; i < p_ae->i_stream_number_count; i++ )
    {
        if( !ASF_HAVE(2) )
            break;
        p_ae->pi_stream_number[i] = ASF_READ2();
    }
    p_ae->i_stream_number_count = i;
 
#ifdef ASF_DEBUG
    msg_Dbg( s, "read \"advanced mutual exclusion object\"" );
    for( i = 0; i < p_ae->i_stream_number_count; i++ )
        msg_Dbg( s, "  - stream=%d", p_ae->pi_stream_number[i] );
#endif
    return VLC_SUCCESS;
}
static void ASF_FreeObject_advanced_mutual_exclusion( asf_object_t *p_obj)
{
    asf_object_advanced_mutual_exclusion_t *p_ae = &p_obj->advanced_mutual_exclusion;

    FREENULL( p_ae->pi_stream_number );
}


static int ASF_ReadObject_stream_prioritization( stream_t *s,
                                                 asf_object_t *p_obj)
{
    asf_object_stream_prioritization_t *p_sp = &p_obj->stream_prioritization;
    const uint8_t *p_peek, *p_data;
    int i_peek;
    int i;

    if( ( i_peek = stream_Peek( s, &p_peek, p_sp->i_object_size ) ) < 26 )
       return VLC_EGENERIC;

    p_data = &p_peek[24];

    p_sp->i_priority_count = ASF_READ2();

    p_sp->pi_priority_flag = calloc( p_sp->i_priority_count, sizeof(int) );
    p_sp->pi_priority_stream_number =
                             calloc( p_sp->i_priority_count, sizeof(int) );

    if( !p_sp->pi_priority_flag || !p_sp->pi_priority_stream_number )
    {
        free( p_sp->pi_priority_flag );
        free( p_sp->pi_priority_stream_number );
        return VLC_ENOMEM;
    }

    for( i = 0; i < p_sp->i_priority_count; i++ )
    {
        if( !ASF_HAVE(2+2) )
            break;
        p_sp->pi_priority_stream_number[i] = ASF_READ2();
        p_sp->pi_priority_flag[i] = ASF_READ2();
    }
    p_sp->i_priority_count = i;

#ifdef ASF_DEBUG
    msg_Dbg( s, "read \"stream prioritization object\"" );
    for( i = 0; i < p_sp->i_priority_count; i++ )
        msg_Dbg( s, "  - Stream:%d flags=0x%x",
                 p_sp->pi_priority_stream_number[i],
                 p_sp->pi_priority_flag[i] );
#endif
    return VLC_SUCCESS;
}
static void ASF_FreeObject_stream_prioritization( asf_object_t *p_obj)
{
    asf_object_stream_prioritization_t *p_sp = &p_obj->stream_prioritization;

    FREENULL( p_sp->pi_priority_stream_number );
    FREENULL( p_sp->pi_priority_flag );
}


static int ASF_ReadObject_extended_content_description( stream_t *s,
                                                        asf_object_t *p_obj)
{
    asf_object_extended_content_description_t *p_ec =
                                        &p_obj->extended_content_description;
    const uint8_t *p_peek, *p_data;
    int i_peek;
    int i;

    if( ( i_peek = stream_Peek( s, &p_peek, p_ec->i_object_size ) ) < 26 )
       return VLC_EGENERIC;

    p_data = &p_peek[24];

    p_ec->i_count = ASF_READ2();
    p_ec->ppsz_name  = calloc( p_ec->i_count, sizeof(char*) );
    p_ec->ppsz_value = calloc( p_ec->i_count, sizeof(char*) );
    if( !p_ec->ppsz_name || !p_ec->ppsz_value )
    {
        free( p_ec->ppsz_name );
        free( p_ec->ppsz_value );
        return VLC_ENOMEM;
    }
    for( i = 0; i < p_ec->i_count; i++ )
    {
        int i_size;
        int i_type;

        if( !ASF_HAVE(2 + 2+2) )
            break;

        p_ec->ppsz_name[i] = ASF_READS( ASF_READ2() );

        /* Grrr */
        i_type = ASF_READ2();
        i_size = ASF_READ2();

        if( i_type == 0 )
        {
            p_ec->ppsz_value[i] = ASF_READS( i_size );
        }
        else if( i_type == 1 )
        {
            /* Byte array */
            static const char hex[16] = "0123456789ABCDEF";
            int j;

            p_ec->ppsz_value[i] = malloc( 2*i_size + 1 );
            if( p_ec->ppsz_value[i] )
            {
                char *psz_value = p_ec->ppsz_value[i];
                for( j = 0; j < i_size; j++ )
                {
                    const uint8_t v = ASF_READ1();
                    psz_value[2*j+0] = hex[v>>4];
                    psz_value[2*j+1] = hex[v&0xf];
                }
                psz_value[2*i_size] = '\0';
            }
        }
        else if( i_type == 2 )
        {
            /* Bool */
            p_ec->ppsz_value[i] = strdup( ASF_READ1() ? "true" : "false" );
            ASF_SKIP(i_size-1);
        }
        else if( i_type == 3 )
        {
            /* DWord */
            if( asprintf( &p_ec->ppsz_value[i], "%d", ASF_READ4() ) == -1 )
                p_ec->ppsz_value[i] = NULL;
        }
        else if( i_type == 4 )
        {
            /* QWord */
            if( asprintf( &p_ec->ppsz_value[i], "%"PRId64, ASF_READ8() ) == -1 )
                p_ec->ppsz_value[i] = NULL;
        }
        else if( i_type == 5 )
        {
            /* Word */
            if( asprintf( &p_ec->ppsz_value[i], "%d", ASF_READ2() ) == -1 )
                p_ec->ppsz_value[i] = NULL;
        }
        else
        {
            p_ec->ppsz_value[i] = NULL;
            ASF_SKIP(i_size);
        }
    }
    p_ec->i_count = i;

#ifdef ASF_DEBUG
    msg_Dbg( s, "read \"extended content description object\"" );
    for( i = 0; i < p_ec->i_count; i++ )
        msg_Dbg( s, "  - '%s' = '%s'",
                 p_ec->ppsz_name[i],
                 p_ec->ppsz_value[i] );
#endif
    return VLC_SUCCESS;
}
static void ASF_FreeObject_extended_content_description( asf_object_t *p_obj)
{
    asf_object_extended_content_description_t *p_ec =
                                        &p_obj->extended_content_description;
    int i;

    for( i = 0; i < p_ec->i_count; i++ )
    {
        FREENULL( p_ec->ppsz_name[i] );
        FREENULL( p_ec->ppsz_value[i] );
    }
    FREENULL( p_ec->ppsz_name );
    FREENULL( p_ec->ppsz_value );
}


#if 0
static int ASF_ReadObject_XXX(stream_t *s, asf_object_t *p_obj)
{
    asf_object_XXX_t *p_XX =
        (asf_object_XXX_t *)p_obj;
    const uint8_t *p_peek;
    uint8_t *p_data;
    int i_peek;

    if( ( i_peek = stream_Peek( s, &p_peek, p_XX->i_object_size ) ) < XXX )
       return VLC_EGENERIC;

    p_data = &p_peek[24];

#ifdef ASF_DEBUG
    msg_Dbg( s,
             "Read \"XXX object\"" );
#endif
    return VLC_SUCCESS;
}
static void ASF_FreeObject_XXX( asf_object_t *p_obj)
{
    asf_object_XXX_t *p_XX =
        (asf_object_XXX_t *)p_obj;
}
#endif


/* */
static const struct
{
    const guid_t  *p_id;
    int     i_type;
    int     (*ASF_ReadObject_function)( stream_t *, asf_object_t *p_obj );
    void    (*ASF_FreeObject_function)( asf_object_t *p_obj );

} ASF_Object_Function [] =
{
    { &asf_object_header_guid, ASF_OBJECT_HEADER,
      ASF_ReadObject_Header, ASF_FreeObject_Null },
    { &asf_object_data_guid, ASF_OBJECT_DATA,
      ASF_ReadObject_Data, ASF_FreeObject_Null },
    { &asf_object_index_guid, ASF_OBJECT_INDEX,
      ASF_ReadObject_Index, ASF_FreeObject_Index },
    { &asf_object_file_properties_guid, ASF_OBJECT_FILE_PROPERTIES,
      ASF_ReadObject_file_properties, ASF_FreeObject_Null },
    { &asf_object_stream_properties_guid, ASF_OBJECT_STREAM_PROPERTIES,
      ASF_ReadObject_stream_properties,ASF_FreeObject_stream_properties },
    { &asf_object_header_extension_guid, ASF_OBJECT_HEADER_EXTENSION,
      ASF_ReadObject_header_extension, ASF_FreeObject_header_extension},
    { &asf_object_metadata_guid, ASF_OBJECT_METADATA,
      ASF_ReadObject_metadata, ASF_FreeObject_metadata},
    { &asf_object_codec_list_guid, ASF_OBJECT_CODEC_LIST,
      ASF_ReadObject_codec_list, ASF_FreeObject_codec_list },
    { &asf_object_marker_guid, ASF_OBJECT_MARKER, NULL, NULL },
    { &asf_object_padding, ASF_OBJECT_PADDING, NULL, NULL },
    { &asf_object_content_description_guid, ASF_OBJECT_CONTENT_DESCRIPTION,
      ASF_ReadObject_content_description, ASF_FreeObject_content_description },
    { &asf_object_language_list, ASF_OBJECT_OTHER,
      ASF_ReadObject_language_list, ASF_FreeObject_language_list },
    { &asf_object_stream_bitrate_properties, ASF_OBJECT_OTHER,
      ASF_ReadObject_stream_bitrate_properties,
      ASF_FreeObject_stream_bitrate_properties },
    { &asf_object_extended_stream_properties, ASF_OBJECT_OTHER,
      ASF_ReadObject_extended_stream_properties,
      ASF_FreeObject_extended_stream_properties },
    { &asf_object_advanced_mutual_exclusion, ASF_OBJECT_OTHER,
      ASF_ReadObject_advanced_mutual_exclusion,
      ASF_FreeObject_advanced_mutual_exclusion },
    { &asf_object_stream_prioritization, ASF_OBJECT_OTHER,
      ASF_ReadObject_stream_prioritization,
      ASF_FreeObject_stream_prioritization },
    { &asf_object_extended_content_description, ASF_OBJECT_OTHER,
      ASF_ReadObject_extended_content_description,
      ASF_FreeObject_extended_content_description },

    { &asf_object_null_guid, 0, NULL, NULL }
};

static int ASF_ReadObject( stream_t *s, asf_object_t *p_obj,
                           asf_object_t *p_father )
{
    int i_result;
    int i_index;

    if( !p_obj )
        return 0;

    memset( p_obj, 0, sizeof( *p_obj ) );

    if( ASF_ReadObjectCommon( s, p_obj ) )
    {
        msg_Warn( s, "cannot read one asf object" );
        return VLC_EGENERIC;
    }
    p_obj->common.p_father = p_father;
    p_obj->common.p_first = NULL;
    p_obj->common.p_next = NULL;
    p_obj->common.p_last = NULL;

    if( p_obj->common.i_object_size < 24 )
    {
        msg_Warn( s, "found a corrupted asf object (size<24)" );
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
        msg_Warn( s, "unknown asf object (not loaded)" );
        i_result = VLC_SUCCESS;
    }
    else
    {
        /* XXX ASF_ReadObject_function realloc *pp_obj XXX */
        i_result =
          (ASF_Object_Function[i_index].ASF_ReadObject_function)( s, p_obj );
    }

    /* link this object with father */
    if( p_father && ! i_result )
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

    return i_result;
}

static void ASF_FreeObject( stream_t *s, asf_object_t *p_obj )
{
    int i_index;
    asf_object_t *p_child;

    if( !p_obj )
        return;

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
        msg_Warn( s,
                  "unknown asf object " GUID_FMT,
                  GUID_PRINT( p_obj->common.i_object_id ) );
    }
    else
    {
#ifdef ASF_DEBUG
        msg_Dbg( s,
                  "free asf object " GUID_FMT,
                  GUID_PRINT( p_obj->common.i_object_id ) );
#endif
        (ASF_Object_Function[i_index].ASF_FreeObject_function)( p_obj );
    }
    free( p_obj );
}

/*****************************************************************************
 * ASF_ObjectDumpDebug:
 *****************************************************************************/
static const struct
{
    const guid_t *p_id;
    const char *psz_name;
} ASF_ObjectDumpDebugInfo[] =
{
    { &asf_object_header_guid, "Header" },
    { &asf_object_data_guid, "Data" },
    { &asf_object_index_guid, "Index" },
    { &asf_object_file_properties_guid, "File Properties" },
    { &asf_object_stream_properties_guid, "Stream Properties" },
    { &asf_object_content_description_guid, "Content Description" },
    { &asf_object_header_extension_guid, "Header Extension" },
    { &asf_object_metadata_guid, "Metadata" },
    { &asf_object_codec_list_guid, "Codec List" },
    { &asf_object_marker_guid, "Marker" },
    { &asf_object_stream_type_audio, "Stream Type Audio" },
    { &asf_object_stream_type_video, "Stream Type Video" },
    { &asf_object_stream_type_command, "Stream Type Command" },
    { &asf_object_language_list, "Language List" },
    { &asf_object_stream_bitrate_properties, "Stream Bitrate Properties" },
    { &asf_object_padding, "Padding" },
    { &asf_object_extended_stream_properties, "Extended Stream Properties" },
    { &asf_object_advanced_mutual_exclusion, "Advanced Mutual Exclusion" },
    { &asf_object_stream_prioritization, "Stream Prioritization" },
    { &asf_object_extended_content_description, "Extended content description"},

    { NULL, "Unknown" },
};


static void ASF_ObjectDumpDebug( vlc_object_t *p_obj,
                                 asf_object_common_t *p_node, int i_level )
{
    char str[1024];
    int i;
    union asf_object_u *p_child;
    const char *psz_name;

    /* Find the name */
    for( i = 0; ASF_ObjectDumpDebugInfo[i].p_id != NULL; i++ )
    {
        if( ASF_CmpGUID( ASF_ObjectDumpDebugInfo[i].p_id,
                          &p_node->i_object_id ) )
            break;
    }
    psz_name = ASF_ObjectDumpDebugInfo[i].psz_name;

    memset( str, ' ', sizeof( str ) );
    for( i = 1; i < i_level; i++ )
    {
        str[i * 5] = '|';
    }
    snprintf( str + 5*i_level, 1024,
             "+ '%s' GUID "GUID_FMT" size:%"PRIu64"pos:%"PRIu64,
             psz_name,
             GUID_PRINT( p_node->i_object_id ),
             p_node->i_object_size, p_node->i_object_pos );

    msg_Dbg( p_obj, "%s", str );

    for( p_child = p_node->p_first; p_child != NULL;
                                             p_child = p_child->common.p_next )
    {
        ASF_ObjectDumpDebug( p_obj, &p_child->common, i_level + 1 );
    }
}

/*****************************************************************************
 * ASF_ReadObjetRoot : parse the entire stream/file
 *****************************************************************************/
asf_object_root_t *ASF_ReadObjectRoot( stream_t *s, int b_seekable )
{
    asf_object_root_t *p_root = malloc( sizeof( asf_object_root_t ) );
    asf_object_t *p_obj;

    if( !p_root )
        return NULL;

    p_root->i_type = ASF_OBJECT_ROOT;
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

        if( !p_obj || ASF_ReadObject( s, p_obj, (asf_object_t*)p_root ) )
        {
            free( p_obj );
            break;
        }
        switch( p_obj->common.i_type )
        {
            case( ASF_OBJECT_HEADER ):
                p_root->p_hdr = (asf_object_header_t*)p_obj;
                break;
            case( ASF_OBJECT_DATA ):
                p_root->p_data = (asf_object_data_t*)p_obj;
                break;
            case( ASF_OBJECT_INDEX ):
                p_root->p_index = (asf_object_index_t*)p_obj;
                break;
            default:
                msg_Warn( s, "unknow object found" );
                break;
        }
        if( p_obj->common.i_type == ASF_OBJECT_DATA &&
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
            break;
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
                int i_ext_stream;
                int i;

                p_root->p_metadata =
                    ASF_FindObject( p_hdr_ext,
                                    &asf_object_metadata_guid, 0 );
                /* Special case for broken designed file format :( */
                i_ext_stream = ASF_CountObject( p_hdr_ext,
                                    &asf_object_extended_stream_properties );
                for( i = 0; i < i_ext_stream; i++ )
                {
                    asf_object_t *p_esp =
                        ASF_FindObject( p_hdr_ext,
                                   &asf_object_extended_stream_properties, i );
                    if( p_esp->ext_stream.p_sp )
                    {
                        asf_object_t *p_sp =
                                         (asf_object_t*)p_esp->ext_stream.p_sp;

                        /* Insert this p_sp */
                        p_root->p_hdr->p_last->common.p_next = p_sp;
                        p_root->p_hdr->p_last = p_sp;

                        p_sp->common.p_father = (asf_object_t*)p_root->p_hdr;
                    }
                }
            }

            ASF_ObjectDumpDebug( VLC_OBJECT(s),
                                 (asf_object_common_t*)p_root, 0 );
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

    if( !p_obj )
        return 0;

    i_count = 0;
    p_child = p_obj->common.p_first;
    while( p_child )
    {
        if( ASF_CmpGUID( &p_child->common.i_object_id, p_guid ) )
            i_count++;

        p_child = p_child->common.p_next;
    }
    return i_count;
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
                return p_child;

            i_number--;
        }
        p_child = p_child->common.p_next;
    }
    return NULL;
}
