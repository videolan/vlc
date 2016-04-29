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

# ifdef __cplusplus
extern "C" {
# endif

/**
 * \defgroup stream Stream
 * \ingroup input
 * Buffered input byte streams
 * @{
 * \file
 * Byte streams and byte stream filter modules interface
 */

/**
 * stream_t definition
 */

struct stream_t
{
    VLC_COMMON_MEMBERS

    /* Module properties for stream filter */
    module_t    *p_module;

    char        *psz_url;

    /* Stream source for stream filter */
    stream_t *p_source;

    /* */
    ssize_t     (*pf_read)(stream_t *, void *, size_t);
    int         (*pf_readdir)( stream_t *, input_item_node_t * );
    int         (*pf_seek)(stream_t *, uint64_t);
    int         (*pf_control)( stream_t *, int i_query, va_list );

    /* Private data for module */
    stream_sys_t *p_sys;

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
    STREAM_GET_SIZE=6,          /**< arg1= uint64_t *     res=can fail */
    STREAM_IS_DIRECTORY,        /**< arg1= bool *, res=can fail*/

    /* */
    STREAM_GET_PTS_DELAY = 0x101,/**< arg1= int64_t* res=cannot fail */
    STREAM_GET_TITLE_INFO, /**< arg1=input_title_t*** arg2=int* res=can fail */
    STREAM_GET_TITLE,       /**< arg1=unsigned * res=can fail */
    STREAM_GET_SEEKPOINT,   /**< arg1=unsigned * res=can fail */
    STREAM_GET_META,        /**< arg1= vlc_meta_t *       res=can fail */
    STREAM_GET_CONTENT_TYPE,    /**< arg1= char **         res=can fail */
    STREAM_GET_SIGNAL,      /**< arg1=double *pf_quality, arg2=double *pf_strength   res=can fail */

    STREAM_SET_PAUSE_STATE = 0x200, /**< arg1= bool        res=can fail */
    STREAM_SET_TITLE,       /**< arg1= int          res=can fail */
    STREAM_SET_SEEKPOINT,   /**< arg1= int          res=can fail */

    /* XXX only data read through stream_Read/Block will be recorded */
    STREAM_SET_RECORD_STATE,     /**< arg1=bool, arg2=const char *psz_ext (if arg1 is true)  res=can fail */

    STREAM_SET_PRIVATE_ID_STATE = 0x1000, /* arg1= int i_private_data, bool b_selected    res=can fail */
    STREAM_SET_PRIVATE_ID_CA,             /* arg1= int i_program_number, uint16_t i_vpid, uint16_t i_apid1, uint16_t i_apid2, uint16_t i_apid3, uint8_t i_length, uint8_t *p_data */
    STREAM_GET_PRIVATE_ID_STATE,          /* arg1=int i_private_data arg2=bool *          res=can fail */
    STREAM_GET_PRIVATE_BLOCK, /**< arg1= block_t **b, arg2=bool *eof */
};

/**
 * Reads data from a byte stream.
 *
 * This function always waits for the requested number of bytes, unless a fatal
 * error is encountered or the end-of-stream is reached first.
 *
 * If the buffer is NULL, data is skipped instead of read. This is effectively
 * a relative forward seek, but it works even on non-seekable streams.
 *
 * \param buf start of buffer to read data into [OUT]
 * \param len number of bytes to read
 * \return the number of bytes read or a negative value on error.
 */
VLC_API ssize_t stream_Read(stream_t *, void *, size_t) VLC_USED;

/**
 * Peeks at data from a byte stream.
 *
 * This function buffers for the requested number of bytes, waiting if
 * necessary. Then it stores a pointer to the buffer. Unlike stream_Read()
 * or stream_Block(), this function does not modify the stream read offset.
 *
 * \note
 * The buffer remains valid until the next read/peek or seek operation on the
 * same stream. In case of error, the buffer address is undefined.
 *
 * \param bufp storage space for the buffer address [OUT]
 * \param len number of bytes to peek
 * \return the number of bytes actually available (shorter than requested if
 * the end-of-stream is reached), or a negative value on error.
 */
VLC_API ssize_t stream_Peek(stream_t *, const uint8_t **, size_t) VLC_USED;

/**
 * Tells the current stream position.
 *
 * @return the byte offset from the beginning of the stream (cannot fail)
 */
VLC_API uint64_t stream_Tell(const stream_t *) VLC_USED;

/**
 * Sets the current stream position.
 *
 * @param offset byte offset from the beginning of the stream
 * @return zero on success, a negative value on error
 */
VLC_API int stream_Seek(stream_t *, uint64_t offset) VLC_USED;

VLC_API int stream_vaControl( stream_t *s, int i_query, va_list args );
VLC_API void stream_Delete( stream_t *s );
VLC_API int stream_Control( stream_t *s, int i_query, ... );
VLC_API block_t * stream_Block( stream_t *s, size_t );
VLC_API char * stream_ReadLine( stream_t * );
VLC_API int stream_ReadDir( stream_t *, input_item_node_t * );

/**
 * Low level custom stream creation.
 */
VLC_API stream_t *stream_CustomNew(vlc_object_t *, void (*)(stream_t *));

/**
 * Get the size of the stream.
 */
VLC_USED static inline int stream_GetSize( stream_t *s, uint64_t *size )
{
    return stream_Control( s, STREAM_GET_SIZE, size );
}

static inline int64_t stream_Size( stream_t *s )
{
    uint64_t i_pos;

    if( stream_GetSize( s, &i_pos ) )
        return 0;
    if( i_pos >> 62 )
        return (int64_t)1 << 62;
    return i_pos;
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
 * Default ReadDir implementation for stream Filter. This implementation just
 * forward the pf_readdir call to the p_source stream.
 */
VLC_API int stream_FilterDefaultReadDir( stream_t *s, input_item_node_t *p_node );

/**
 * Sets stream_FilterDefaultReadDir as the pf_readdir callback for this stream filter
 */
#define stream_FilterSetDefaultReadDir(p_stream) \
    p_stream->pf_readdir = stream_FilterDefaultReadDir;

/**
 * @}
 */

# ifdef __cplusplus
}
# endif

#endif
