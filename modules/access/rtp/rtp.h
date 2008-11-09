/**
 * @file rtp.h
 * @brief RTP demux module shared declarations
 */
/*****************************************************************************
 * Copyright © 2008 Rémi Denis-Courmont
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2.0
 * of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 ****************************************************************************/

/* RTP payload format */
typedef struct rtp_pt_t rtp_pt_t;
struct rtp_pt_t
{
    void   *(*init) (demux_t *);
    void    (*destroy) (demux_t *, void *);
    void    (*decode) (demux_t *, void *, block_t *);
    uint32_t  frequency; /* RTP clock rate (Hz) */
    uint8_t   number;
};

/* RTP session */
typedef struct rtp_session_t rtp_session_t;
rtp_session_t *rtp_session_create (demux_t *);
void rtp_session_destroy (demux_t *, rtp_session_t *);
void rtp_receive (demux_t *, rtp_session_t *, block_t *);
int rtp_add_type (demux_t *demux, rtp_session_t *ses, const rtp_pt_t *pt);


/* Global data */
struct demux_sys_t
{
    rtp_session_t *session;
    struct srtp_session_t *srtp;
    int           fd;
    int           rtcp_fd;

    unsigned      caching;
    unsigned      timeout;
    uint8_t       max_src;
    uint16_t      max_dropout;
    uint16_t      max_misorder;
    bool          autodetect;
    bool          framed_rtp;
};

