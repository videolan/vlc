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

#include <vlc_common.h>
#include <vlc_memory.h>
#include <vlc_access.h>
#include <vlc_charset.h>

#include <libvlc.h>
#include "stream.h"

typedef struct stream_priv_t
{
    stream_t stream;

    /* UTF-16 and UTF-32 file reading */
    struct {
        vlc_iconv_t   conv;
        unsigned char char_width;
        bool          little_endian;
    } text;
} stream_priv_t;

/**
 * Allocates a VLC stream object
 */
stream_t *stream_CommonNew(vlc_object_t *parent)
{
    stream_priv_t *priv = vlc_custom_create(parent, sizeof (*priv), "stream");
    if (unlikely(priv == NULL))
        return NULL;

    stream_t *s = &priv->stream;

    s->psz_access = NULL;
    s->psz_path = NULL;

    /* UTF16 and UTF32 text file conversion */
    priv->text.conv = (vlc_iconv_t)(-1);
    priv->text.char_width = 1;
    priv->text.little_endian = false;

    return s;
}

/**
 * Destroys a VLC stream object
 */
void stream_CommonDelete( stream_t *s )
{
    stream_priv_t *priv = (stream_priv_t *)s;

    if (priv->text.conv != (vlc_iconv_t)(-1))
        vlc_iconv_close(priv->text.conv);

    free( s->psz_access );
    free( s->psz_path );
    vlc_object_release( s );
}

#undef stream_UrlNew
/****************************************************************************
 * stream_UrlNew: create a stream from a access
 ****************************************************************************/
stream_t *stream_UrlNew( vlc_object_t *p_parent, const char *psz_url )
{
    if( !psz_url )
        return NULL;

    access_t *p_access = vlc_access_NewMRL( p_parent, psz_url );
    if( p_access == NULL )
    {
        msg_Err( p_parent, "no suitable access module for `%s'", psz_url );
        return NULL;
    }

    return stream_AccessNew( p_access );
}

/**
 * Read from the stream untill first newline.
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

/**
 * Try to read "i_read" bytes into a buffer pointed by "p_read".  If
 * "p_read" is NULL then data are skipped instead of read.
 * \return The real number of bytes read/skip. If this value is less
 * than i_read that means that it's the end of the stream.
 * \note stream_Read increments the stream position, and when p_read is NULL,
 * this is its only task.
 */
int stream_Read( stream_t *s, void *p_read, int i_read )
{
    return s->pf_read( s, p_read, i_read );
}

/**
 * Store in pp_peek a pointer to the next "i_peek" bytes in the stream
 * \return The real number of valid bytes. If it's less
 * or equal to 0, *pp_peek is invalid.
 * \note pp_peek is a pointer to internal buffer and it will be invalid as
 * soons as other stream_* functions are called.
 * \note Contrary to stream_Read, stream_Peek doesn't modify the stream
 * position, and doesn't necessarily involve copying of data. It's mainly
 * used by the modules to quickly probe the (head of the) stream.
 * \note Due to input limitation, the return value could be less than i_peek
 * without meaning the end of the stream (but only when you have i_peek >=
 * p_input->i_bufsize)
 */
int stream_Peek( stream_t *s, const uint8_t **pp_peek, int i_peek )
{
    return s->pf_peek( s, pp_peek, i_peek );
}

/**
 * Use to control the "stream_t *". Look at #stream_query_e for
 * possible "i_query" value and format arguments.  Return VLC_SUCCESS
 * if ... succeed ;) and VLC_EGENERIC if failed or unimplemented
 */
int stream_vaControl( stream_t *s, int i_query, va_list args )
{
    return s->pf_control( s, i_query, args );
}

/**
 * Destroy a stream
 */
void stream_Delete( stream_t *s )
{
    s->pf_destroy( s );
}

int stream_Control( stream_t *s, int i_query, ... )
{
    va_list args;
    int     i_result;

    if( s == NULL )
        return VLC_EGENERIC;

    va_start( args, i_query );
    i_result = s->pf_control( s, i_query, args );
    va_end( args );
    return i_result;
}

/**
 * Read "i_size" bytes and store them in a block_t.
 * It always read i_size bytes unless you are at the end of the stream
 * where it return what is available.
 */
block_t *stream_Block( stream_t *s, int i_size )
{
    if( i_size <= 0 ) return NULL;

    /* emulate block read */
    block_t *p_bk = block_Alloc( i_size );
    if( p_bk )
    {
        int i_read = stream_Read( s, p_bk->p_buffer, i_size );
        if( i_read > 0 )
        {
            p_bk->i_buffer = i_read;
            return p_bk;
        }
        block_Release( p_bk );
    }
    return NULL;
}

/**
 * Read the remaining of the data if there is less than i_max_size bytes, otherwise
 * return NULL.
 *
 * The stream position is unknown after the call.
 */
block_t *stream_BlockRemaining( stream_t *s, int i_max_size )
{
    int     i_allocate = __MIN(1000000, i_max_size);
    int64_t i_size = stream_Size( s );
    if( i_size > 0 )
    {
        int64_t i_position = stream_Tell( s );
        if( i_position + i_max_size < i_size )
        {
            msg_Err( s, "Remaining stream size is greater than %d bytes",
                     i_max_size );
            return NULL;
        }
        i_allocate = i_size - i_position;
    }
    if( i_allocate <= 0 )
        return NULL;

    block_t *p_block = block_Alloc( i_allocate );
    int i_index = 0;
    while( p_block )
    {
        int i_read = stream_Read( s, &p_block->p_buffer[i_index],
                                     p_block->i_buffer - i_index);
        if( i_read <= 0 )
            break;
        i_index += i_read;
        i_max_size -= i_read;
        if( i_max_size <= 0 )
            break;
        p_block = block_Realloc( p_block, 0, p_block->i_buffer +
                                             __MIN(1000000, i_max_size) );
    }
    if( p_block )
        p_block->i_buffer = i_index;
    return p_block;
}

/**
 * Read the next input_item_t from the directory stream. It returns the next
 * input item on success or NULL in case of error or end of stream. The item
 * must be released with input_item_Release.
 */
input_item_t *stream_ReadDir( stream_t *s )
{
    return s->pf_readdir( s );
}
