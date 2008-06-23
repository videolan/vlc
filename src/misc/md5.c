/*****************************************************************************
 * md5.c: not so strong MD5 hashing
 *****************************************************************************
 * Copyright (C) 2004-2005 the VideoLAN team
 * $Id$
 *
 * Authors: Jon Lech Johansen <jon-vl@nanocrew.net>
 *          Sam Hocevar <sam@zoy.org>
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <string.h>

#include <vlc_common.h>
#include <vlc_md5.h>

#ifdef WORDS_BIGENDIAN
/*****************************************************************************
 * Reverse: reverse byte order
 *****************************************************************************/
static inline void Reverse( uint32_t *p_buffer, int n )
{
    int i;

    for( i = 0; i < n; i++ )
    {
        p_buffer[ i ] = GetDWLE(&p_buffer[ i ]);
    }
}
#    define REVERSE( p, n ) Reverse( p, n )
#else
#    define REVERSE( p, n )
#endif

#define F1( x, y, z ) ((z) ^ ((x) & ((y) ^ (z))))
#define F2( x, y, z ) F1((z), (x), (y))
#define F3( x, y, z ) ((x) ^ (y) ^ (z))
#define F4( x, y, z ) ((y) ^ ((x) | ~(z)))

#define MD5_DO( f, w, x, y, z, data, s ) \
    ( w += f(x, y, z) + data,  w = w<<s | w>>(32-s),  w += x )

/*****************************************************************************
 * DigestMD5: update the MD5 digest with 64 bytes of data
 *****************************************************************************/
static void DigestMD5( struct md5_s *p_md5, uint32_t *p_input )
{
    uint32_t a, b, c, d;

    REVERSE( p_input, 16 );

    a = p_md5->p_digest[ 0 ];
    b = p_md5->p_digest[ 1 ];
    c = p_md5->p_digest[ 2 ];
    d = p_md5->p_digest[ 3 ];

    MD5_DO( F1, a, b, c, d, p_input[  0 ] + 0xd76aa478,  7 );
    MD5_DO( F1, d, a, b, c, p_input[  1 ] + 0xe8c7b756, 12 );
    MD5_DO( F1, c, d, a, b, p_input[  2 ] + 0x242070db, 17 );
    MD5_DO( F1, b, c, d, a, p_input[  3 ] + 0xc1bdceee, 22 );
    MD5_DO( F1, a, b, c, d, p_input[  4 ] + 0xf57c0faf,  7 );
    MD5_DO( F1, d, a, b, c, p_input[  5 ] + 0x4787c62a, 12 );
    MD5_DO( F1, c, d, a, b, p_input[  6 ] + 0xa8304613, 17 );
    MD5_DO( F1, b, c, d, a, p_input[  7 ] + 0xfd469501, 22 );
    MD5_DO( F1, a, b, c, d, p_input[  8 ] + 0x698098d8,  7 );
    MD5_DO( F1, d, a, b, c, p_input[  9 ] + 0x8b44f7af, 12 );
    MD5_DO( F1, c, d, a, b, p_input[ 10 ] + 0xffff5bb1, 17 );
    MD5_DO( F1, b, c, d, a, p_input[ 11 ] + 0x895cd7be, 22 );
    MD5_DO( F1, a, b, c, d, p_input[ 12 ] + 0x6b901122,  7 );
    MD5_DO( F1, d, a, b, c, p_input[ 13 ] + 0xfd987193, 12 );
    MD5_DO( F1, c, d, a, b, p_input[ 14 ] + 0xa679438e, 17 );
    MD5_DO( F1, b, c, d, a, p_input[ 15 ] + 0x49b40821, 22 );

    MD5_DO( F2, a, b, c, d, p_input[  1 ] + 0xf61e2562,  5 );
    MD5_DO( F2, d, a, b, c, p_input[  6 ] + 0xc040b340,  9 );
    MD5_DO( F2, c, d, a, b, p_input[ 11 ] + 0x265e5a51, 14 );
    MD5_DO( F2, b, c, d, a, p_input[  0 ] + 0xe9b6c7aa, 20 );
    MD5_DO( F2, a, b, c, d, p_input[  5 ] + 0xd62f105d,  5 );
    MD5_DO( F2, d, a, b, c, p_input[ 10 ] + 0x02441453,  9 );
    MD5_DO( F2, c, d, a, b, p_input[ 15 ] + 0xd8a1e681, 14 );
    MD5_DO( F2, b, c, d, a, p_input[  4 ] + 0xe7d3fbc8, 20 );
    MD5_DO( F2, a, b, c, d, p_input[  9 ] + 0x21e1cde6,  5 );
    MD5_DO( F2, d, a, b, c, p_input[ 14 ] + 0xc33707d6,  9 );
    MD5_DO( F2, c, d, a, b, p_input[  3 ] + 0xf4d50d87, 14 );
    MD5_DO( F2, b, c, d, a, p_input[  8 ] + 0x455a14ed, 20 );
    MD5_DO( F2, a, b, c, d, p_input[ 13 ] + 0xa9e3e905,  5 );
    MD5_DO( F2, d, a, b, c, p_input[  2 ] + 0xfcefa3f8,  9 );
    MD5_DO( F2, c, d, a, b, p_input[  7 ] + 0x676f02d9, 14 );
    MD5_DO( F2, b, c, d, a, p_input[ 12 ] + 0x8d2a4c8a, 20 );

    MD5_DO( F3, a, b, c, d, p_input[  5 ] + 0xfffa3942,  4 );
    MD5_DO( F3, d, a, b, c, p_input[  8 ] + 0x8771f681, 11 );
    MD5_DO( F3, c, d, a, b, p_input[ 11 ] + 0x6d9d6122, 16 );
    MD5_DO( F3, b, c, d, a, p_input[ 14 ] + 0xfde5380c, 23 );
    MD5_DO( F3, a, b, c, d, p_input[  1 ] + 0xa4beea44,  4 );
    MD5_DO( F3, d, a, b, c, p_input[  4 ] + 0x4bdecfa9, 11 );
    MD5_DO( F3, c, d, a, b, p_input[  7 ] + 0xf6bb4b60, 16 );
    MD5_DO( F3, b, c, d, a, p_input[ 10 ] + 0xbebfbc70, 23 );
    MD5_DO( F3, a, b, c, d, p_input[ 13 ] + 0x289b7ec6,  4 );
    MD5_DO( F3, d, a, b, c, p_input[  0 ] + 0xeaa127fa, 11 );
    MD5_DO( F3, c, d, a, b, p_input[  3 ] + 0xd4ef3085, 16 );
    MD5_DO( F3, b, c, d, a, p_input[  6 ] + 0x04881d05, 23 );
    MD5_DO( F3, a, b, c, d, p_input[  9 ] + 0xd9d4d039,  4 );
    MD5_DO( F3, d, a, b, c, p_input[ 12 ] + 0xe6db99e5, 11 );
    MD5_DO( F3, c, d, a, b, p_input[ 15 ] + 0x1fa27cf8, 16 );
    MD5_DO( F3, b, c, d, a, p_input[  2 ] + 0xc4ac5665, 23 );

    MD5_DO( F4, a, b, c, d, p_input[  0 ] + 0xf4292244,  6 );
    MD5_DO( F4, d, a, b, c, p_input[  7 ] + 0x432aff97, 10 );
    MD5_DO( F4, c, d, a, b, p_input[ 14 ] + 0xab9423a7, 15 );
    MD5_DO( F4, b, c, d, a, p_input[  5 ] + 0xfc93a039, 21 );
    MD5_DO( F4, a, b, c, d, p_input[ 12 ] + 0x655b59c3,  6 );
    MD5_DO( F4, d, a, b, c, p_input[  3 ] + 0x8f0ccc92, 10 );
    MD5_DO( F4, c, d, a, b, p_input[ 10 ] + 0xffeff47d, 15 );
    MD5_DO( F4, b, c, d, a, p_input[  1 ] + 0x85845dd1, 21 );
    MD5_DO( F4, a, b, c, d, p_input[  8 ] + 0x6fa87e4f,  6 );
    MD5_DO( F4, d, a, b, c, p_input[ 15 ] + 0xfe2ce6e0, 10 );
    MD5_DO( F4, c, d, a, b, p_input[  6 ] + 0xa3014314, 15 );
    MD5_DO( F4, b, c, d, a, p_input[ 13 ] + 0x4e0811a1, 21 );
    MD5_DO( F4, a, b, c, d, p_input[  4 ] + 0xf7537e82,  6 );
    MD5_DO( F4, d, a, b, c, p_input[ 11 ] + 0xbd3af235, 10 );
    MD5_DO( F4, c, d, a, b, p_input[  2 ] + 0x2ad7d2bb, 15 );
    MD5_DO( F4, b, c, d, a, p_input[  9 ] + 0xeb86d391, 21 );

    p_md5->p_digest[ 0 ] += a;
    p_md5->p_digest[ 1 ] += b;
    p_md5->p_digest[ 2 ] += c;
    p_md5->p_digest[ 3 ] += d;
}

/*****************************************************************************
 * InitMD5: initialise an MD5 message
 *****************************************************************************
 * The MD5 message-digest algorithm is described in RFC 1321
 *****************************************************************************/
void InitMD5( struct md5_s *p_md5 )
{
    p_md5->p_digest[ 0 ] = 0x67452301;
    p_md5->p_digest[ 1 ] = 0xefcdab89;
    p_md5->p_digest[ 2 ] = 0x98badcfe;
    p_md5->p_digest[ 3 ] = 0x10325476;

    memset( p_md5->p_data, 0, 64 );
    p_md5->i_bits = 0;
}

/*****************************************************************************
 * AddMD5: add i_len bytes to an MD5 message
 *****************************************************************************/
void AddMD5( struct md5_s *p_md5, const void *p_src, size_t i_len )
{
    unsigned int i_current; /* Current bytes in the spare buffer */
    size_t i_offset = 0;

    i_current = (p_md5->i_bits / 8) & 63;

    p_md5->i_bits += 8 * i_len;

    /* If we can complete our spare buffer to 64 bytes, do it and add the
     * resulting buffer to the MD5 message */
    if( i_len >= (64 - i_current) )
    {
        memcpy( ((uint8_t *)p_md5->p_data) + i_current, p_src,
                (64 - i_current) );
        DigestMD5( p_md5, p_md5->p_data );

        i_offset += (64 - i_current);
        i_len -= (64 - i_current);
        i_current = 0;
    }

    /* Add as many entire 64 bytes blocks as we can to the MD5 message */
    while( i_len >= 64 )
    {
        uint32_t p_tmp[ 16 ];
        memcpy( p_tmp, ((const uint8_t *)p_src) + i_offset, 64 );
        DigestMD5( p_md5, p_tmp );
        i_offset += 64;
        i_len -= 64;
    }

    /* Copy our remaining data to the message's spare buffer */
    memcpy( ((uint8_t *)p_md5->p_data) + i_current,
            ((const uint8_t *)p_src) + i_offset, i_len );
}

/*****************************************************************************
 * EndMD5: finish an MD5 message
 *****************************************************************************
 * This function adds adequate padding to the end of the message, and appends
 * the bit count so that we end at a block boundary.
 *****************************************************************************/
void EndMD5( struct md5_s *p_md5 )
{
    unsigned int i_current;

    i_current = (p_md5->i_bits / 8) & 63;

    /* Append 0x80 to our buffer. No boundary check because the temporary
     * buffer cannot be full, otherwise AddMD5 would have emptied it. */
    ((uint8_t *)p_md5->p_data)[ i_current++ ] = 0x80;

    /* If less than 8 bytes are available at the end of the block, complete
     * this 64 bytes block with zeros and add it to the message. We'll add
     * our length at the end of the next block. */
    if( i_current > 56 )
    {
        memset( ((uint8_t *)p_md5->p_data) + i_current, 0, (64 - i_current) );
        DigestMD5( p_md5, p_md5->p_data );
        i_current = 0;
    }

    /* Fill the unused space in our last block with zeroes and put the
     * message length at the end. */
    memset( ((uint8_t *)p_md5->p_data) + i_current, 0, (56 - i_current) );
    p_md5->p_data[ 14 ] = p_md5->i_bits & 0xffffffff;
    p_md5->p_data[ 15 ] = (p_md5->i_bits >> 32);
    REVERSE( &p_md5->p_data[ 14 ], 2 );

    DigestMD5( p_md5, p_md5->p_data );
}


