/**
 * @file input.h
 * @brief RTP demux input shared declarations
 */
/*****************************************************************************
 * Copyright © 2008 Rémi Denis-Courmont
 *             2023 VideoLabs, VideoLAN and VLC Authors
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1
 * of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 ****************************************************************************/

typedef struct
{
#ifdef HAVE_SRTP
    struct srtp_session_t *srtp;
#endif
    struct vlc_dtls *rtp_sock;
    struct vlc_dtls *rtcp_sock;
} rtp_input_sys_t;

/* Global data */
typedef struct
{
    struct vlc_logger *logger;
    rtp_session_t *session;
    vlc_thread_t  thread;
    rtp_input_sys_t input_sys;
} rtp_sys_t;
