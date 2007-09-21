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

typedef struct rtsp_stream_t rtsp_stream_t;
typedef struct rtsp_stream_id_t rtsp_stream_id_t;

rtsp_stream_t *RtspSetup( sout_stream_t *p_stream, const vlc_url_t *url );
void RtspUnsetup( rtsp_stream_t *rtsp );

rtsp_stream_id_t *RtspAddId( rtsp_stream_t *rtsp, sout_stream_id_t *sid,
                             unsigned i,
                             const char *dst, int ttl,
                             unsigned loport, unsigned hiport );
void RtspDelId( rtsp_stream_t *rtsp, rtsp_stream_id_t * );

char *SDPGenerate( const sout_stream_t *p_stream, const char *rtsp_url );

int rtp_add_sink( sout_stream_id_t *id, int fd, vlc_bool_t rtcp_mux );
void rtp_del_sink( sout_stream_id_t *id, int fd );

/* RTCP */
typedef struct rtcp_sender_t rtcp_sender_t;
rtcp_sender_t *OpenRTCP (vlc_object_t *obj, int rtp_fd, int proto,
                         vlc_bool_t mux);
void CloseRTCP (rtcp_sender_t *rtcp);
void SendRTCP (rtcp_sender_t *restrict rtcp, const block_t *rtp);
