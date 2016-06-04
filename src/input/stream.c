/*****************************************************************************
 * stream.c
 *****************************************************************************
 * Copyright (C) 1999-2004 VLC authors and VideoLAN
 * Copyright 2008-2015 Rémi Denis-Courmont
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include <vlc_common.h>
#include <vlc_block.h>
#include <vlc_memory.h>
#include <vlc_access.h>
#include <vlc_charset.h>
#include <vlc_interrupt.h>

#include <libvlc.h>
#include "stream.h"

typedef struct stream_priv_t
{
    stream_t stream;
    void (*destroy)(stream_t *);
    block_t *peek;
    uint64_t offset;

    /* UTF-16 and UTF-32 file reading */
    struct {
        vlc_iconv_t   conv;
        unsigned char char_width;
        bool          little_endian;
    } text;
} stream_priv_t;

/**
 * Allocates a custom VLC stream object
 */
stream_t *stream_CustomNew(vlc_object_t *parent, void (*destroy)(stream_t *))
{
    return stream_CommonNew(parent, destroy);
}

/**
 * Allocates a VLC stream object
 */
stream_t *stream_CommonNew(vlc_object_t *parent, void (*destroy)(stream_t *))
{
    stream_priv_t *priv = vlc_custom_create(parent, sizeof (*priv), "stream");
    if (unlikely(priv == NULL))
        return NULL;

    stream_t *s = &priv->stream;

    s->p_module = NULL;
    s->psz_url = NULL;
    s->p_source = NULL;
    s->pf_read = NULL;
    s->pf_readdir = NULL;
    s->pf_control = NULL;
    s->p_sys = NULL;
    s->p_input = NULL;
    assert(destroy != NULL);
    priv->destroy = destroy;
    priv->peek = NULL;
    priv->offset = 0;

    /* UTF16 and UTF32 text file conversion */
    priv->text.conv = (vlc_iconv_t)(-1);
    priv->text.char_width = 1;
    priv->text.little_endian = false;

    return s;
}

void stream_CommonDelete(stream_t *s)
{
    stream_priv_t *priv = (stream_priv_t *)s;

    if (priv->text.conv != (vlc_iconv_t)(-1))
        vlc_iconv_close(priv->text.conv);

    if (priv->peek != NULL)
        block_Release(priv->peek);

    free(s->psz_url);
    vlc_object_release(s);
}

/**
 * Destroy a stream
 */
void stream_Delete(stream_t *s)
{
    stream_priv_t *priv = (stream_priv_t *)s;

    priv->destroy(s);
    stream_CommonDelete(s);
}

#undef stream_UrlNew
/****************************************************************************
 * stream_UrlNew: create a stream from a access
 ****************************************************************************/
stream_t *stream_UrlNew( vlc_object_t *p_parent, const char *psz_url )
{
    if( !psz_url )
        return NULL;

    stream_t *s = stream_AccessNew( p_parent, NULL, false, psz_url );
    if( s == NULL )
        msg_Err( p_parent, "no suitable access module for `%s'", psz_url );
    return s;
}

/**
 * Read from the stream until first newline.
 * \param s Stream handle to read from
 * \return A pointer to the allocated output string. You need to free this when you are done.
 */
#define STREAM_PROBE_LINE 2048
#define STREAM_LINE_MAX (2048*100)
char *stream_ReadLine( stream_t *s )
{
    stream_priv_t *priv = (stream_priv_t *)s;
    char *p_line = NULL;
    int i_line = 0, i_read = 0;

    /* Let's fail quickly if this is a readdir access */
    if( s->pf_read == NULL )
        return NULL;

    for( ;; )
    {
        char *psz_eol;
        const uint8_t *p_data;
        int i_data;
        int64_t i_pos;

        /* Probe new data */
        i_data = stream_Peek( s, &p_data, STREAM_PROBE_LINE );
        if( i_data <= 0 ) break; /* No more data */

        /* BOM detection */
        i_pos = stream_Tell( s );
        if( i_pos == 0 && i_data >= 2 )
        {
            const char *psz_encoding = NULL;

            if( !memcmp( p_data, "\xFF\xFE", 2 ) )
            {
                psz_encoding = "UTF-16LE";
                priv->text.little_endian = true;
            }
            else if( !memcmp( p_data, "\xFE\xFF", 2 ) )
            {
                psz_encoding = "UTF-16BE";
            }

            /* Open the converter if we need it */
            if( psz_encoding != NULL )
            {
                msg_Dbg( s, "UTF-16 BOM detected" );
                priv->text.char_width = 2;
                priv->text.conv = vlc_iconv_open( "UTF-8", psz_encoding );
                if( priv->text.conv == (vlc_iconv_t)-1 )
                    msg_Err( s, "iconv_open failed" );
            }
        }

        if( i_data % priv->text.char_width )
        {
            /* keep i_char_width boundary */
            i_data = i_data - ( i_data % priv->text.char_width );
            msg_Warn( s, "the read is not i_char_width compatible");
        }

        if( i_data == 0 )
            break;

        /* Check if there is an EOL */
        if( priv->text.char_width == 1 )
        {
            /* UTF-8: 0A <LF> */
            psz_eol = memchr( p_data, '\n', i_data );
            if( psz_eol == NULL )
                /* UTF-8: 0D <CR> */
                psz_eol = memchr( p_data, '\r', i_data );
        }
        else
        {
            const uint8_t *p_last = p_data + i_data - priv->text.char_width;
            uint16_t eol = priv->text.little_endian ? 0x0A00 : 0x00A0;

            assert( priv->text.char_width == 2 );
            psz_eol = NULL;
            /* UTF-16: 000A <LF> */
            for( const uint8_t *p = p_data; p <= p_last; p += 2 )
            {
                if( U16_AT( p ) == eol )
                {
                     psz_eol = (char *)p + 1;
                     break;
                }
            }

            if( psz_eol == NULL )
            {   /* UTF-16: 000D <CR> */
                eol = priv->text.little_endian ? 0x0D00 : 0x00D0;
                for( const uint8_t *p = p_data; p <= p_last; p += 2 )
                {
                    if( U16_AT( p ) == eol )
                    {
                        psz_eol = (char *)p + 1;
                        break;
                    }
                }
            }
        }

        if( psz_eol )
        {
            i_data = (psz_eol - (char *)p_data) + 1;
            p_line = realloc_or_free( p_line,
                        i_line + i_data + priv->text.char_width ); /* add \0 */
            if( !p_line )
                goto error;
            i_data = stream_Read( s, &p_line[i_line], i_data );
            if( i_data <= 0 ) break; /* Hmmm */
            i_line += i_data - priv->text.char_width; /* skip \n */;
            i_read += i_data;

            /* We have our line */
            break;
        }

        /* Read data (+1 for easy \0 append) */
        p_line = realloc_or_free( p_line,
                          i_line + STREAM_PROBE_LINE + priv->text.char_width );
        if( !p_line )
            goto error;
        i_data = stream_Read( s, &p_line[i_line], STREAM_PROBE_LINE );
        if( i_data <= 0 ) break; /* Hmmm */
        i_line += i_data;
        i_read += i_data;

        if( i_read >= STREAM_LINE_MAX )
            goto error; /* line too long */
    }

    if( i_read > 0 )
    {
        memset(p_line + i_line, 0, priv->text.char_width);
        i_line += priv->text.char_width; /* the added \0 */

        if( priv->text.char_width > 1 )
        {
            int i_new_line = 0;
            size_t i_in = 0, i_out = 0;
            const char * p_in = NULL;
            char * p_out = NULL;
            char * psz_new_line = NULL;

            /* iconv */
            /* UTF-8 needs at most 150% of the buffer as many as UTF-16 */
            i_new_line = i_line * 3 / 2;
            psz_new_line = malloc( i_new_line );
            if( psz_new_line == NULL )
                goto error;
            i_in = (size_t)i_line;
            i_out = (size_t)i_new_line;
            p_in = p_line;
            p_out = psz_new_line;

            if( vlc_iconv( priv->text.conv, &p_in, &i_in, &p_out, &i_out ) == (size_t)-1 )
            {
                msg_Err( s, "iconv failed" );
                msg_Dbg( s, "original: %d, in %d, out %d", i_line, (int)i_in, (int)i_out );
            }
            free( p_line );
            p_line = psz_new_line;
            i_line = (size_t)i_new_line - i_out; /* does not include \0 */
        }

        /* Remove trailing LF/CR */
        while( i_line >= 2 && ( p_line[i_line-2] == '\r' ||
            p_line[i_line-2] == '\n') ) i_line--;

        /* Make sure the \0 is there */
        p_line[i_line-1] = '\0';

        return p_line;
    }

error:
    /* We failed to read any data, probably EOF */
    free( p_line );

    /* */
    if( priv->text.conv != (vlc_iconv_t)(-1) )
    {
        vlc_iconv_close( priv->text.conv );
        priv->text.conv = (vlc_iconv_t)(-1);
    }
    return NULL;
}

static ssize_t stream_ReadRaw(stream_t *s, void *buf, size_t len)
{
    stream_priv_t *priv = (stream_priv_t *)s;
    size_t copy = 0;
    ssize_t ret = 0;

    while (len > 0)
    {
        if (vlc_killed())
        {
            ret = -1;
            break;
        }

        ret = s->pf_read(s, buf, len);
        if (ret <= 0)
            break;

        assert((size_t)ret <= len);
        if (buf != NULL)
            buf = (unsigned char *)buf + ret;
        len -= ret;
        copy += ret;
        priv->offset += ret;
    }

    return (copy > 0) ? (ssize_t)copy : ret;
}

ssize_t stream_Read(stream_t *s, void *buf, size_t len)
{
    stream_priv_t *priv = (stream_priv_t *)s;
    block_t *peek = priv->peek;
    size_t copy = 0;

    if (peek != NULL)
    {
        copy = peek->i_buffer < len ? peek->i_buffer : len;

        if (unlikely(len == 0))
            return 0;

        if (buf != NULL)
            memcpy(buf, peek->p_buffer, copy);

        peek->p_buffer += copy;
        peek->i_buffer -= copy;
        if (peek->i_buffer == 0)
        {
            block_Release(peek);
            priv->peek = NULL;
        }

        if (buf != NULL)
            buf = (unsigned char *)buf + copy;
        len -= copy;
        if (len == 0)
            return copy;
    }

    ssize_t ret = stream_ReadRaw(s, buf, len);
    return (ret >= 0) ? (ssize_t)(ret + copy)
                      : ((copy > 0) ? (ssize_t)copy : ret);
}

ssize_t stream_Peek(stream_t *s, const uint8_t **restrict bufp, size_t len)
{
    stream_priv_t *priv = (stream_priv_t *)s;
    block_t *peek = priv->peek;

    if (peek == NULL)
    {
        peek = block_Alloc(len);
        if (unlikely(peek == NULL))
            return VLC_ENOMEM;

        *bufp = peek->p_buffer;

        if (unlikely(len == 0))
        {
            priv->peek = peek;
            return 0;
        }

        ssize_t ret = stream_ReadRaw(s, peek->p_buffer, len);
        if (ret < 0)
        {
            block_Release(peek);
            return ret;
        }

        peek->i_buffer = ret;
        priv->peek = peek;
        return ret;
    }

    if (peek->i_buffer < len)
    {
        size_t avail = peek->i_buffer;

        peek = block_TryRealloc(peek, 0, len);
        if (unlikely(peek == NULL))
            return VLC_ENOMEM;

        priv->peek = peek;
        peek->i_buffer = avail;

        ssize_t ret = stream_ReadRaw(s, peek->p_buffer + avail, len - avail);
        *bufp = peek->p_buffer;
        if (ret >= 0)
            peek->i_buffer += ret;
        return peek->i_buffer;
    }

    /* Nothing to do */
    *bufp = peek->p_buffer;
    return len;
}

uint64_t stream_Tell(const stream_t *s)
{
    const stream_priv_t *priv = (const stream_priv_t *)s;
    uint64_t pos = priv->offset;

    if (priv->peek != NULL)
    {
        assert(pos >= priv->peek->i_buffer);
        pos -= priv->peek->i_buffer;
    }

    return pos;
}

int stream_Seek(stream_t *s, uint64_t offset)
{
    stream_priv_t *priv = (stream_priv_t *)s;

    block_t *peek = priv->peek;
    if (peek != NULL)
    {
        if ((priv->offset - peek->i_buffer) <= offset
         && offset <= priv->offset)
        {
            size_t fwd = offset - (priv->offset - priv->peek->i_buffer);
            if (fwd <= peek->i_buffer)
            {   /* Seeking within the peek buffer */
                peek->p_buffer += fwd;
                peek->i_buffer -= fwd;

                if (peek->i_buffer == 0)
                {
                    priv->peek = NULL;
                    block_Release(peek);
                }

                assert(stream_Tell(s) == offset);
                return VLC_SUCCESS;
            }
        }
    }
    else
    {
        if (priv->offset == offset)
        {
            assert(stream_Tell(s) == offset);
            return VLC_SUCCESS; /* Nothing to do! */
        }
    }

    if (s->pf_seek == NULL)
        return VLC_EGENERIC;

    int ret = s->pf_seek(s, offset);
    if (ret != VLC_SUCCESS)
        return ret;

    priv->offset = offset;

    if (peek != NULL)
    {
        priv->peek = NULL;
        block_Release(peek);
    }

    assert(stream_Tell(s) == offset);
    return VLC_SUCCESS;
}

static int stream_ControlInternal(stream_t *s, int cmd, ...)
{
    va_list ap;
    int ret;

    va_start(ap, cmd);
    ret = s->pf_control(s, cmd, ap);
    va_end(ap);
    return ret;
}

/**
 * Use to control the "stream_t *". Look at #stream_query_e for
 * possible "i_query" value and format arguments.  Return VLC_SUCCESS
 * if ... succeed ;) and VLC_EGENERIC if failed or unimplemented
 */
int stream_vaControl(stream_t *s, int cmd, va_list args)
{
    stream_priv_t *priv = (stream_priv_t *)s;

    switch (cmd)
    {
        case STREAM_SET_TITLE:
        case STREAM_SET_SEEKPOINT:
        {
            int ret = s->pf_control(s, cmd, args);
            if (ret != VLC_SUCCESS)
                return ret;

            priv->offset = 0;

            if (priv->peek != NULL)
            {
                block_Release(priv->peek);
                priv->peek = NULL;
            }
            return VLC_SUCCESS;
        }

        case STREAM_GET_PRIVATE_BLOCK:
        {
            block_t **b = va_arg(args, block_t **);
            bool *eof = va_arg(args, bool *);

            if (priv->peek != NULL)
            {
                *b = priv->peek;
                priv->peek = NULL;
                *eof = false;
                return VLC_SUCCESS;
            }
            return stream_ControlInternal(s, STREAM_GET_PRIVATE_BLOCK, b, eof);
        }
    }
    return s->pf_control(s, cmd, args);
}

int stream_Control( stream_t *s, int i_query, ... )
{
    va_list args;
    int     i_result;

    if( s == NULL )
        return VLC_EGENERIC;

    va_start( args, i_query );
    i_result = stream_vaControl( s, i_query, args );
    va_end( args );
    return i_result;
}

/**
 * Read data into a block.
 *
 * @param s stream to read data from
 * @param size number of bytes to read
 * @return a block of data, or NULL on error
 @ note The block size may be shorter than requested if the end-of-stream was
 * reached.
 */
block_t *stream_Block( stream_t *s, size_t size )
{
    if( unlikely(size > SSIZE_MAX) )
        return NULL;

    block_t *block = block_Alloc( size );
    if( unlikely(block == NULL) )
        return NULL;

    ssize_t val = stream_Read( s, block->p_buffer, size );
    if( val <= 0 )
    {
        block_Release( block );
        return NULL;
    }

    block->i_buffer = val;
    return block;
}

/**
 * Returns a node containing all the input_item of the directory pointer by
 * this stream. returns VLC_SUCCESS on success.
 */
int stream_ReadDir( stream_t *s, input_item_node_t *p_node )
{
    return s->pf_readdir( s, p_node );
}
