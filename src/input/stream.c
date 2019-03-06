/*****************************************************************************
 * stream.c
 *****************************************************************************
 * Copyright (C) 1999-2004 VLC authors and VideoLAN
 * Copyright 2008-2015 Rémi Denis-Courmont
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
#include <stdalign.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <errno.h>

#include <vlc_common.h>
#include <vlc_block.h>
#include <vlc_access.h>
#include <vlc_charset.h>
#include <vlc_interrupt.h>
#include <vlc_stream_extractor.h>

#include <libvlc.h>
#include "stream.h"
#include "mrl_helpers.h"

typedef struct stream_priv_t
{
    stream_t stream;
    void (*destroy)(stream_t *);
    block_t *block;
    block_t *peek;
    uint64_t offset;
    bool eof;

    /* UTF-16 and UTF-32 file reading */
    struct {
        vlc_iconv_t   conv;
        unsigned char char_width;
        bool          little_endian;
    } text;

    max_align_t private_data[];
} stream_priv_t;

/**
 * Allocates a VLC stream object
 */
stream_t *vlc_stream_CustomNew(vlc_object_t *parent,
                               void (*destroy)(stream_t *), size_t size,
                               const char *type_name)
{
    stream_priv_t *priv = vlc_custom_create(parent, sizeof (*priv) + size,
                                            type_name);
    if (unlikely(priv == NULL))
        return NULL;

    stream_t *s = &priv->stream;

    s->psz_url = NULL;
    s->s = NULL;
    s->pf_read = NULL;
    s->pf_block = NULL;
    s->pf_readdir = NULL;
    s->pf_seek = NULL;
    s->pf_control = NULL;
    s->p_sys = NULL;
    s->p_input_item = NULL;
    assert(destroy != NULL);
    priv->destroy = destroy;
    priv->block = NULL;
    priv->peek = NULL;
    priv->offset = 0;
    priv->eof = false;

    /* UTF16 and UTF32 text file conversion */
    priv->text.conv = (vlc_iconv_t)(-1);
    priv->text.char_width = 1;
    priv->text.little_endian = false;

    return s;
}

void *vlc_stream_Private(stream_t *stream)
{
    return ((stream_priv_t *)stream)->private_data;
}

stream_t *vlc_stream_CommonNew(vlc_object_t *parent,
                               void (*destroy)(stream_t *))
{
    return vlc_stream_CustomNew(parent, destroy, 0, "stream");
}

void stream_CommonDelete(stream_t *s)
{
    stream_priv_t *priv = (stream_priv_t *)s;

    if (priv->text.conv != (vlc_iconv_t)(-1))
        vlc_iconv_close(priv->text.conv);

    if (priv->peek != NULL)
        block_Release(priv->peek);
    if (priv->block != NULL)
        block_Release(priv->block);

    free(s->psz_url);
    vlc_object_delete(s);
}

/**
 * Destroy a stream
 */
void vlc_stream_Delete(stream_t *s)
{
    stream_priv_t *priv = (stream_priv_t *)s;

    priv->destroy(s);
    stream_CommonDelete(s);
}

stream_t *(vlc_stream_NewURL)(vlc_object_t *p_parent, const char *psz_url)
{
    if( !psz_url )
        return NULL;

    stream_t *s = stream_AccessNew( p_parent, NULL, NULL, false, psz_url );
    if( s == NULL )
        msg_Err( p_parent, "no suitable access module for `%s'", psz_url );
    else
        s = stream_FilterAutoNew(s);
    return s;
}

stream_t *(vlc_stream_NewMRL)(vlc_object_t* parent, const char* mrl )
{
    stream_t* stream = vlc_stream_NewURL( parent, mrl );

    if( stream == NULL )
        return NULL;

    char const* anchor = strchr( mrl, '#' );

    if( anchor == NULL )
        return stream;

    char const* extra;
    if( stream_extractor_AttachParsed( &stream, anchor + 1, &extra ) )
    {
        msg_Err( parent, "unable to open %s", mrl );
        vlc_stream_Delete( stream );
        return NULL;
    }

    if( extra && *extra )
        msg_Warn( parent, "ignoring extra fragment data: %s", extra );

    return stream;
}

/**
 * Read from the stream until first newline.
 * \param s Stream handle to read from
 * \return A pointer to the allocated output string. You need to free this when you are done.
 */
#define STREAM_PROBE_LINE 2048
#define STREAM_LINE_MAX (2048*100)
char *vlc_stream_ReadLine( stream_t *s )
{
    stream_priv_t *priv = (stream_priv_t *)s;
    char *p_line = NULL;
    int i_line = 0, i_read = 0;

    /* Let's fail quickly if this is a readdir access */
    if( s->pf_read == NULL && s->pf_block == NULL )
        return NULL;

    for( ;; )
    {
        char *psz_eol;
        const uint8_t *p_data;
        int i_data;
        int64_t i_pos;

        /* Probe new data */
        i_data = vlc_stream_Peek( s, &p_data, STREAM_PROBE_LINE );
        if( i_data <= 0 ) break; /* No more data */

        /* BOM detection */
        i_pos = vlc_stream_Tell( s );
        if( i_pos == 0 && i_data >= 2 )
        {
            const char *psz_encoding = NULL;

            if( unlikely(priv->text.conv != (vlc_iconv_t)-1) )
            {   /* seek back to beginning? reset */
                vlc_iconv_close( priv->text.conv );
                priv->text.conv = (vlc_iconv_t)-1;
            }

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
                priv->text.conv = vlc_iconv_open( "UTF-8", psz_encoding );
                if( unlikely(priv->text.conv == (vlc_iconv_t)-1) )
                {
                    msg_Err( s, "iconv_open failed" );
                    goto error;
                }
                priv->text.char_width = 2;
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
            i_data = vlc_stream_Read( s, &p_line[i_line], i_data );
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
        i_data = vlc_stream_Read( s, &p_line[i_line], STREAM_PROBE_LINE );
        if( i_data <= 0 ) break; /* Hmmm */
        i_line += i_data;
        i_read += i_data;

        if( i_read >= STREAM_LINE_MAX )
            goto error; /* line too long */
    }

    if( i_read > 0 )
    {
        if( priv->text.char_width > 1 )
        {
            int i_new_line = 0;
            size_t i_in = 0, i_out = 0;
            const char * p_in = NULL;
            char * p_out = NULL;
            char * psz_new_line = NULL;

            /* iconv */
            /* UTF-8 needs at most 150% of the buffer as many as UTF-16 */
            i_new_line = i_line * 3 / 2 + 1;
            psz_new_line = malloc( i_new_line );
            if( psz_new_line == NULL )
                goto error;
            i_in = (size_t)i_line;
            i_out = (size_t)i_new_line;
            p_in = p_line;
            p_out = psz_new_line;

            if( vlc_iconv( priv->text.conv, &p_in, &i_in, &p_out, &i_out ) == (size_t)-1 )
            {
                msg_Err( s, "conversion error: %s", vlc_strerror_c( errno ) );
                msg_Dbg( s, "original: %d, in %zu, out %zu", i_line, i_in, i_out );
            }
            free( p_line );
            p_line = psz_new_line;
            i_line = (size_t)i_new_line - i_out; /* does not include \0 */
        }

        /* Remove trailing LF/CR */
        while( i_line >= 1 &&
               (p_line[i_line - 1] == '\r' || p_line[i_line - 1] == '\n') )
            i_line--;

        /* Make sure the \0 is there */
        p_line[i_line] = '\0';

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

static ssize_t vlc_stream_CopyBlock(block_t **restrict pp,
                                    void *buf, size_t len)
{
    block_t *block = *pp;

    if (block == NULL)
        return -1;

    if (len > block->i_buffer)
        len = block->i_buffer;

    if (buf != NULL)
        memcpy(buf, block->p_buffer, len);

    block->p_buffer += len;
    block->i_buffer -= len;

    if (block->i_buffer == 0)
    {
        block_Release(block);
        *pp = NULL;
    }

    return likely(len > 0) ? (ssize_t)len : -1;
}

static ssize_t vlc_stream_ReadRaw(stream_t *s, void *buf, size_t len)
{
    stream_priv_t *priv = (stream_priv_t *)s;
    ssize_t ret;

    assert(len <= SSIZE_MAX);

    if (vlc_killed())
        return 0;

    if (s->pf_read != NULL)
    {
        assert(priv->block == NULL);
        if (buf == NULL)
        {
            if (unlikely(len == 0))
                return 0;

            char dummy[(len <= 256 ? len : 256)];
            ret = s->pf_read(s, dummy, sizeof (dummy));
        }
        else
            ret = s->pf_read(s, buf, len);
        return ret;
    }

    ret = vlc_stream_CopyBlock(&priv->block, buf, len);
    if (ret >= 0)
        return ret;

    if (s->pf_block != NULL)
    {
        bool eof = false;

        priv->block = s->pf_block(s, &eof);
        ret = vlc_stream_CopyBlock(&priv->block, buf, len);
        if (ret >= 0)
            return ret;
        return eof ? 0 : -1;
    }

    return 0;
}

ssize_t vlc_stream_ReadPartial(stream_t *s, void *buf, size_t len)
{
    stream_priv_t *priv = (stream_priv_t *)s;
    ssize_t ret;

    ret = vlc_stream_CopyBlock(&priv->peek, buf, len);
    if (ret >= 0)
    {
        priv->offset += ret;
        assert(ret <= (ssize_t)len);
        return ret;
    }

    ret = vlc_stream_ReadRaw(s, buf, len);
    if (ret > 0)
        priv->offset += ret;
    if (ret == 0)
        priv->eof = len != 0;
    assert(ret <= (ssize_t)len);
    return ret;
}

ssize_t vlc_stream_Read(stream_t *s, void *buf, size_t len)
{
    size_t copied = 0;

    while (len > 0)
    {
        ssize_t ret = vlc_stream_ReadPartial(s, buf, len);
        if (ret < 0)
            continue;
        if (ret == 0)
            break;

        if (buf != NULL)
            buf = (char *)buf + ret;
        assert(len >= (size_t)ret);
        len -= ret;
        copied += ret;
    }

    return copied;
}

ssize_t vlc_stream_Peek(stream_t *s, const uint8_t **restrict bufp, size_t len)
{
    stream_priv_t *priv = (stream_priv_t *)s;
    block_t *peek;

    peek = priv->peek;
    if (peek == NULL)
    {
        peek = priv->block;
        priv->peek = peek;
        priv->block = NULL;
    }

    if (peek == NULL)
    {
        peek = block_Alloc(len);
        if (unlikely(peek == NULL))
            return VLC_ENOMEM;

        peek->i_buffer = 0;
    }
    else
    if (peek->i_buffer < len)
    {
        size_t avail = peek->i_buffer;

        peek = block_TryRealloc(peek, 0, len);
        if (unlikely(peek == NULL))
            return VLC_ENOMEM;

        peek->i_buffer = avail;
    }

    priv->peek = peek;
    *bufp = peek->p_buffer;

    while (peek->i_buffer < len)
    {
        size_t avail = peek->i_buffer;
        ssize_t ret;

        ret = vlc_stream_ReadRaw(s, peek->p_buffer + avail, len - avail);
        if (ret < 0)
            continue;

        peek->i_buffer += ret;

        if (ret == 0)
            return peek->i_buffer;
    }

    return len;
}

block_t *vlc_stream_ReadBlock(stream_t *s)
{
    stream_priv_t *priv = (stream_priv_t *)s;
    block_t *block;

    if (vlc_killed())
    {
        priv->eof = true;
        return NULL;
    }

    if (priv->peek != NULL)
    {
        block = priv->peek;
        priv->peek = NULL;
    }
    else if (priv->block != NULL)
    {
        block = priv->block;
        priv->block = NULL;
    }
    else if (s->pf_block != NULL)
    {
        priv->eof = false;
        block = s->pf_block(s, &priv->eof);
    }
    else
    {
        block = block_Alloc(4096);
        if (unlikely(block == NULL))
            return NULL;

        ssize_t ret = s->pf_read(s, block->p_buffer, block->i_buffer);
        if (ret > 0)
            block->i_buffer = ret;
        else
        {
            block_Release(block);
            block = NULL;
        }

        priv->eof = !ret;
    }

    if (block != NULL)
        priv->offset += block->i_buffer;

    return block;
}

uint64_t vlc_stream_Tell(const stream_t *s)
{
    const stream_priv_t *priv = (const stream_priv_t *)s;

    return priv->offset;
}

bool vlc_stream_Eof(const stream_t *s)
{
    const stream_priv_t *priv = (const stream_priv_t *)s;

    return priv->eof;
}

int vlc_stream_Seek(stream_t *s, uint64_t offset)
{
    stream_priv_t *priv = (stream_priv_t *)s;

    priv->eof = false;

    block_t *peek = priv->peek;
    if (peek != NULL)
    {
        if (offset >= priv->offset
         && offset <= (priv->offset + peek->i_buffer))
        {   /* Seeking within the peek buffer */
            size_t fwd = offset - priv->offset;

            peek->p_buffer += fwd;
            peek->i_buffer -= fwd;
            priv->offset = offset;

            if (peek->i_buffer == 0)
            {
                priv->peek = NULL;
                block_Release(peek);
            }

            return VLC_SUCCESS;
        }
    }
    else
    {
        if (priv->offset == offset)
            return VLC_SUCCESS; /* Nothing to do! */
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

    if (priv->block != NULL)
    {
        block_Release(priv->block);
        priv->block = NULL;
    }

    return VLC_SUCCESS;
}

/**
 * Use to control the "stream_t *". Look at #stream_query_e for
 * possible "i_query" value and format arguments.  Return VLC_SUCCESS
 * if ... succeed ;) and VLC_EGENERIC if failed or unimplemented
 */
int vlc_stream_vaControl(stream_t *s, int cmd, va_list args)
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

            if (priv->block != NULL)
            {
                block_Release(priv->block);
                priv->block = NULL;
            }

            return VLC_SUCCESS;
        }
    }
    return s->pf_control(s, cmd, args);
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
block_t *vlc_stream_Block( stream_t *s, size_t size )
{
    if( unlikely(size > SSIZE_MAX) )
        return NULL;

    block_t *block = block_Alloc( size );
    if( unlikely(block == NULL) )
        return NULL;

    ssize_t val = vlc_stream_Read( s, block->p_buffer, size );
    if( val <= 0 )
    {
        block_Release( block );
        return NULL;
    }

    block->i_buffer = val;
    return block;
}

int vlc_stream_ReadDir( stream_t *s, input_item_node_t *p_node )
{
    assert(s->pf_readdir != NULL);
    return s->pf_readdir( s, p_node );
}
