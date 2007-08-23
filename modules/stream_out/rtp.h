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

/*ypedef struct rtsp_stream_t rtsp_stream_t;*/
typedef struct rtsp_stream_id_t rtsp_stream_id_t;
typedef struct rtsp_client_t rtsp_client_t;

int RtspSetup( sout_stream_t *p_stream, const vlc_url_t *url );
void RtspUnsetup( sout_stream_t *p_stream );

rtsp_stream_id_t *RtspAddId( sout_stream_t *p_stream, sout_stream_id_t *sid,
                             unsigned loport, unsigned hiport );
void RtspDelId( sout_stream_t *p_stream, rtsp_stream_id_t * );

char *SDPGenerate( const sout_stream_t *p_stream,
                   const char *psz_destination, vlc_bool_t b_rtsp );

int rtp_add_sink( sout_stream_id_t *id, sout_access_out_t *access );
void rtp_del_sink( sout_stream_id_t *id, sout_access_out_t *access );

typedef int (*pf_rtp_packetizer_t)( sout_stream_t *, sout_stream_id_t *,
                                    block_t * );

struct sout_stream_sys_t
{
    /* sdp */
    int64_t i_sdp_id;
    int     i_sdp_version;
    char    *psz_sdp;
    vlc_mutex_t  lock_sdp;

    char        *psz_session_name;
    char        *psz_session_description;
    char        *psz_session_url;
    char        *psz_session_email;

    /* */
    vlc_bool_t b_export_sdp_file;
    char *psz_sdp_file;
    /* sap */
    vlc_bool_t b_export_sap;
    session_descriptor_t *p_session;

    httpd_host_t *p_httpd_host;
    httpd_file_t *p_httpd_file;

    httpd_host_t *p_rtsp_host;
    httpd_url_t  *p_rtsp_url;
    char         *psz_rtsp_control;
    char         *psz_rtsp_path;

    /* */
    char *psz_destination;
    int  i_port;
    int  i_port_audio;
    int  i_port_video;
    int  i_ttl;
    vlc_bool_t b_latm;

    /* when need to use a private one or when using muxer */
    int i_payload_type;

    /* in case we do TS/PS over rtp */
    sout_mux_t        *p_mux;
    sout_access_out_t *p_access;
    int               i_mtu;
    sout_access_out_t *p_grab;
    uint16_t          i_sequence;
    uint32_t          i_timestamp_start;
    uint8_t           ssrc[4];
    block_t           *packet;

    /* */
    vlc_mutex_t      lock_es;
    int              i_es;
    sout_stream_id_t **es;

    /* */
    int              i_rtsp;
    rtsp_client_t    **rtsp;
};
