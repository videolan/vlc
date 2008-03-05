/*****************************************************************************
 * rtmp_amf_flv.h: RTMP, AMF and FLV over RTMP implementation.
 *****************************************************************************
 * Copyright (C) URJC - LADyR - Luis Lopez Fernandez
 *
 * Author: Miguel Angel Cabrera Moya
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
 * Local prototypes (continued from access.c)
 *****************************************************************************/
typedef struct rtmp_packet_t rtmp_packet_t;
typedef struct rtmp_body_t rtmp_body_t;
typedef struct rtmp_control_thread_t rtmp_control_thread_t;
typedef void (*rtmp_handler_t)( rtmp_control_thread_t *rtmp_control_thread, rtmp_packet_t *rtmp_packet );

struct rtmp_packet_t
{
    int length_header;
    int stream_index;
    uint32_t timestamp;
    uint32_t timestamp_relative;
    int32_t length_encoded;
    int32_t length_body;
    uint8_t content_type;
    uint32_t src_dst;
    rtmp_body_t *body;
};

struct rtmp_body_t
{
    int32_t length_body; /* without interchunk headers */
    int32_t length_buffer;
    uint8_t *body;
};

struct rtmp_control_thread_t
{
    VLC_COMMON_MEMBERS

    int fd;

    block_fifo_t *p_fifo_media;
    block_fifo_t *p_empty_blocks;

    vlc_mutex_t lock;
    vlc_cond_t  wait;

    int result_connect;
    int result_play;
    int result_publish;

    double stream_client;
    double stream_server;

    char *psz_publish;

    /* Rebuild FLV variables */
    int has_audio;
    int has_video;
    int metadata_received;
    uint8_t metadata_stereo;
    uint8_t metadata_samplesize;
    uint8_t metadata_samplerate;
    uint8_t metadata_audiocodecid;
    uint8_t metadata_videocodecid;
    uint8_t metadata_frametype;
    int first_media_packet;
    uint32_t flv_tag_previous_tag_size;

    /* vars for channel state */
    rtmp_packet_t rtmp_headers_recv[64]; /* RTMP_HEADER_STREAM_MAX */
    rtmp_packet_t rtmp_headers_send[64];

    rtmp_handler_t rtmp_handler[21]; /* index by RTMP_CONTENT_TYPE */
};

struct access_sys_t
{
    int active;

    int fd;

    vlc_url_t url;
    char *psz_application;
    char *psz_media;

    /* vars for reading from fifo */
    block_t *flv_packet;
    int read_packet;

    /* thread for filtering and handling control messages */
    rtmp_control_thread_t *p_thread;
};

/*****************************************************************************
 * RTMP header:
 ******************************************************************************/
int rtmp_handshake_passive( vlc_object_t *p_this );
int rtmp_handshake_active( vlc_object_t *p_this );
int rtmp_connect_active( vlc_object_t *p_this );
//int rtmp_seek( access_t *p_access, int64_t i_pos ); TODO
int rtmp_send_bytes_read( access_t *p_access, uint32_t reply );
int rtmp_send_publish_start( access_t *p_access );

rtmp_packet_t *rtmp_read_net_packet( rtmp_control_thread_t *p_thread );
void rtmp_init_handler( rtmp_handler_t *rtmp_handler );
/*****************************************************************************
 * FLV header:
 ******************************************************************************/
block_t *flv_get_metadata( access_t *p_access );
block_t *flv_insert_header( access_t *p_access, block_t *first_packet );
