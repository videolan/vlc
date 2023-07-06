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
 * \defgroup rtp_pt RTP payload formats
 *
 * RTP is a somewhat simplistic protocol to carry multiplexed and timestamped
 * data over an unreliable network. It cannot be used as is: depending on the
 * concrete type and subtype of data, a format must be selected which
 * specifically defines how the data is carried as the payload of RTP packets.
 * This format is known as an RTP payload format.
 *
 * A given RTP session (\ref rtp_session_t) can use up to 128 different payload
 * types (\ref vlc_rtp_pt).
 * Each payload type is identified by a 7-bit value and designates a
 * payload format and an associated set of format-dependent parameters
 * specified by a payload type mapping (\ref vlc_sdp_pt).
 *
 * @{
 */

/**
 * Payload type mapping.
 *
 * This structure represents an RTP payload type mapping for an RTP payload
 * format extracted from an \ref sdp description.
 */
struct vlc_sdp_pt {
    const struct vlc_sdp_media *media; /**< Containant SDP media description */
    char name[16]; /**< RTP payload format name, i.e. MIME subtype */
    unsigned int clock_rate; /**< RTP clock rate (in Hertz) */
    unsigned char channel_count; /**< Number of channels (0 if unspecified) */
    const char *parameters; /**< Format parameters from the a=fmtp line */
};

/**
 * RTP packet infos.
 *
 * This structure conveys infos extracted from the header of an RTP packet
 * to payload format parsers.
 */
struct vlc_rtp_pktinfo {
    bool m; /**< M bit from the RTP header */
};

/**
 * RTP payload type operations.
 *
 * This structures contains the callbacks provided by an RTP payload type
 * (\ref vlc_rtp_pt).
 */
struct vlc_rtp_pt_operations {
    /**
     * Releases the payload type.
     *
     * This optional callback releases any resources associated with the
     * payload format, such as copies of the payload format parameters.
     *
     * \param pt RTP payload type that is being released
     */
    void (*release)(struct vlc_rtp_pt *pt);

    /**
     * Starts using a payload type.
     *
     * This required callback initialises per-source resources for the payload
     * type, such as an elementary stream output.
     *
     * \note There may be multiple RTP sources using the same payload type
     * concurrently within single given RTP session. This callback is invoked
     * for each source.
     *
     * \param pt RTP payload type being taken into use
     * \return a data pointer for decode() and destroy() callbacks
     */
    void *(*init)(struct vlc_rtp_pt *pt);

    /**
     * Stops using a payload type.
     *
     * This optional callback deinitialises per-source resources.
     *
     * \param pt RTP payload type to relinquish
     * \param data data pointer returned by init()
     */
    void (*destroy)(struct vlc_rtp_pt *pt, void *data);

    /**
     * Processes a data payload.
     *
     * \param pt RTP payload type of the payload
     * \param data data pointer returned by init()
     * \param block payload of a received RTP packet
     * \param info RTP packet header infos
     */
    void (*decode)(struct vlc_rtp_pt *pt, void *data, block_t *block,
                   const struct vlc_rtp_pktinfo *restrict info);
};

struct vlc_rtp_pt_owner;
struct vlc_rtp_es;

/**
 * RTP payload type owner operations.
 *
 * This structures contains the callbacks provided by an RTP payload type owner
 * (\ref vlc_rtp_pt_owner).
 */
struct vlc_rtp_pt_owner_operations {
    struct vlc_rtp_es *(*request_es)(struct vlc_rtp_pt *pt,
                                     const es_format_t *restrict fmt);
    struct vlc_rtp_es *(*request_mux)(struct vlc_rtp_pt *pt, const char *name);
};

/**
 * RTP payload type owner.
 *
 * This structure embedded in \ref vlc_rtp_pt conveys the callbacks provided by
 * the owner of a payload type.
 */
struct vlc_rtp_pt_owner {
    const struct vlc_rtp_pt_owner_operations *ops; /**< Owner callbacks */
    void *data; /**< Owner private data */
};

/**
 * RTP payload type.
 *
 * This structures represents a payload format within an RTP session
 * (\ref rtp_session_t).
 */
struct vlc_rtp_pt
{
    const struct vlc_rtp_pt_operations *ops; /**< Payload format callbacks */
    void *opaque; /**< Private data pointer */
    struct vlc_rtp_pt_owner owner;
    uint32_t frequency; /**< RTP clock rate (Hz) */
    uint8_t number; /**< RTP payload type number within the session (0-127) */
    uint8_t channel_count; /**< Channel count (zero if unspecified) */
};

/**
 * Destroys a payload type parameter set.
 *
 * This function destroys a payload type.
 * It can only be called when the payload type has no active sources.
 */
void vlc_rtp_pt_release(struct vlc_rtp_pt *pt);

/**
 * Binds a payload type to a source.
 *
 * A given SDP media can have multiple alternative payload types, each with
 * their set of parameters. The RTP session can then have multiple concurrent
 * RTP sources (SSRC). This function starts an association of a given payload
 * type with an unique RTP source.
 *
 * @param pt RTP payload type to associate with a source
 * @return private data for the type-source association
 */
static inline void *vlc_rtp_pt_begin(struct vlc_rtp_pt *pt)
{
    assert(pt->ops->init != NULL);
    return pt->ops->init(pt);
}

/**
 * Unbinds a payload type from a source.
 *
 * This removes an association between a payload type and a source created by
 * vlc_rtp_pt_begin().
 *
 * @param pt RTP payload type to deassociate
 * @param data private data as returned by vlc_rtp_pt_begin()
 */
static inline void vlc_rtp_pt_end(struct vlc_rtp_pt *pt, void *data)
{
    if (pt->ops->destroy != NULL)
        pt->ops->destroy(pt, data);
}

/**
 * Processes a payload packet.
 *
 * This passes a data payload from an RTP packet for processing to the payload
 * type bound to the source of the packet. The payload type is determined from
 * the RTP header PT field and the source from the SSRC field.
 */
static inline
void vlc_rtp_pt_decode(struct vlc_rtp_pt *pt, void *data, block_t *pkt,
                       const struct vlc_rtp_pktinfo *restrict info)
{
    assert(pt->ops->decode != NULL);
    pt->ops->decode(pt, data, pkt, info);
}

/**
 * Starts an elementary stream (ES).
 *
 * This function is used by an active RTP payload type to create an
 * elementary (audio, video or subtitle) stream to process encoded data
 * extracted from an RTP source.
 *
 * A given payload type normally maintains one such elementary stream per
 * active type-source association (\ref vlc_rtp_pt_begin).
 */
static inline
struct vlc_rtp_es *vlc_rtp_pt_request_es(struct vlc_rtp_pt *pt,
                                         const es_format_t *restrict fmt)
{
    return pt->owner.ops->request_es(pt, fmt);
}

/**
 * Starts a complete multiplex.
 *
 * This function creates a complete multiplexed multimedia stream to process
 * data extracted from RTP packets. This should be avoided as much as possible
 * as RTP is designed to carry raw elementary streams.
 */
static inline
struct vlc_rtp_es *vlc_rtp_pt_request_mux(struct vlc_rtp_pt *pt,
                                          const char *name)
{
    return pt->owner.ops->request_mux(pt, name);
}

/**
 * RTP abstract output stream operations.
 *
 * This structures contains the callbacks provided by an RTP ES
 * (\ref vlc_rtp_pt).
 */
struct vlc_rtp_es_operations {
    /**
     * Destroys the corresponding \ref vlc_rtp_es.
     *
     * Use vlc_rtp_es_destroy() instead.
     */
    void (*destroy)(struct vlc_rtp_es *es);
    /**
     * Passes data for processing to a \ref vlc_rtp_es.
     *
     * Use vlc_rtp_es_send() instead.
     */
    void (*send)(struct vlc_rtp_es *es, block_t *block);
};

/**
 * RTP abstract output stream.
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
 * A (pointer to a) dummy output that discards data.
 */
extern struct vlc_rtp_es *const vlc_rtp_es_dummy;

/**
 * Callback prototype for RTP parser module.
 *
 * This is the callback prototype for any RTP payload format parser module.
 *
 * \param obj VLC object for logging and configuration
 * \param pt RTP payload type
 * \param[in] desc SDP payload format description and type mapping
 *
 * \return VLC_SUCCESS on success, an error code on failure.
 */
typedef int (*vlc_rtp_parser_cb)(vlc_object_t *obj, struct vlc_rtp_pt *pt,
                                 const struct vlc_sdp_pt *desc);

/**
 * Helper to set the RTP payload format parser module capability and callback.
 */
#define set_rtp_parser_callback(cb) \
    { \
        vlc_rtp_parser_cb cb__ = (cb); (void) cb__; \
        set_callback(cb) \
        set_capability("rtp parser", 0) \
    }

int vlc_rtp_pt_instantiate(vlc_object_t *obj, struct vlc_rtp_pt *restrict pt,
                           const struct vlc_sdp_pt *restrict desc);

void rtp_autodetect(vlc_object_t *, rtp_session_t *,
                    const struct vlc_rtp_pt_owner *restrict);

static inline uint8_t rtp_ptype (const block_t *block)
{
    return block->p_buffer[1] & 0x7F;
}

/** @} */

/**
 * \defgroup rtp_session RTP session
 * @{
 */
#define RTP_MAX_SRC_DEFAULT 1
#define RTP_MAX_DROPOUT_DEFAULT 3000
#define RTP_MAX_TIMEOUT_DEFAULT 5
#define RTP_MAX_MISORDER_DEFAULT 100

rtp_session_t *rtp_session_create (void);
rtp_session_t *rtp_session_create_custom (uint16_t max_dropout, uint16_t max_misorder,
                                          uint8_t max_src, vlc_tick_t timeout);
void rtp_session_destroy (struct vlc_logger *, rtp_session_t *);
void rtp_queue (struct vlc_logger *, rtp_session_t *, block_t *);
bool rtp_dequeue (struct vlc_logger *, const rtp_session_t *, vlc_tick_t, vlc_tick_t *);
int rtp_add_type(rtp_session_t *ses, rtp_pt_t *pt);
int vlc_rtp_add_media_types(vlc_object_t *obj, rtp_session_t *ses,
                            const struct vlc_sdp_media *media,
                            const struct vlc_rtp_pt_owner *restrict owner);

void *rtp_dgram_thread (void *data);

/** @} */
/** @} */
