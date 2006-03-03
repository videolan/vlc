/*****************************************************************************
 * rtp.h: RTP/RTCP headerfile
 *****************************************************************************
 * Copyright (C) 2005 M2X
 *
 * $Id$
 *
 * Authors: Jean-Paul Saman <jpsaman #_at_# videolan dot org>
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

#ifndef _RTP_H
#define _RTP_H 1

#define RTP_HEADER_LEN 12
#define RTP_SEQ_NUM_SIZE 65536

/*
 * See RFC 1889 & RFC 2250.
 */
typedef struct {
    int fd;        /*< socket descriptor of rtp stream */

    unsigned int u_version;      /*< rtp version number */
    unsigned int u_CSRC_count;   /*< CSRC count */
    unsigned int u_payload_type; /*< type of RTP payload stream */
    vlc_bool_t   b_extension;    /*< The header is followed by exactly one header extension */
    unsigned int u_marker;       /*< marker field expect 1 */
    unsigned int u_seq_no;       /*< sequence number of RTP stream */
    uint32_t i_timestamp;        /*< timestamp of stream */
    unsigned int ssrc;           /*< stream number is used here. */

    unsigned int u_ext_length;   /*< lenght of extension field */

    /*int (*pf_connect)( void *p_userdata, char *p_server, int i_port );
    int (*pf_disconnect)( void *p_userdata );
    int (*pf_read)( void *p_userdata, uint8_t *p_buffer, int i_buffer );
    int (*pf_write)( void *p_userdata, uint8_t *p_buffer, int i_buffer );*/
} rtp_t;

#endif /* RTP_H */
