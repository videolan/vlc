/*****************************************************************************
 * atsc_a65.c : ATSC A65 decoding helpers
 *****************************************************************************
 * Copyright (C) 2016 - VideoLAN Authors
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
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_charset.h>

#include "atsc_a65.h"

enum
{
    ATSC_A65_COMPRESSION_NONE             = 0x00,
    ATSC_A65_COMPRESSION_HUFFMAN_C4C5     = 0x01,
    ATSC_A65_COMPRESSION_HUFFMAN_C6C7     = 0x02,
    ATSC_A65_COMPRESSION_RESERVED_FIRST   = 0x03,
    ATSC_A65_COMPRESSION_RESERVED_LAST    = 0xAF,
    ATSC_A65_COMPRESSION_OTHER_FIRST      = 0xB0,
    ATSC_A65_COMPRESSION_OTHER_LAST       = 0xFF,
};

enum
{
    ATSC_A65_MODE_UNICODE_RANGE_START     = 0x00, /* See reserved ranges */
    ATSC_A65_MODE_UNICODE_RANGE_END       = 0x33,
    ATSC_A65_MODE_SCSU                    = 0x3E,
    ATSC_A65_MODE_UNICODE_UTF16           = 0x3F,
    ATSC_A65_MODE_TAIWAN_FIRST            = 0x40,
    ATSC_A65_MODE_TAIWAN_LAST             = 0x41,
    ATSC_A65_MODE_SOUTH_KOREA             = 0x48,
    ATSC_A65_MODE_OTHER_FIRST             = 0xE0,
    ATSC_A65_MODE_OTHER_LAST              = 0xFE,
    ATSC_A65_MODE_NOT_APPLICABLE          = 0xFF,
};

const uint8_t ATSC_A65_MODE_RESERVED_RANGES[12] = {
    /* start, end */
    0x07, 0x08,
    0x11, 0x1F,
    0x28, 0x2F,
    0x34, 0x3D,
    0x42, 0x47,
    0x49, 0xDF,
};

struct atsc_a65_handle_t
{
    char *psz_lang;
    vlc_iconv_t iconv_u16be;
};

atsc_a65_handle_t *atsc_a65_handle_New( const char *psz_lang )
{
    atsc_a65_handle_t *p_handle = malloc( sizeof(*p_handle) );
    if( p_handle )
    {
        if( psz_lang && strlen(psz_lang) > 2 )
            p_handle->psz_lang = strdup( psz_lang );
        else
            p_handle->psz_lang = NULL;

        p_handle->iconv_u16be = NULL;
    }
    return p_handle;
}

void atsc_a65_handle_Release( atsc_a65_handle_t *p_handle )
{
    if( p_handle->iconv_u16be )
        vlc_iconv_close( p_handle->iconv_u16be );
    free( p_handle->psz_lang );
    free( p_handle );
}

static char *enlarge_to16( const uint8_t *p_src, size_t i_src, uint8_t i_prefix )
{
    if( i_src == 0 )
        return NULL;

    char *psz_new_allocated = malloc( i_src * 2 + 1 );
    char *psz_new = psz_new_allocated;

    if( psz_new )
    {
        memset( psz_new, i_prefix, i_src * 2 );
        psz_new[ i_src * 2 ] = 0;
        while( i_src-- )
        {
            psz_new[1] = p_src[0];
            p_src++;
            psz_new += 2;
        }
    }
    return psz_new_allocated;
}

static bool convert_encoding_set( atsc_a65_handle_t *p_handle,
                                  const uint8_t *p_src, size_t i_src,
                                  char **ppsz_merg, size_t *pi_mergmin1,
                                  uint8_t i_mode )
{
    char *psz_dest = *ppsz_merg;
    size_t i_mergmin1 = *pi_mergmin1;
    bool b_ret = true;

    if( i_src == 0 )
        return NULL;

    /* First exclude reserved ranges */
    for( unsigned i=0; i<12; i+=2 )
    {
        if( i_mode >= ATSC_A65_MODE_RESERVED_RANGES[i]   &&
            i_mode <= ATSC_A65_MODE_RESERVED_RANGES[i+1] )
            return false;
    }

    if( i_mode <= ATSC_A65_MODE_UNICODE_RANGE_END ) /* 8 range prefix + 8 */
    {
        if( !p_handle->iconv_u16be )
        {
            if ( !(p_handle->iconv_u16be = vlc_iconv_open("UTF-8", "UTF-16BE")) )
                return false;
        }
        else if ( VLC_ICONV_ERR == vlc_iconv( p_handle->iconv_u16be, NULL, NULL, NULL, NULL ) ) /* reset */
        {
            return false;
        }

        char *psz16 = enlarge_to16( p_src, i_src, i_mode ); /* Maybe we can skip and feed iconv 2 by 2 */
        if( psz16 )
        {
            char *psz_realloc = realloc( psz_dest, i_mergmin1 + (4 * i_src) + 1 );
            if( psz_realloc )
            {
                const char *p_inbuf = psz16;
                char *p_outbuf = &psz_realloc[i_mergmin1];
                const size_t i_outbuf_size = i_src * 4;
                size_t i_inbuf_remain = i_src * 2;
                size_t i_outbuf_remain = i_outbuf_size;
                b_ret = ( VLC_ICONV_ERR != vlc_iconv( p_handle->iconv_u16be, &p_inbuf, &i_inbuf_remain,
                                                                            &p_outbuf, &i_outbuf_remain ) );
                psz_dest = psz_realloc;
                i_mergmin1 += (i_outbuf_size - i_outbuf_remain);
                *p_outbuf = '\0';
            }
            free( psz16 );
        }
        else return false;
    }
    else
    {
        /* Unsupported encodings */
        return false;
    }

    *ppsz_merg = psz_dest;
    *pi_mergmin1 = i_mergmin1;
    return b_ret;
}

#define BUF_ADVANCE(n) p_buffer += n; i_buffer -= n;

char * atsc_a65_Decode_multiple_string( atsc_a65_handle_t *p_handle, const uint8_t *p_buffer, size_t i_buffer )
{
    char *psz_res = NULL;
    size_t i_resmin1 = 0;

    if( i_buffer < 1 )
        return NULL;

    uint8_t i_nb = p_buffer[0];
    BUF_ADVANCE(1);

    for( ; i_nb > 0; i_nb-- )
    {
        if( i_buffer < 4 )
            goto error;

        bool b_skip = ( p_handle->psz_lang && memcmp(p_buffer, p_handle->psz_lang, 3) );
        BUF_ADVANCE(3);

        uint8_t i_seg = p_buffer[0];
        BUF_ADVANCE(1);
        for( ; i_seg > 0; i_seg-- )
        {
            if( i_buffer < 3 )
                goto error;

            const uint8_t i_compression = p_buffer[0];
            const uint8_t i_mode = p_buffer[1];
            const uint8_t i_bytes = p_buffer[2];
            BUF_ADVANCE(3);

            if( i_buffer < i_bytes )
                goto error;

            if( i_compression != ATSC_A65_COMPRESSION_NONE ) // TBD
            {
                b_skip = true;
            }

            if( !b_skip )
            {
                (void) convert_encoding_set( p_handle, p_buffer, i_bytes,
                                             &psz_res, &i_resmin1, i_mode );
            }

            BUF_ADVANCE(i_bytes);
        }
    }

    return psz_res;

error:
    free( psz_res );
    return NULL;
}

#undef BUF_ADVANCE

char * atsc_a65_Decode_simple_UTF16_string( atsc_a65_handle_t *p_handle, const uint8_t *p_buffer, size_t i_buffer )
{
    if( i_buffer < 1 )
        return NULL;

    if( !p_handle->iconv_u16be )
    {
        if ( !(p_handle->iconv_u16be = vlc_iconv_open("UTF-8", "UTF-16BE")) )
            return NULL;
    }
    else if ( VLC_ICONV_ERR == vlc_iconv( p_handle->iconv_u16be, NULL, NULL, NULL, NULL ) ) /* reset */
    {
        return NULL;
    }

    const size_t i_target_buffer = i_buffer * 3 / 2;
    size_t i_target_remaining = i_target_buffer;
    const char *psz_toconvert = (const char *) p_buffer;
    char *psz_converted_end;
    char *psz_converted = psz_converted_end = malloc( i_target_buffer );

    if( unlikely(!psz_converted) )
        return NULL;

    if( VLC_ICONV_ERR == vlc_iconv( p_handle->iconv_u16be, &psz_toconvert, &i_buffer,
                                                           &psz_converted_end, &i_target_remaining ) )
    {
        free( psz_converted );
        psz_converted = NULL;
    }
    else
        psz_converted[ i_target_buffer - i_target_remaining - 1 ] = 0;

    return psz_converted;
}
