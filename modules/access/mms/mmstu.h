/*****************************************************************************
 * mmstu.h: MMS access plug-in
 *****************************************************************************
 * Copyright (C) 2001, 2002 VLC authors and VideoLAN
 * $Id$
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

#ifndef _MMSTU_H_
#define _MMSTU_H_ 1

#define MMS_PACKET_ANY          0
#define MMS_PACKET_CMD          1
#define MMS_PACKET_HEADER       2
#define MMS_PACKET_MEDIA        3
#define MMS_PACKET_UDP_TIMING   4

#define MMS_CMD_HEADERSIZE  48

#define MMS_BUFFER_SIZE 100000

struct access_sys_t
{
    int         i_proto;        /* MMS_PROTO_TCP, MMS_PROTO_UDP */
    int         i_handle_tcp;   /* TCP socket for communication with server */
    int         i_handle_udp;   /* Optional UDP socket for data(media/header packet) */
                                /* send by server */
    char        sz_bind_addr[NI_MAXNUMERICHOST]; /* used by udp */

    vlc_url_t   url;

    asf_header_t    asfh;

    unsigned    i_timeout;

    /* */
    uint8_t     buffer_tcp[MMS_BUFFER_SIZE];
    int         i_buffer_tcp;

    uint8_t     buffer_udp[MMS_BUFFER_SIZE];
    int         i_buffer_udp;

    /* data necessary to send data to server */
    guid_t      guid;
    int         i_command_level;
    int         i_seq_num;
    uint32_t    i_header_packet_id_type;
    uint32_t    i_media_packet_id_type;

    int         i_packet_seq_num;

    uint8_t     *p_cmd;     /* latest command read */
    size_t      i_cmd;      /* allocated at the begining */

    uint8_t     *p_header;  /* allocated by mms_ReadPacket */
    size_t      i_header;

    uint8_t     *p_media;   /* allocated by mms_ReadPacket */
    size_t      i_media;
    size_t      i_media_used;

    /* extracted information */
    int         i_command;

    /* from 0x01 answer (not yet set) */
    char        *psz_server_version;
    char        *psz_tool_version;
    char        *psz_update_player_url;
    char        *psz_encryption_type;

    /* from 0x06 answer */
    uint32_t    i_flags_broadcast;
    uint32_t    i_media_length;
    size_t      i_packet_length;
    uint32_t    i_packet_count;
    int         i_max_bit_rate;
    size_t      i_header_size;

    /* misc */
    bool  b_seekable;

    vlc_mutex_t  lock_netwrite;
    bool         b_keep_alive;
    vlc_thread_t keep_alive;
};

#endif
