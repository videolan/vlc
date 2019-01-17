/*****************************************************************************
 * mmsh.h:
 *****************************************************************************
 * Copyright (C) 2001, 2002 VLC authors and VideoLAN
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

#ifndef VLC_MMS_MMSH_H_
#define VLC_MMS_MMSH_H_

typedef struct
{
    uint16_t i_type;
    uint16_t i_size;

    uint32_t i_sequence;
    uint16_t i_unknown;

    uint16_t i_size2;

    int      i_data;
    uint8_t  *p_data;

} chunk_t;

#define BUFFER_SIZE 65536
typedef struct
{
    int             i_proto;

    struct vlc_tls *stream;
    vlc_url_t       url;

    bool      b_proxy;
    vlc_url_t       proxy;

    int             i_request_context;

    uint8_t         buffer[BUFFER_SIZE + 1];

    bool      b_broadcast;

    uint8_t         *p_header;
    int             i_header;

    uint8_t         *p_packet;
    uint32_t        i_packet_sequence;
    unsigned int    i_packet_used;
    unsigned int    i_packet_length;

    uint64_t        i_start;
    uint64_t        i_position;

    asf_header_t    asfh;
    vlc_guid_t          guid;
} access_sys_t;

#endif
