/*****************************************************************************
 * libasf.h : 
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: libasf.h,v 1.4 2002/11/14 16:17:47 fenrir Exp $
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

#include "codecs.h"                        /* BITMAPINFOHEADER, WAVEFORMATEX */

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

#define ASF_OBJECT_TYPE_NULL      0x0000
#define ASF_OBJECT_TYPE_ROOT      0x0001
#define ASF_OBJECT_TYPE_HEADER    0x0002
#define ASF_OBJECT_TYPE_DATA      0x0003
#define ASF_OBJECT_TYPE_INDEX     0x0004
#define ASF_OBJECT_TYPE_FILE_PROPERTIES     0x0005
#define ASF_OBJECT_TYPE_STREAM_PROPERTIES   0x0006
#define ASF_OBJECT_TYPE_EXTENTION_HEADER    0x0007
#define ASF_OBJECT_TYPE_CODEC_LIST          0x0008
#define ASF_OBJECT_TYPE_MARKER              0x0009
#define ASF_OBJECT_TYPE_CONTENT_DESCRIPTION 0x000a

static const guid_t asf_object_null_guid =
{
    0x00000000,
    0x0000,
    0x0000,
    { 0x00,0x00, 0x00,0x00,0x00,0x00,0x00,0x00 }
};

static const guid_t asf_object_header_guid = 
{
    0x75B22630,
    0x668E,
    0x11CF,
    { 0xA6,0xD9, 0x00,0xAA,0x00,0x62,0xCE,0x6C }
};

static const guid_t asf_object_data_guid = 
{
    0x75B22636,
    0x668E,
    0x11CF,
    { 0xA6,0xD9, 0x00,0xAA,0x00,0x62,0xCE,0x6C }
};



static const guid_t asf_object_index_guid =
{
    0x33000890,
    0xE5B1,
    0x11CF,
    { 0x89,0xF4, 0x00,0xA0,0xC9,0x03,0x49,0xCB }
};

static const guid_t asf_object_file_properties_guid =
{
    0x8cabdca1,
    0xa947,
    0x11cf,
    { 0x8e,0xe4, 0x00,0xC0,0x0C,0x20,0x53,0x65 }

};
static const guid_t asf_object_stream_properties_guid =
{
    0xB7DC0791,
    0xA9B7,
    0x11CF,
    { 0x8E,0xE6, 0x00,0xC0,0x0C,0x20,0x53,0x65 }

};

static const guid_t asf_object_content_description_guid =
{
    0x75B22633,
    0x668E,
    0x11CF,
    { 0xa6, 0xd9, 0x00, 0xaa, 0x00, 0x62, 0xce, 0x6c }
};

static const guid_t asf_object_header_extention_guid = 
{
   0x5FBF03B5,
   0xA92E,
   0x11CF,
   { 0x8E,0xE3, 0x00,0xC0,0x0C,0x20,0x53,0x65 } 
};

static const guid_t asf_object_codec_list_guid =
{
    0x86D15240,
    0x311D,
    0x11D0,
    { 0xA3,0xA4, 0x00,0xA0,0xC9,0x03,0x48,0xF6 }
};

static const guid_t asf_object_marker_guid =
{
    0xF487CD01,
    0xA951,
    0x11CF,
    { 0x8E,0xE6, 0x00,0xC0,0x0C,0x20,0x53,0x65 }

};

static const guid_t asf_object_stream_type_audio =
{
    0xF8699E40,
    0x5B4D,
    0x11CF,
    { 0xA8,0xFD, 0x00,0x80,0x5F,0x5C,0x44,0x2B }
};

static const guid_t asf_object_stream_type_video =
{
    0xbc19efc0,
    0x5B4D,
    0x11CF,
    { 0xA8,0xFD, 0x00,0x80,0x5F,0x5C,0x44,0x2B }
};

static const guid_t asf_object_stream_type_command =
{
    0x59DACFC0,
    0x59E6,
    0x11D0,
    { 0xA3,0xAC, 0x00,0xA0,0xC9,0x03,0x48,0xF6 }
};

#if 0
static const guid_t asf_object_
{


};
#endif
#if 0
typedef struct asf_packet_s
{
    int i_stream_number;

    int i_payload_size;
    u8  *p_payload_data;
    
} asf_packet_t;
#endif

#define ASF_OBJECT_COMMON           \
    int          i_type;            \
    guid_t       i_object_id;       \
    u64          i_object_size;     \
    u64          i_object_pos;      \
    union asf_object_u *p_father;  \
    union asf_object_u *p_first;   \
    union asf_object_u *p_last;    \
    union asf_object_u *p_next;

typedef struct asf_object_common_s
{
    ASF_OBJECT_COMMON

} asf_object_common_t;

typedef struct asf_index_entry_s
{
    uint32_t i_packet_number;
    uint16_t i_packet_count;

} asf_index_entry_t;

/****************************************************************************
 * High level asf object 
 ****************************************************************************/
/* This is the first header find in a asf file
 * It's the only object that have subobject */
typedef struct asf_object_header_s
{
    ASF_OBJECT_COMMON
    uint32_t i_sub_object_count;
    uint8_t  i_reserved1; /* 0x01, but could be safely ignored */
    uint8_t  i_reserved2; /* 0x02, if not must failed to source the contain */
   
} asf_object_header_t;

typedef struct asf_object_data_s
{
    ASF_OBJECT_COMMON
    guid_t      i_file_id;
    uint64_t    i_total_data_packets;
    uint16_t    i_reserved;
    
} asf_object_data_t;


typedef struct asf_object_index_s
{
    ASF_OBJECT_COMMON
    guid_t      i_file_id;
    uint64_t    i_index_entry_time_interval;
    uint32_t    i_max_packet_count;
    uint32_t    i_index_entry_count;
    
    asf_index_entry_t *index_entry;

} asf_object_index_t;

typedef struct asf_object_root_s
{
    ASF_OBJECT_COMMON
    
    asf_object_header_t *p_hdr;
    asf_object_data_t   *p_data;
    asf_object_index_t  *p_index;

} asf_object_root_t;

/****************************************************************************
 * Sub level asf object
 ****************************************************************************/
#define ASF_FILE_PROPERTIES_BROADCAST   0x01
#define ASF_FILE_PROPERTIES_SEEKABLE    0x02

typedef struct asf_object_file_properties_s
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
typedef struct asf_object_stream_properties_s
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

typedef struct asf_object_header_extention_s
{
    ASF_OBJECT_COMMON

    guid_t      i_reserved1;
    uint16_t    i_reserved2;
    uint32_t    i_header_extention_size;
    uint8_t     *p_header_extention_data;

} asf_object_header_extention_t;

typedef struct asf_objec_content_description_s
{
    ASF_OBJECT_COMMON

    char *psz_title;
    char *psz_author;
    char *psz_copyright;
    char *psz_description;
    char *psz_rating;

} asf_object_content_description_t;

typedef struct string16_s
{
    uint16_t i_length;
    uint16_t *i_char;
} string16_t;

#define ASF_CODEC_TYPE_VIDEO    0x0001
#define ASF_CODEC_TYPE_AUDIO    0x0002
#define ASF_CODEC_TYPE_UNKNOW   0xffff

typedef struct asf_codec_entry_s
{
    uint16_t    i_type;
    char        *psz_name;
    char        *psz_description;
    
    uint16_t    i_information_length;
    uint8_t     *p_information;
} asf_codec_entry_t;

typedef struct asf_object_codec_list_s
{
    ASF_OBJECT_COMMON
    guid_t      i_reserved;
    uint32_t    i_codec_entries_count;
    asf_codec_entry_t *codec; 

} asf_object_codec_list_t;

#if 0
typedef struct asf_object_script_command_s
{
    ASF_OBJECT_COMMON

    
} asf_object_script_command_t;
#endif
typedef struct asf_marker_s
{
    uint64_t     i_offset;
    uint64_t     i_presentation_time;
    uint16_t     i_entry_length;
    uint32_t     i_send_time;
    uint32_t     i_flags;
    uint32_t     i_marker_description_length;
    uint8_t      *i_marker_description;
    /* u8 padding */
            
} asf_marker_t;

typedef struct asf_object_marker_s
{
    ASF_OBJECT_COMMON
    guid_t      i_reserved1;
    uint32_t    i_count;
    uint16_t    i_reserved2;
    string16_t name;
    asf_marker_t *marker;

} asf_object_marker_t;

#if 0
typedef struct asf_object__s
{
    ASF_OBJECT_COMMON

} asf_object__t;
#endif

typedef union asf_object_u
{
    asf_object_common_t common;
    asf_object_header_t header;
    asf_object_data_t   data;
    asf_object_index_t  index;
    asf_object_root_t   root;
    asf_object_file_properties_t    file_properties;
    asf_object_stream_properties_t  stream_properties;
    asf_object_header_extention_t   header_extention;   
    asf_object_codec_list_t         codec_list;
    asf_object_marker_t             marker;
    
} asf_object_t;


off_t   ASF_TellAbsolute( input_thread_t *p_input );
int     ASF_SeekAbsolute( input_thread_t *p_input, off_t i_pos);
int     ASF_ReadData( input_thread_t *p_input, uint8_t *p_buff, int i_size );
int     ASF_SkipBytes( input_thread_t *p_input, int i_count );

void GetGUID( guid_t *p_guid, uint8_t *p_data );
int  CmpGUID( const guid_t *p_guid1, const guid_t *p_guid2 );
    
int  ASF_ReadObjectCommon( input_thread_t *p_input,
                           asf_object_t *p_obj );
int  ASF_NextObject( input_thread_t *p_input,
                     asf_object_t *p_obj );
int  ASF_GotoObject( input_thread_t *p_input,
                     asf_object_t *p_obj );

int  ASF_ReadObject( input_thread_t *p_input,
                     asf_object_t *p_obj,
                     asf_object_t *p_father );
void ASF_FreeObject( input_thread_t *p_input,
                     asf_object_t *p_obj );
int  ASF_ReadObjectRoot( input_thread_t *p_input,
                         asf_object_root_t *p_root,
                         int b_seekable );
void ASF_FreeObjectRoot( input_thread_t *p_input,
                         asf_object_root_t *p_root );
#define ASF_CountObject( a, b ) __ASF_CountObject( (asf_object_t*)(a), b )
int  __ASF_CountObject( asf_object_t *p_obj, const guid_t *p_guid );

#define ASF_FindObject( a, b, c )  __ASF_FindObject( (asf_object_t*)(a), b, c )
void *__ASF_FindObject( asf_object_t *p_obj, const guid_t *p_guid, int i_number );


