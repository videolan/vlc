/*****************************************************************************
 * libasf.c : asf stream demux module for vlc
 *****************************************************************************
 * Copyright © 2001-2004, 2006-2008 VLC authors and VideoLAN
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Gildas Bazin <gbazin@videolan.org>
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

#include <limits.h>

#include <vlc_demux.h>
#include <vlc_charset.h>          /* FromCharset */

#include "libasf.h"

#ifndef NDEBUG
# define ASF_DEBUG 1
#endif

/* Helpers:
 * They ensure that invalid reads will not create problems.
 * They are expansion safe
 * They make the following assumptions:
 *  const uint8_t *p_peek exists and points to the start of a buffer
 *  ssize_t i_peek the size of the buffer pointed to by p_peek
 *  const uint8_t *p_data exits and points to the data inside p_peek to be read.
 */
/* ASF_HAVE(n):
 *  Check that n bytes can be read */
static inline bool AsfObjectHelperHave( const uint8_t *p_peek, size_t i_peek, const uint8_t *p_current, size_t i_wanted )
{
    if( i_wanted > i_peek )
        return false;
    return &p_current[i_wanted] <= &p_peek[i_peek];
}
#define ASF_HAVE(n) AsfObjectHelperHave( p_peek, i_peek, p_data, n )

/* ASF_SKIP(n)
 *  Skip n bytes if possible */
static inline void AsfObjectHelperSkip( const uint8_t *p_peek, size_t i_peek, uint8_t **pp_data, size_t i_wanted )
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
static inline type AsfObjectHelperRead##x( const uint8_t *p_peek, size_t i_peek, uint8_t **pp_data ) { \
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
static char *AsfObjectHelperReadString( const uint8_t *p_peek, size_t i_peek, uint8_t **pp_data, size_t i_size )
{
    uint8_t *p_data = *pp_data;
    char *psz_string = NULL;
    if( ASF_HAVE(i_size) )
    {
        psz_string = FromCharset( "UTF-16LE", p_data, i_size );
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
 *
 ****************************************************************************/
static int ASF_ReadObjectCommon( stream_t *s, asf_object_t *p_obj )
{
    asf_object_common_t *p_common = &p_obj->common;
    const uint8_t *p_peek;

    if( vlc_stream_Peek( s, &p_peek, ASF_OBJECT_COMMON_SIZE ) < ASF_OBJECT_COMMON_SIZE )
        return VLC_EGENERIC;

    ASF_GetGUID( &p_common->i_object_id, p_peek );
    p_common->i_object_size = GetQWLE( p_peek + 16 );
    p_common->i_object_pos  = vlc_stream_Tell( s );
    p_common->p_next = NULL;

#ifdef ASF_DEBUG
    msg_Dbg( s,
             "found object guid: " GUID_FMT " size:%"PRIu64" at %"PRIu64,
             GUID_PRINT( p_common->i_object_id ),
             p_common->i_object_size, p_common->i_object_pos );
#endif

    return VLC_SUCCESS;
}

static int ASF_NextObject( stream_t *s, asf_object_t *p_obj, uint64_t i_boundary )
{
    asf_object_t obj;

    int64_t i_pos = vlc_stream_Tell( s );
    if ( i_boundary && i_pos >= 0 && (uint64_t) i_pos >= i_boundary )
    {
        return VLC_EGENERIC;
    }

    if( p_obj == NULL )
    {
        if( ASF_ReadObjectCommon( s, &obj ) )
            return VLC_EGENERIC;

        p_obj = &obj;
    }

    if( p_obj->common.i_object_size <= 0 )
        return VLC_EGENERIC;

    if( ( UINT64_MAX - p_obj->common.i_object_pos ) < p_obj->common.i_object_size )
        return VLC_EGENERIC;

    if( p_obj->common.p_father &&
        p_obj->common.p_father->common.i_object_size != 0 )
    {
        if( p_obj->common.p_father->common.i_object_pos +
            p_obj->common.p_father->common.i_object_size <
                p_obj->common.i_object_pos + p_obj->common.i_object_size + ASF_OBJECT_COMMON_SIZE )
                                /* ASF_OBJECT_COMMON_SIZE is min size of an object */
        {
            return VLC_EGENERIC;
        }

    }

    return vlc_stream_Seek( s, p_obj->common.i_object_pos +
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
    const uint8_t       *p_peek;

    if( vlc_stream_Peek( s, &p_peek, 30 ) < 30 )
       return VLC_EGENERIC;

    p_hdr->i_sub_object_count = GetDWLE( p_peek + ASF_OBJECT_COMMON_SIZE );
    p_hdr->i_reserved1 = p_peek[28];
    p_hdr->i_reserved2 = p_peek[29];
    p_hdr->p_first = NULL;
    p_hdr->p_last  = NULL;

#ifdef ASF_DEBUG
    msg_Dbg( s,
             "read \"header object\" subobj:%u, reserved1:%u, reserved2:%u",
             p_hdr->i_sub_object_count,
             p_hdr->i_reserved1,
             p_hdr->i_reserved2 );
#endif

    if( vlc_stream_Read( s, NULL, 30 ) != 30 )
        return VLC_EGENERIC;

    /* Now load sub object */
    for( ; ; )
    {
        p_subobj = malloc( sizeof( asf_object_t ) );

        if( !p_subobj || ASF_ReadObject( s, p_subobj, (asf_object_t*)p_hdr ) )
        {
            free( p_subobj );
            break;
        }
        if( ASF_NextObject( s, p_subobj, 0 ) ) /* Go to the next object */
            break;
    }
    return VLC_SUCCESS;
}

static int ASF_ReadObject_Data( stream_t *s, asf_object_t *p_obj )
{
    asf_object_data_t *p_data = &p_obj->data;
    const uint8_t     *p_peek;

    if( vlc_stream_Peek( s, &p_peek, 50 ) < 50 )
       return VLC_EGENERIC;

    ASF_GetGUID( &p_data->i_file_id, p_peek + ASF_OBJECT_COMMON_SIZE );
    p_data->i_total_data_packets = GetQWLE( p_peek + 40 );
    p_data->i_reserved = GetWLE( p_peek + 48 );

#ifdef ASF_DEBUG
    msg_Dbg( s,
             "read \"data object\" file_id:" GUID_FMT " total data packet:"
             "%"PRIu64" reserved:%u",
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
    if( p_index->i_object_size < 56
     || p_index->i_object_size > INT32_MAX
     || vlc_stream_Peek( s, &p_peek, p_index->i_object_size )
        < (int64_t)p_index->i_object_size )
        return VLC_SUCCESS;

    ASF_GetGUID( &p_index->i_file_id, p_peek + ASF_OBJECT_COMMON_SIZE );
    p_index->i_index_entry_time_interval = GetQWLE( p_peek + 40 );
    p_index->i_max_packet_count = GetDWLE( p_peek + 48 );
    p_index->i_index_entry_count = GetDWLE( p_peek + 52 );
    p_index->index_entry = NULL;

#ifdef ASF_DEBUG
    msg_Dbg( s,
            "read \"index object\" file_id:" GUID_FMT
            " index_entry_time_interval:%"PRId64" max_packet_count:%u "
            "index_entry_count:%u",
            GUID_PRINT( p_index->i_file_id ),
            p_index->i_index_entry_time_interval,
            p_index->i_max_packet_count,
            p_index->i_index_entry_count );
#endif

    /* Sanity checking */
    if( !p_index->i_index_entry_time_interval )
    {
        /* We can't use this index if it has an invalid time interval */
        p_index->i_index_entry_count = 0;
        return VLC_ENOMEM;
    }
    if( p_index->i_index_entry_count > (p_index->i_object_size - 56) / 6 )
        p_index->i_index_entry_count = (p_index->i_object_size - 56) / 6;

    p_index->index_entry = calloc( p_index->i_index_entry_count,
                                   sizeof(asf_index_entry_t) );
    if( !p_index->index_entry )
    {
        p_index->i_index_entry_count = 0;
        return VLC_ENOMEM;
    }

    for( i = 0, p_peek += 56; i < p_index->i_index_entry_count; i++, p_peek += 6 )
    {
        p_index->index_entry[i].i_packet_number = GetDWLE( p_peek );
        p_index->index_entry[i].i_packet_count = GetWLE( p_peek + 4 );
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
    const uint8_t *p_peek;

    if( vlc_stream_Peek( s, &p_peek,  104 ) < 104 )
       return VLC_EGENERIC;

    ASF_GetGUID( &p_fp->i_file_id, p_peek + ASF_OBJECT_COMMON_SIZE );
    p_fp->i_file_size = GetQWLE( p_peek + 40 );
    p_fp->i_creation_date = GetQWLE( p_peek + 48 );
    p_fp->i_data_packets_count = GetQWLE( p_peek + 56 );
    p_fp->i_play_duration = GetQWLE( p_peek + 64 );
    p_fp->i_send_duration = GetQWLE( p_peek + 72 );
    p_fp->i_preroll = VLC_TICK_FROM_MS(GetQWLE( p_peek + 80 ));
    p_fp->i_flags = GetDWLE( p_peek + 88 );
    p_fp->i_min_data_packet_size = __MAX( GetDWLE( p_peek + 92 ), (uint32_t) 1 );
    p_fp->i_max_data_packet_size = __MAX( GetDWLE( p_peek + 96 ), (uint32_t) 1 );
    p_fp->i_max_bitrate = GetDWLE( p_peek + 100 );

#ifdef ASF_DEBUG
    msg_Dbg( s,
            "read \"file properties object\" file_id:" GUID_FMT
            " file_size:%"PRIu64" creation_date:%"PRIu64" data_packets_count:"
            "%"PRIu64" play_duration:%"PRId64" send_duration:%"PRId64" preroll:%"PRId64
            " flags:%u min_data_packet_size:%d "
            " max_data_packet_size:%u max_bitrate:%u",
            GUID_PRINT( p_fp->i_file_id ), p_fp->i_file_size,
            p_fp->i_creation_date, p_fp->i_data_packets_count,
            p_fp->i_play_duration, p_fp->i_send_duration,
            MS_FROM_VLC_TICK(p_fp->i_preroll), p_fp->i_flags,
            p_fp->i_min_data_packet_size, p_fp->i_max_data_packet_size,
            p_fp->i_max_bitrate );
#endif

    return VLC_SUCCESS;
}

static void ASF_FreeObject_metadata( asf_object_t *p_obj )
{
    asf_object_metadata_t *p_meta = &p_obj->metadata;

    for( uint32_t i = 0; i < p_meta->i_record_entries_count; i++ )
    {
        free( p_meta->record[i].psz_name );
        free( p_meta->record[i].p_data );
    }
    free( p_meta->record );
}

static int ASF_ReadObject_metadata( stream_t *s, asf_object_t *p_obj )
{
    asf_object_metadata_t *p_meta = &p_obj->metadata;

    uint32_t i;
    const uint8_t *p_peek, *p_data;

    if( p_meta->i_object_size < 26 || p_meta->i_object_size > INT32_MAX )
        return VLC_EGENERIC;

    ssize_t i_peek = vlc_stream_Peek( s, &p_peek, p_meta->i_object_size );
    if( i_peek < (int64_t)p_meta->i_object_size )
       return VLC_EGENERIC;

    p_meta->i_record_entries_count = GetWLE( p_peek + ASF_OBJECT_COMMON_SIZE );

    p_data = p_peek + 26;

    p_meta->record = calloc( p_meta->i_record_entries_count,
                             sizeof(asf_metadata_record_t) );
    if( !p_meta->record )
    {
        p_meta->i_record_entries_count = 0;
        return VLC_ENOMEM;
    }

    for( i = 0; i < p_meta->i_record_entries_count; i++ )
    {
        asf_metadata_record_t *p_record = &p_meta->record[i];
        uint16_t i_name;
        uint32_t i_data;

        if( !ASF_HAVE( 2+2+2+2+4 ) )
            break;

        if( ASF_READ2() != 0 )
            break;

        p_record->i_stream = ASF_READ2();
        i_name = ASF_READ2();
        p_record->i_type = ASF_READ2();
        i_data = ASF_READ4();

        if( UINT32_MAX - i_name < i_data ||
            !ASF_HAVE( i_name + i_data ) )
            break;

        /* Read name */
        p_record->psz_name = ASF_READS( i_name );

        /* Read data */
        if( p_record->i_type == ASF_METADATA_TYPE_STRING )
        {
            p_record->p_data = (uint8_t *)ASF_READS( i_data );
            if( p_record->p_data )
                p_record->i_data = i_data/2; /* FIXME Is that needed ? */
        }
        else if( p_record->i_type == ASF_METADATA_TYPE_BYTE )
        {
            p_record->p_data = malloc( i_data );
            if( p_record->p_data )
            {
                p_record->i_data = i_data;
                if( p_record->p_data && i_data > 0 )
                    memcpy( p_record->p_data, p_data, i_data );
            }
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
             "read \"metadata object\" %"PRIu32" entries",
            p_meta->i_record_entries_count );
    for( uint32_t j = 0; j < p_meta->i_record_entries_count; j++ )
    {
        asf_metadata_record_t *p_rec = &p_meta->record[j];

        if( p_rec->i_type == ASF_METADATA_TYPE_STRING )
            msg_Dbg( s, "  - %s=%s",
                     p_rec->psz_name, p_rec->p_data );
        else if( p_rec->i_type == ASF_METADATA_TYPE_BYTE )
            msg_Dbg( s, "  - %s (%u bytes)",
                     p_rec->psz_name, p_rec->i_data );
        else
            msg_Dbg( s, "  - %s=%"PRIu64,
                     p_rec->psz_name, p_rec->i_val );
    }
#endif

    return VLC_SUCCESS;
}

static int ASF_ReadObject_header_extension( stream_t *s, asf_object_t *p_obj )
{
    asf_object_header_extension_t *p_he = &p_obj->header_extension;
    const uint8_t *p_peek;

    if( p_he->i_object_size > INT32_MAX )
        return VLC_EGENERIC;

    ssize_t i_peek = vlc_stream_Peek( s, &p_peek, p_he->i_object_size );
    if( i_peek < 46 )
       return VLC_EGENERIC;

    ASF_GetGUID( &p_he->i_reserved1, p_peek + ASF_OBJECT_COMMON_SIZE );
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
        p_he->i_header_extension_size = 0;
    }

#ifdef ASF_DEBUG
    msg_Dbg( s,
            "read \"header extension object\" reserved1:" GUID_FMT
            " reserved2:%u header_extension_size:%"PRIu32,
            GUID_PRINT( p_he->i_reserved1 ), p_he->i_reserved2,
            p_he->i_header_extension_size );
#endif

    if( !p_he->i_header_extension_size ) return VLC_SUCCESS;

    /* Read the extension objects */
    if( vlc_stream_Read( s, NULL, 46 ) != 46 )
    {
        free( p_he->p_header_extension_data );
        return VLC_EGENERIC;
    }

    for( ; ; )
    {
        asf_object_t *p_child = malloc( sizeof( asf_object_t ) );

        if( p_child == NULL
         || ASF_ReadObject( s, p_child, (asf_object_t*)p_he ) )
        {
            free( p_child );
            break;
        }

        if( ASF_NextObject( s, p_child, 0 ) ) /* Go to the next object */
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
    const uint8_t *p_peek;

#if UINT64_MAX > SSIZE_MAX
    if( p_sp->i_object_size > SSIZE_MAX )
    {
        msg_Err( s, "unable to peek: object size is too large" );
        return VLC_EGENERIC;
    }
#endif

    if( p_sp->i_object_size > INT32_MAX )
        return VLC_EGENERIC;

    ssize_t i_peek = vlc_stream_Peek( s, &p_peek, p_sp->i_object_size );
    if( i_peek < 78 )
       return VLC_EGENERIC;

    ASF_GetGUID( &p_sp->i_stream_type, p_peek + ASF_OBJECT_COMMON_SIZE );
    ASF_GetGUID( &p_sp->i_error_correction_type, p_peek + 40 );
    p_sp->i_time_offset = GetQWLE( p_peek + 56 );
    p_sp->i_type_specific_data_length = GetDWLE( p_peek + 64 );
    p_sp->i_error_correction_data_length = GetDWLE( p_peek + 68 );
    p_sp->i_flags = GetWLE( p_peek + 72 );
    p_sp->i_stream_number = p_sp->i_flags&0x07f;
    if ( p_sp->i_stream_number > ASF_MAX_STREAMNUMBER )
        return VLC_EGENERIC;
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
            " error_correction_type:" GUID_FMT " time_offset:%"PRIu64
            " type_specific_data_length:%"PRIu32" error_correction_data_length:%"PRIu32
            " flags:0x%x stream_number:%u",
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

static void ASF_FreeObject_codec_list( asf_object_t *p_obj )
{
    asf_object_codec_list_t *p_cl = &p_obj->codec_list;

    for( asf_codec_entry_t *codec = p_cl->codecs, *next;
         codec != NULL;
         codec = next )
    {
        next = codec->p_next;
        free( codec->psz_name );
        free( codec->psz_description );
        free( codec->p_information );
        free( codec );
    }
}

static int ASF_ReadObject_codec_list( stream_t *s, asf_object_t *p_obj )
{
    asf_object_codec_list_t *p_cl = &p_obj->codec_list;
    const uint8_t *p_peek, *p_data;

    if( p_cl->i_object_size > INT32_MAX )
        return VLC_EGENERIC;

    ssize_t i_peek = vlc_stream_Peek( s, &p_peek, p_cl->i_object_size );
    if( i_peek < 44 )
       return VLC_EGENERIC;

    ASF_GetGUID( &p_cl->i_reserved, p_peek + ASF_OBJECT_COMMON_SIZE );
    uint32_t count = GetDWLE( p_peek + 40 );
#ifdef ASF_DEBUG
    msg_Dbg( s, "read \"codec list object\" reserved_guid:" GUID_FMT
             " codec_entries_count:%u", GUID_PRINT( p_cl->i_reserved ),
             count );
#endif

    p_data = p_peek + 44;

    asf_codec_entry_t **pp = &p_cl->codecs;

    for( uint32_t i = 0; i < count; i++ )
    {
        asf_codec_entry_t *p_codec = malloc( sizeof( *p_codec ) );

        if( unlikely(p_codec == NULL) || !ASF_HAVE( 2+2+2 ) )
        {
            free( p_codec );
            *pp = NULL;
            goto error;
        }

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
        if( ASF_HAVE( p_codec->i_information_length ) )
        {
            p_codec->p_information = malloc( p_codec->i_information_length );
            if( likely(p_codec->p_information != NULL) )
                memcpy( p_codec->p_information, p_data,
                        p_codec->i_information_length );
            p_data += p_codec->i_information_length;
        }
        else
            p_codec->p_information = NULL;

#ifdef ASF_DEBUG
        msg_Dbg( s, "  - codec[%"PRIu32"] %s name:\"%s\" "
                 "description:\"%s\" information_length:%u", i,
                 ( p_codec->i_type == ASF_CODEC_TYPE_VIDEO ) ? "video"
                 : ( ( p_codec->i_type == ASF_CODEC_TYPE_AUDIO ) ? "audio"
                 : "unknown" ), p_codec->psz_name,
                 p_codec->psz_description, p_codec->i_information_length );
#endif
        *pp = p_codec;
        pp = &p_codec->p_next;
    }

    *pp = NULL;
    return VLC_SUCCESS;

error:
    ASF_FreeObject_codec_list( p_obj );
    return VLC_EGENERIC;
}

static inline char *get_wstring( const uint8_t *p_data, size_t i_size )
{
    char *psz_str = FromCharset( "UTF-16LE", p_data, i_size );
    if( psz_str )
        p_data += i_size;
    return psz_str;
}

/* Microsoft should go to hell. This time the length give number of bytes
 * and for the some others object, length give char16 count ... */
static int ASF_ReadObject_content_description(stream_t *s, asf_object_t *p_obj)
{
    asf_object_content_description_t *p_cd = &p_obj->content_description;
    const uint8_t *p_peek, *p_data;
    uint16_t i_title, i_artist, i_copyright, i_description, i_rating;

    if( p_cd->i_object_size > INT32_MAX )
        return VLC_EGENERIC;

    ssize_t i_peek = vlc_stream_Peek( s, &p_peek, p_cd->i_object_size );
    if( i_peek < 34 )
       return VLC_EGENERIC;

    p_data = p_peek + ASF_OBJECT_COMMON_SIZE;

    i_title         = ASF_READ2();
    i_artist        = ASF_READ2();
    i_copyright     = ASF_READ2();
    i_description   = ASF_READ2();
    i_rating        = ASF_READ2();

    if( !ASF_HAVE( i_title+i_artist+i_copyright+i_description+i_rating ) )
        return VLC_EGENERIC;

    p_cd->psz_title = get_wstring( p_data, i_title );
    p_cd->psz_artist = get_wstring( p_data, i_artist );
    p_cd->psz_copyright = get_wstring( p_data, i_copyright );
    p_cd->psz_description = get_wstring( p_data, i_description );
    p_cd->psz_rating = get_wstring( p_data, i_rating );

#ifdef ASF_DEBUG
    msg_Dbg( s,
             "read \"content description object\" title:\"%s\" artist:\"%s\" copyright:\"%s\" description:\"%s\" rating:\"%s\"",
             p_cd->psz_title,
             p_cd->psz_artist,
             p_cd->psz_copyright,
             p_cd->psz_description,
             p_cd->psz_rating );
#endif

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
    uint16_t i;

    if( p_ll->i_object_size > INT32_MAX )
        return VLC_EGENERIC;

    ssize_t i_peek = vlc_stream_Peek( s, &p_peek, p_ll->i_object_size );
    if( i_peek < 26 )
       return VLC_EGENERIC;

    p_data = &p_peek[ASF_OBJECT_COMMON_SIZE];

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
    msg_Dbg( s, "read \"language list object\" %u entries",
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
    uint16_t i;

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
    uint16_t i;

    if( p_sb->i_object_size > INT32_MAX )
        return VLC_EGENERIC;

    ssize_t i_peek = vlc_stream_Peek( s, &p_peek, p_sb->i_object_size );
    if( i_peek < 26 )
       return VLC_EGENERIC;

    p_data = &p_peek[ASF_OBJECT_COMMON_SIZE];

    p_sb->i_bitrate = ASF_READ2();
    if( p_sb->i_bitrate > ASF_MAX_STREAMNUMBER )
        p_sb->i_bitrate = ASF_MAX_STREAMNUMBER;  /* Buggy ? */
    for( i = 0; i < p_sb->i_bitrate; i++ )
    {
        if( !ASF_HAVE(2 + 4) )
            break;
        p_sb->bitrate[i].i_stream_number = (uint8_t) ASF_READ2()& 0x7f;
        if ( p_sb->bitrate[i].i_stream_number > ASF_MAX_STREAMNUMBER )
            return VLC_EGENERIC;
        p_sb->bitrate[i].i_avg_bitrate = ASF_READ4();
    }
    p_sb->i_bitrate = i;

#ifdef ASF_DEBUG
    msg_Dbg( s,"read \"stream bitrate properties object\"" );
    for( i = 0; i < p_sb->i_bitrate; i++ )
    {
        msg_Dbg( s,"  - stream=%u bitrate=%"PRIu32,
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

static void ASF_FreeObject_extended_stream_properties( asf_object_t *p_obj)
{
    asf_object_extended_stream_properties_t *p_esp = &p_obj->ext_stream;

    if ( p_esp->p_ext )
    {
        for( uint16_t i = 0; i < p_esp->i_payload_extension_system_count; i++ )
            free( p_esp->p_ext[i].pi_info );
        FREENULL( p_esp->p_ext );
    }
    for( uint16_t i = 0; i < p_esp->i_stream_name_count; i++ )
        FREENULL( p_esp->ppsz_stream_name[i] );
    FREENULL( p_esp->pi_stream_name_language );
    FREENULL( p_esp->ppsz_stream_name );
}

static int ASF_ReadObject_extended_stream_properties( stream_t *s,
                                                      asf_object_t *p_obj)
{
    asf_object_extended_stream_properties_t *p_esp = &p_obj->ext_stream;
    const uint8_t *p_peek, *p_data;
    uint16_t i;

    if( p_esp->i_object_size > INT32_MAX )
        return VLC_EGENERIC;

    ssize_t i_peek = vlc_stream_Peek( s, &p_peek, p_esp->i_object_size );
    if( i_peek < 88 )
       return VLC_EGENERIC;

    p_data = &p_peek[ASF_OBJECT_COMMON_SIZE];

    p_esp->i_start_time = GetQWLE( &p_data[0] );
    p_esp->i_end_time = GetQWLE( &p_data[8] );
    p_esp->i_data_bitrate = GetDWLE( &p_data[16] );
    p_esp->i_buffer_size = GetDWLE( &p_data[20] );
    p_esp->i_initial_buffer_fullness = GetDWLE( &p_data[ASF_OBJECT_COMMON_SIZE] );
    p_esp->i_alternate_data_bitrate = GetDWLE( &p_data[28] );
    p_esp->i_alternate_buffer_size = GetDWLE( &p_data[32] );
    p_esp->i_alternate_initial_buffer_fullness = GetDWLE( &p_data[36] );
    p_esp->i_maximum_object_size = GetDWLE( &p_data[40] );
    p_esp->i_flags = GetDWLE( &p_data[44] );
    p_esp->i_stream_number = GetWLE( &p_data[48] );
    if ( p_esp->i_stream_number > ASF_MAX_STREAMNUMBER )
        return VLC_EGENERIC;
    p_esp->i_language_index = GetWLE( &p_data[50] );
    p_esp->i_average_time_per_frame= GetQWLE( &p_data[52] );
    p_esp->i_stream_name_count = GetWLE( &p_data[60] );
    p_esp->i_payload_extension_system_count = GetWLE( &p_data[62] );

    p_data += 64;

    p_esp->pi_stream_name_language = calloc( p_esp->i_stream_name_count,
                                             sizeof(uint16_t) );
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

    p_esp->p_ext = calloc( p_esp->i_payload_extension_system_count,
                           sizeof( asf_payload_extension_system_t ) );
    if ( p_esp->p_ext )
    {
        for( i = 0; i < p_esp->i_payload_extension_system_count; i++ )
        {
            asf_payload_extension_system_t *p_ext = & p_esp->p_ext[i];
            if( !ASF_HAVE( 16+2+4 ) ) break;
            ASF_GetGUID( &p_ext->i_extension_id, p_data );
            ASF_SKIP( 16 );   // GUID
            p_ext->i_data_size = ASF_READ2();
            p_ext->i_info_length = ASF_READ4();
            if ( p_ext->i_info_length )
            {
                if( !ASF_HAVE( p_ext->i_info_length ) ) break;
                p_ext->pi_info = malloc( p_ext->i_info_length );
                if ( p_ext->pi_info )
                    memcpy( p_ext->pi_info, p_data, p_ext->i_info_length );
                ASF_SKIP( p_ext->i_info_length );
            }
        }
        p_esp->i_payload_extension_system_count = i;
    } else p_esp->i_payload_extension_system_count = 0;

    p_esp->p_sp = NULL;

    /* Read tail objects */
    if( p_data < &p_peek[i_peek] )
    {
        if( vlc_stream_Read( s, NULL, p_data - p_peek ) != (p_data - p_peek) )
        {
            ASF_FreeObject_extended_stream_properties( p_obj );
            return VLC_EGENERIC;
        }

        asf_object_t *p_sp = malloc( sizeof( asf_object_t ) );
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
    msg_Dbg( s, "  - start=%"PRIu64" end=%"PRIu64,
             p_esp->i_start_time, p_esp->i_end_time );
    msg_Dbg( s, "  - data bitrate=%"PRId32" buffer=%"PRId32" initial fullness=%"PRId32,
             p_esp->i_data_bitrate,
             p_esp->i_buffer_size,
             p_esp->i_initial_buffer_fullness );
    msg_Dbg( s, "  - alternate data bitrate=%"PRId32" buffer=%"PRId32" initial fullness=%"PRId32,
             p_esp->i_alternate_data_bitrate,
             p_esp->i_alternate_buffer_size,
             p_esp->i_alternate_initial_buffer_fullness );
    msg_Dbg( s, "  - maximum object size=%"PRId32, p_esp->i_maximum_object_size );
    msg_Dbg( s, "  - flags=0x%x", p_esp->i_flags );
    msg_Dbg( s, "  - stream number=%u language=%u",
             p_esp->i_stream_number, p_esp->i_language_index );
    msg_Dbg( s, "  - average time per frame=%"PRIu64,
             p_esp->i_average_time_per_frame );
    msg_Dbg( s, "  - stream name count=%u", p_esp->i_stream_name_count );
    for( i = 0; i < p_esp->i_stream_name_count; i++ )
        msg_Dbg( s, "     - lang id=%u name=%s",
                 p_esp->pi_stream_name_language[i],
                 p_esp->ppsz_stream_name[i] );
    msg_Dbg( s, "  - payload extension system count=%u",
             p_esp->i_payload_extension_system_count );
    for( i = 0; i < p_esp->i_payload_extension_system_count; i++ )
        msg_Dbg( s, "  - %u  - payload extension: " GUID_FMT, i,
                 GUID_PRINT( p_esp->p_ext[i].i_extension_id ) );
#endif
    return VLC_SUCCESS;
}

static int ASF_ReadObject_advanced_mutual_exclusion( stream_t *s,
                                                     asf_object_t *p_obj)
{
    asf_object_advanced_mutual_exclusion_t *p_ae = &p_obj->advanced_mutual_exclusion;
    const uint8_t *p_peek, *p_data;
    uint16_t i;

    if( p_ae->i_object_size > INT32_MAX )
        return VLC_EGENERIC;

    ssize_t i_peek = vlc_stream_Peek( s, &p_peek, p_ae->i_object_size );
    if( i_peek < 42 )
       return VLC_EGENERIC;

    p_data = &p_peek[ASF_OBJECT_COMMON_SIZE];

    if( !ASF_HAVE( 16 + 2 * sizeof(uint16_t) ) ) /* at least one entry */
        return VLC_EGENERIC;

    if ( guidcmp( (const vlc_guid_t *) p_data, &asf_guid_mutex_language ) )
        p_ae->exclusion_type = LANGUAGE;
    else if ( guidcmp( (const vlc_guid_t *) p_data, &asf_guid_mutex_bitrate ) )
        p_ae->exclusion_type = BITRATE;
    ASF_SKIP( 16 );

    p_ae->i_stream_number_count = ASF_READ2();
    p_ae->pi_stream_number = calloc( p_ae->i_stream_number_count, sizeof(uint16_t) );
    if ( !p_ae->pi_stream_number )
    {
        p_ae->i_stream_number_count = 0;
        return VLC_ENOMEM;
    }

    for( i = 0; i < p_ae->i_stream_number_count; i++ )
    {
        if( !ASF_HAVE(2) )
            break;
        p_ae->pi_stream_number[i] = ASF_READ2();
        if ( p_ae->pi_stream_number[i] > ASF_MAX_STREAMNUMBER )
            break;
    }
    p_ae->i_stream_number_count = i;

#ifdef ASF_DEBUG
    msg_Dbg( s, "read \"advanced mutual exclusion object\" type %s",
             p_ae->exclusion_type == LANGUAGE ? "Language" :
             ( p_ae->exclusion_type == BITRATE ) ? "Bitrate" : "Unknown"
    );
    for( i = 0; i < p_ae->i_stream_number_count; i++ )
        msg_Dbg( s, "  - stream=%u", p_ae->pi_stream_number[i] );
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
    uint16_t i;

    if( p_sp->i_object_size > INT32_MAX )
        return VLC_EGENERIC;

    ssize_t i_peek = vlc_stream_Peek( s, &p_peek, p_sp->i_object_size );
    if( i_peek < 26 )
       return VLC_EGENERIC;

    p_data = &p_peek[ASF_OBJECT_COMMON_SIZE];

    p_sp->i_priority_count = ASF_READ2();

    p_sp->pi_priority_flag = calloc( p_sp->i_priority_count, sizeof(uint16_t) );
    p_sp->pi_priority_stream_number =
                             calloc( p_sp->i_priority_count, sizeof(uint16_t) );

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
        msg_Dbg( s, "  - Stream:%u flags=0x%x",
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

static int ASF_ReadObject_bitrate_mutual_exclusion( stream_t *s, asf_object_t *p_obj )
{
    asf_object_bitrate_mutual_exclusion_t *p_ex = &p_obj->bitrate_mutual_exclusion;
    const uint8_t *p_peek, *p_data;

    if( p_ex->i_object_size > INT32_MAX )
        return VLC_EGENERIC;

    ssize_t i_peek = vlc_stream_Peek( s, &p_peek, p_ex->i_object_size );
    if( i_peek < 42 )
       return VLC_EGENERIC;

    p_data = &p_peek[ASF_OBJECT_COMMON_SIZE];

    if( !ASF_HAVE( 16 + 2 * sizeof(uint16_t) ) ) /* at least one entry */
        return VLC_EGENERIC;

    if ( guidcmp( (const vlc_guid_t *) p_data, &asf_guid_mutex_language ) )
        p_ex->exclusion_type = LANGUAGE;
    else if ( guidcmp( (const vlc_guid_t *) p_data, &asf_guid_mutex_bitrate ) )
        p_ex->exclusion_type = BITRATE;
    ASF_SKIP( 16 );

    p_ex->i_stream_number_count = ASF_READ2();
    p_ex->pi_stream_numbers = calloc( p_ex->i_stream_number_count, sizeof(uint16_t) );
    if ( ! p_ex->pi_stream_numbers )
    {
        p_ex->i_stream_number_count = 0;
        return VLC_ENOMEM;
    }

    for( uint16_t i = 0; i < p_ex->i_stream_number_count; i++ )
    {
        if( !ASF_HAVE(2) )
            break;
        p_ex->pi_stream_numbers[i] = ASF_READ2();
        if ( p_ex->pi_stream_numbers[i] > ASF_MAX_STREAMNUMBER )
        {
            free( p_ex->pi_stream_numbers );
            return VLC_EGENERIC;
        }
    }

#ifdef ASF_DEBUG
    msg_Dbg( s, "read \"bitrate exclusion object\" type %s",
             p_ex->exclusion_type == LANGUAGE ? "Language" :
             ( p_ex->exclusion_type == BITRATE ) ? "Bitrate" : "Unknown"
    );
    for( uint16_t i = 0; i < p_ex->i_stream_number_count; i++ )
        msg_Dbg( s, "  - stream=%u", p_ex->pi_stream_numbers[i] );
#endif

    return VLC_SUCCESS;
}
static void ASF_FreeObject_bitrate_mutual_exclusion( asf_object_t *p_obj)
{
    asf_object_bitrate_mutual_exclusion_t *p_ex = &p_obj->bitrate_mutual_exclusion;

    FREENULL( p_ex->pi_stream_numbers );
    p_ex->i_stream_number_count = 0;
}

static int ASF_ReadObject_extended_content_description( stream_t *s,
                                                        asf_object_t *p_obj)
{
    asf_object_extended_content_description_t *p_ec =
                                        &p_obj->extended_content_description;
    const uint8_t *p_peek, *p_data;
    uint16_t i;

    if( p_ec->i_object_size > INT32_MAX )
        return VLC_EGENERIC;

    ssize_t i_peek = vlc_stream_Peek( s, &p_peek, p_ec->i_object_size );
    if( i_peek < 26 )
       return VLC_EGENERIC;

    p_data = &p_peek[ASF_OBJECT_COMMON_SIZE];

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
        uint16_t i_size;
        uint16_t i_type;

        if( !ASF_HAVE(2 + 2+2) )
            break;

        p_ec->ppsz_name[i] = ASF_READS( ASF_READ2() );

        /* Grrr */
        i_type = ASF_READ2();
        i_size = ASF_READ2();

        if( i_type == ASF_METADATA_TYPE_STRING )
        {
            /* Unicode string */
            p_ec->ppsz_value[i] = ASF_READS( i_size );
        }
        else if( i_type == ASF_METADATA_TYPE_BYTE )
        {
            /* Byte array */
            static const char hex[16] = "0123456789ABCDEF";

            p_ec->ppsz_value[i] = malloc( 2*i_size + 1 );
            if( p_ec->ppsz_value[i] )
            {
                char *psz_value = p_ec->ppsz_value[i];
                for( int j = 0; j < i_size; j++ )
                {
                    const uint8_t v = ASF_READ1();
                    psz_value[2*j+0] = hex[v>>4];
                    psz_value[2*j+1] = hex[v&0xf];
                }
                psz_value[2*i_size] = '\0';
            }
        }
        else if( i_type == ASF_METADATA_TYPE_BOOL )
        {
            /* Bool */
            p_ec->ppsz_value[i] = strdup( ASF_READ1() ? "true" : "false" );
            ASF_SKIP(i_size-1);
        }
        else if( i_type == ASF_METADATA_TYPE_DWORD )
        {
            /* DWord */
            if( asprintf( &p_ec->ppsz_value[i], "%u", ASF_READ4() ) == -1 )
                p_ec->ppsz_value[i] = NULL;
        }
        else if( i_type == ASF_METADATA_TYPE_QWORD )
        {
            /* QWord */
            if( asprintf( &p_ec->ppsz_value[i], "%"PRIu64, ASF_READ8() ) == -1 )
                p_ec->ppsz_value[i] = NULL;
        }
        else if( i_type == ASF_METADATA_TYPE_WORD )
        {
            /* Word */
            if( asprintf( &p_ec->ppsz_value[i], "%u", ASF_READ2() ) == -1 )
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

    for( uint16_t i = 0; i < p_ec->i_count; i++ )
    {
        FREENULL( p_ec->ppsz_name[i] );
        FREENULL( p_ec->ppsz_value[i] );
    }
    FREENULL( p_ec->ppsz_name );
    FREENULL( p_ec->ppsz_value );
}

static int ASF_ReadObject_marker(stream_t *s, asf_object_t *p_obj)
{
    asf_object_marker_t *p_mk = (asf_object_marker_t *)p_obj;
    const uint8_t *p_peek, *p_data;

    if( p_mk->i_object_size > INT32_MAX )
        return VLC_EGENERIC;

    ssize_t i_peek = vlc_stream_Peek( s, &p_peek, p_mk->i_object_size );
    if( i_peek < ASF_OBJECT_COMMON_SIZE )
       return VLC_EGENERIC;

    p_data = &p_peek[ASF_OBJECT_COMMON_SIZE];

    if( !ASF_HAVE( 16+4+2+2 ) )
        return VLC_EGENERIC;

    ASF_GetGUID( &p_mk->i_reserved1, p_data );
    ASF_SKIP( 16 );
    p_mk->i_count = ASF_READ4();
    p_mk->i_reserved2 = ASF_READ2();
    p_mk->name = ASF_READS( ASF_READ2() );

    if( p_mk->i_count > 0 )
    {
        p_mk->marker = calloc( p_mk->i_count,
                              sizeof( asf_marker_t ) );
        if( !p_mk->marker )
            return VLC_ENOMEM;

        for( uint32_t i = 0; i < p_mk->i_count; i++ )
        {
            asf_marker_t *p_marker = &p_mk->marker[i];

            if( !ASF_HAVE(8+8+2+4+4+4) )
                break;

            p_marker->i_offset = ASF_READ8();
            p_marker->i_presentation_time = ASF_READ8();
            p_marker->i_entry_length = ASF_READ2();
            p_marker->i_send_time = ASF_READ4();
            p_marker->i_flags = ASF_READ4();
            p_marker->i_marker_description_length = ASF_READ4();
            if( p_marker->i_marker_description_length <= (UINT32_MAX / 2) )
                p_marker->p_marker_description = ASF_READS( p_marker->i_marker_description_length * 2 );
            else
                p_marker->i_marker_description_length = 0;
        }
    }

#ifdef ASF_DEBUG
    msg_Dbg( s, "Read \"marker object\": %"PRIu32" chapters: %s", p_mk->i_count, p_mk->name );

    for( unsigned i = 0; i < p_mk->i_count; i++ )
        msg_Dbg( s, "New chapter named: %s", p_mk->marker[i].p_marker_description );
#endif
    return VLC_SUCCESS;
}
static void ASF_FreeObject_marker( asf_object_t *p_obj)
{
    asf_object_marker_t *p_mk = (asf_object_marker_t *)p_obj;

    for( uint32_t i = 0; i < p_mk->i_count; i++ )
    {
        FREENULL( p_mk->marker[i].p_marker_description  );
    }
    FREENULL( p_mk->marker );
    FREENULL( p_mk->name );
}

static int ASF_ReadObject_Raw(stream_t *s, asf_object_t *p_obj)
{
    VLC_UNUSED(s);
    VLC_UNUSED(p_obj);
    return VLC_SUCCESS;
}

/* */
static const struct ASF_Object_Function_entry
{
    const vlc_guid_t  *p_id;
    int     i_type;
    int     (*ASF_ReadObject_function)( stream_t *, asf_object_t *p_obj );
    void    (*ASF_FreeObject_function)( asf_object_t *p_obj );

} ASF_Object_Function [] =
{
    { &asf_object_header_guid, ASF_OBJECT_HEADER,
      ASF_ReadObject_Header, ASF_FreeObject_Null },
    { &asf_object_data_guid, ASF_OBJECT_DATA,
      ASF_ReadObject_Data, ASF_FreeObject_Null },
    { &asf_object_simple_index_guid, ASF_OBJECT_INDEX,
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
    { &asf_object_marker_guid, ASF_OBJECT_MARKER,
      ASF_ReadObject_marker, ASF_FreeObject_marker },
    { &asf_object_padding, ASF_OBJECT_PADDING, NULL, NULL },
    { &asf_object_compatibility_guid, ASF_OBJECT_OTHER, NULL, NULL },
    { &asf_object_content_description_guid, ASF_OBJECT_CONTENT_DESCRIPTION,
      ASF_ReadObject_content_description, ASF_FreeObject_content_description },
    { &asf_object_language_list, ASF_OBJECT_OTHER,
      ASF_ReadObject_language_list, ASF_FreeObject_language_list },
    { &asf_object_stream_bitrate_properties, ASF_OBJECT_OTHER,
      ASF_ReadObject_stream_bitrate_properties,
      ASF_FreeObject_stream_bitrate_properties },
    { &asf_object_extended_stream_properties_guid, ASF_OBJECT_OTHER,
      ASF_ReadObject_extended_stream_properties,
      ASF_FreeObject_extended_stream_properties },
    { &asf_object_advanced_mutual_exclusion, ASF_OBJECT_OTHER,
      ASF_ReadObject_advanced_mutual_exclusion,
      ASF_FreeObject_advanced_mutual_exclusion },
    { &asf_object_stream_prioritization, ASF_OBJECT_OTHER,
      ASF_ReadObject_stream_prioritization,
      ASF_FreeObject_stream_prioritization },
    { &asf_object_bitrate_mutual_exclusion_guid, ASF_OBJECT_OTHER,
      ASF_ReadObject_bitrate_mutual_exclusion,
      ASF_FreeObject_bitrate_mutual_exclusion },
    { &asf_object_extended_content_description, ASF_OBJECT_OTHER,
      ASF_ReadObject_extended_content_description,
      ASF_FreeObject_extended_content_description },
    { &asf_object_content_encryption_guid, ASF_OBJECT_OTHER,
      ASF_ReadObject_Raw, ASF_FreeObject_Null },
    { &asf_object_advanced_content_encryption_guid, ASF_OBJECT_OTHER,
      ASF_ReadObject_Raw, ASF_FreeObject_Null },
    { &asf_object_extended_content_encryption_guid, ASF_OBJECT_OTHER,
      ASF_ReadObject_Raw, ASF_FreeObject_Null },
};

static void ASF_ParentObject( asf_object_t *p_father, asf_object_t *p_obj )
{
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
}

static const struct ASF_Object_Function_entry * ASF_GetObject_Function( const vlc_guid_t *id )
{
    for( size_t i = 0; i < ARRAY_SIZE(ASF_Object_Function); i++ )
    {
        if( guidcmp( ASF_Object_Function[i].p_id, id ) )
            return &ASF_Object_Function[i];
    }
    return NULL;
}

static int ASF_ReadObject( stream_t *s, asf_object_t *p_obj,
                           asf_object_t *p_father )
{
    int i_result = VLC_SUCCESS;

    if( !p_obj )
        return VLC_SUCCESS;

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
    p_obj->common.i_type = 0;

    if( p_obj->common.i_object_size < ASF_OBJECT_COMMON_SIZE )
    {
        msg_Warn( s, "found a corrupted asf object (size<24)" );
        return VLC_EGENERIC;
    }

    const struct ASF_Object_Function_entry *p_reader =
            ASF_GetObject_Function( &p_obj->common.i_object_id );
    if( p_reader )
    {
        p_obj->common.i_type = p_reader->i_type;

        /* Now load this object */
        if( p_reader->ASF_ReadObject_function != NULL )
            i_result = p_reader->ASF_ReadObject_function( s, p_obj );
    }
    else
    {
        msg_Warn( s, "unknown asf object (not loaded): " GUID_FMT,
                GUID_PRINT( p_obj->common.i_object_id ) );
    }

    /* link this object with father */
    if ( i_result == VLC_SUCCESS )
        ASF_ParentObject( p_father, p_obj );

    return i_result;
}

static void ASF_FreeObject( stream_t *s, asf_object_t *p_obj )
{
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
    const struct ASF_Object_Function_entry *p_entry =
            ASF_GetObject_Function( &p_obj->common.i_object_id );
    if( p_entry && p_entry->ASF_FreeObject_function )
    {
        /* Now free this object */
#ifdef ASF_DEBUG
        msg_Dbg( s,
                  "freing asf object " GUID_FMT,
                  GUID_PRINT( p_obj->common.i_object_id ) );
#endif
        p_entry->ASF_FreeObject_function( p_obj );
    }

    free( p_obj );
}

/*****************************************************************************
 * ASF_ObjectDumpDebug:
 *****************************************************************************/
static const struct
{
    const vlc_guid_t *p_id;
    const char *psz_name;
} ASF_ObjectDumpDebugInfo[] =
{
    { &vlc_object_root_guid, "Root" },
    { &asf_object_header_guid, "Header" },
    { &asf_object_data_guid, "Data" },
    { &asf_object_index_guid, "Index" },
    { &asf_object_simple_index_guid, "Simple Index" },
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
    { &asf_object_extended_stream_properties_guid, "Extended Stream Properties" },
    { &asf_object_advanced_mutual_exclusion, "Advanced Mutual Exclusion" },
    { &asf_object_stream_prioritization, "Stream Prioritization" },
    { &asf_object_bitrate_mutual_exclusion_guid, "Bitrate Mutual Exclusion" },
    { &asf_object_extended_content_description, "Extended content description"},
    { &asf_object_content_encryption_guid, "Content Encryption"},
    { &asf_object_advanced_content_encryption_guid, "Advanced Content Encryption"},
    { &asf_object_extended_content_encryption_guid, "Entended Content Encryption"},
    /* Non Readable from this point */
    { &nonasf_object_index_placeholder_guid, "Index Placeholder"},
    { &nonasf_object_compatibility, "Object Compatibility"},

    { NULL, "Unknown" },
};


static void ASF_ObjectDumpDebug( vlc_object_t *p_obj,
                                 asf_object_common_t *p_node, unsigned i_level )
{
    unsigned i;
    union asf_object_u *p_child;
    const char *psz_name;

    /* Find the name */
    for( i = 0; ASF_ObjectDumpDebugInfo[i].p_id != NULL; i++ )
    {
        if( guidcmp( ASF_ObjectDumpDebugInfo[i].p_id,
                          &p_node->i_object_id ) )
            break;
    }
    psz_name = ASF_ObjectDumpDebugInfo[i].psz_name;

    char str[512];
    if( i_level >= (sizeof(str) - 1)/5 )
        return;

    memset( str, ' ', sizeof( str ) );
    for( i = 0; i < i_level; i++ )
    {
        str[i * 4] = '|';
    }
    snprintf( &str[4*i_level], sizeof(str) - 5*i_level,
             "+ '%s'"
#ifdef ASF_DEBUG
             "GUID "GUID_FMT" size:%"PRIu64" pos:%"PRIu64
#endif
             , psz_name

#ifdef ASF_DEBUG
             , GUID_PRINT( p_node->i_object_id ),
             p_node->i_object_size, p_node->i_object_pos
#endif
             );


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
    uint64_t i_boundary = 0;

    if( !p_root )
        return NULL;

    p_root->i_type = ASF_OBJECT_ROOT;
    memcpy( &p_root->i_object_id, &vlc_object_root_guid, sizeof( vlc_guid_t ) );
    p_root->i_object_pos = vlc_stream_Tell( s );
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
                if ( p_root->p_index || p_root->p_data || p_root->p_hdr ) break;
                p_root->p_hdr = (asf_object_header_t*)p_obj;
                break;
            case( ASF_OBJECT_DATA ):
                if ( p_root->p_index || p_root->p_data ) break;
                p_root->p_data = (asf_object_data_t*)p_obj;
            break;
            case( ASF_OBJECT_INDEX ):
                if ( p_root->p_index ) break;
                p_root->p_index = (asf_object_index_t*)p_obj;
                break;
            default:
                msg_Warn( s, "unknown top-level object found: " GUID_FMT,
                      GUID_PRINT( p_obj->common.i_object_id ) );
                break;
        }

        /* Set a limit to avoid junk when possible */
        if ( guidcmp( &p_obj->common.i_object_id, &asf_object_file_properties_guid ) )
        {
            i_boundary = p_obj->file_properties.i_file_size;
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

        if( ASF_NextObject( s, p_obj, i_boundary ) ) /* Go to the next object */
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

                p_root->p_metadata =
                    ASF_FindObject( p_hdr_ext,
                                    &asf_object_metadata_guid, 0 );
                /* Special case for broken designed file format :( */
                i_ext_stream = ASF_CountObject( p_hdr_ext,
                                    &asf_object_extended_stream_properties_guid );
                for( int i = 0; i < i_ext_stream; i++ )
                {
                    asf_object_t *p_esp =
                        ASF_FindObject( p_hdr_ext,
                                   &asf_object_extended_stream_properties_guid, i );
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

int ASF_CountObject( void *_p_obj, const vlc_guid_t *p_guid )
{
    int i_count;
    asf_object_t *p_child, *p_obj;

    p_obj = (asf_object_t *)_p_obj;
    if( !p_obj )
        return 0;

    i_count = 0;
    p_child = p_obj->common.p_first;
    while( p_child )
    {
        if( guidcmp( &p_child->common.i_object_id, p_guid ) )
            i_count++;

        p_child = p_child->common.p_next;
    }
    return i_count;
}

void *ASF_FindObject( void *_p_obj, const vlc_guid_t *p_guid,
                        int i_number )
{
    asf_object_t *p_child, *p_obj;

    p_obj = (asf_object_t *)_p_obj;
    p_child = p_obj->common.p_first;

    while( p_child )
    {
        if( guidcmp( &p_child->common.i_object_id, p_guid ) )
        {
            if( i_number == 0 )
                return p_child;

            i_number--;
        }
        p_child = p_child->common.p_next;
    }
    return NULL;
}
