/*****************************************************************************
 * stream_output.h : stream output module
 *****************************************************************************
 * Copyright (C) 2002 VideoLAN
 * $Id: stream_output.h,v 1.6 2003/02/24 10:45:55 fenrir Exp $
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

    int                     i_bitrate;

    struct sout_buffer_t    *p_next;
};

struct sout_packet_format_t
{
    int             i_cat;      // AUDIO_ES, VIDEO_ES, SPU_ES
    vlc_fourcc_t    i_fourcc;

    void            *p_format;  // WAVEFORMATEX or BITMAPINFOHEADER

};

struct sout_fifo_t
{
    vlc_mutex_t         lock;                         /* fifo data lock */
    vlc_cond_t          wait;         /* fifo data conditional variable */

    int                 i_depth;
    sout_buffer_t       *p_first;
    sout_buffer_t       **pp_last;
};

struct sout_input_t
{
    vlc_mutex_t             lock;

    sout_instance_t         *p_sout;

    sout_packet_format_t    input_format;
    sout_fifo_t             *p_fifo;

    void                    *p_mux_data;
};

#define SOUT_METHOD_NONE        0x00
#define SOUT_METHOD_FILE        0x10
#define SOUT_METHOD_NETWORK     0x20


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

struct sout_instance_t
{
    VLC_COMMON_MEMBERS

    char * psz_dest;
    char * psz_access;
    char * psz_mux;
    char * psz_name;

    int                     i_method;

    sout_access_out_t       *p_access;

    module_t                *p_mux;
    void                    *p_mux_data;
    int                     i_mux_preheader;
    int                     (* pf_mux_capacity)  ( sout_instance_t *,
                                                   int, void *, void *);
    int                     (* pf_mux_addstream )( sout_instance_t *,
                                                   sout_input_t * );
    int                     (* pf_mux_delstream )( sout_instance_t *,
                                                   sout_input_t * );
    int                     (* pf_mux )          ( sout_instance_t * );


    vlc_mutex_t             lock;

    int                     i_nb_inputs;
    sout_input_t            **pp_inputs;
};




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
VLC_EXPORT( sout_input_t *, __sout_InputNew,       ( vlc_object_t *, sout_packet_format_t * ) );
VLC_EXPORT( int,            sout_InputDelete,      ( sout_input_t * ) );
VLC_EXPORT( int,            sout_InputSendBuffer,  ( sout_input_t *, sout_buffer_t* ) );

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

