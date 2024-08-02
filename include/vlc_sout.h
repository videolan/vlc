/*****************************************************************************
 * vlc_sout.h : stream output module
 *****************************************************************************
 * Copyright (C) 2002-2008 VLC authors and VideoLAN
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
 *          Laurent Aimar <fenrir@via.ecp.fr>
 *          Eric Petit <titer@videolan.org>
 *          Jean-Paul Saman <jpsaman #_at_# m2x.nl>
 *          Rémi Denis-Courmont
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef VLC_SOUT_H_
#define VLC_SOUT_H_

#include <sys/types.h>
#include <vlc_es.h>
#include <vlc_clock.h>
#include <vlc_tick.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \defgroup sout Stream output
 * \ingroup output
 * @{
 * \file
 * Stream output modules interface
 */

/**
 * \defgroup sout_access Access output
 * Raw output byte streams
 * @{
 */

/** Stream output access_output */
struct sout_access_out_t
{
    struct vlc_object_t obj;

    module_t                *p_module;
    char                    *psz_access;

    char                    *psz_path;
    void                    *p_sys;
    int                     (*pf_seek)( sout_access_out_t *, uint64_t );
    ssize_t                 (*pf_read)( sout_access_out_t *, block_t * );
    ssize_t                 (*pf_write)( sout_access_out_t *, block_t * );
    int                     (*pf_control)( sout_access_out_t *, int, va_list );

    config_chain_t          *p_cfg;
};

enum access_out_query_e
{
    ACCESS_OUT_CONTROLS_PACE, /* arg1=bool *, can fail (assume true) */
    ACCESS_OUT_CAN_SEEK, /* arg1=bool *, can fail (assume false) */
};

VLC_API sout_access_out_t * sout_AccessOutNew( vlc_object_t *, const char *psz_access, const char *psz_name ) VLC_USED;
#define sout_AccessOutNew( obj, access, name ) \
        sout_AccessOutNew( VLC_OBJECT(obj), access, name )
VLC_API void sout_AccessOutDelete( sout_access_out_t * );
VLC_API int sout_AccessOutSeek( sout_access_out_t *, uint64_t );
VLC_API ssize_t sout_AccessOutRead( sout_access_out_t *, block_t * );
VLC_API ssize_t sout_AccessOutWrite( sout_access_out_t *, block_t * );
VLC_API int sout_AccessOutControl( sout_access_out_t *, int, ... );

static inline bool sout_AccessOutCanControlPace( sout_access_out_t *p_ao )
{
    bool b;
    if( sout_AccessOutControl( p_ao, ACCESS_OUT_CONTROLS_PACE, &b ) )
        return true;
    return b;
}

/**
 * @}
 * \defgroup sout_mux Multiplexer
 * Multiplexers (file formatters)
 * @{
 */

/** Muxer structure */
struct  sout_mux_t
{
    struct vlc_object_t obj;
    module_t            *p_module;

    char                *psz_mux;
    config_chain_t          *p_cfg;

    sout_access_out_t   *p_access;

    int                 (*pf_addstream)( sout_mux_t *, sout_input_t * );
    void                (*pf_delstream)( sout_mux_t *, sout_input_t * );
    int                 (*pf_mux)      ( sout_mux_t * );
    int                 (*pf_control)  ( sout_mux_t *, int, va_list );

    /* here are all inputs accepted by muxer */
    int                 i_nb_inputs;
    sout_input_t        **pp_inputs;

    /* mux private */
    void                *p_sys;

    /* XXX private to stream_output.c */
    /* if muxer doesn't support adding stream at any time then we first wait
     *  for stream then we refuse all stream and start muxing */
    bool  b_add_stream_any_time;
    bool  b_waiting_stream;
    /* we wait 1.5 second after first stream added */
    vlc_tick_t  i_add_stream_start;
};

enum sout_mux_query_e
{
    /* capabilities */
    MUX_CAN_ADD_STREAM_WHILE_MUXING,    /* arg1= bool *,      res=cannot fail */
    /* properties */
    MUX_GET_MIME,                       /* arg1= char **            res=can fail    */
};

struct sout_input_t
{
    const es_format_t *p_fmt;
    block_fifo_t      *p_fifo;
    void              *p_sys;
    es_format_t        fmt;
};


VLC_API sout_mux_t * sout_MuxNew( sout_access_out_t *, const char * ) VLC_USED;
VLC_API sout_input_t *sout_MuxAddStream( sout_mux_t *, const es_format_t * ) VLC_USED;
VLC_API void sout_MuxDeleteStream( sout_mux_t *, sout_input_t * );
VLC_API void sout_MuxDelete( sout_mux_t * );
VLC_API int sout_MuxSendBuffer( sout_mux_t *, sout_input_t  *, block_t * );
VLC_API int sout_MuxGetStream(sout_mux_t *, unsigned, vlc_tick_t *);
VLC_API void sout_MuxFlush( sout_mux_t *, sout_input_t * );

static inline int sout_MuxControl( sout_mux_t *p_mux, int i_query, ... )
{
    va_list args;
    int     i_result;

    va_start( args, i_query );
    i_result = p_mux->pf_control( p_mux, i_query, args );
    va_end( args );
    return i_result;
}

/** @} */

/**
 * Stream output control list.
 *
 * Call the related actions with ::sout_StreamControl().
 */
enum sout_stream_query_e
{
    /**
     * Some ES such as closed captions are considered optional and shouldn't be
     * added to the stream output modules that return false for that query.
     *
     * \param bool* Closed caption support value, should be assumed false if the
     * control fails.
     *
     * Usage:
     * \code{c}
     * bool supports_substreams;
     * if (sout_StreamControl(stream, SOUT_STREAM_WANTS_SUBSTREAMS, &supports_substreams) != VLC_SUCCESS)
     *     supports_substreams = false;
     * \endcode
     */
    SOUT_STREAM_WANTS_SUBSTREAMS,

    /**
     * Signal the currently selected subtitle track that should be displayed to
     * the stream output.
     * This control should fail and do nothing if not implemented.
     *
     * \param void* Stream ID
     * \param vlc_spu_highlight_t* Selected spu data.
     *
     * Usage:
     * \code{c}
     * void *stream_id;
     * const vlc_spu_highlight_t hl_data = {... SPU infos...};
     * sout_StreamControl(stream, SOUT_STREAM_ID_SPU_HIGHLIGHT, stream_id, &hl_data);
     * \endcode
     */
    SOUT_STREAM_ID_SPU_HIGHLIGHT,

    /**
     * A synchronous stream output is a stream paced by the input clock. The
     * data will be sent at input rate if true is returned.
     *
     * \param bool* True if the stream output should be input paced. Should be
     * assumed false if the control fails.
     *
     * Usage:
     * \code{c}
     * bool is_input_paced;
     * if (sout_StreamControl(stream, SOUT_STREAM_IS_SYNCHRONOUS, &supports_substreams) != VLC_SUCCESS)
     *     supports_substreams = false;
     * \endcode
     */
    SOUT_STREAM_IS_SYNCHRONOUS,
};

typedef struct vlc_frame_t vlc_frame_t;
struct sout_stream_operations {
    /**
     * Implementation of ::sout_StreamIdAdd().
     *
     * \note Mandatory callback.
     */
    void *(*add)(sout_stream_t *, const es_format_t *, const char *);
    /**
     * Implementation of ::sout_StreamIdDel().
     *
     * \note Mandatory callback.
     */
    void (*del)(sout_stream_t *, void *);
    /**
     * Implementation of ::sout_StreamIdSend().
     *
     * \note Mandatory callback.
     */
    int (*send)(sout_stream_t *, void *, vlc_frame_t *);
    /**
     * Implementation of ::sout_StreamControl().
     *
     * \note Optional callback.
     */
    int (*control)( sout_stream_t *, int, va_list );
    /**
     * Implementation of ::sout_StreamFlush().
     *
     * \note Optional callback.
     */
    void (*flush)( sout_stream_t *, void *);
    /**
     * Implementation of ::sout_StreamSetPCR().
     *
     * \note Optional callback.
     */
    void (*set_pcr)(sout_stream_t *, vlc_tick_t);
    /**
     * \note Optional callback.
     */
    void (*close)(sout_stream_t *);
};

struct sout_stream_t
{
    struct vlc_object_t obj;

    char              *psz_name;
    config_chain_t    *p_cfg;
    sout_stream_t     *p_next;

    const struct sout_stream_operations *ops;
    void              *p_sys;
};

/**
 * Allocate an empty Stream Output object.
 *
 * The object is empty, operation callbacks should be populated manually by the
 * caller. To create a stream output associated with a module, use
 * ::sout_StreamChainNew() instead.
 *
 * \note The stream should be destroyed with ::sout_StreamChainDelete().
 *
 * \param parent The parent object of the stream output.
 * \param config A valid config chain of the object, of the form
 *               "objname{option=*,option=*,...}"
 *
 * \retval An empty allocated Stream Output object.
 * \retval NULL on allocation error.
 */
VLC_API sout_stream_t *sout_StreamNew(vlc_object_t *parent, const char *config) VLC_USED;

VLC_API void sout_StreamChainDelete(sout_stream_t *first, sout_stream_t *end);

/**
 * Creates a complete "stream_out" modules chain
 *
 * Chain format: module1{option=*:option=*}[:module2{option=*:...}]
 *
 * The modules are created starting from the last one and linked together
 *
 * \retval A pointer to the first module.
 * \retval NULL if the chain creation failed.
 */
VLC_API sout_stream_t *sout_StreamChainNew(vlc_object_t *parent,
        const char *psz_chain, sout_stream_t *p_next) VLC_USED;

/**
 * Add an ES to the stream output.
 *
 * The returned opaque identifier should be released by ::sout_StreamIdDel().
 *
 * \param s
 * \param fmt A non-NULL es-format descriptor.
 * \param es_id A non-NULL unique string describing the ES. This string is
 *              guaranteed to be valid for the whole lifetime of the ES (at
 *              least until ::sout_StreamIdDel() is called).
 *              Note that if stream output filters creates or duplicate a new
 *              ES, they are responsible for the validity and uniqueness of the
 *              string ID they pass to the next stream.
 *
 * \return An opaque pointer identifying the ES.
 * \retval NULL In case of error.
 */
VLC_API void *sout_StreamIdAdd(sout_stream_t *s,
                               const es_format_t *fmt,
                               const char *es_id) VLC_USED;

/**
 * Delete an ES from the stream output.
 *
 * \param s
 * \param id An opaque pointer identifying the ES returned by
 * ::sout_StreamIdAdd().
 */
VLC_API void sout_StreamIdDel(sout_stream_t *s, void *id);

/**
 * Pass a \ref vlc_frame_t to the stream output.
 *
 * Takes ownership of the frame, it should be considered as invalid
 * and released after this call.
 *
 * \warning Only single frames are expected through this call, for frame chains,
 * you'll have to call this for each frames.
 *
 * \param s
 * \param id The ES identifier that sent the frame.
 * \param f a frame that will be consumed (through vlc_frame_Release)
 *
 * \retval VLC_SUCCESS on success.
 * \retval VLC_EGENERIC on non-recoverable unspecific error cases.
 * \retval (-ERRNO) A negated errno value describing the error case.
 */
VLC_API int sout_StreamIdSend(sout_stream_t *s, void *id, vlc_frame_t *f);

/**
 * Signal a flush of an ES to the stream output.
 *
 * Flush is an optional control, if implemented, it will drop all the bufferized
 * data from ES and/or forward the Flush command to the next stream.
 *
 * \param s
 * \param id An identifier of the ES to flush.
 */
VLC_API void sout_StreamFlush(sout_stream_t *s, void *id);

/**
 * Signal a PCR update to the stream output.
 *
 * The PCR (Program Clock Reference from the MPEG-TS spec.) gives a global
 * stream advancement timestamp.
 * The demuxer is required to:
 *   - Yield a PCR value at fix and frequent interval. Even if no ES are added
 *     to the stream output.
 *   - Send frames that have timestamp values greater than the last PCR value.
 *
 * \note PCR resets in case of handled discontinuity are implied by a frame
 * marked by \ref VLC_FRAME_FLAG_DISCONTINUITY and/or by a ::sout_StreamFlush()
 * call.
 */
VLC_API void sout_StreamSetPCR(sout_stream_t *, vlc_tick_t pcr);

struct vlc_sout_clock_bus;
VLC_API struct vlc_sout_clock_bus *sout_ClockMainCreate(sout_stream_t *) VLC_USED;
VLC_API void sout_ClockMainDelete(struct vlc_sout_clock_bus *);
VLC_API void sout_ClockMainSetFirstPcr(struct vlc_sout_clock_bus *, vlc_tick_t pcr);
VLC_API vlc_clock_t *sout_ClockCreate(struct vlc_sout_clock_bus *, const es_format_t *) VLC_USED;
VLC_API void sout_ClockDelete(vlc_clock_t *);


VLC_API int sout_StreamControlVa(sout_stream_t *, int i_query, va_list args);

/**
 * Various controls forwarded through the stream output chain.
 *
 * Controls are various misc accessors or set of actions that can be used to
 * query the stream output.
 * See \ref sout_stream_query_e for the list of availables controls.
 */
static inline int sout_StreamControl( sout_stream_t *s, int i_query, ... )
{
    va_list args;
    int     i_result;

    va_start( args, i_query );
    i_result = sout_StreamControlVa( s, i_query, args );
    va_end( args );
    return i_result;
}

static inline bool sout_StreamIsSynchronous(sout_stream_t *s)
{
    bool b;

    if (sout_StreamControl(s, SOUT_STREAM_IS_SYNCHRONOUS, &b))
        b = false;

    return b;
}

/****************************************************************************
 * Encoder
 ****************************************************************************/

VLC_API encoder_t * sout_EncoderCreate( vlc_object_t *, size_t );
#define sout_EncoderCreate(o,s) sout_EncoderCreate(VLC_OBJECT(o),s)

/****************************************************************************
 * Announce handler
 ****************************************************************************/
VLC_API session_descriptor_t* sout_AnnounceRegisterSDP( vlc_object_t *, const char *, const char * ) VLC_USED;
VLC_API void sout_AnnounceUnRegister(vlc_object_t *,session_descriptor_t* );
#define sout_AnnounceRegisterSDP(o, sdp, addr) \
        sout_AnnounceRegisterSDP(VLC_OBJECT (o), sdp, addr)
#define sout_AnnounceUnRegister(o, a) \
        sout_AnnounceUnRegister(VLC_OBJECT (o), a)

/** @} */

#ifdef __cplusplus
}
#endif

#endif
