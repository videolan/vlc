/*****************************************************************************
 * rtp.h: rtp stream output module header
 *****************************************************************************
 * Copyright (C) 2003-2007 the VideoLAN team
 * $Id$
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
                             unsigned i, uint32_t ssrc,
                             const char *dst, int ttl,
                             unsigned loport, unsigned hiport );
void RtspDelId( rtsp_stream_t *rtsp, rtsp_stream_id_t * );

char *SDPGenerate( const sout_stream_t *p_stream, const char *rtsp_url );

int rtp_add_sink( sout_stream_id_t *id, int fd, bool rtcp_mux );
void rtp_del_sink( sout_stream_id_t *id, int fd );
uint16_t rtp_get_seq( const sout_stream_id_t *id );
unsigned rtp_get_num( const sout_stream_id_t *id );

/* RTP packetization */
void rtp_packetize_common (sout_stream_id_t *id, block_t *out,
                           int b_marker, int64_t i_pts);
void rtp_packetize_send (sout_stream_id_t *id, block_t *out);
size_t rtp_mtu (const sout_stream_id_t *id);

int rtp_packetize_mpa  (sout_stream_id_t *, block_t *);
int rtp_packetize_mpv  (sout_stream_id_t *, block_t *);
int rtp_packetize_ac3  (sout_stream_id_t *, block_t *);
int rtp_packetize_split(sout_stream_id_t *, block_t *);
int rtp_packetize_mp4a (sout_stream_id_t *, block_t *);
int rtp_packetize_mp4a_latm (sout_stream_id_t *, block_t *);
int rtp_packetize_h263 (sout_stream_id_t *, block_t *);
int rtp_packetize_h264 (sout_stream_id_t *, block_t *);
int rtp_packetize_amr  (sout_stream_id_t *, block_t *);
int rtp_packetize_spx  (sout_stream_id_t *, block_t *);
int rtp_packetize_t140 (sout_stream_id_t *, block_t *);
int rtp_packetize_g726_16 (sout_stream_id_t *, block_t *);
int rtp_packetize_g726_24 (sout_stream_id_t *, block_t *);
int rtp_packetize_g726_32 (sout_stream_id_t *, block_t *);
int rtp_packetize_g726_40 (sout_stream_id_t *, block_t *);

/* RTCP */
typedef struct rtcp_sender_t rtcp_sender_t;
rtcp_sender_t *OpenRTCP (vlc_object_t *obj, int rtp_fd, int proto,
                         bool mux);
void CloseRTCP (rtcp_sender_t *rtcp);
void SendRTCP (rtcp_sender_t *restrict rtcp, const block_t *rtp);
