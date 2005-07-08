/*****************************************************************************
 * vlc_stream.h
 *****************************************************************************
 * Copyright (C) 1999-2004 VideoLAN (Centrale Réseaux) and its contributors
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

#ifndef _VLC_STREAM_H
#define _VLC_STREAM_H 1

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
    STREAM_CAN_SEEK,            /**< arg1= vlc_bool_t *   res=cannot fail*/
    STREAM_CAN_FASTSEEK,        /**< arg1= vlc_bool_t *   res=cannot fail*/

    /* */
    STREAM_SET_POSITION,        /**< arg1= int64_t        res=can fail  */
    STREAM_GET_POSITION,        /**< arg1= int64_t *      res=cannot fail*/

    STREAM_GET_SIZE,            /**< arg1= int64_t *      res=cannot fail (0 if no sense)*/

    STREAM_GET_MTU,             /**< arg1= int *          res=cannot fail (0 if no sense)*/

    /* Special for direct access control from demuxer.
     * XXX: avoid using it by all means */
    STREAM_CONTROL_ACCESS,      /* arg1= int i_access_query, args   res: can fail
                                   if access unreachable or access control answer */
};

/**
 * stream_t definition
 */
struct stream_t
{
    VLC_COMMON_MEMBERS

    block_t *(*pf_block)  ( stream_t *, int i_size );
    int      (*pf_read)   ( stream_t *, void *p_read, int i_read );
    int      (*pf_peek)   ( stream_t *, uint8_t **pp_peek, int i_peek );
    int      (*pf_control)( stream_t *, int i_query, va_list );
    void     (*pf_destroy)( stream_t *);

    stream_sys_t *p_sys;
};

/**
 * Try to read "i_read" bytes into a buffer pointed by "p_read".  If
 * "p_read" is NULL then data are skipped instead of read.  The return
 * value is the real numbers of bytes read/skip. If this value is less
 * than i_read that means that it's the end of the stream.
 */
static inline int stream_Read( stream_t *s, void *p_read, int i_read )
{
    return s->pf_read( s, p_read, i_read );
}

/**
 * Store in pp_peek a pointer to the next "i_peek" bytes in the stream
 * \return The real numbers of valid bytes, if it's less
 * or equal to 0, *pp_peek is invalid.
 * \note pp_peek is a pointer to internal buffer and it will be invalid as
 * soons as other stream_* functions are called.
 * \note Due to input limitation, it could be less than i_peek without meaning
 * the end of the stream (but only when you have i_peek >=
 * p_input->i_bufsize)
 */
static inline int stream_Peek( stream_t *s, uint8_t **pp_peek, int i_peek )
{
    return s->pf_peek( s, pp_peek, i_peek );
}

/**
 * Use to control the "stream_t *". Look at #stream_query_e for
 * possible "i_query" value and format arguments.  Return VLC_SUCCESS
 * if ... succeed ;) and VLC_EGENERIC if failed or unimplemented
 */
static inline int stream_vaControl( stream_t *s, int i_query, va_list args )
{
    return s->pf_control( s, i_query, args );
}

/**
 * Destroy a stream
 */
static inline void stream_Delete( stream_t *s )
{
    s->pf_destroy( s );
}

static inline int stream_Control( stream_t *s, int i_query, ... )
{
    va_list args;
    int     i_result;

    va_start( args, i_query );
    i_result = s->pf_control( s, i_query, args );
    va_end( args );
    return i_result;
}

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
 * Read "i_size" bytes and store them in a block_t. If less than "i_size"
 * bytes are available then return what is left and if nothing is availble,
 * return NULL.
 */
static inline block_t *stream_Block( stream_t *s, int i_size )
{
    if( i_size <= 0 ) return NULL;

    if( s->pf_block )
    {
        return s->pf_block( s, i_size );
    }
    else
    {
        /* emulate block read */
        block_t *p_bk = block_New( s, i_size );
        if( p_bk )
        {
            p_bk->i_buffer = stream_Read( s, p_bk->p_buffer, i_size );
            if( p_bk->i_buffer > 0 )
            {
                return p_bk;
            }
        }
        if( p_bk ) block_Release( p_bk );
        return NULL;
    }
}

VLC_EXPORT( char *, stream_ReadLine, ( stream_t * ) );

/**
 * Create a special stream and a demuxer, this allows chaining demuxers
 */
#define stream_DemuxNew( a, b, c ) __stream_DemuxNew( VLC_OBJECT(a), b, c)
VLC_EXPORT( stream_t *,__stream_DemuxNew, ( vlc_object_t *p_obj, char *psz_demux, es_out_t *out ) );
VLC_EXPORT( void,      stream_DemuxSend,  ( stream_t *s, block_t *p_block ) );
VLC_EXPORT( void,      stream_DemuxDelete,( stream_t *s ) );


#define stream_MemoryNew( a, b, c, d ) __stream_MemoryNew( VLC_OBJECT(a), b, c, d )
VLC_EXPORT( stream_t *,__stream_MemoryNew, (vlc_object_t *p_obj, uint8_t *p_buffer, int64_t i_size, vlc_bool_t i_preserve_memory ) );
#define stream_UrlNew( a, b ) __stream_UrlNew( VLC_OBJECT(a), b )
VLC_EXPORT( stream_t *,__stream_UrlNew, (vlc_object_t *p_this, const char *psz_url ) );

/**
 * @}
 */

# ifdef __cplusplus
}
# endif

#endif
