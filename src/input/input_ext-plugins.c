/*****************************************************************************
 * input_ext-plugins.c: useful functions for access and demux plug-ins
 *****************************************************************************
 * Copyright (C) 2001, 2002 VideoLAN
 * $Id: input_ext-plugins.c,v 1.10 2002/05/21 00:23:37 sam Exp $
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
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
 * Preamble
 *****************************************************************************/
#include <stdlib.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

#include <videolan/vlc.h>

#ifdef HAVE_SYS_TIME_H
#    include <sys/time.h>
#endif

#ifdef HAVE_UNISTD_H
#   include <unistd.h>
#elif defined( _MSC_VER ) && defined( _WIN32 )
#   include <io.h>
#endif

#ifdef WIN32
#   include <winsock2.h>
#   include <ws2tcpip.h>
#elif !defined( SYS_BEOS ) && !defined( SYS_NTO )
#   include <netdb.h>                                         /* hostent ... */
#   include <sys/socket.h>
#   include <netinet/in.h>
#   ifdef HAVE_ARPA_INET_H
#       include <arpa/inet.h>                    /* inet_ntoa(), inet_aton() */
#   endif
#endif

#ifdef WIN32
#   include <winsock2.h>
#   include <ws2tcpip.h>
#elif !defined( SYS_BEOS ) && !defined( SYS_NTO )
#   include <netdb.h>                                         /* hostent ... */
#   include <sys/socket.h>
#   include <netinet/in.h>
#   ifdef HAVE_ARPA_INET_H
#       include <arpa/inet.h>                    /* inet_ntoa(), inet_aton() */
#   endif
#endif



#include "stream_control.h"
#include "input_ext-intf.h"
#include "input_ext-dec.h"
#include "input_ext-plugins.h"


/*
 * Buffers management : internal functions
 *
 * All functions are static, but exported versions with mutex protection
 * start with input_*. Not all of these exported functions are actually used,
 * but they are included here for completeness.
 */

#define BUFFERS_CACHE_SIZE 500
#define DATA_CACHE_SIZE 1000
#define PES_CACHE_SIZE 1000

/*****************************************************************************
 * data_buffer_t: shared data type
 *****************************************************************************/
typedef struct data_buffer_s
{
    struct data_buffer_s * p_next;

    /* number of data packets this buffer is referenced from - when it falls
     * down to 0, the buffer is freed */
    int i_refcount;

    /* size of the current buffer (starting right after this byte) */
    size_t i_size;
} data_buffer_t;

/*****************************************************************************
 * input_buffers_t: defines a LIFO per data type to keep
 *****************************************************************************/
#define PACKETS_LIFO( TYPE, NAME )                                          \
struct                                                                      \
{                                                                           \
    TYPE * p_stack;                                                         \
    unsigned int i_depth;                                                   \
} NAME;

typedef struct input_buffers_s
{
    vlc_mutex_t lock;
    PACKETS_LIFO( pes_packet_t, pes )
    PACKETS_LIFO( data_packet_t, data )
    PACKETS_LIFO( data_buffer_t, buffers )
    size_t i_allocated;
} input_buffers_t;


/*****************************************************************************
 * input_BuffersInit: initialize the cache structures, return a pointer to it
 *****************************************************************************/
void * input_BuffersInit( void )
{
    input_buffers_t * p_buffers = malloc( sizeof( input_buffers_t ) );

    if( p_buffers == NULL )
    {
        return( NULL );
    }

    memset( p_buffers, 0, sizeof( input_buffers_t ) );
    vlc_mutex_init( &p_buffers->lock );

    return( p_buffers );
}

/*****************************************************************************
 * input_BuffersEnd: free all cached structures
 *****************************************************************************/
#define BUFFERS_END_PACKETS_LOOP                                            \
    while( p_packet != NULL )                                               \
    {                                                                       \
        p_next = p_packet->p_next;                                          \
        free( p_packet );                                                   \
        p_packet = p_next;                                                  \
    }

void input_BuffersEnd( input_buffers_t * p_buffers )
{
    if( p_buffers != NULL )
    {
        if( p_main->b_stats )
        {
            intf_StatMsg( "input buffers stats: pes: %d packets",
                          p_buffers->pes.i_depth );
            intf_StatMsg( "input buffers stats: data: %d packets",
                          p_buffers->data.i_depth );
            intf_StatMsg( "input buffers stats: buffers: %d packets",
                          p_buffers->buffers.i_depth );
        }

        {
            /* Free PES */
            pes_packet_t * p_next, * p_packet = p_buffers->pes.p_stack;
            BUFFERS_END_PACKETS_LOOP;
        }

        {
            /* Free data packets */
            data_packet_t * p_next, * p_packet = p_buffers->data.p_stack;
            BUFFERS_END_PACKETS_LOOP;
        }

        {
            /* Free buffers */
            data_buffer_t * p_next, * p_buf = p_buffers->buffers.p_stack;
            while( p_buf != NULL )
            {
                p_next = p_buf->p_next;
                p_buffers->i_allocated -= p_buf->i_size;
                free( p_buf );
                p_buf = p_next;
            }
        } 

        if( p_buffers->i_allocated )
        {
            intf_ErrMsg( "input buffers error: %d bytes have not been"
                         " freed, expect memory leak",
                         p_buffers->i_allocated );
        }

        vlc_mutex_destroy( &p_buffers->lock );
        free( p_buffers );
    }
}

/*****************************************************************************
 * input_NewBuffer: return a pointer to a data buffer of the appropriate size
 *****************************************************************************/
static inline data_buffer_t * NewBuffer( input_buffers_t * p_buffers,
                                         size_t i_size )
{
    data_buffer_t * p_buf;

    /* Safety check */
    if( p_buffers->i_allocated > INPUT_MAX_ALLOCATION )
    {
        intf_ErrMsg( "INPUT_MAX_ALLOCATION reached (%d)",
                     p_buffers->i_allocated );
        return NULL;
    } 

    if( p_buffers->buffers.p_stack != NULL )
    {
        /* Take the buffer from the cache */
        p_buf = p_buffers->buffers.p_stack;
        p_buffers->buffers.p_stack = p_buf->p_next;
        p_buffers->buffers.i_depth--;

        /* Reallocate the packet if it is too small or too large */
        if( p_buf->i_size < i_size || p_buf->i_size > 3 * i_size )
        {
            p_buffers->i_allocated -= p_buf->i_size;
            free( p_buf );
            p_buf = malloc( sizeof(input_buffers_t) + i_size );
            if( p_buf == NULL )
            {
                intf_ErrMsg( "Out of memory" );
                return NULL;
            }
            p_buf->i_size = i_size;
            p_buffers->i_allocated += i_size;
        }
    }
    else
    {
        /* Allocate a new buffer */
        p_buf = malloc( sizeof(input_buffers_t) + i_size );
        if( p_buf == NULL )
        {
            intf_ErrMsg( "Out of memory" );
            return NULL;
        }
        p_buf->i_size = i_size;
        p_buffers->i_allocated += i_size;
    }

    /* Initialize data */
    p_buf->p_next = NULL;
    p_buf->i_refcount = 0;

    return( p_buf );
}

data_buffer_t * input_NewBuffer( input_buffers_t * p_buffers, size_t i_size )
{
    data_buffer_t * p_buf;

    vlc_mutex_lock( &p_buffers->lock );
    p_buf = NewBuffer( p_buffers, i_size );
    vlc_mutex_unlock( &p_buffers->lock );

    return( p_buf );
}

/*****************************************************************************
 * input_ReleaseBuffer: put a buffer back into the cache
 *****************************************************************************/
static inline void ReleaseBuffer( input_buffers_t * p_buffers,
                                  data_buffer_t * p_buf )
{
    /* Decrement refcount */
    if( --p_buf->i_refcount > 0 )
    {
        return;
    }

    if( p_buffers->buffers.i_depth < BUFFERS_CACHE_SIZE )
    {
        /* Cache not full : store the buffer in it */
        p_buf->p_next = p_buffers->buffers.p_stack;
        p_buffers->buffers.p_stack = p_buf;
        p_buffers->buffers.i_depth++;
    }
    else
    {
        p_buffers->i_allocated -= p_buf->i_size;
        free( p_buf );
    }
}

void input_ReleaseBuffer( input_buffers_t * p_buffers, data_buffer_t * p_buf )
{
    vlc_mutex_lock( &p_buffers->lock );
    ReleaseBuffer( p_buffers, p_buf );
    vlc_mutex_unlock( &p_buffers->lock );
}

/*****************************************************************************
 * input_ShareBuffer: allocate a data_packet_t pointing to a given buffer
 *****************************************************************************/
static inline data_packet_t * ShareBuffer( input_buffers_t * p_buffers,
                                           data_buffer_t * p_buf )
{
    data_packet_t * p_data;

    if( p_buffers->data.p_stack != NULL )
    {
        /* Take the packet from the cache */
        p_data = p_buffers->data.p_stack;
        p_buffers->data.p_stack = p_data->p_next;
        p_buffers->data.i_depth--;
    }
    else
    {
        /* Allocate a new packet */
        p_data = malloc( sizeof(data_packet_t) );
        if( p_data == NULL )
        {
            intf_ErrMsg( "Out of memory" );
            return NULL;
        }
    }

    p_data->p_buffer = p_buf;
    p_data->p_next = NULL;
    p_data->b_discard_payload = 0;
    p_data->p_payload_start = p_data->p_demux_start
                            = (byte_t *)p_buf + sizeof(input_buffers_t);
    p_data->p_payload_end = p_data->p_demux_start + p_buf->i_size;
    p_buf->i_refcount++;

    return( p_data );
}

data_packet_t * input_ShareBuffer( input_buffers_t * p_buffers,
                                   data_buffer_t * p_buf )
{
    data_packet_t * p_data;

    vlc_mutex_lock( &p_buffers->lock );
    p_data = ShareBuffer( p_buffers, p_buf );
    vlc_mutex_unlock( &p_buffers->lock );

    return( p_data );
}

/*****************************************************************************
 * input_NewPacket: allocate a packet along with a buffer
 *****************************************************************************/
static inline data_packet_t * NewPacket( input_buffers_t * p_buffers,
                                         size_t i_size )
{
    data_buffer_t * p_buf = NewBuffer( p_buffers, i_size );
    data_packet_t * p_data;

    if( p_buf == NULL )
    {
        return( NULL );
    }

    p_data = ShareBuffer( p_buffers, p_buf );
    if( p_data == NULL )
    {
        ReleaseBuffer( p_buffers, p_buf );
    }
    return( p_data );
}

data_packet_t * input_NewPacket( input_buffers_t * p_buffers, size_t i_size )
{
    data_packet_t * p_data;

    vlc_mutex_lock( &p_buffers->lock );
    p_data = NewPacket( p_buffers, i_size );
    vlc_mutex_unlock( &p_buffers->lock );

    return( p_data );
}

/*****************************************************************************
 * input_DeletePacket: deallocate a packet and its buffers
 *****************************************************************************/
static inline void DeletePacket( input_buffers_t * p_buffers,
                                 data_packet_t * p_data )
{
    while( p_data != NULL )
    {
        data_packet_t * p_next = p_data->p_next;

        ReleaseBuffer( p_buffers, p_data->p_buffer );

        if( p_buffers->data.i_depth < DATA_CACHE_SIZE )
        {
            /* Cache not full : store the packet in it */
            p_data->p_next = p_buffers->data.p_stack;
            p_buffers->data.p_stack = p_data;
            p_buffers->data.i_depth++;
        }
        else
        {
            free( p_data );
        }

        p_data = p_next;
    }
}

void input_DeletePacket( input_buffers_t * p_buffers, data_packet_t * p_data )
{
    vlc_mutex_lock( &p_buffers->lock );
    DeletePacket( p_buffers, p_data );
    vlc_mutex_unlock( &p_buffers->lock );
}

/*****************************************************************************
 * input_NewPES: return a pointer to a new PES packet
 *****************************************************************************/
static inline pes_packet_t * NewPES( input_buffers_t * p_buffers )
{
    pes_packet_t * p_pes;

    if( p_buffers->pes.p_stack != NULL )
    {
        /* Take the packet from the cache */
        p_pes = p_buffers->pes.p_stack;
        p_buffers->pes.p_stack = p_pes->p_next;
        p_buffers->pes.i_depth--;
    }
    else
    {
        /* Allocate a new packet */
        p_pes = malloc( sizeof(pes_packet_t) );
        if( p_pes == NULL )
        {
            intf_ErrMsg( "Out of memory" );
            return NULL;
        }
    }

    p_pes->p_next = NULL;
    p_pes->b_data_alignment = p_pes->b_discontinuity =
        p_pes->i_pts = p_pes->i_dts = 0;
    p_pes->p_first = p_pes->p_last = NULL;
    p_pes->i_pes_size = 0;
    p_pes->i_nb_data = 0;

    return( p_pes );
}

pes_packet_t * input_NewPES( input_buffers_t * p_buffers )
{
    pes_packet_t * p_pes;

    vlc_mutex_lock( &p_buffers->lock );
    p_pes = NewPES( p_buffers );
    vlc_mutex_unlock( &p_buffers->lock );

    return( p_pes );
}

/*****************************************************************************
 * input_DeletePES: put a pes and all data packets and all buffers back into
 *                  the cache
 *****************************************************************************/
static inline void DeletePES( input_buffers_t * p_buffers,
                              pes_packet_t * p_pes )
{
    while( p_pes != NULL )
    {
        pes_packet_t * p_next = p_pes->p_next;

        /* Delete all data packets */
        if( p_pes->p_first != NULL )
        {
            DeletePacket( p_buffers, p_pes->p_first );
        }

        if( p_buffers->pes.i_depth < PES_CACHE_SIZE )
        {
            /* Cache not full : store the packet in it */
            p_pes->p_next = p_buffers->pes.p_stack;
            p_buffers->pes.p_stack = p_pes;
            p_buffers->pes.i_depth++;
        }
        else
        {
            free( p_pes );
        }

        p_pes = p_next;
    }
}

void input_DeletePES( input_buffers_t * p_buffers, pes_packet_t * p_pes )
{
    vlc_mutex_lock( &p_buffers->lock );
    DeletePES( p_buffers, p_pes );
    vlc_mutex_unlock( &p_buffers->lock );
}


/*
 * Buffers management : external functions
 *
 * These functions make the glu between the access plug-in (pf_read) and
 * the demux plug-in (pf_demux). We fill in a large buffer (approx. 10s kB)
 * with a call to pf_read, then allow the demux plug-in to have a peep at
 * it (input_Peek), and to split it in data_packet_t (input_SplitBuffer).
 */
/*****************************************************************************
 * input_FillBuffer: fill in p_data_buffer with data from pf_read
 *****************************************************************************/
ssize_t input_FillBuffer( input_thread_t * p_input )
{
    ptrdiff_t i_remains = p_input->p_last_data - p_input->p_current_data;
    data_buffer_t * p_buf;
    ssize_t i_ret;

    vlc_mutex_lock( &p_input->p_method_data->lock );

    p_buf = NewBuffer( p_input->p_method_data,
                       i_remains + p_input->i_bufsize );
    if( p_buf == NULL )
    {
        return( -1 );
    }
    p_buf->i_refcount = 1;

    if( p_input->p_data_buffer != NULL )
    {
        if( i_remains )
        {
            FAST_MEMCPY( (byte_t *)p_buf + sizeof(data_buffer_t),
                         p_input->p_current_data, (size_t)i_remains );
        }
        ReleaseBuffer( p_input->p_method_data, p_input->p_data_buffer );
    }

    /* Do not hold the lock during pf_read (blocking call). */
    vlc_mutex_unlock( &p_input->p_method_data->lock );

    i_ret = p_input->pf_read( p_input,
                             (byte_t *)p_buf + sizeof(data_buffer_t)
                              + i_remains,
                             p_input->i_bufsize );

    if( i_ret < 0 ) i_ret = 0;
    p_input->p_data_buffer = p_buf;
    p_input->p_current_data = (byte_t *)p_buf + sizeof(data_buffer_t);
    p_input->p_last_data = p_input->p_current_data + i_remains + i_ret;

    return( (ssize_t)i_remains + i_ret );
}

/*****************************************************************************
 * input_Peek: give a pointer to the next available bytes in the buffer
 *             (min. i_size bytes)
 * Returns the number of bytes read, or -1 in case of error
 *****************************************************************************/
ssize_t input_Peek( input_thread_t * p_input, byte_t ** pp_byte, size_t i_size )
{
    if( p_input->p_last_data - p_input->p_current_data < i_size )
    {
        /* Go to the next buffer */
        ssize_t i_ret = input_FillBuffer( p_input );

        if( i_size == -1 )
        {
            return( -1 );
        }
        else if( i_ret < i_size )
        {
            i_size = i_ret;
        }
    }
    *pp_byte = p_input->p_current_data;
    return( i_size );
}

/*****************************************************************************
 * input_SplitBuffer: give a pointer to a data packet containing i_size bytes
 * Returns the number of bytes read, or -1 in case of error
 *****************************************************************************/
ssize_t input_SplitBuffer( input_thread_t * p_input,
                           data_packet_t ** pp_data, size_t i_size )
{
    if( p_input->p_last_data - p_input->p_current_data < i_size )
    {
        /* Go to the next buffer */
        ssize_t i_ret = input_FillBuffer( p_input );

        if( i_ret == -1 )
        {
            return( -1 );
        }
        else if( i_ret < i_size )
        {
            i_size = i_ret;
        }
    }

    *pp_data = input_ShareBuffer( p_input->p_method_data,
                                  p_input->p_data_buffer );

    (*pp_data)->p_demux_start = (*pp_data)->p_payload_start
        = p_input->p_current_data;
    (*pp_data)->p_payload_end = (*pp_data)->p_demux_start + i_size;

    p_input->p_current_data += i_size;

    return( i_size );
}

/*****************************************************************************
 * input_AccessInit: initialize access plug-in wrapper structures
 *****************************************************************************/
int input_AccessInit( input_thread_t * p_input )
{
    p_input->p_method_data = input_BuffersInit();
    if( p_input->p_method_data == NULL ) return( -1 );
    p_input->p_data_buffer = NULL;
    p_input->p_current_data = NULL;
    p_input->p_last_data = NULL;
    return( 0 );
}

/*****************************************************************************
 * input_AccessReinit: reinit structures after a random seek
 *****************************************************************************/
void input_AccessReinit( input_thread_t * p_input )
{
    if( p_input->p_data_buffer != NULL )
    {
        ReleaseBuffer( p_input->p_method_data, p_input->p_data_buffer );
    }
    p_input->p_data_buffer = NULL;
    p_input->p_current_data = NULL;
    p_input->p_last_data = NULL;
}

/*****************************************************************************
 * input_AccessEnd: free access plug-in wrapper structures
 *****************************************************************************/
void input_AccessEnd( input_thread_t * p_input )
{
    if( p_input->p_data_buffer != NULL )
    {
        ReleaseBuffer( p_input->p_method_data, p_input->p_data_buffer );
    }

    input_BuffersEnd( p_input->p_method_data );
}


/*
 * Optional file descriptor management functions, for use by access plug-ins
 * base on file descriptors (file, udp, http...).
 */

/*****************************************************************************
 * input_FDClose: close the target
 *****************************************************************************/
void input_FDClose( input_thread_t * p_input )
{
    input_socket_t * p_access_data = (input_socket_t *)p_input->p_access_data;

    intf_WarnMsg( 2, "input: closing `%s/%s:%s'", 
                  p_input->psz_access, p_input->psz_demux, p_input->psz_name );
 
    close( p_access_data->i_handle );
    free( p_access_data );
}

/*****************************************************************************
 * input_FDNetworkClose: close the target
 *****************************************************************************/
void input_FDNetworkClose( input_thread_t * p_input )
{
    input_socket_t * p_access_data = (input_socket_t *)p_input->p_access_data;

    intf_WarnMsg( 2, "input: closing network `%s/%s:%s'", 
                  p_input->psz_access, p_input->psz_demux, p_input->psz_name );
 
#ifdef WIN32
    closesocket( p_access_data->i_handle );
#else
    close( p_access_data->i_handle );
#endif

    free( p_access_data );
}

/*****************************************************************************
 * input_FDRead: standard read on a file descriptor.
 *****************************************************************************/
ssize_t input_FDRead( input_thread_t * p_input, byte_t * p_buffer, size_t i_len )
{
    input_socket_t * p_access_data = (input_socket_t *)p_input->p_access_data;
 
    ssize_t i_ret = read( p_access_data->i_handle, p_buffer, i_len );
 
    if( i_ret > 0 )
    {
        vlc_mutex_lock( &p_input->stream.stream_lock );
        p_input->stream.p_selected_area->i_tell += i_ret;
        vlc_mutex_unlock( &p_input->stream.stream_lock );
    }
 
    if( i_ret < 0 )
    {
        intf_ErrMsg( "input error: read() failed (%s)", strerror(errno) );
    }
 
    return( i_ret );
}

/*****************************************************************************
 * NetworkSelect: Checks whether data is available on a file descriptor
 *****************************************************************************/
static inline int NetworkSelect( input_thread_t * p_input )
{
    input_socket_t * p_access_data = (input_socket_t *)p_input->p_access_data;
    struct timeval  timeout;
    fd_set          fds;
    int             i_ret;

    /* Initialize file descriptor set */
    FD_ZERO( &fds );
    FD_SET( p_access_data->i_handle, &fds );

    /* We'll wait 0.5 second if nothing happens */
    timeout.tv_sec = 0;
    timeout.tv_usec = 500000;
 
    /* Find if some data is available */
    i_ret = select( p_access_data->i_handle + 1, &fds,
                     NULL, NULL, &timeout );
 
    if( i_ret == -1 && errno != EINTR )
    {
        intf_ErrMsg( "input error: network select error (%s)", strerror(errno) );
    }

    return( i_ret );
}

/*****************************************************************************
 * input_FDNetworkRead: read on a file descriptor, checking periodically
 * p_input->b_die
 *****************************************************************************/
ssize_t input_FDNetworkRead( input_thread_t * p_input, byte_t * p_buffer,
                             size_t i_len )
{
    if( NetworkSelect( p_input ) > 0 )
    {
        input_socket_t * p_access_data
                             = (input_socket_t *)p_input->p_access_data;

        ssize_t i_ret = recv( p_access_data->i_handle, p_buffer, i_len, 0 );

        if( i_ret > 0 )
        {
            vlc_mutex_lock( &p_input->stream.stream_lock );
            p_input->stream.p_selected_area->i_tell += i_ret;
            vlc_mutex_unlock( &p_input->stream.stream_lock );
        }

        if( i_ret < 0 )
        {
            intf_ErrMsg( "input error: recv() failed (%s)", strerror(errno) );
        }

        return( i_ret );
    }
    
    return( 0 );
}

/*****************************************************************************
 * input_FDSeek: seek to a specific location in a file
 *****************************************************************************/
void input_FDSeek( input_thread_t * p_input, off_t i_pos )
{
#define S p_input->stream
    input_socket_t * p_access_data = (input_socket_t *)p_input->p_access_data;

    lseek( p_access_data->i_handle, i_pos, SEEK_SET );

    vlc_mutex_lock( &S.stream_lock );
    S.p_selected_area->i_tell = i_pos;
    if( S.p_selected_area->i_tell > S.p_selected_area->i_size )
    {
        intf_ErrMsg( "input error: seeking too far" );
        S.p_selected_area->i_tell = S.p_selected_area->i_size;
    }
    else if( S.p_selected_area->i_tell < 0 )
    {
        intf_ErrMsg( "input error: seeking too early" );
        S.p_selected_area->i_tell = 0;
    }
    vlc_mutex_unlock( &S.stream_lock );
#undef S
}


