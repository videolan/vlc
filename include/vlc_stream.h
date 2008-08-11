/*****************************************************************************
 * vlc_stream.h: Stream (between access and demux) descriptor and methods
 *****************************************************************************
 * Copyright (C) 1999-2004 the VideoLAN team
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef VLC_STREAM_H
#define VLC_STREAM_H 1

#include <vlc_block.h>

/**
 * \file
 * This file defines structures and functions for stream (between access and demux) descriptor in vlc
 */

# ifdef __cplusplus
extern "C" {
# endif

/**
 * \defgroup stream Stream
 *
 *  This will allow you to easily handle read/seek in demuxer modules.
 * @{
 */

/**
 * Possible commands to send to stream_Control() and stream_vaControl()
 */
enum stream_query_e
{
    /* capabilities */
    STREAM_CAN_SEEK,            /**< arg1= bool *   res=cannot fail*/
    STREAM_CAN_FASTSEEK,        /**< arg1= bool *   res=cannot fail*/

    /* */
    STREAM_SET_POSITION,        /**< arg1= int64_t        res=can fail  */
    STREAM_GET_POSITION,        /**< arg1= int64_t *      res=cannot fail*/

    STREAM_GET_SIZE,            /**< arg1= int64_t *      res=cannot fail (0 if no sense)*/

    STREAM_GET_MTU,             /**< arg1= int *          res=cannot fail (0 if no sense)*/

    /* Special for direct access control from demuxer.
     * XXX: avoid using it by all means */
    STREAM_CONTROL_ACCESS,  /* arg1= int i_access_query, args   res: can fail
                             if access unreachable or access control answer */

    STREAM_GET_CONTENT_TYPE,   /**< arg1= char **         res=can file */
};

VLC_EXPORT( int, stream_Read, ( stream_t *s, void *p_read, int i_read ) );
VLC_EXPORT( int, stream_Peek, ( stream_t *s, const uint8_t **pp_peek, int i_peek ) );
VLC_EXPORT( int, stream_vaControl, ( stream_t *s, int i_query, va_list args ) );
VLC_EXPORT( void, stream_Delete, ( stream_t *s ) );
VLC_EXPORT( int, stream_Control, ( stream_t *s, int i_query, ... ) );
VLC_EXPORT( block_t *, stream_Block, ( stream_t *s, int i_size ) );
VLC_EXPORT( char *, stream_ReadLine, ( stream_t * ) );

/**
 * Get the current position in a stream
 */
static inline int64_t stream_Tell( stream_t *s )
{
    int64_t i_pos;
    stream_Control( s, STREAM_GET_POSITION, &i_pos );
    return i_pos;
}

/**
 * Get the size of the stream.
 */
static inline int64_t stream_Size( stream_t *s )
{
    int64_t i_pos;
    stream_Control( s, STREAM_GET_SIZE, &i_pos );
    return i_pos;
}

static inline int stream_MTU( stream_t *s )
{
    int i_mtu;
    stream_Control( s, STREAM_GET_MTU, &i_mtu );
    return i_mtu;
}

static inline int stream_Seek( stream_t *s, int64_t i_pos )
{
    return stream_Control( s, STREAM_SET_POSITION, i_pos );
}

/**
 * Get the Content-Type of a stream, or NULL if unknown.
 * Result must be free()'d.
 */
static inline char *stream_ContentType( stream_t *s )
{
    char *res;
    if( stream_Control( s, STREAM_GET_CONTENT_TYPE, &res ) )
        return NULL;
    return res;
}

/**
 * Create a special stream and a demuxer, this allows chaining demuxers
 */
#define stream_DemuxNew( a, b, c ) __stream_DemuxNew( VLC_OBJECT(a), b, c)
VLC_EXPORT( stream_t *,__stream_DemuxNew, ( vlc_object_t *p_obj, const char *psz_demux, es_out_t *out ) );
VLC_EXPORT( void,      stream_DemuxSend,  ( stream_t *s, block_t *p_block ) );
VLC_EXPORT( void,      stream_DemuxDelete,( stream_t *s ) );

#define stream_MemoryNew( a, b, c, d ) __stream_MemoryNew( VLC_OBJECT(a), b, c, d )
VLC_EXPORT( stream_t *,__stream_MemoryNew, (vlc_object_t *p_obj, uint8_t *p_buffer, int64_t i_size, bool i_preserve_memory ) );
#define stream_UrlNew( a, b ) __stream_UrlNew( VLC_OBJECT(a), b )
VLC_EXPORT( stream_t *,__stream_UrlNew, (vlc_object_t *p_this, const char *psz_url ) );

/**
 * @}
 */

# ifdef __cplusplus
}
# endif

# if defined (__PLUGIN__) || defined (__BUILTIN__)
   /* FIXME UGLY HACK to keep VLC_OBJECT working */
   /* Maybe we should make VLC_OBJECT a simple cast noawadays... */
struct stream_t
{
    VLC_COMMON_MEMBERS
};
# endif

#endif
