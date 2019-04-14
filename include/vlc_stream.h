/*****************************************************************************
 * vlc_stream.h: Stream (between access and demux) descriptor and methods
 *****************************************************************************
 * Copyright (C) 1999-2004 VLC authors and VideoLAN
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
    struct vlc_object_t obj;

    char        *psz_name;
    char        *psz_url; /**< Full URL or MRL (can be NULL) */
    const char  *psz_location; /**< Location (URL with the scheme stripped) */
    char        *psz_filepath; /**< Local file path (if applicable) */
    bool         b_preparsing; /**< True if this access is used to preparse */
    input_item_t *p_input_item;/**< Input item (can be NULL) */

    union {
        /**
         * Input stream
         *
         * Depending on the module capability:
         * - "stream filter" or "demux": input byte stream (not NULL)
         * - "access": a NULL pointer
         * - "demux_filter": undefined
         */
        stream_t    *s;
        /**
         * Input demuxer
         *
         * If the module capability is "demux_filter", this is the upstream
         * demuxer or demux filter. Otherwise, this is undefined.
         */
        demux_t *p_next;
    };

    /* es output */
    es_out_t    *out;   /* our p_es_out */

    /**
     * Read data.
     *
     * Callback to read data from the stream into a caller-supplied buffer.
     *
     * This may be NULL if the stream is actually a directory rather than a
     * byte stream, or if \ref stream_t.pf_block is non-NULL.
     *
     * \param buf buffer to read data into
     * \param len buffer length (in bytes)
     *
     * \retval -1 no data available yet
     * \retval 0 end of stream (incl. fatal error)
     * \retval positive number of bytes read (no more than len)
     */
    ssize_t     (*pf_read)(stream_t *, void *buf, size_t len);

    /**
     * Read data block.
     *
     * Callback to read a block of data. The data is read into a block of
     * memory allocated by the stream. For some streams, data can be read more
     * efficiently in block of a certain size, and/or using a custom allocator
     * for buffers. In such case, this callback should be provided instead of
     * \ref stream_t.pf_read; otherwise, this should be NULL.
     *
     * \param eof storage space for end-of-stream flag [OUT]
     * (*eof is always false when invoking pf_block(); pf_block() should set
     *  *eof to true if it detects the end of the stream)
     *
     * \return a data block,
     * NULL if no data available yet, on error and at end-of-stream
     */
    block_t    *(*pf_block)(stream_t *, bool *eof);

    /**
     * Read directory.
     *
     * Callback to fill an item node from a directory
     * (see doc/browsing.txt for details).
     *
     * NULL if the stream is not a directory.
     */
    int         (*pf_readdir)(stream_t *, input_item_node_t *);

    int         (*pf_demux)(stream_t *);

    /**
     * Seek.
     *
     * Callback to set the stream pointer (in bytes from start).
     *
     * May be NULL if seeking is not supported.
     */
    int         (*pf_seek)(stream_t *, uint64_t);

    /**
     * Stream control.
     *
     * Cannot be NULL.
     *
     * \see stream_query_e
     */
    int         (*pf_control)(stream_t *, int i_query, va_list);

    /**
     * Private data pointer
     */
    void *p_sys;
};

/**
 * Possible commands to send to vlc_stream_Control() and vlc_stream_vaControl()
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

    /* */
    STREAM_GET_PTS_DELAY = 0x101,/**< arg1= vlc_tick_t* res=cannot fail */
    STREAM_GET_TITLE_INFO, /**< arg1=input_title_t*** arg2=int* res=can fail */
    STREAM_GET_TITLE,       /**< arg1=unsigned * res=can fail */
    STREAM_GET_SEEKPOINT,   /**< arg1=unsigned * res=can fail */
    STREAM_GET_META,        /**< arg1= vlc_meta_t *       res=can fail */
    STREAM_GET_CONTENT_TYPE,    /**< arg1= char **         res=can fail */
    STREAM_GET_SIGNAL,      /**< arg1=double *pf_quality, arg2=double *pf_strength   res=can fail */
    STREAM_GET_TAGS,        /**< arg1=const block_t ** res=can fail */

    STREAM_SET_PAUSE_STATE = 0x200, /**< arg1= bool        res=can fail */
    STREAM_SET_TITLE,       /**< arg1= int          res=can fail */
    STREAM_SET_SEEKPOINT,   /**< arg1= int          res=can fail */

    /* XXX only data read through vlc_stream_Read/Block will be recorded */
    STREAM_SET_RECORD_STATE,     /**< arg1=bool, arg2=const char *psz_ext (if arg1 is true)  res=can fail */

    STREAM_SET_PRIVATE_ID_STATE = 0x1000, /* arg1= int i_private_data, bool b_selected    res=can fail */
    STREAM_SET_PRIVATE_ID_CA,             /* arg1= void * */
    STREAM_GET_PRIVATE_ID_STATE,          /* arg1=int i_private_data arg2=bool *          res=can fail */
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
VLC_API ssize_t vlc_stream_Read(stream_t *, void *buf, size_t len) VLC_USED;

/**
 * Reads partial data from a byte stream.
 *
 * This function waits until some data is available for reading from the
 * stream, a fatal error is encountered or the end-of-stream is reached.
 *
 * Unlike vlc_stream_Read(), this function does not wait for the full requested
 * bytes count. It can return a short count even before the end of the stream
 * and in the absence of any error.
 *
 * \param buf start of buffer to read data into [OUT]
 * \param len buffer size (maximum number of bytes to read)
 * \return the number of bytes read or a negative value on error.
 */
VLC_API ssize_t vlc_stream_ReadPartial(stream_t *, void *buf, size_t len)
VLC_USED;

/**
 * Peeks at data from a byte stream.
 *
 * This function buffers for the requested number of bytes, waiting if
 * necessary. Then it stores a pointer to the buffer. Unlike vlc_stream_Read()
 * or vlc_stream_Block(), this function does not modify the stream read offset.
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
VLC_API ssize_t vlc_stream_Peek(stream_t *, const uint8_t **, size_t) VLC_USED;

/**
 * Reads a data block from a byte stream.
 *
 * This function dequeues the next block of data from the byte stream. The
 * byte stream back-end decides on the size of the block; the caller cannot
 * make any assumption about it.
 *
 * The function might also return NULL spuriously - this does not necessarily
 * imply that the stream is ended nor that it has encountered a nonrecoverable
 * error.
 *
 * This function should be used instead of vlc_stream_Read() or
 * vlc_stream_Peek() when the caller can handle reads of any size.
 *
 * \return either a data block or NULL
 */
VLC_API block_t *vlc_stream_ReadBlock(stream_t *) VLC_USED;

/**
 * Tells the current stream position.
 *
 * This function tells the current read offset (in bytes) from the start of
 * the start of the stream.
 * @note The read offset may be larger than the stream size, either because of
 * a seek past the end, or because the stream shrank asynchronously.
 *
 * @return the byte offset from the beginning of the stream (cannot fail)
 */
VLC_API uint64_t vlc_stream_Tell(const stream_t *) VLC_USED;

/**
 * Checks for end of stream.
 *
 * Checks if the last attempt to reads data from the stream encountered the
 * end of stream before the attempt could be fully satisfied.
 * The value is initially false, and is reset to false by vlc_stream_Seek().
 *
 * \note The function can return false even though the current stream position
 * is equal to the stream size. It will return true after the following attempt
 * to read more than zero bytes.
 *
 * \note It might be possible to read after the end of the stream.
 * It implies the size of the stream increased asynchronously in the mean time.
 * Streams of most types cannot trigger such a case,
 * but regular local files notably can.
 *
 * \note In principles, the stream size should match the stream offset when
 * the end-of-stream is reached. But that rule is not enforced; it is entirely
 * dependent on the underlying implementation of the stream.
 */
VLC_API bool vlc_stream_Eof(const stream_t *) VLC_USED;

/**
 * Sets the current stream position.
 *
 * This function changes the read offset within a stream, if the stream
 * supports seeking. In case of error, the read offset is not changed.
 *
 * @note It is possible (but not useful) to seek past the end of a stream.
 *
 * @param offset byte offset from the beginning of the stream
 * @return zero on success, a negative value on error
 */
VLC_API int vlc_stream_Seek(stream_t *, uint64_t offset) VLC_USED;

VLC_API int vlc_stream_vaControl(stream_t *s, int query, va_list args);

static inline int vlc_stream_Control(stream_t *s, int query, ...)
{
    va_list ap;
    int ret;

    va_start(ap, query);
    ret = vlc_stream_vaControl(s, query, ap);
    va_end(ap);
    return ret;
}

VLC_API block_t *vlc_stream_Block(stream_t *s, size_t);
VLC_API char *vlc_stream_ReadLine(stream_t *);

/**
 * Reads a directory.
 *
 * This function fills an input item node with any and all the items within
 * a directory. The behaviour is undefined if the stream is not a directory.
 *
 * \param s directory object to read from
 * \param node node to store the items into
 * \return VLC_SUCCESS on success
 */
VLC_API int vlc_stream_ReadDir(stream_t *s, input_item_node_t *node);

/**
 * Closes a byte stream.
 * \param s byte stream to close
 */
VLC_API void vlc_stream_Delete(stream_t *s);

VLC_API stream_t *vlc_stream_CommonNew(vlc_object_t *, void (*)(stream_t *));

/**
 * Get the size of the stream.
 */
VLC_USED static inline int vlc_stream_GetSize( stream_t *s, uint64_t *size )
{
    return vlc_stream_Control( s, STREAM_GET_SIZE, size );
}

static inline int64_t stream_Size( stream_t *s )
{
    uint64_t i_pos;

    if( vlc_stream_GetSize( s, &i_pos ) )
        return 0;
    if( i_pos >> 62 )
        return (int64_t)1 << 62;
    return i_pos;
}

VLC_USED
static inline bool stream_HasExtension( stream_t *s, const char *extension )
{
    const char *name = (s->psz_filepath != NULL) ? s->psz_filepath
                                                 : s->psz_url;
    const char *ext = strrchr( name, '.' );
    return ext != NULL && !strcasecmp( ext, extension );
}

/**
 * Get the Content-Type of a stream, or NULL if unknown.
 * Result must be free()'d.
 */
static inline char *stream_ContentType( stream_t *s )
{
    char *res;
    if( vlc_stream_Control( s, STREAM_GET_CONTENT_TYPE, &res ) )
        return NULL;
    return res;
}

/**
 * Get the mime-type of a stream
 *
 * \warning the returned resource is to be freed by the caller
 * \return the mime-type, or `NULL` if unknown
 **/
VLC_USED
static inline char *stream_MimeType( stream_t *s )
{
    char* mime_type = stream_ContentType( s );

    if( mime_type ) /* strip parameters */
        mime_type[strcspn( mime_type, " ;" )] = '\0';

    return mime_type;
}

/**
 * Checks for a MIME type.
 *
 * Checks if the stream has a specific MIME type.
 */
VLC_USED
static inline bool stream_IsMimeType(stream_t *s, const char *type)
{
    char *mime = stream_MimeType(s);
    if (mime == NULL)
        return false;

    bool ok = !strcasecmp(mime, type);
    free(mime);
    return ok;
}

/**
 * Create a stream from a memory buffer.
 *
 * \param obj parent VLC object
 * \param base start address of the memory buffer to read from
 * \param size size in bytes of the memory buffer
 * \param preserve if false, free(base) will be called when the stream is
 *                 destroyed; if true, the memory buffer is preserved
 */
VLC_API stream_t *vlc_stream_MemoryNew(vlc_object_t *obj, uint8_t *base,
                                       size_t size, bool preserve) VLC_USED;
#define vlc_stream_MemoryNew(a, b, c, d) \
        vlc_stream_MemoryNew(VLC_OBJECT(a), b, c, d)

/**
 * Create a stream_t reading from a URL.
 * You must delete it using vlc_stream_Delete.
 */
VLC_API stream_t * vlc_stream_NewURL(vlc_object_t *obj, const char *url)
VLC_USED;
#define vlc_stream_NewURL(a, b) vlc_stream_NewURL(VLC_OBJECT(a), b)

/**
 * \defgroup stream_fifo FIFO stream
 * In-memory anonymous pipe
  @{
 */

typedef struct vlc_stream_fifo vlc_stream_fifo_t;

/**
 * Creates a FIFO stream.
 *
 * Creates a non-seekable byte stream object whose byte stream is generated
 * by another thread in the process. This is the LibVLC equivalent of an
 * anonymous pipe/FIFO.
 *
 * On the reader side, the normal stream functions are used,
 * e.g. vlc_stream_Read() and vlc_stream_Delete().
 *
 * The created stream object is automatically destroyed when both the reader
 * and the writer sides have been closed, with vlc_stream_Delete() and
 * vlc_stream_fifo_Close() respectively.
 *
 * \param parent parent VLC object for the stream
 * \param reader location to store read side stream pointer [OUT]
 * \return a FIFO stream object or NULL on memory error.
 */
VLC_API vlc_stream_fifo_t *vlc_stream_fifo_New(vlc_object_t *parent,
                                               stream_t **reader);

/**
 * Writes a block to a FIFO stream.
 *
 * \param s FIFO stream created by vlc_stream_fifo_New()
 * \param block data block to write to the stream
 * \return 0 on success. -1 if the reader end has already been closed
 * (errno is then set to EPIPE, and the block is deleted).
 *
 * \bug No congestion control is performed. If the reader end is not keeping
 * up with the writer end, buffers will accumulate in memory.
 */
VLC_API int vlc_stream_fifo_Queue(vlc_stream_fifo_t *s, block_t *block);

/**
 * Writes data to a FIFO stream.
 *
 * This is a convenience helper for vlc_stream_fifo_Queue().
 * \param s FIFO stream created by vlc_stream_fifo_New()
 * \param buf start address of data to write
 * \param len length of data to write in bytes
 * \return len on success, or -1 on error (errno is set accordingly)
 */
VLC_API ssize_t vlc_stream_fifo_Write(vlc_stream_fifo_t *s, const void *buf,
                                      size_t len);

/**
 * Terminates a FIFO stream.
 *
 * Marks the end of the FIFO stream and releases any underlying resources.
 * \param s FIFO stream created by vlc_stream_fifo_New()
 */
VLC_API void vlc_stream_fifo_Close(vlc_stream_fifo_t *s);

/**
 * @}
 */

/**
 * Try to add a stream filter to an open stream.
 * @return New stream to use, or NULL if the filter could not be added.
 **/
VLC_API stream_t* vlc_stream_FilterNew( stream_t *p_source, const char *psz_stream_filter );

/**
 * @}
 */

# ifdef __cplusplus
}
# endif

#endif
