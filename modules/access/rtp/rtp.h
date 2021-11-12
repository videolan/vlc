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

/**
 * \defgroup rtp RTP
 * Real-time Transport Protocol
 * \ingroup net
 *
 * @{
 * \file
 */

typedef struct vlc_rtp_pt rtp_pt_t;
typedef struct rtp_session_t rtp_session_t;

struct vlc_demux_chained_t;
struct vlc_sdp_media;

/**
 * \defgroup rtp_pt RTP payload format
 * @{
 */

/**
 * Payload type mapping.
 *
 * This structure represents a mapping for an RTP payload format
 * extracted from an \ref sdp description.
 */
struct vlc_sdp_pt {
    const struct vlc_sdp_media *media; /**< Containant SDP media description */
    char name[16]; /**< RTP payload format name, i.e. MIME subtype */
    unsigned int clock_rate; /**< RTP clock rate (in Hertz) */
    unsigned char channel_count; /**< Number of channels (0 if unspecified) */
    const char *parameters; /**< Format parameters from the a=fmtp line */
};

/**
 * RTP payload format operations.
 *
 * This structures contains the callbacks provided by an RTP payload format.
 */
struct vlc_rtp_pt_operations {
    /**
     * Releases the payload format.
     *
     * This optional callback releases any resources associated with the
     * payload format, such as copies of the payload format parameters.
     *
     * \param pt RTP payload format that is being released
     */
    void (*release)(struct vlc_rtp_pt *pt);

    /**
     * Starts using a payload format.
     *
     * This required callback initialises per-source resources for the payload
     * format, such as an elementary stream output.
     *
     * \note There may be multiple RTP sources using the same payload format
     * concurrently within single given RTP session. This callback is invoked
     * for each source.
     *
     * \param pt RTP payload format being taken into use
     * \return a data pointer for decode() and destroy() callbacks
     */
    void *(*init)(struct vlc_rtp_pt *pt, demux_t *);

    /**
     * Stops using a payload format.
     *
     * This optional callback deinitialises per-source resources.
     *
     * \param data data pointer returned by init()
     */
    void (*destroy)(demux_t *, void *data);

    /**
     * Processes a data payload.
     *
     * \param data data pointer returned by init()
     * \param block payload of a received RTP packet
     */
    void (*decode)(demux_t *, void *data, block_t *block);
};

/**
 * RTP payload format.
 *
 * This structures represents a payload format within an RTP session
 * (\ref vlc_rtp_session_t).
 */
struct vlc_rtp_pt
{
    const struct vlc_rtp_pt_operations *ops; /**< Payload format callbacks */
    void *opaque; /**< Private data pointer */
    uint32_t frequency; /**< RTP clock rate (Hz) */
    uint8_t number; /**< RTP payload type number within the session (0-127) */
    uint8_t channel_count; /**< Channel count (zero if unspecified) */
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

struct vlc_rtp_es;

/**
 * RTP elementary output stream operations.
 */
struct vlc_rtp_es_operations {
    void (*destroy)(struct vlc_rtp_es *es);
    void (*send)(struct vlc_rtp_es *es, block_t *block);
};

/**
 * RTP elementary output stream.
 *
 * This structure represents a data sink for an active instance of a payload
 * format, typically an output elementary stream (ES) \ref es_out_id_t.
 */
struct vlc_rtp_es {
    const struct vlc_rtp_es_operations *ops;
};

/**
 * Destroys an \ref vlc_rtp_es.
 *
 * \param es object to release
 */
static inline void vlc_rtp_es_destroy(struct vlc_rtp_es *es)
{
    assert(es->ops->destroy != NULL);
    es->ops->destroy(es);
}

/**
 * Sends coded data for output.
 *
 * \param es output stream to send the data to
 * \param block data block to process
 */
static inline void vlc_rtp_es_send(struct vlc_rtp_es *es, block_t *block)
{
    assert(es->ops->send != NULL);
    es->ops->send(es, block);
}

/**
 * A dummy output that discards data.
 */
extern struct vlc_rtp_es *const vlc_rtp_es_dummy;

void rtp_autodetect(vlc_object_t *, rtp_session_t *, const block_t *);

static inline uint8_t rtp_ptype (const block_t *block)
{
    return block->p_buffer[1] & 0x7F;
}

void *codec_init (demux_t *demux, es_format_t *fmt);
void codec_destroy (demux_t *demux, void *data);
void codec_decode (demux_t *demux, void *data, block_t *block);

extern const struct vlc_rtp_pt_operations rtp_video_theora;

/** @} */

/**
 * \defgroup rtp_session RTP session
 * @{
 */
rtp_session_t *rtp_session_create (demux_t *);
void rtp_session_destroy (demux_t *, rtp_session_t *);
void rtp_queue (demux_t *, rtp_session_t *, block_t *);
bool rtp_dequeue (demux_t *, const rtp_session_t *, vlc_tick_t *);
int rtp_add_type(rtp_session_t *ses, rtp_pt_t *pt);
int vlc_rtp_add_media_types(vlc_object_t *obj, rtp_session_t *ses,
                            const struct vlc_sdp_media *media);

void *rtp_dgram_thread (void *data);

/** @} */
/** @} */

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

