/*****************************************************************************
 * vlc_sout.h : stream output module
 *****************************************************************************
 * Copyright (C) 2002-2008 VLC authors and VideoLAN
 * $Id$
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

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/types.h>
#include <vlc_es.h>

/**
 * \defgroup sout Stream output
 * \ingroup output
 * @{
 * \file
 * Stream output modules interface
 */

/** Stream output instance (FIXME: should be private to src/ to avoid
 * invalid unsynchronized access) */
struct sout_instance_t
{
    VLC_COMMON_MEMBERS

    char *psz_sout;

    /** count of output that can't control the space */
    int                 i_out_pace_nocontrol;

    vlc_mutex_t         lock;
    sout_stream_t       *p_stream;
};

/****************************************************************************
 * sout_stream_id_sys_t: opaque (private for all sout_stream_t)
 ****************************************************************************/
typedef struct sout_stream_id_sys_t  sout_stream_id_sys_t;

/**
 * \defgroup sout_access Access output
 * Raw output byte streams
 * @{
 */

/** Stream output access_output */
struct sout_access_out_t
{
    VLC_COMMON_MEMBERS

    module_t                *p_module;
    char                    *psz_access;

    char                    *psz_path;
    sout_access_out_sys_t   *p_sys;
    int                     (*pf_seek)( sout_access_out_t *, off_t );
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
VLC_API int sout_AccessOutSeek( sout_access_out_t *, off_t );
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
    VLC_COMMON_MEMBERS
    module_t            *p_module;

    sout_instance_t     *p_sout;

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
    sout_mux_sys_t      *p_sys;

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
    MUX_GET_ADD_STREAM_WAIT,            /* arg1= bool *,      res=cannot fail */
    MUX_GET_MIME,                       /* arg1= char **            res=can fail    */
};

struct sout_input_t
{
    const es_format_t *p_fmt;
    block_fifo_t      *p_fifo;
    void              *p_sys;
    es_format_t        fmt;
};


VLC_API sout_mux_t * sout_MuxNew( sout_instance_t*, const char *, sout_access_out_t * ) VLC_USED;
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

enum sout_stream_query_e {
    SOUT_STREAM_EMPTY,    /* arg1=bool *,       res=can fail (assume true) */
};

struct sout_stream_t
{
    VLC_COMMON_MEMBERS

    module_t          *p_module;
    sout_instance_t   *p_sout;

    char              *psz_name;
    config_chain_t    *p_cfg;
    sout_stream_t     *p_next;

    /* add, remove a stream */
    sout_stream_id_sys_t *(*pf_add)( sout_stream_t *, const es_format_t * );
    void              (*pf_del)( sout_stream_t *, sout_stream_id_sys_t * );
    /* manage a packet */
    int               (*pf_send)( sout_stream_t *, sout_stream_id_sys_t *, block_t* );
    int               (*pf_control)( sout_stream_t *, int, va_list );
    void              (*pf_flush)( sout_stream_t *, sout_stream_id_sys_t * );

    sout_stream_sys_t *p_sys;
    bool pace_nocontrol;
};

VLC_API void sout_StreamChainDelete(sout_stream_t *p_first, sout_stream_t *p_last );
VLC_API sout_stream_t *sout_StreamChainNew(sout_instance_t *p_sout,
        const char *psz_chain, sout_stream_t *p_next, sout_stream_t **p_last) VLC_USED;

static inline sout_stream_id_sys_t *sout_StreamIdAdd( sout_stream_t *s,
                                                      const es_format_t *fmt )
{
    return s->pf_add( s, fmt );
}

static inline void sout_StreamIdDel( sout_stream_t *s,
                                     sout_stream_id_sys_t *id )
{
    s->pf_del( s, id );
}

static inline int sout_StreamIdSend( sout_stream_t *s,
                                     sout_stream_id_sys_t *id, block_t *b )
{
    return s->pf_send( s, id, b );
}

static inline void sout_StreamFlush( sout_stream_t *s,
                                     sout_stream_id_sys_t *id )
{
    if (s->pf_flush)
        s->pf_flush( s, id );
}

static inline int sout_StreamControl( sout_stream_t *s, int i_query, ... )
{
    va_list args;
    int     i_result;

    va_start( args, i_query );
    if ( !s->pf_control )
        i_result = VLC_EGENERIC;
    else
        i_result = s->pf_control( s, i_query, args );
    va_end( args );
    return i_result;
}

/****************************************************************************
 * Encoder
 ****************************************************************************/

VLC_API encoder_t * sout_EncoderCreate( vlc_object_t *obj );
#define sout_EncoderCreate(o) sout_EncoderCreate(VLC_OBJECT(o))

/****************************************************************************
 * Announce handler
 ****************************************************************************/
VLC_API session_descriptor_t* sout_AnnounceRegisterSDP( vlc_object_t *, const char *, const char * ) VLC_USED;
VLC_API void sout_AnnounceUnRegister(vlc_object_t *,session_descriptor_t* );
#define sout_AnnounceRegisterSDP(o, sdp, addr) \
        sout_AnnounceRegisterSDP(VLC_OBJECT (o), sdp, addr)
#define sout_AnnounceUnRegister(o, a) \
        sout_AnnounceUnRegister(VLC_OBJECT (o), a)

/** SDP */

struct sockaddr;
struct vlc_memstream;

VLC_API int vlc_sdp_Start(struct vlc_memstream *, vlc_object_t *obj,
                          const char *cfgpref,
                          const struct sockaddr *src, size_t slen,
                          const struct sockaddr *addr, size_t alen) VLC_USED;
VLC_API void sdp_AddMedia(struct vlc_memstream *, const char *type,
                          const char *protocol, int dport, unsigned pt,
                          bool bw_indep, unsigned bw, const char *ptname,
                          unsigned clockrate, unsigned channels,
                          const char *fmtp);
VLC_API void sdp_AddAttribute(struct vlc_memstream *, const char *name,
                              const char *fmt, ...) VLC_FORMAT(3, 4);

/** Description module */
typedef struct sout_description_data_t
{
    int i_es;
    es_format_t **es;
    vlc_sem_t *sem;
} sout_description_data_t;

/** @} */

#ifdef __cplusplus
}
#endif

#endif
