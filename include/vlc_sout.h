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

enum sout_stream_query_e {
    SOUT_STREAM_WANTS_SUBSTREAMS,  /* arg1=bool *, res=can fail (assume false) */
    SOUT_STREAM_ID_SPU_HIGHLIGHT,  /* arg1=void *, arg2=const vlc_spu_highlight_t *, res=can fail */
    SOUT_STREAM_IS_SYNCHRONOUS, /* arg1=bool *, can fail (assume false) */
};

struct sout_stream_operations {
    void *(*add)(sout_stream_t *, const es_format_t *);
    void (*del)(sout_stream_t *, void *);
    int (*send)(sout_stream_t *, void *, block_t *);
    int (*control)( sout_stream_t *, int, va_list );
    void (*flush)( sout_stream_t *, void *);
    void (*set_pcr)(sout_stream_t *, vlc_tick_t);
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

VLC_API void sout_StreamChainDelete(sout_stream_t *first, sout_stream_t *end);
VLC_API sout_stream_t *sout_StreamChainNew(vlc_object_t *parent,
        const char *psz_chain, sout_stream_t *p_next) VLC_USED;

VLC_API void *sout_StreamIdAdd(sout_stream_t *s, const es_format_t *fmt);
VLC_API void sout_StreamIdDel(sout_stream_t *s, void *id);
VLC_API int sout_StreamIdSend( sout_stream_t *s, void *id, block_t *b);
VLC_API void sout_StreamFlush(sout_stream_t *s, void *id);
VLC_API void sout_StreamSetPCR(sout_stream_t *s, vlc_tick_t pcr);
VLC_API int sout_StreamControlVa(sout_stream_t *s, int i_query, va_list args);

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
