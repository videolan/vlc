/*****************************************************************************
 * mmsh.h:
 *****************************************************************************
 * Copyright (C) 2001, 2002 VideoLAN
 * $Id: mmsh.h,v 1.5 2004/01/21 16:56:16 fenrir Exp $
 *
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

#define BUFFER_SIZE 150000
struct access_sys_t
{
    int             i_proto;

    int             fd;
    url_t           *p_url;

    int             i_request_context;

    int             i_buffer;
    int             i_buffer_pos;
    uint8_t         buffer[BUFFER_SIZE + 1];

    vlc_bool_t      b_broadcast;

    uint8_t         *p_header;
    int             i_header;

    uint8_t         *p_packet;
    uint32_t        i_packet_sequence;
    unsigned int    i_packet_used;
    unsigned int    i_packet_length;

    off_t           i_pos;

    asf_header_t    asfh;
    guid_t          guid;
};

typedef struct http_field_s
{
    char *psz_name;
    char *psz_value;

    struct http_field_s *p_next;

} http_field_t;

typedef struct
{
    int     i_version;
    int     i_error;
    char    *psz_answer;

    http_field_t *p_fields;

    uint8_t *p_body;
    int     i_body;

} http_answer_t;


