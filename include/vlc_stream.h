/*****************************************************************************
 * vlc_stream.h: Stream (between access and demux) descriptor and methods
 *****************************************************************************
 * Copyright (C) 1999-2004 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
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

/* Opaque definition for text reader context */
typedef struct stream_text_t stream_text_t;

/**
 * stream_t definition
 */

struct stream_t
{
    VLC_COMMON_MEMBERS
    bool        b_error;

    /* Module properties for stream filter */
    module_t    *p_module;

    char        *psz_access;
    /* Real or virtual path (it can only be changed during stream_t opening) */
    char        *psz_path;

    /* Stream source for stream filter */
    stream_t *p_source;

    /* */
    int      (*pf_read)   ( stream_t *, void *p_read, unsigned int i_read );
    int      (*pf_peek)   ( stream_t *, const uint8_t **pp_peek, unsigned int i_peek );
    int      (*pf_control)( stream_t *, int i_query, va_list );

    /* */
    void     (*pf_destroy)( stream_t *);

    /* Private data for module */
    stream_sys_t *p_sys;

    /* Text reader state */
    stream_text_t *p_text;

    /* Weak link to parent input */
    input_thread_t *p_input;
};

/**
 * Possible commands to send to stream_Control() and stream_vaControl()
 */
enum stream_query_e
{
    /* capabilities */
    STREAM_CAN_SEEK,            /**< arg1= bool *   res=cannot fail*/
    STREAM_CAN_FASTSEEK,        /**< arg1= bool *   res=cannot fail*/
    STREAM_CAN_PAUSE,           /**< arg1= bool *   res=cannot fail*/
    STREAM_CAN_CONTROL_PACE,    /**< arg1= bool *   res=cannot fail*/

    /* */
    STREAM_SET_POSITION,        /**< arg1= uint64_t       res=can fail  */
    STREAM_GET_POSITION,        /**< arg1= uint64_t *     res=cannot fail*/

    STREAM_GET_SIZE,            /**< arg1= uint64_t *     res=cannot fail (0 if no sense)*/

    /* Special for direct access control from demuxer.
     * XXX: avoid using it by all means */
    STREAM_CONTROL_ACCESS,  /* arg1= int i_access_query, args   res: can fail
                             if access unreachable or access control answer */

    /* You should update size of source if any and then update size 
     * FIXME find a way to avoid it */
    STREAM_UPDATE_SIZE,

    /* */
    STREAM_GET_TITLE_INFO = 0x102, /**< arg1=input_title_t*** arg2=int* res=can fail */
    STREAM_GET_META,        /**< arg1= vlc_meta_t **       res=can fail */
    STREAM_GET_CONTENT_TYPE,    /**< arg1= char **         res=can fail */
    STREAM_GET_SIGNAL,      /**< arg1=double *pf_quality, arg2=double *pf_strength   res=can fail */

    STREAM_SET_PAUSE_STATE = 0x200, /**< arg1= bool        res=can fail */
    STREAM_SET_TITLE,       /**< arg1= int          res=can fail */
    STREAM_SET_SEEKPOINT,   /**< arg1= int          res=can fail */

    /* XXX only data read through stream_Read/Block will be recorded */
    STREAM_SET_RECORD_STATE,     /**< arg1=bool, arg2=const char *psz_ext (if arg1 is true)  res=can fail */
};

VLC_API int stream_Read( stream_t *s, void *p_read, int i_read );
VLC_API int stream_Peek( stream_t *s, const uint8_t **pp_peek, int i_peek );
VLC_API int stream_vaControl( stream_t *s, int i_query, va_list args );
VLC_API void stream_Delete( stream_t *s );
VLC_API int stream_Control( stream_t *s, int i_query, ... );
VLC_API block_t * stream_Block( stream_t *s, int i_size );
VLC_API block_t * stream_BlockRemaining( stream_t *s, int i_max_size );
VLC_API char * stream_ReadLine( stream_t * );

/**
 * Get the current position in a stream
 */
static inline int64_t stream_Tell( stream_t *s )
{
    uint64_t i_pos;
    stream_Control( s, STREAM_GET_POSITION, &i_pos );
    if( i_pos >> 62 )
        return (int64_t)1 << 62;
    return i_pos;
}

/**
 * Get the size of the stream.
 */
static inline int64_t stream_Size( stream_t *s )
{
    uint64_t i_pos;
    stream_Control( s, STREAM_GET_SIZE, &i_pos );
    if( i_pos >> 62 )
        return (int64_t)1 << 62;
    return i_pos;
}

static inline int stream_Seek( stream_t *s, uint64_t i_pos )
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
 * You must delete it using stream_Delete.
 */
VLC_API stream_t * stream_DemuxNew( demux_t *p_demux, const char *psz_demux, es_out_t *out );

/**
 * Send data to a stream handle created by stream_DemuxNew().
 */
VLC_API void stream_DemuxSend( stream_t *s, block_t *p_block );

/**
 * Perform a <b>demux</b> (i.e. DEMUX_...) control request on a stream handle
 * created by stream_DemuxNew().
 */
VLC_API int stream_DemuxControlVa( stream_t *s, int, va_list );

static inline int stream_DemuxControl( stream_t *s, int query, ... )
{
    va_list ap;
    int ret;

    va_start( ap, query );
    ret = stream_DemuxControlVa( s, query, ap );
    va_end( ap );
    return ret;
}

/**
 * Create a stream_t reading from memory.
 * You must delete it using stream_Delete.
 */
VLC_API stream_t * stream_MemoryNew(vlc_object_t *p_obj, uint8_t *p_buffer, uint64_t i_size, bool b_preserve_memory );
#define stream_MemoryNew( a, b, c, d ) stream_MemoryNew( VLC_OBJECT(a), b, c, d )

/**
 * Create a stream_t reading from a URL.
 * You must delete it using stream_Delete.
 */
VLC_API stream_t * stream_UrlNew(vlc_object_t *p_this, const char *psz_url );
#define stream_UrlNew( a, b ) stream_UrlNew( VLC_OBJECT(a), b )


/**
 * Try to add a stream filter to an open stream.
 * @return New stream to use, or NULL if the filter could not be added.
 **/
VLC_API stream_t* stream_FilterNew( stream_t *p_source, const char *psz_stream_filter );
/**
 * @}
 */

# ifdef __cplusplus
}
# endif

#endif
