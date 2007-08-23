/*****************************************************************************
 * rtp.h: rtp stream output module header
 *****************************************************************************
 * Copyright (C) 2003-2007 the VideoLAN team
 * $Id: rtp.c 21407 2007-08-22 20:10:41Z courmisch $
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Rémi Denis-Courmont
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

typedef struct rtsp_client_t rtsp_client_t;

int RtspSetup( sout_stream_t *p_stream, vlc_url_t * );

int RtspCallbackId( httpd_callback_sys_t *, httpd_client_t *,
                    httpd_message_t *, httpd_message_t * );

void RtspClientDel( sout_stream_t *, rtsp_client_t * );

char *SDPGenerate( const sout_stream_t *p_stream,
                   const char *psz_destination, vlc_bool_t b_rtsp );

typedef int (*pf_rtp_packetizer_t)( sout_stream_t *, sout_stream_id_t *,
                                    block_t * );

struct sout_stream_id_t
{
    sout_stream_t *p_stream;
    /* rtp field */
    uint8_t     i_payload_type;
    uint16_t    i_sequence;
    uint32_t    i_timestamp_start;
    uint8_t     ssrc[4];

    /* for sdp */
    int         i_clock_rate;
    char        *psz_rtpmap;
    char        *psz_fmtp;
    char        *psz_destination;
    int         i_port;
    int         i_cat;
    int         i_bitrate;

    /* Packetizer specific fields */
    pf_rtp_packetizer_t pf_packetize;
    int           i_mtu;

    /* for sending the packets */
    sout_access_out_t *p_access;

    vlc_mutex_t       lock_rtsp;
    int               i_rtsp_access;
    sout_access_out_t **rtsp_access;

    /* */
    sout_input_t      *p_input;

    /* RTSP url control */
    httpd_url_t  *p_rtsp_url;
};
