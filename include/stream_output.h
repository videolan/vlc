/*****************************************************************************
 * stream_output.h : stream output module
 *****************************************************************************
 * Copyright (C) 2002 VideoLAN
 * $Id$
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
 *          Laurent Aimar <fenrir@via.ecp.fr>
 *          Eric Petit <titer@videolan.org>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

/*****************************************************************************
 * sout_instance_t: stream output thread descriptor
 *****************************************************************************/

#include "vlc_es.h"


typedef struct sout_stream_id_t  sout_stream_id_t;

/* for mux */
struct sout_input_t
{
    sout_instance_t *p_sout;

    es_format_t     *p_fmt;
    block_fifo_t    *p_fifo;

    void            *p_sys;
};

/* for packetizer */
struct sout_packetizer_input_t
{
    sout_instance_t     *p_sout;

    es_format_t         *p_fmt;

    sout_stream_id_t    *id;
};


#define SOUT_METHOD_NONE        0x00
#define SOUT_METHOD_FILE        0x10
#define SOUT_METHOD_NETWORK     0x20

typedef struct sout_access_out_sys_t   sout_access_out_sys_t;
struct sout_access_out_t
{
    VLC_COMMON_MEMBERS

    module_t                *p_module;

    sout_instance_t         *p_sout;

    char                    *psz_access;
    sout_cfg_t              *p_cfg;

    char                    *psz_name;
    sout_access_out_sys_t   *p_sys;
    int                     (*pf_seek)( sout_access_out_t *, off_t );
    int                     (*pf_read)( sout_access_out_t *, block_t * );
    int                     (*pf_write)( sout_access_out_t *, block_t * );
};

/*
 * i_query parameter of pf_mux_capacity
 */
/* SOUT_MUX_CAP_GET_ADD_STREAM_ANY_TIME:    p_args=NULL, p_answer=&boolean */
#define SOUT_MUX_CAP_GET_ADD_STREAM_ANY_TIME    0x01
/* SOUT_MUX_CAP_GET_STREAMABLE:             p_args=NULL, p_answer=&boolean */
#define SOUT_MUX_CAP_GET_STREAMABLE             0x02
/* SOUT_MUX_CAP_GET_ADD_STREAM_WAIT:        p_args=NULL, p_answer=&boolean */
#define SOUT_MUX_CAP_GET_ADD_STREAM_WAIT        0x03

/*
 * return error code
 */
#define SOUT_MUX_CAP_ERR_OK                 0x00
#define SOUT_MUX_CAP_ERR_UNKNOWN            0x01
#define SOUT_MUX_CAP_ERR_UNIMPLEMENTED      0x02

typedef struct sout_mux_sys_t sout_mux_sys_t;
struct  sout_mux_t
{
    VLC_COMMON_MEMBERS
    module_t            *p_module;

    sout_instance_t     *p_sout;

    char                *psz_mux;
    sout_cfg_t          *p_cfg;

    sout_access_out_t   *p_access;

    int                 (*pf_capacity)( sout_mux_t *, int, void *, void *);
    int                 (*pf_addstream)( sout_mux_t *, sout_input_t * );
    int                 (*pf_delstream)( sout_mux_t *, sout_input_t * );
    int                 (*pf_mux)      ( sout_mux_t * );

    /* here are all inputs accepted by muxer */
    int                 i_nb_inputs;
    sout_input_t        **pp_inputs;


    /* mux private */
    sout_mux_sys_t      *p_sys;

    /* XXX private to stream_output.c */
    /* if muxer doesn't support adding stream at any time then we first wait
     *  for stream then we refuse all stream and start muxing */
    vlc_bool_t  b_add_stream_any_time;
    vlc_bool_t  b_waiting_stream;
    /* we wait one second after first stream added */
    mtime_t     i_add_stream_start;
};



struct sout_cfg_t
{
    sout_cfg_t  *p_next;

    char        *psz_name;
    char        *psz_value;
};

typedef struct sout_stream_sys_t sout_stream_sys_t;
struct sout_stream_t
{
    VLC_COMMON_MEMBERS

    module_t          *p_module;
    sout_instance_t   *p_sout;

    char              *psz_name;
    sout_cfg_t        *p_cfg;
    char              *psz_next;

    /* add, remove a stream */
    sout_stream_id_t *(*pf_add)( sout_stream_t *, es_format_t * );
    int               (*pf_del)( sout_stream_t *, sout_stream_id_t * );
    /* manage a packet */
    int               (*pf_send)( sout_stream_t *, sout_stream_id_t *, block_t* );

    /* private */
    sout_stream_sys_t *p_sys;
};

typedef struct sout_instance_sys_t sout_instance_sys_t;
struct sout_instance_t
{
    VLC_COMMON_MEMBERS

    char *psz_sout;
    char *psz_chain;

    /* meta data (Read only) XXX it won't be set before the first packet received */
    vlc_meta_t          *p_meta;

    int                 i_out_pace_nocontrol;   /* count of output that can't control the space */

    vlc_mutex_t         lock;
    sout_stream_t       *p_stream;

    /* sout private */
    sout_instance_sys_t *p_sys;
};

static inline sout_cfg_t *sout_cfg_find( sout_cfg_t *p_cfg, char *psz_name )
{
    while( p_cfg && strcmp( p_cfg->psz_name, psz_name ) )
    {
        p_cfg = p_cfg->p_next;
    }

    return p_cfg;
}

static inline char *sout_cfg_find_value( sout_cfg_t *p_cfg, char *psz_name )
{
    while( p_cfg && strcmp( p_cfg->psz_name, psz_name ) )
    {
        p_cfg = p_cfg->p_next;
    }

    if( p_cfg && p_cfg->psz_value )
    {
        return( p_cfg->psz_value );
    }

    return NULL;
}
/*****************************************************************************
 * Prototypes
 *****************************************************************************/
#define sout_NewInstance(a,b) __sout_NewInstance(VLC_OBJECT(a),b)
VLC_EXPORT( sout_instance_t *,  __sout_NewInstance,  ( vlc_object_t *, char * ) );
VLC_EXPORT( void,               sout_DeleteInstance, ( sout_instance_t * ) );

VLC_EXPORT( sout_packetizer_input_t *, sout_InputNew,( sout_instance_t *, es_format_t * ) );
VLC_EXPORT( int,                sout_InputDelete,      ( sout_packetizer_input_t * ) );
VLC_EXPORT( int,                sout_InputSendBuffer,  ( sout_packetizer_input_t *, block_t* ) );

VLC_EXPORT( sout_access_out_t *,sout_AccessOutNew, ( sout_instance_t *, char *psz_access, char *psz_name ) );
VLC_EXPORT( void,               sout_AccessOutDelete, ( sout_access_out_t * ) );
VLC_EXPORT( int,                sout_AccessOutSeek,   ( sout_access_out_t *, off_t ) );
VLC_EXPORT( int,                sout_AccessOutRead,   ( sout_access_out_t *, block_t * ) );
VLC_EXPORT( int,                sout_AccessOutWrite,  ( sout_access_out_t *, block_t * ) );

VLC_EXPORT( sout_mux_t *,       sout_MuxNew,          ( sout_instance_t*, char *, sout_access_out_t * ) );
VLC_EXPORT( sout_input_t *,     sout_MuxAddStream,    ( sout_mux_t *, es_format_t * ) );
VLC_EXPORT( void,               sout_MuxDeleteStream, ( sout_mux_t *, sout_input_t * ) );
VLC_EXPORT( void,               sout_MuxDelete,       ( sout_mux_t * ) );
VLC_EXPORT( void,               sout_MuxSendBuffer, ( sout_mux_t *, sout_input_t  *, block_t * ) );

VLC_EXPORT( char *,             sout_cfg_parser, ( char **, sout_cfg_t **, char * ) );
VLC_EXPORT( sout_stream_t *,    sout_stream_new, ( sout_instance_t *, char *psz_chain ) );
VLC_EXPORT( void,               sout_stream_delete, ( sout_stream_t *p_stream ) );

