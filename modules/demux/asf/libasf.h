/*****************************************************************************
 * libasf.h :
 *****************************************************************************
 * Copyright © 2001-2004, 2011 VLC authors and VideoLAN
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
#ifndef VLC_ASF_LIBASF_H_
#define VLC_ASF_LIBASF_H_

#define ASF_MAX_STREAMNUMBER 127
#define ASF_OBJECT_COMMON_SIZE 24

/*****************************************************************************
 * Structure needed for decoder
 *****************************************************************************/

#include "libasf_guid.h"

#define ASF_OBJECT_COMMON          \
    int          i_type;           \
    vlc_guid_t       i_object_id;      \
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
    vlc_guid_t      i_file_id;
    uint64_t    i_total_data_packets;
    uint16_t    i_reserved;

} asf_object_data_t;


typedef struct
{
    ASF_OBJECT_COMMON
    vlc_guid_t      i_file_id;
    msftime_t   i_index_entry_time_interval;
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

    vlc_guid_t  i_file_id;
    uint64_t     i_file_size;
    uint64_t     i_creation_date;
    uint64_t     i_data_packets_count;
    msftime_t    i_play_duration;
    msftime_t    i_send_duration;
    vlc_tick_t   i_preroll;
    uint32_t     i_flags;
    uint32_t     i_min_data_packet_size;
    uint32_t     i_max_data_packet_size;
    uint32_t     i_max_bitrate;

} asf_object_file_properties_t;

#define ASF_STREAM_PROPERTIES_ENCRYPTED 0x8000
typedef struct
{
    ASF_OBJECT_COMMON

    vlc_guid_t  i_stream_type;
    vlc_guid_t  i_error_correction_type;
    msftime_t    i_time_offset;
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

    vlc_guid_t      i_reserved1;
    uint16_t    i_reserved2;
    uint32_t    i_header_extension_size;
    uint8_t     *p_header_extension_data;

} asf_object_header_extension_t;

enum {
    ASF_METADATA_TYPE_STRING,
    ASF_METADATA_TYPE_BYTE,
    ASF_METADATA_TYPE_BOOL,
    ASF_METADATA_TYPE_DWORD,
    ASF_METADATA_TYPE_QWORD,
    ASF_METADATA_TYPE_WORD,
};

typedef struct
{
    uint16_t    i_stream;
    uint16_t    i_type;
    char        *psz_name;

    uint64_t i_val;
    uint16_t i_data;
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

#define ASF_CODEC_TYPE_VIDEO    0x0001
#define ASF_CODEC_TYPE_AUDIO    0x0002
#define ASF_CODEC_TYPE_UNKNOWN  0xffff

typedef struct asf_codec_entry
{
    uint16_t    i_type;
    char        *psz_name;
    char        *psz_description;

    uint16_t    i_information_length;
    uint8_t     *p_information;

    struct asf_codec_entry *p_next;
} asf_codec_entry_t;

typedef struct
{
    ASF_OBJECT_COMMON
    vlc_guid_t      i_reserved;
    asf_codec_entry_t *codecs;

} asf_object_codec_list_t;

typedef struct
{
    uint64_t     i_offset;
    uint64_t     i_presentation_time;
    uint16_t     i_entry_length;
    uint32_t     i_send_time;
    uint32_t     i_flags;
    uint32_t     i_marker_description_length;
    char         *p_marker_description;

} asf_marker_t;

typedef struct
{
    ASF_OBJECT_COMMON
    vlc_guid_t      i_reserved1;
    uint32_t    i_count;
    uint16_t    i_reserved2;
    char        *name;
    asf_marker_t *marker;

} asf_object_marker_t;

typedef struct
{
    ASF_OBJECT_COMMON
    uint16_t  i_language;
    char **ppsz_language;

} asf_object_language_list_t;

typedef struct
{
    ASF_OBJECT_COMMON

    uint16_t i_bitrate;
    struct
    {
        uint8_t  i_stream_number;
        uint32_t i_avg_bitrate;
    } bitrate[ASF_MAX_STREAMNUMBER + 1];
} asf_object_stream_bitrate_properties_t;


typedef struct
{
    vlc_guid_t   i_extension_id;
    uint16_t i_data_size;
    uint32_t i_info_length;
    char     *pi_info;
} asf_payload_extension_system_t;
#define ASF_EXTENSION_VIDEOFRAME_NEWFRAME  0x08
#define ASF_EXTENSION_VIDEOFRAME_IFRAME    0x01
#define ASF_EXTENSION_VIDEOFRAME_TYPE_MASK 0x07

typedef struct
{
    ASF_OBJECT_COMMON

    uint64_t i_start_time;
    uint64_t i_end_time;
    uint32_t i_data_bitrate;
    uint32_t i_buffer_size;
    uint32_t i_initial_buffer_fullness;
    uint32_t i_alternate_data_bitrate;
    uint32_t i_alternate_buffer_size;
    uint32_t i_alternate_initial_buffer_fullness;
    uint32_t i_maximum_object_size;

    uint32_t i_flags;
    uint16_t i_stream_number;
    uint16_t i_language_index;
    msftime_t i_average_time_per_frame;

    uint16_t i_stream_name_count;

    uint16_t i_payload_extension_system_count;
    asf_payload_extension_system_t *p_ext;

    uint16_t *pi_stream_name_language;
    char    **ppsz_stream_name;

    asf_object_stream_properties_t *p_sp;
} asf_object_extended_stream_properties_t;

#define ASF_MAX_EXCLUSION_TYPE 2
typedef enum
{
    LANGUAGE = ASF_MAX_EXCLUSION_TYPE,
    BITRATE = 1,
    UNKNOWN = 0
} asf_exclusion_type_t;

typedef struct
{
    ASF_OBJECT_COMMON

    asf_exclusion_type_t exclusion_type;
    uint16_t i_stream_number_count;
    uint16_t *pi_stream_number;

} asf_object_advanced_mutual_exclusion_t;

typedef struct
{
    ASF_OBJECT_COMMON

    uint16_t i_priority_count;
    uint16_t *pi_priority_flag;
    uint16_t *pi_priority_stream_number;
} asf_object_stream_prioritization_t;

typedef struct
{
    ASF_OBJECT_COMMON

    asf_exclusion_type_t exclusion_type;
    uint16_t i_stream_number_count;
    uint16_t *pi_stream_numbers;
} asf_object_bitrate_mutual_exclusion_t;

typedef struct
{
    ASF_OBJECT_COMMON

    uint16_t i_count;
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
    asf_object_bitrate_mutual_exclusion_t bitrate_mutual_exclusion;
    asf_object_extended_content_description_t extended_content_description;

} asf_object_t;

asf_object_root_t *ASF_ReadObjectRoot( stream_t *, int b_seekable );
void               ASF_FreeObjectRoot( stream_t *, asf_object_root_t *p_root );

int ASF_CountObject ( void *p_obj, const vlc_guid_t *p_guid );

void *ASF_FindObject( void *p_obj, const vlc_guid_t *p_guid, int i_number );
#endif
