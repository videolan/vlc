/*****************************************************************************
 * stream_output.h : stream output module
 *****************************************************************************
 * Copyright (C) 2002 VideoLAN
 * $Id: stream_output.h,v 1.10 2003/04/13 20:00:20 fenrir Exp $
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

/*
 * i_allocated_size: size of allocated buffer
 * p_allocated_buffer: where data has been allocated
 *
 * i_buffer_size: sizeof buffer from p_buffer
 * p_buffer: where data begins
 * i_size: size of valid data
 *
 */
#define SOUT_BUFFER_FLAGS_HEADER    0x0001
struct sout_buffer_t
{
    size_t                  i_allocated_size;
    byte_t                  *p_allocated_buffer;

    size_t                  i_buffer_size;
    byte_t                  *p_buffer;

    size_t                  i_size;
    mtime_t                 i_length;

    mtime_t                 i_dts;
    mtime_t                 i_pts;

    uint32_t                i_flags;
    int                     i_bitrate;

    struct sout_buffer_t    *p_next;
};

struct sout_format_t
{
    int             i_cat;
    vlc_fourcc_t    i_fourcc;

    /* audio */
    int             i_sample_rate;
    int             i_channels;
    int             i_block_align;

    /* video */
    int             i_width;
    int             i_height;

    int             i_bitrate;
    int             i_extra_data;
    uint8_t         *p_extra_data;

};

struct sout_fifo_t
{
    vlc_mutex_t         lock;                         /* fifo data lock */
    vlc_cond_t          wait;         /* fifo data conditional variable */

    int                 i_depth;
    sout_buffer_t       *p_first;
    sout_buffer_t       **pp_last;
};

typedef struct sout_stream_id_t  sout_stream_id_t;

/* for mux */
struct sout_input_t
{
    sout_instance_t *p_sout;

    sout_format_t   *p_fmt;
    sout_fifo_t     *p_fifo;

    void            *p_sys;
};

/* for packetizer */
struct sout_packetizer_input_t
{

    sout_instance_t     *p_sout;

    sout_format_t       *p_fmt;

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
    char                    *psz_name;
    sout_access_out_sys_t   *p_sys;
    int                     (* pf_seek  )( sout_access_out_t *,
                                           off_t );
    int                     (* pf_write )( sout_access_out_t *,
                                           sout_buffer_t * );
};
/*
 * i_query parameter of pf_mux_capacity
 */
/* SOUT_MUX_CAP_GET_ADD_STREAM_ANY_TIME:    p_args=NULL, p_answer=&boolean */
#define SOUT_MUX_CAP_GET_ADD_STREAM_ANY_TIME    0x01
/* SOUT_MUX_CAP_GET_STREAMABLE:             p_args=NULL, p_answer=&boolean */
#define SOUT_MUX_CAP_GET_STREAMABLE             0x02
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
    module_t                *p_module;

    sout_instance_t         *p_sout;

    char                    *psz_mux;

    sout_access_out_t       *p_access;

    int                     i_preheader;
    int                     (* pf_capacity)  ( sout_mux_t *,
                                               int, void *, void *);
    int                     (* pf_addstream )( sout_mux_t *,
                                               sout_input_t * );
    int                     (* pf_delstream )( sout_mux_t *,
                                               sout_input_t * );
    int                     (* pf_mux )      ( sout_mux_t * );


    /* here are all inputs accepted by muxer */
    int                     i_nb_inputs;
    sout_input_t            **pp_inputs;


    /* mux private */
    sout_mux_sys_t          *p_sys;

//    /* creater private */
//    void                    *p_sys_owner;

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

    module_t                *p_module;
    sout_instance_t         *p_sout;

    char                    *psz_name;
    sout_cfg_t              *p_cfg;
    char                    *psz_next;

    /* add, remove a stream */
    sout_stream_id_t *      (*pf_add) ( sout_stream_t *, sout_format_t * );
    int                     (*pf_del) ( sout_stream_t *, sout_stream_id_t * );

    /* manage a packet */
    int                     (*pf_send)( sout_stream_t *, sout_stream_id_t *, sout_buffer_t* );

    /* private */
    sout_stream_sys_t       *p_sys;
};

typedef struct sout_instance_sys_t sout_instance_sys_t;
struct sout_instance_t
{
    VLC_COMMON_MEMBERS

    char * psz_sout;
    char * psz_chain;

    /* muxer data */
    int                     i_preheader;    /* max over all muxer */

    vlc_mutex_t             lock;
    sout_stream_t           *p_stream;

    /* sout private */
    sout_instance_sys_t     *p_sys;
};

/* some macro */
#define TAB_APPEND( count, tab, p )             \
    if( (count) > 0 )                           \
    {                                           \
        (tab) = realloc( (tab), sizeof( void ** ) * ( (count) + 1 ) ); \
    }                                           \
    else                                        \
    {                                           \
        (tab) = malloc( sizeof( void ** ) );    \
    }                                           \
    (void**)(tab)[(count)] = (void*)(p);        \
    (count)++

#define TAB_FIND( count, tab, p, index )        \
    {                                           \
        int _i_;                                \
        (index) = -1;                           \
        for( _i_ = 0; _i_ < (count); _i_++ )    \
        {                                       \
            if((void**)(tab)[_i_]==(void*)(p))  \
            {                                   \
                (index) = _i_;                  \
                break;                          \
            }                                   \
        }                                       \
    }

#define TAB_REMOVE( count, tab, p )             \
    {                                           \
        int i_index;                            \
        TAB_FIND( count, tab, p, i_index );     \
        if( i_index >= 0 )                      \
        {                                       \
            if( count > 1 )                     \
            {                                   \
                memmove( ((void**)tab + i_index),    \
                         ((void**)tab + i_index+1),  \
                         ( (count) - i_index - 1 ) * sizeof( void* ) );\
            }                                   \
            else                                \
            {                                   \
                free( tab );                    \
                (tab) = NULL;                   \
            }                                   \
            (count)--;                          \
        }                                       \
    }

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
VLC_EXPORT( sout_instance_t *, __sout_NewInstance,  ( vlc_object_t *, char * ) );
VLC_EXPORT( void,              sout_DeleteInstance, ( sout_instance_t * ) );

VLC_EXPORT( sout_fifo_t *,   sout_FifoCreate,     ( sout_instance_t * ) );
VLC_EXPORT( void,            sout_FifoDestroy,    ( sout_instance_t *, sout_fifo_t * ) );
VLC_EXPORT( void,            sout_FifoFree,       ( sout_instance_t *,sout_fifo_t * ) );

VLC_EXPORT( void,            sout_FifoPut,        ( sout_fifo_t *, sout_buffer_t* ) );
VLC_EXPORT( sout_buffer_t *, sout_FifoGet,        ( sout_fifo_t * ) );
VLC_EXPORT( sout_buffer_t *, sout_FifoShow,       ( sout_fifo_t * ) );


#define sout_InputNew( a, b ) __sout_InputNew( VLC_OBJECT(a), b )
VLC_EXPORT( sout_packetizer_input_t *, __sout_InputNew,       ( vlc_object_t *, sout_format_t * ) );
VLC_EXPORT( int,            sout_InputDelete,      ( sout_packetizer_input_t * ) );
VLC_EXPORT( int,            sout_InputSendBuffer,  ( sout_packetizer_input_t *, sout_buffer_t* ) );

VLC_EXPORT( sout_buffer_t*, sout_BufferNew,    ( sout_instance_t *, size_t ) );
VLC_EXPORT( int,            sout_BufferRealloc,( sout_instance_t *, sout_buffer_t*, size_t ) );
VLC_EXPORT( int,            sout_BufferReallocFromPreHeader,( sout_instance_t *, sout_buffer_t*, size_t ) );
VLC_EXPORT( int,            sout_BufferDelete, ( sout_instance_t *, sout_buffer_t* ) );
VLC_EXPORT( sout_buffer_t*, sout_BufferDuplicate,(sout_instance_t *, sout_buffer_t * ) );
VLC_EXPORT( void,           sout_BufferChain,  ( sout_buffer_t **, sout_buffer_t * ) );

VLC_EXPORT( sout_access_out_t *, sout_AccessOutNew, ( sout_instance_t *, char *psz_access, char *psz_name ) );
VLC_EXPORT( void,                sout_AccessOutDelete, ( sout_access_out_t * ) );
VLC_EXPORT( int,                 sout_AccessOutSeek,   ( sout_access_out_t *, off_t ) );
VLC_EXPORT( int,                 sout_AccessOutWrite,  ( sout_access_out_t *, sout_buffer_t * ) );

VLC_EXPORT( sout_mux_t *,       sout_MuxNew,          ( sout_instance_t*, char *, sout_access_out_t * ) );
VLC_EXPORT( sout_input_t *,     sout_MuxAddStream,    ( sout_mux_t *, sout_format_t * ) );
VLC_EXPORT( void,               sout_MuxDeleteStream, ( sout_mux_t *, sout_input_t * ) );
VLC_EXPORT( void,               sout_MuxDelete,       ( sout_mux_t * ) );
VLC_EXPORT( void,               sout_MuxSendBuffer, ( sout_mux_t *, sout_input_t  *, sout_buffer_t * ) );

VLC_EXPORT( char *,             sout_cfg_parser, ( char **, sout_cfg_t **, char * ) );
VLC_EXPORT( sout_stream_t *,    sout_stream_new, ( sout_instance_t *, char *psz_chain ) );
VLC_EXPORT( void,               sout_stream_delete, ( sout_stream_t *p_stream ) );

