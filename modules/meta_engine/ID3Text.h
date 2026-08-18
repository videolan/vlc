/*****************************************************************************
 * ID3Text.h : ID3v2 Text Helper
 *****************************************************************************
 * Copyright (C) 2016 VLC authors and VideoLAN
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
#ifndef ID3TEXT_H
#define ID3TEXT_H

#include <vlc_charset.h>

/* Text encoding byte of the ID3v2 frames */
enum
{
    ID3_ENCODING_ISO_8859_1 = 0x00, // ISO-8859-1
    ID3_ENCODING_UTF16      = 0x01, // UTF-16 with BOM
    ID3_ENCODING_UTF16BE    = 0x02, // UTF-16BE without BOM, ID3v2.4
    ID3_ENCODING_UTF8       = 0x03, // UTF-8, ID3v2.4
};

static const char * ID3TextConv( const uint8_t *p_buf, size_t i_buf,
                                 uint8_t i_charset, char **ppsz_allocated )
{
    char *p_alloc = NULL;
    const char *psz = p_alloc;
    if( i_buf > 0 && i_charset <= ID3_ENCODING_UTF8 )
    {
        switch( i_charset )
        {
            case ID3_ENCODING_ISO_8859_1:
                psz = p_alloc = FromCharset( "ISO_8859-1", p_buf, i_buf );
                break;
            case ID3_ENCODING_UTF16:
                psz = p_alloc = FromCharset( "UTF-16", p_buf, i_buf );
                break;
            case ID3_ENCODING_UTF16BE:
                psz = p_alloc = FromCharset( "UTF-16BE", p_buf, i_buf );
                break;
            default:
            case ID3_ENCODING_UTF8:
                if( p_buf[ i_buf - 1 ] != 0x00 )
                {
                    psz = p_alloc = (char *) malloc( i_buf + 1 );
                    if( p_alloc )
                    {
                        memcpy( p_alloc, p_buf, i_buf );
                        p_alloc[i_buf] = '\0';
                    }
                }
                else
                {
                    psz = (const char *) p_buf;
                }
                break;
        }
    }
    *ppsz_allocated = p_alloc;
    return psz;
}

static inline const char * ID3TextConvert( const uint8_t *p_buf, size_t i_buf,
                                           char **ppsz_allocated )
{
    if( i_buf == 0 )
    {
        *ppsz_allocated = NULL;
        return NULL;
    }
    return ID3TextConv( &p_buf[1], i_buf - 1, p_buf[0], ppsz_allocated );
}

/* Size in bytes of the terminator of a string: UTF-16 strings end with a
 * null code unit */
static inline size_t ID3TextTerminatorSize( uint8_t i_charset )
{
    return ( i_charset == ID3_ENCODING_UTF16 ||
             i_charset == ID3_ENCODING_UTF16BE ) ? 2 : 1;
}

/* Length in bytes of the first string of the buffer, terminator excluded,
 * or -1 if it is not terminated */
static inline ssize_t ID3TextLength( const uint8_t *p_buf, size_t i_buf,
                                     uint8_t i_charset )
{
    if( ID3TextTerminatorSize( i_charset ) == 2 )
    {
        for( size_t i = 0; i + 1 < i_buf; i += 2 )
            if( GetWBE( &p_buf[i] ) == 0 )
                return i;
        return -1;
    }

    const size_t i_len = strnlen( (const char *) p_buf, i_buf );
    return i_len < i_buf ? (ssize_t) i_len : -1;
}

#endif
