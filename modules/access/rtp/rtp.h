/**
 * @file rtp.h
 * @brief RTP demux module shared declarations
 */
/*****************************************************************************
 * Copyright © 2008 Rémi Denis-Courmont
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

typedef struct vlc_rtp_pt rtp_pt_t;
typedef struct rtp_session_t rtp_session_t;

struct vlc_demux_chained_t;
struct vlc_sdp_media;

/** @section RTP payload format */

struct vlc_sdp_pt {
    const struct vlc_sdp_media *media;
    char name[16];
    unsigned int clock_rate;
    unsigned char channel_count;
    const char *parameters;
};

struct vlc_rtp_pt_operations {
    void *(*init)(struct vlc_rtp_pt *, demux_t *);
    void (*destroy)(demux_t *, void *);
    void (*decode)(demux_t *, void *, block_t *);
};

struct vlc_rtp_pt
{
    const struct vlc_rtp_pt_operations *ops;
    uint32_t  frequency; /* RTP clock rate (Hz) */
    uint8_t   number;
    uint8_t channel_count;
};

/**
 * Destroys a payload type parameter set.
 */
void vlc_rtp_pt_release(struct vlc_rtp_pt *pt);

/**
 * Instantiates a payload type from a set of parameters.
 *
 * A given SDP media can have multiple alternative payload types, each with
 * their set of parameters. The RTP session can then have multiple concurrent
 * RTP sources (SSRC). This function creates an instance of a given payload
 * type for use by an unique RTP source.
 *
 * @param pt RTP payload type to instantiate
 * @param demux demux object for output
 * @return private data for the instance
 */
static inline void *vlc_rtp_pt_begin(struct vlc_rtp_pt *pt, demux_t *demux)
{
    assert(pt->ops->init != NULL);
    return pt->ops->init(pt, demux);
}

/**
 * Deinstantiates a payload type.
 *
 * This destroys an instance of a payload type created by vlc_rtp_pt_begin().
 *
 * @param pt RTP payload type to deinstantiate
 * @param demux demux object that was used by the instance
 * @param data instance private data as returned by vlc_rtp_pt_begin()
 */
static inline void vlc_rtp_pt_end(struct vlc_rtp_pt *pt, demux_t *demux,
                                  void *data)
{
    if (pt->ops->destroy != NULL)
        pt->ops->destroy(demux, data);
}

/**
 * Processes an payload packet.
 *
 * This passes a data payload of an RTP packet to the instance of the
 * payload type specified in the packet (PT and SSRC fields).
 */
static inline void vlc_rtp_pt_decode(const struct vlc_rtp_pt *pt,
                                     demux_t *demux,
                                     void *data, block_t *pkt)
{
    assert(pt->ops->decode != NULL);
    pt->ops->decode(demux, data, pkt);
}

void rtp_autodetect (demux_t *, rtp_session_t *, const block_t *);

static inline uint8_t rtp_ptype (const block_t *block)
{
    return block->p_buffer[1] & 0x7F;
}

void *codec_init (demux_t *demux, es_format_t *fmt);
void codec_destroy (demux_t *demux, void *data);
void codec_decode (demux_t *demux, void *data, block_t *block);

extern const struct vlc_rtp_pt_operations rtp_video_theora;

/** @section RTP session */
rtp_session_t *rtp_session_create (demux_t *);
void rtp_session_destroy (demux_t *, rtp_session_t *);
void rtp_queue (demux_t *, rtp_session_t *, block_t *);
bool rtp_dequeue (demux_t *, const rtp_session_t *, vlc_tick_t *);
int rtp_add_type (demux_t *demux, rtp_session_t *ses, const rtp_pt_t *pt);
int vlc_rtp_add_media_types(demux_t *demux, rtp_session_t *ses,
                            const struct vlc_sdp_media *media);

void *rtp_dgram_thread (void *data);

/* Global data */
typedef struct
{
    rtp_session_t *session;
    struct vlc_demux_chained_t *chained_demux;
#ifdef HAVE_SRTP
    struct srtp_session_t *srtp;
#endif
    struct vlc_dtls *rtp_sock;
    struct vlc_dtls *rtcp_sock;
    vlc_thread_t  thread;

    vlc_tick_t    timeout;
    uint16_t      max_dropout; /**< Max packet forward misordering */
    uint16_t      max_misorder; /**< Max packet backward misordering */
    uint8_t       max_src; /**< Max simultaneous RTP sources */
    bool          autodetect; /**< Payload type autodetection pending */
} demux_sys_t;

