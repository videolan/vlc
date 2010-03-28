/**
 * @file rtp.h
 * @brief RTP demux module shared declarations
 */
/*****************************************************************************
 * Copyright © 2008 Rémi Denis-Courmont
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 ****************************************************************************/

typedef struct rtp_pt_t rtp_pt_t;
typedef struct rtp_session_t rtp_session_t;

/** @section RTP payload format */
struct rtp_pt_t
{
    void   *(*init) (demux_t *);
    void    (*destroy) (demux_t *, void *);
    void    (*decode) (demux_t *, void *, block_t *);
    uint32_t  frequency; /* RTP clock rate (Hz) */
    uint8_t   number;
};
int rtp_autodetect (demux_t *, rtp_session_t *, const block_t *);

static inline uint8_t rtp_ptype (const block_t *block)
{
    return block->p_buffer[1] & 0x7F;
}

/** @section RTP session */
rtp_session_t *rtp_session_create (demux_t *);
void rtp_session_destroy (demux_t *, rtp_session_t *);
void rtp_queue (demux_t *, rtp_session_t *, block_t *);
bool rtp_dequeue (demux_t *, const rtp_session_t *, mtime_t *);
int rtp_add_type (demux_t *demux, rtp_session_t *ses, const rtp_pt_t *pt);

void *rtp_thread (void *data);

/* Global data */
struct demux_sys_t
{
    rtp_session_t *session;
#ifdef HAVE_SRTP
    struct srtp_session_t *srtp;
#endif
    int           fd;
    int           rtcp_fd;
    vlc_thread_t  thread;
    vlc_timer_t   timer;
    vlc_mutex_t   lock;

    mtime_t       timeout;
    unsigned      caching;
    uint16_t      max_dropout; /**< Max packet forward misordering */
    uint16_t      max_misorder; /**< Max packet backward misordering */
    uint8_t       max_src; /**< Max simultaneous RTP sources */
    bool          framed_rtp; /**< Framed RTP packets over TCP */
    bool          thread_ready;
#if 0
    bool          dead; /**< End of stream */
#endif
};

