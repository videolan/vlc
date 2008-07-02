/*****************************************************************************
 * libasf.h :
 *****************************************************************************
 * Copyright (C) 2001-2003 the VideoLAN team
 * $Id$
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
 * Structure needed for decoder
 *****************************************************************************/
typedef struct guid_s
{
    uint32_t v1; /* le */
    uint16_t v2; /* le */
    uint16_t v3; /* le */
    uint8_t  v4[8];
} guid_t;

enum
{
    ASF_OBJECT_NULL = 0,
    ASF_OBJECT_ROOT,
    ASF_OBJECT_HEADER,
    ASF_OBJECT_DATA,
    ASF_OBJECT_INDEX,
    ASF_OBJECT_FILE_PROPERTIES,
    ASF_OBJECT_STREAM_PROPERTIES,
    ASF_OBJECT_HEADER_EXTENSION,
    ASF_OBJECT_CODEC_LIST,
    ASF_OBJECT_MARKER,
    ASF_OBJECT_CONTENT_DESCRIPTION,
    ASF_OBJECT_METADATA,
    ASF_OBJECT_PADDING,
    ASF_OBJECT_OTHER,
};

static const guid_t asf_object_null_guid =
{
    0x00000000,
    0x0000,
    0x0000,
    { 0x00,0x00, 0x00,0x00,0x00,0x00,0x00,0x00 }
};

static const guid_t asf_object_header_guid =
{0x75B22630, 0x668E, 0x11CF, {0xA6, 0xD9, 0x00, 0xAA, 0x00, 0x62, 0xCE, 0x6C}};

static const guid_t asf_object_data_guid =
{0x75B22636, 0x668E, 0x11CF, {0xA6, 0xD9, 0x00, 0xAA, 0x00, 0x62, 0xCE, 0x6C}};

static const guid_t asf_object_index_guid =
{0x33000890, 0xE5B1, 0x11CF, {0x89, 0xF4, 0x00, 0xA0, 0xC9, 0x03, 0x49, 0xCB}};

static const guid_t asf_object_file_properties_guid =
{0x8cabdca1, 0xa947, 0x11cf, {0x8e, 0xe4, 0x00, 0xC0, 0x0C, 0x20, 0x53, 0x65}};

static const guid_t asf_object_stream_properties_guid =
{0xB7DC0791, 0xA9B7, 0x11CF, {0x8E, 0xE6, 0x00, 0xC0, 0x0C, 0x20, 0x53, 0x65}};

static const guid_t asf_object_content_description_guid =
{0x75B22633, 0x668E, 0x11CF, {0xa6, 0xd9, 0x00, 0xaa, 0x00, 0x62, 0xce, 0x6c}};

static const guid_t asf_object_header_extension_guid =
{0x5FBF03B5, 0xA92E, 0x11CF, {0x8E, 0xE3, 0x00, 0xC0, 0x0C, 0x20, 0x53, 0x65}};

static const guid_t asf_object_metadata_guid =
{0xC5F8CBEA, 0x5BAF, 0x4877, {0x84, 0x67, 0xAA, 0x8C, 0x44, 0xFA, 0x4C, 0xCA}};

static const guid_t asf_object_codec_list_guid =
{0x86D15240, 0x311D, 0x11D0, {0xA3, 0xA4, 0x00, 0xA0, 0xC9, 0x03, 0x48, 0xF6}};

static const guid_t asf_object_marker_guid =
{0xF487CD01, 0xA951, 0x11CF, {0x8E, 0xE6, 0x00, 0xC0, 0x0C, 0x20, 0x53, 0x65}};

static const guid_t asf_object_stream_type_audio =
{0xF8699E40, 0x5B4D, 0x11CF, {0xA8, 0xFD, 0x00, 0x80, 0x5F, 0x5C, 0x44, 0x2B}};

static const guid_t asf_object_stream_type_video =
{0xbc19efc0, 0x5B4D, 0x11CF, {0xA8, 0xFD, 0x00, 0x80, 0x5F, 0x5C, 0x44, 0x2B}};

static const guid_t asf_object_stream_type_command =
{0x59DACFC0, 0x59E6, 0x11D0, {0xA3, 0xAC, 0x00, 0xA0, 0xC9, 0x03, 0x48, 0xF6}};

/* TODO */
static const guid_t asf_object_stream_bitrate_properties =
{0x7BF875CE, 0x468D, 0x11D1, {0x8D, 0x82, 0x00, 0x60, 0x97, 0xC9, 0xA2, 0xB2}};

static const guid_t asf_object_language_list =
{0x7C4346A9, 0xEFE0, 0x4BFC, {0xB2, 0x29, 0x39, 0x3E, 0xDE, 0x41, 0x5C, 0x85}};

static const guid_t asf_object_extended_stream_properties =
{0x14E6A5CB, 0xC672, 0x4332, {0x83, 0x99, 0xA9, 0x69, 0x52, 0x06, 0x5B, 0x5A}};

static const guid_t asf_object_advanced_mutual_exclusion =
{0xA08649CF, 0x4775, 0x4670, {0x8A, 0x16, 0x6E, 0x35, 0x35, 0x75, 0x66, 0xCD}};

static const guid_t asf_object_padding =
{0x1806D474, 0xCADF, 0x4509, {0xA4, 0xBA, 0x9A, 0xAB, 0xCB, 0x96, 0xAA, 0xE8}};

static const guid_t asf_object_stream_prioritization =
{0xD4FED15B, 0x88D3, 0x454F, {0x81, 0xF0, 0xED, 0x5C, 0x45, 0x99, 0x9E, 0x24}};

static const guid_t asf_object_extended_content_description =
{0xD2D0A440, 0xE307, 0x11D2, {0x97, 0xF0, 0x00, 0xA0, 0xC9, 0x5E, 0xA8, 0x50}};

static const guid_t asf_object_extended_stream_header =
{0x3afb65e2, 0x47ef, 0x40f2, { 0xac, 0x2c, 0x70, 0xa9, 0x0d, 0x71, 0xd3, 0x43}};

static const guid_t asf_object_extended_stream_type_audio =
{0x31178c9d, 0x03e1, 0x4528, { 0xb5, 0x82, 0x3d, 0xf9, 0xdb, 0x22, 0xf5, 0x03}};

#define ASF_OBJECT_COMMON          \
    int          i_type;           \
    guid_t       i_object_id;      \
    uint64_t     i_object_size;    \
    uint64_t     i_object_pos;     \
    union asf_object_u *p_father;  \
    union asf_object_u *p_first;   \
    union asf_object_u *p_last;    \
    union asf_object_u *p_next;

typedef struct
{
    ASF_OBJECT_COMMON

} asf_object_common_t;

typedef struct
{
    uint32_t i_packet_number;
    uint16_t i_packet_count;

} asf_index_entry_t;

/****************************************************************************
 * High level asf object
 ****************************************************************************/
/* This is the first header found in an asf file
 * It's the only object that has subobjects */
typedef struct
{
    ASF_OBJECT_COMMON
    uint32_t i_sub_object_count;
    uint8_t  i_reserved1; /* 0x01, but could be safely ignored */
    uint8_t  i_reserved2; /* 0x02, if not must failed to source the contain */

} asf_object_header_t;

typedef struct
{
    ASF_OBJECT_COMMON
    guid_t      i_file_id;
    uint64_t    i_total_data_packets;
    uint16_t    i_reserved;

} asf_object_data_t;


typedef struct
{
    ASF_OBJECT_COMMON
    guid_t      i_file_id;
    uint64_t    i_index_entry_time_interval;
    uint32_t    i_max_packet_count;
    uint32_t    i_index_entry_count;

    asf_index_entry_t *index_entry;

} asf_object_index_t;

/****************************************************************************
 * Sub level asf object
 ****************************************************************************/
#define ASF_FILE_PROPERTIES_BROADCAST   0x01
#define ASF_FILE_PROPERTIES_SEEKABLE    0x02

typedef struct
{
    ASF_OBJECT_COMMON

    guid_t  i_file_id;
    uint64_t     i_file_size;
    uint64_t     i_creation_date;
    uint64_t     i_data_packets_count;
    uint64_t     i_play_duration;
    uint64_t     i_send_duration;
    uint64_t     i_preroll;
    uint32_t     i_flags;
    uint32_t     i_min_data_packet_size;
    uint32_t     i_max_data_packet_size;
    uint32_t     i_max_bitrate;

} asf_object_file_properties_t;

#define ASF_STREAM_PROPERTIES_ENCRYPTED 0x8000
typedef struct
{
    ASF_OBJECT_COMMON

    guid_t  i_stream_type;
    guid_t  i_error_correction_type;
    uint64_t     i_time_offset;
    uint32_t     i_type_specific_data_length;
    uint32_t     i_error_correction_data_length;
    uint16_t     i_flags;
        /* extrated from flags */
        uint8_t i_stream_number;
    uint32_t    i_reserved;
    uint8_t     *p_type_specific_data;
    uint8_t     *p_error_correction_data;
} asf_object_stream_properties_t;

typedef struct
{
    ASF_OBJECT_COMMON

    guid_t      i_reserved1;
    uint16_t    i_reserved2;
    uint32_t    i_header_extension_size;
    uint8_t     *p_header_extension_data;

} asf_object_header_extension_t;

#define ASF_METADATA_TYPE_STRING 0x0000
#define ASF_METADATA_TYPE_BYTE   0x0001
#define ASF_METADATA_TYPE_BOOL   0x0002
#define ASF_METADATA_TYPE_DWORD  0x0003
#define ASF_METADATA_TYPE_QWORD  0x0004
#define ASF_METADATA_TYPE_WORD   0x0005

typedef struct
{
    uint16_t    i_stream;
    uint16_t    i_type;
    char        *psz_name;

    int64_t i_val;
    int i_data;
    uint8_t *p_data;

} asf_metadata_record_t;

typedef struct
{
    ASF_OBJECT_COMMON

    uint32_t i_record_entries_count;
    asf_metadata_record_t *record;

} asf_object_metadata_t;

typedef struct
{
    ASF_OBJECT_COMMON

    char *psz_title;
    char *psz_artist;
    char *psz_copyright;
    char *psz_description;
    char *psz_rating;

} asf_object_content_description_t;

typedef struct
{
    uint16_t i_length;
    uint16_t *i_char;

} string16_t;

#define ASF_CODEC_TYPE_VIDEO    0x0001
#define ASF_CODEC_TYPE_AUDIO    0x0002
#define ASF_CODEC_TYPE_UNKNOW   0xffff

typedef struct
{
    uint16_t    i_type;
    char        *psz_name;
    char        *psz_description;

    uint16_t    i_information_length;
    uint8_t     *p_information;
} asf_codec_entry_t;

typedef struct
{
    ASF_OBJECT_COMMON
    guid_t      i_reserved;
    uint32_t    i_codec_entries_count;
    asf_codec_entry_t *codec;

} asf_object_codec_list_t;

typedef struct
{
    uint64_t     i_offset;
    uint64_t     i_presentation_time;
    uint16_t     i_entry_length;
    uint32_t     i_send_time;
    uint32_t     i_flags;
    uint32_t     i_marker_description_length;
    uint8_t      *i_marker_description;

} asf_marker_t;

typedef struct
{
    ASF_OBJECT_COMMON
    guid_t      i_reserved1;
    uint32_t    i_count;
    uint16_t    i_reserved2;
    string16_t name;
    asf_marker_t *marker;

} asf_object_marker_t;

typedef struct
{
    ASF_OBJECT_COMMON
    int  i_language;
    char **ppsz_language;

} asf_object_language_list_t;

typedef struct
{
    ASF_OBJECT_COMMON

    int i_bitrate;
    struct
    {
        int      i_stream_number;
        uint32_t i_avg_bitrate;
    } bitrate[128];
} asf_object_stream_bitrate_properties_t;

typedef struct
{
    ASF_OBJECT_COMMON

    int64_t i_start_time;
    int64_t i_end_time;
    int32_t i_data_bitrate;
    int32_t i_buffer_size;
    int32_t i_initial_buffer_fullness;
    int32_t i_alternate_data_bitrate;
    int32_t i_alternate_buffer_size;
    int32_t i_alternate_initial_buffer_fullness;
    int32_t i_maximum_object_size;

    int32_t i_flags;
    int16_t i_stream_number;
    int16_t i_language_index;
    int64_t i_average_time_per_frame;

    int     i_stream_name_count;
    int     i_payload_extension_system_count;

    int     *pi_stream_name_language;
    char    **ppsz_stream_name;

    asf_object_stream_properties_t *p_sp;
} asf_object_extended_stream_properties_t;

typedef struct
{
    ASF_OBJECT_COMMON

    guid_t  type;
    int16_t i_stream_number_count;
    int16_t *pi_stream_number;

} asf_object_advanced_mutual_exclusion_t;

typedef struct
{
    ASF_OBJECT_COMMON

    int i_priority_count;
    int *pi_priority_flag;
    int *pi_priority_stream_number;
} asf_object_stream_prioritization_t;

typedef struct
{
    ASF_OBJECT_COMMON

    int i_count;
    char **ppsz_name;
    char **ppsz_value;
} asf_object_extended_content_description_t;

/****************************************************************************
 * Special Root Object
 ****************************************************************************/
typedef struct
{
    ASF_OBJECT_COMMON

    asf_object_header_t *p_hdr;
    asf_object_data_t   *p_data;
    /* could be NULL if !b_seekable or not-present */
    asf_object_index_t  *p_index;

    /* from asf_object_header_t */
    asf_object_file_properties_t *p_fp;

    /* from asf_object_header_extension_t */
    asf_object_metadata_t *p_metadata;

} asf_object_root_t;

/****************************************************************************
 * asf_object_t: union of all objects.
 ****************************************************************************/
typedef union asf_object_u
{
    asf_object_common_t common;
    asf_object_header_t header;
    asf_object_data_t   data;
    asf_object_index_t  index;
    asf_object_root_t   root;
    asf_object_file_properties_t    file_properties;
    asf_object_stream_properties_t  stream_properties;
    asf_object_header_extension_t   header_extension;
    asf_object_metadata_t           metadata;
    asf_object_codec_list_t         codec_list;
    asf_object_marker_t             marker;
    asf_object_language_list_t      language_list;
    asf_object_stream_bitrate_properties_t stream_bitrate;
    asf_object_extended_stream_properties_t ext_stream;
    asf_object_content_description_t content_description;
    asf_object_advanced_mutual_exclusion_t advanced_mutual_exclusion;
    asf_object_stream_prioritization_t stream_prioritization;
    asf_object_extended_content_description_t extended_content_description;

} asf_object_t;


void ASF_GetGUID( guid_t *p_guid, const uint8_t *p_data );
bool ASF_CmpGUID( const guid_t *p_guid1, const guid_t *p_guid2 );

asf_object_root_t *ASF_ReadObjectRoot( stream_t *, int b_seekable );
void               ASF_FreeObjectRoot( stream_t *, asf_object_root_t *p_root );

#define ASF_CountObject( a, b ) __ASF_CountObject( (asf_object_t*)(a), b )
int  __ASF_CountObject ( asf_object_t *p_obj, const guid_t *p_guid );

#define ASF_FindObject( a, b, c )  __ASF_FindObject( (asf_object_t*)(a), b, c )
void *__ASF_FindObject( asf_object_t *p_obj, const guid_t *p_guid, int i_number );
