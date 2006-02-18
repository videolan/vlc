/*****************************************************************************
 * drms.c: DRMS
 *****************************************************************************
 * Copyright (C) 2004 the VideoLAN team
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

#include <stdlib.h>                                      /* malloc(), free() */

#ifdef WIN32
#   include <io.h>
#else
#   include <stdio.h>
#endif

#ifdef __VLC__
#   include <vlc/vlc.h>
#   include <vlc_md5.h>
#   include "libmp4.h"
#   include "charset.h"
#else
#   include "drmsvl.h"
#endif

#ifdef HAVE_ERRNO_H
#   include <errno.h>
#endif

#ifdef WIN32
#   if !defined( UNDER_CE )
#       include <direct.h>
#   endif
#   include <tchar.h>
#   include <shlobj.h>
#   include <windows.h>
#endif

#ifdef HAVE_SYS_STAT_H
#   include <sys/stat.h>
#endif
#ifdef HAVE_SYS_TYPES_H
#   include <sys/types.h>
#endif

/* In Solaris (and perhaps others) PATH_MAX is in limits.h. */
#ifdef HAVE_LIMITS_H
#   include <limits.h>
#endif

#ifdef __APPLE__
#   include <mach/mach.h>
#   include <IOKit/IOKitLib.h>
#   include <CoreFoundation/CFNumber.h>
#endif

#ifdef HAVE_SYSFS_LIBSYSFS_H
#   include <sysfs/libsysfs.h>
#endif

#include "drms.h"
#include "drmstables.h"

#if !defined( UNDER_CE )
/*****************************************************************************
 * aes_s: AES keys structure
 *****************************************************************************
 * This structure stores a set of keys usable for encryption and decryption
 * with the AES/Rijndael algorithm.
 *****************************************************************************/
struct aes_s
{
    uint32_t pp_enc_keys[ AES_KEY_COUNT + 1 ][ 4 ];
    uint32_t pp_dec_keys[ AES_KEY_COUNT + 1 ][ 4 ];
};

#ifdef __VLC__
# define Digest DigestMD5
#else
/*****************************************************************************
 * md5_s: MD5 message structure
 *****************************************************************************
 * This structure stores the static information needed to compute an MD5
 * hash. It has an extra data buffer to allow non-aligned writes.
 *****************************************************************************/
struct md5_s
{
    uint64_t i_bits;      /* Total written bits */
    uint32_t p_digest[4]; /* The MD5 digest */
    uint32_t p_data[16];  /* Buffer to cache non-aligned writes */
};
#endif

/*****************************************************************************
 * shuffle_s: shuffle structure
 *****************************************************************************
 * This structure stores the static information needed to shuffle data using
 * a custom algorithm.
 *****************************************************************************/
struct shuffle_s
{
    uint32_t i_version;
    uint32_t p_commands[ 20 ];
    uint32_t p_bordel[ 16 ];
};

#define SWAP( a, b ) { (a) ^= (b); (b) ^= (a); (a) ^= (b); }

/*****************************************************************************
 * drms_s: DRMS structure
 *****************************************************************************
 * This structure stores the static information needed to decrypt DRMS data.
 *****************************************************************************/
struct drms_s
{
    uint32_t i_user;
    uint32_t i_key;
    uint8_t  p_iviv[ 16 ];
    uint8_t *p_name;

    uint32_t p_key[ 4 ];
    struct aes_s aes;

    char     psz_homedir[ PATH_MAX ];
};

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static void InitAES       ( struct aes_s *, uint32_t * );
static void DecryptAES    ( struct aes_s *, uint32_t *, const uint32_t * );

#ifndef __VLC__
static void InitMD5       ( struct md5_s * );
static void AddMD5        ( struct md5_s *, const uint8_t *, uint32_t );
static void EndMD5        ( struct md5_s * );
static void Digest        ( struct md5_s *, uint32_t * );
#endif

static void InitShuffle   ( struct shuffle_s *, uint32_t *, uint32_t );
static void DoShuffle     ( struct shuffle_s *, uint32_t *, uint32_t );

static uint32_t FirstPass ( uint32_t * );
static void SecondPass    ( uint32_t *, uint32_t );
static void ThirdPass     ( uint32_t * );
static void FourthPass    ( uint32_t * );
static void TinyShuffle1  ( uint32_t * );
static void TinyShuffle2  ( uint32_t * );
static void TinyShuffle3  ( uint32_t * );
static void TinyShuffle4  ( uint32_t * );
static void TinyShuffle5  ( uint32_t * );
static void TinyShuffle6  ( uint32_t * );
static void TinyShuffle7  ( uint32_t * );
static void TinyShuffle8  ( uint32_t * );
static void DoExtShuffle  ( uint32_t * );

static int GetSystemKey   ( uint32_t *, vlc_bool_t );
static int WriteUserKey   ( void *, uint32_t * );
static int ReadUserKey    ( void *, uint32_t * );
static int GetUserKey     ( void *, uint32_t * );

static int GetSCIData     ( char *, uint32_t **, uint32_t * );
static int HashSystemInfo ( uint32_t * );
static int GetiPodID      ( int64_t * );

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

/*****************************************************************************
 * BlockXOR: XOR two 128 bit blocks
 *****************************************************************************/
static inline void BlockXOR( uint32_t *p_dest, uint32_t *p_s1, uint32_t *p_s2 )
{
    int i;

    for( i = 0; i < 4; i++ )
    {
        p_dest[ i ] = p_s1[ i ] ^ p_s2[ i ];
    }
}

/*****************************************************************************
 * drms_alloc: allocate a DRMS structure
 *****************************************************************************/
void *drms_alloc( char *psz_homedir )
{
    struct drms_s *p_drms;

    p_drms = malloc( sizeof(struct drms_s) );

    if( p_drms == NULL )
    {
        return NULL;
    }

    memset( p_drms, 0, sizeof(struct drms_s) );

    strncpy( p_drms->psz_homedir, psz_homedir, PATH_MAX );
    p_drms->psz_homedir[ PATH_MAX - 1 ] = '\0';

    return (void *)p_drms;
}

/*****************************************************************************
 * drms_free: free a previously allocated DRMS structure
 *****************************************************************************/
void drms_free( void *_p_drms )
{
    struct drms_s *p_drms = (struct drms_s *)_p_drms;

    if( p_drms->p_name != NULL )
    {
        free( (void *)p_drms->p_name );
    }

    free( p_drms );
}

/*****************************************************************************
 * drms_decrypt: unscramble a chunk of data
 *****************************************************************************/
void drms_decrypt( void *_p_drms, uint32_t *p_buffer, uint32_t i_bytes )
{
    struct drms_s *p_drms = (struct drms_s *)_p_drms;
    uint32_t p_key[ 4 ];
    unsigned int i_blocks;

    /* AES is a block cypher, round down the byte count */
    i_blocks = i_bytes / 16;
    i_bytes = i_blocks * 16;

    /* Initialise the key */
    memcpy( p_key, p_drms->p_key, 16 );

    /* Unscramble */
    while( i_blocks-- )
    {
        uint32_t p_tmp[ 4 ];

        REVERSE( p_buffer, 4 );
        DecryptAES( &p_drms->aes, p_tmp, p_buffer );
        BlockXOR( p_tmp, p_key, p_tmp );

        /* Use the previous scrambled data as the key for next block */
        memcpy( p_key, p_buffer, 16 );

        /* Copy unscrambled data back to the buffer */
        memcpy( p_buffer, p_tmp, 16 );
        REVERSE( p_buffer, 4 );

        p_buffer += 4;
    }
}

/*****************************************************************************
 * drms_init: initialise a DRMS structure
 *****************************************************************************
 * Return values:
 *  0: success
 * -1: unimplemented
 * -2: invalid argument
 * -3: could not get system key
 * -4: could not get SCI data
 * -5: no user key found in SCI data
 * -6: invalid user key
 *****************************************************************************/
int drms_init( void *_p_drms, uint32_t i_type,
               uint8_t *p_info, uint32_t i_len )
{
    struct drms_s *p_drms = (struct drms_s *)_p_drms;
    int i_ret = 0;

    switch( i_type )
    {
        case FOURCC_user:
            if( i_len < sizeof(p_drms->i_user) )
            {
                i_ret = -2;
                break;
            }

            p_drms->i_user = U32_AT( p_info );
            break;

        case FOURCC_key:
            if( i_len < sizeof(p_drms->i_key) )
            {
                i_ret = -2;
                break;
            }

            p_drms->i_key = U32_AT( p_info );
            break;

        case FOURCC_iviv:
            if( i_len < sizeof(p_drms->p_key) )
            {
                i_ret = -2;
                break;
            }

            memcpy( p_drms->p_iviv, p_info, 16 );
            break;

        case FOURCC_name:
            p_drms->p_name = strdup( p_info );

            if( p_drms->p_name == NULL )
            {
                i_ret = -2;
            }
            break;

        case FOURCC_priv:
        {
            uint32_t p_priv[ 64 ];
            struct md5_s md5;

            if( i_len < 64 )
            {
                i_ret = -2;
                break;
            }

            InitMD5( &md5 );
            AddMD5( &md5, p_drms->p_name, strlen( p_drms->p_name ) );
            AddMD5( &md5, p_drms->p_iviv, 16 );
            EndMD5( &md5 );

            if( p_drms->i_user == 0 && p_drms->i_key == 0 )
            {
                static char const p_secret[] = "tr1-th3n.y00_by3";
                memcpy( p_drms->p_key, p_secret, 16 );
                REVERSE( p_drms->p_key, 4 );
            }
            else
            {
                i_ret = GetUserKey( p_drms, p_drms->p_key );
                if( i_ret )
                {
                    break;
                }
            }

            InitAES( &p_drms->aes, p_drms->p_key );

            memcpy( p_priv, p_info, 64 );
            memcpy( p_drms->p_key, md5.p_digest, 16 );
            drms_decrypt( p_drms, p_priv, 64 );
            REVERSE( p_priv, 64 );

            if( p_priv[ 0 ] != 0x6e757469 ) /* itun */
            {
                i_ret = -6;
                break;
            }

            InitAES( &p_drms->aes, p_priv + 6 );
            memcpy( p_drms->p_key, p_priv + 12, 16 );

            free( (void *)p_drms->p_name );
            p_drms->p_name = NULL;
        }
        break;
    }

    return i_ret;
}

/* The following functions are local */

/*****************************************************************************
 * InitAES: initialise AES/Rijndael encryption/decryption tables
 *****************************************************************************
 * The Advanced Encryption Standard (AES) is described in RFC 3268
 *****************************************************************************/
static void InitAES( struct aes_s *p_aes, uint32_t *p_key )
{
    unsigned int i, t;
    uint32_t i_key, i_seed;

    memset( p_aes->pp_enc_keys[1], 0, 16 );
    memcpy( p_aes->pp_enc_keys[0], p_key, 16 );

    /* Generate the key tables */
    i_seed = p_aes->pp_enc_keys[ 0 ][ 3 ];

    for( i_key = 0; i_key < AES_KEY_COUNT; i_key++ )
    {
        uint32_t j;

        i_seed = AES_ROR( i_seed, 8 );

        j = p_aes_table[ i_key ];

        j ^= p_aes_encrypt[ (i_seed >> 24) & 0xff ]
              ^ AES_ROR( p_aes_encrypt[ (i_seed >> 16) & 0xff ], 8 )
              ^ AES_ROR( p_aes_encrypt[ (i_seed >> 8) & 0xff ], 16 )
              ^ AES_ROR( p_aes_encrypt[ i_seed & 0xff ], 24 );

        j ^= p_aes->pp_enc_keys[ i_key ][ 0 ];
        p_aes->pp_enc_keys[ i_key + 1 ][ 0 ] = j;
        j ^= p_aes->pp_enc_keys[ i_key ][ 1 ];
        p_aes->pp_enc_keys[ i_key + 1 ][ 1 ] = j;
        j ^= p_aes->pp_enc_keys[ i_key ][ 2 ];
        p_aes->pp_enc_keys[ i_key + 1 ][ 2 ] = j;
        j ^= p_aes->pp_enc_keys[ i_key ][ 3 ];
        p_aes->pp_enc_keys[ i_key + 1 ][ 3 ] = j;

        i_seed = j;
    }

    memcpy( p_aes->pp_dec_keys[ 0 ],
            p_aes->pp_enc_keys[ 0 ], 16 );

    for( i = 1; i < AES_KEY_COUNT; i++ )
    {
        for( t = 0; t < 4; t++ )
        {
            uint32_t j, k, l, m, n;

            j = p_aes->pp_enc_keys[ i ][ t ];

            k = (((j >> 7) & 0x01010101) * 27) ^ ((j & 0xff7f7f7f) << 1);
            l = (((k >> 7) & 0x01010101) * 27) ^ ((k & 0xff7f7f7f) << 1);
            m = (((l >> 7) & 0x01010101) * 27) ^ ((l & 0xff7f7f7f) << 1);

            j ^= m;

            n = AES_ROR( l ^ j, 16 ) ^ AES_ROR( k ^ j, 8 ) ^ AES_ROR( j, 24 );

            p_aes->pp_dec_keys[ i ][ t ] = k ^ l ^ m ^ n;
        }
    }
}

/*****************************************************************************
 * DecryptAES: decrypt an AES/Rijndael 128 bit block
 *****************************************************************************/
static void DecryptAES( struct aes_s *p_aes,
                        uint32_t *p_dest, const uint32_t *p_src )
{
    uint32_t p_wtxt[ 4 ]; /* Working cyphertext */
    uint32_t p_tmp[ 4 ];
    unsigned int i_round, t;

    for( t = 0; t < 4; t++ )
    {
        /* FIXME: are there any endianness issues here? */
        p_wtxt[ t ] = p_src[ t ] ^ p_aes->pp_enc_keys[ AES_KEY_COUNT ][ t ];
    }

    /* Rounds 0 - 8 */
    for( i_round = 0; i_round < (AES_KEY_COUNT - 1); i_round++ )
    {
        for( t = 0; t < 4; t++ )
        {
            p_tmp[ t ] = AES_XOR_ROR( p_aes_itable, p_wtxt );
        }

        for( t = 0; t < 4; t++ )
        {
            p_wtxt[ t ] = p_tmp[ t ]
                    ^ p_aes->pp_dec_keys[ (AES_KEY_COUNT - 1) - i_round ][ t ];
        }
    }

    /* Final round (9) */
    for( t = 0; t < 4; t++ )
    {
        p_dest[ t ] = AES_XOR_ROR( p_aes_decrypt, p_wtxt );
        p_dest[ t ] ^= p_aes->pp_dec_keys[ 0 ][ t ];
    }
}

#ifndef __VLC__
/*****************************************************************************
 * InitMD5: initialise an MD5 message
 *****************************************************************************
 * The MD5 message-digest algorithm is described in RFC 1321
 *****************************************************************************/
static void InitMD5( struct md5_s *p_md5 )
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
static void AddMD5( struct md5_s *p_md5, const uint8_t *p_src, uint32_t i_len )
{
    unsigned int i_current; /* Current bytes in the spare buffer */
    unsigned int i_offset = 0;

    i_current = (p_md5->i_bits / 8) & 63;

    p_md5->i_bits += 8 * i_len;

    /* If we can complete our spare buffer to 64 bytes, do it and add the
     * resulting buffer to the MD5 message */
    if( i_len >= (64 - i_current) )
    {
        memcpy( ((uint8_t *)p_md5->p_data) + i_current, p_src,
                (64 - i_current) );
        Digest( p_md5, p_md5->p_data );

        i_offset += (64 - i_current);
        i_len -= (64 - i_current);
        i_current = 0;
    }

    /* Add as many entire 64 bytes blocks as we can to the MD5 message */
    while( i_len >= 64 )
    {
        uint32_t p_tmp[ 16 ];
        memcpy( p_tmp, p_src + i_offset, 64 );
        Digest( p_md5, p_tmp );
        i_offset += 64;
        i_len -= 64;
    }

    /* Copy our remaining data to the message's spare buffer */
    memcpy( ((uint8_t *)p_md5->p_data) + i_current, p_src + i_offset, i_len );
}

/*****************************************************************************
 * EndMD5: finish an MD5 message
 *****************************************************************************
 * This function adds adequate padding to the end of the message, and appends
 * the bit count so that we end at a block boundary.
 *****************************************************************************/
static void EndMD5( struct md5_s *p_md5 )
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
        Digest( p_md5, p_md5->p_data );
        i_current = 0;
    }

    /* Fill the unused space in our last block with zeroes and put the
     * message length at the end. */
    memset( ((uint8_t *)p_md5->p_data) + i_current, 0, (56 - i_current) );
    p_md5->p_data[ 14 ] = p_md5->i_bits & 0xffffffff;
    p_md5->p_data[ 15 ] = (p_md5->i_bits >> 32);
    REVERSE( &p_md5->p_data[ 14 ], 2 );

    Digest( p_md5, p_md5->p_data );
}

#define F1( x, y, z ) ((z) ^ ((x) & ((y) ^ (z))))
#define F2( x, y, z ) F1((z), (x), (y))
#define F3( x, y, z ) ((x) ^ (y) ^ (z))
#define F4( x, y, z ) ((y) ^ ((x) | ~(z)))

#define MD5_DO( f, w, x, y, z, data, s ) \
    ( w += f(x, y, z) + data,  w = w<<s | w>>(32-s),  w += x )

/*****************************************************************************
 * Digest: update the MD5 digest with 64 bytes of data
 *****************************************************************************/
static void Digest( struct md5_s *p_md5, uint32_t *p_input )
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
#endif

/*****************************************************************************
 * InitShuffle: initialise a shuffle structure
 *****************************************************************************
 * This function initialises tables in the p_shuffle structure that will be
 * used later by DoShuffle. The only external parameter is p_sys_key.
 *****************************************************************************/
static void InitShuffle( struct shuffle_s *p_shuffle, uint32_t *p_sys_key,
                         uint32_t i_version )
{
    char p_secret1[] = "Tv!*";
    static char const p_secret2[] = "____v8rhvsaAvOKM____FfUH%798=[;."
                                    "____f8677680a634____ba87fnOIf)(*";
    unsigned int i;

    p_shuffle->i_version = i_version;

    /* Fill p_commands using the key and a secret seed */
    for( i = 0; i < 20; i++ )
    {
        struct md5_s md5;
        int32_t i_hash;

        InitMD5( &md5 );
        AddMD5( &md5, (uint8_t *)p_sys_key, 16 );
        AddMD5( &md5, (uint8_t *)p_secret1, 4 );
        EndMD5( &md5 );

        p_secret1[ 3 ]++;

        REVERSE( md5.p_digest, 1 );
        i_hash = ((int32_t)U32_AT(md5.p_digest)) % 1024;

        p_shuffle->p_commands[ i ] = i_hash < 0 ? i_hash * -1 : i_hash;
    }

    /* Fill p_bordel with completely meaningless initial values. */
    memcpy( p_shuffle->p_bordel, p_secret2, 64 );
    for( i = 0; i < 4; i++ )
    {
        p_shuffle->p_bordel[ 4 * i ] = U32_AT(p_sys_key + i);
        REVERSE( p_shuffle->p_bordel + 4 * i + 1, 3 );
    }
}

/*****************************************************************************
 * DoShuffle: shuffle buffer
 *****************************************************************************
 * This is so ugly and uses so many MD5 checksums that it is most certainly
 * one-way, though why it needs to be so complicated is beyond me.
 *****************************************************************************/
static void DoShuffle( struct shuffle_s *p_shuffle,
                       uint32_t *p_buffer, uint32_t i_size )
{
    struct md5_s md5;
    uint32_t p_big_bordel[ 16 ];
    uint32_t *p_bordel = p_shuffle->p_bordel;
    unsigned int i;

    static uint32_t i_secret = 0;

    static uint32_t p_secret3[] =
    {
        0xAAAAAAAA, 0x01757700, 0x00554580, 0x01724500, 0x00424580,
        0x01427700, 0x00000080, 0xC1D59D01, 0x80144981, 0x815C8901,
        0x80544981, 0x81D45D01, 0x00000080, 0x81A3BB03, 0x00A2AA82,
        0x01A3BB03, 0x0022A282, 0x813BA202, 0x00000080, 0x6D575737,
        0x4A5275A5, 0x6D525725, 0x4A5254A5, 0x6B725437, 0x00000080,
        0xD5DDB938, 0x5455A092, 0x5D95A013, 0x4415A192, 0xC5DD393A,
        0x00000080, 0x55555555
    };

    static char p_secret4[] =
        "pbclevtug (p) Nccyr Pbzchgre, Vap.  Nyy Evtugf Erfreirq.";

    if( i_secret == 0 )
    {
        REVERSE( p_secret3, sizeof(p_secret3)/sizeof(p_secret3[ 0 ]) );
        for( ; p_secret4[ i_secret ] != '\0'; i_secret++ )
        {
#define ROT13(c) (((c)>='A'&&(c)<='Z')?(((c)-'A'+13)%26)+'A':\
                  ((c)>='a'&&(c)<='z')?(((c)-'a'+13)%26)+'a':c)
            p_secret4[ i_secret ] = ROT13(p_secret4[ i_secret ]);
        }
        i_secret++; /* include zero terminator */
    }

    /* Using the MD5 hash of a memory block is probably not one-way enough
     * for the iTunes people. This function randomises p_bordel depending on
     * the values in p_commands to make things even more messy in p_bordel. */
    for( i = 0; i < 20; i++ )
    {
        uint8_t i_command, i_index;

        if( !p_shuffle->p_commands[ i ] )
        {
            continue;
        }

        i_command = (p_shuffle->p_commands[ i ] & 0x300) >> 8;
        i_index = p_shuffle->p_commands[ i ] & 0xff;

        switch( i_command )
        {
        case 0x3:
            p_bordel[ i_index & 0xf ] = p_bordel[ i_index >> 4 ]
                                      + p_bordel[ ((i_index + 0x10) >> 4) & 0xf ];
            break;
        case 0x2:
            p_bordel[ i_index >> 4 ] ^= p_shuffle_xor[ 0xff - i_index ];
            break;
        case 0x1:
            p_bordel[ i_index >> 4 ] -= p_shuffle_sub[ 0xff - i_index ];
            break;
        default:
            p_bordel[ i_index >> 4 ] += p_shuffle_add[ 0xff - i_index ];
            break;
        }
    }

    if( p_shuffle->i_version == 0x01000300 )
    {
        DoExtShuffle( p_bordel );
    }

    /* Convert our newly randomised p_bordel to big endianness and take
     * its MD5 hash. */
    InitMD5( &md5 );
    for( i = 0; i < 16; i++ )
    {
        p_big_bordel[ i ] = U32_AT(p_bordel + i);
    }
    AddMD5( &md5, (uint8_t *)p_big_bordel, 64 );
    if( p_shuffle->i_version == 0x01000300 )
    {
        AddMD5( &md5, (uint8_t *)p_secret3, sizeof(p_secret3) );
        AddMD5( &md5, (uint8_t *)p_secret4, i_secret );
    }
    EndMD5( &md5 );

    /* XOR our buffer with the computed checksum */
    for( i = 0; i < i_size; i++ )
    {
        p_buffer[ i ] ^= md5.p_digest[ i ];
    }
}

/*****************************************************************************
 * DoExtShuffle: extended shuffle
 *****************************************************************************
 * This is even uglier.
 *****************************************************************************/
static void DoExtShuffle( uint32_t * p_bordel )
{
    uint32_t i_ret;

    i_ret = FirstPass( p_bordel );

    SecondPass( p_bordel, i_ret );

    ThirdPass( p_bordel );

    FourthPass( p_bordel );
}

static uint32_t FirstPass( uint32_t * p_bordel )
{
    uint32_t i, i_cmd, i_ret = 5;

    TinyShuffle1( p_bordel );

    for( ; ; )
    {
        for( ; ; )
        {
            p_bordel[ 1 ] += 0x10000000;
            p_bordel[ 3 ] += 0x12777;

            if( (p_bordel[ 10 ] & 1) && i_ret )
            {
                i_ret--;
                p_bordel[ 1 ] -= p_bordel[ 2 ];
                p_bordel[ 11 ] += p_bordel[ 12 ];
                break;
            }

            if( (p_bordel[ 1 ] + p_bordel[ 2 ]) >= 0x7D0 )
            {
                switch( ((p_bordel[ 3 ] ^ 0x567F) >> 2) & 7 )
                {
                    case 0:
                        for( i = 0; i < 3; i++ )
                        {
                            if( p_bordel[ i + 10 ] > 0x4E20 )
                            {
                                p_bordel[ i + 1 ] += p_bordel[ i + 2 ];
                            }
                        }
                        break;
                    case 4:
                        p_bordel[ 1 ] -= p_bordel[ 2 ];
                        /* no break */
                    case 3:
                        p_bordel[ 11 ] += p_bordel[ 12 ];
                        break;
                    case 6:
                        p_bordel[ 3 ] ^= p_bordel[ 4 ];
                        /* no break */
                    case 8:
                        p_bordel[ 13 ] &= p_bordel[ 14 ];
                        /* no break */
                    case 1:
                        p_bordel[ 0 ] |= p_bordel[ 1 ];
                        if( i_ret )
                        {
                            return i_ret;
                        }
                        break;
                }

                break;
            }
        }

        for( i = 0, i_cmd = 0; i < 16; i++ )
        {
            if( p_bordel[ i ] < p_bordel[ i_cmd ] )
            {
                i_cmd = i;
            }
        }

        if( i_ret && i_cmd != 5 )
        {
            i_ret--;
        }
        else
        {
            if( i_cmd == 5 )
            {
                p_bordel[ 8 ] &= p_bordel[ 6 ] >> 1;
                p_bordel[ 3 ] <<= 1;
            }

            for( i = 0; i < 3; i++ )
            {
                p_bordel[ 11 ] += 1;
                if( p_bordel[ 11 ] & 5 )
                {
                    p_bordel[ 8 ] += p_bordel[ 9 ];
                }
                else if( i_ret )
                {
                    i_ret--;
                    i_cmd = 3;
                    goto break2;
                }
            }

            i_cmd = (p_bordel[ 15 ] + 0x93) >> 3;
            if( p_bordel[ 15 ] & 0x100 )
            {
                i_cmd ^= 0xDEAD;
            }
        }

        switch( i_cmd & 3 )
        {
            case 0:
                while( p_bordel[ 11 ] & 1 )
                {
                    p_bordel[ 11 ] >>= 1;
                    p_bordel[ 12 ] += 1;
                }
                /* no break */
            case 2:
                p_bordel[ 14 ] -= 0x19FE;
                break;
            case 3:
                if( i_ret )
                {
                    i_ret--;
                    p_bordel[ 5 ] += 5;
                    continue;
                }
                break;
        }

        i_cmd = ((p_bordel[ 3 ] + p_bordel[ 4 ] + 10) >> 1) - p_bordel[ 4 ];
        break;
    }
break2:

    switch( i_cmd & 3 )
    {
        case 0:
            p_bordel[ 14 ] >>= 1;
            break;
        case 1:
            p_bordel[ 5 ] <<= 2;
            break;
        case 2:
            p_bordel[ 12 ] |= 5;
            break;
        case 3:
            p_bordel[ 15 ] &= 0x55;
            if( i_ret )
            {
                p_bordel[ 2 ] &= 0xB62FC;
                return i_ret;
            }
            break;
    }

    TinyShuffle2( p_bordel );

    return i_ret;
}

static void SecondPass( uint32_t * p_bordel, uint32_t i_tmp )
{
    uint32_t i, i_cmd, i_jc = 5;

    TinyShuffle3( p_bordel );

    for( i = 0, i_cmd = 0; i < 16; i++ )
    {
        if( p_bordel[ i ] > p_bordel[ i_cmd ] )
        {
            i_cmd = i;
        }
    }

    switch( i_cmd )
    {
        case 0:
            if( p_bordel[ 1 ] < p_bordel[ 8 ] )
            {
                p_bordel[ 5 ] += 1;
            }
            break;
        case 4:
            if( (p_bordel[ 9 ] & 0x7777) == 0x3333 )
            {
                p_bordel[ 5 ] -= 1;
            }
            else
            {
                i_jc--;
                if( p_bordel[ 1 ] < p_bordel[ 8 ] )
                {
                    p_bordel[ 5 ] += 1;
                }
                break;
            }
            /* no break */
        case 7:
            p_bordel[ 2 ] -= 1;
            p_bordel[ 1 ] -= p_bordel[ 5 ];
            for( i = 0; i < 3; i++ )
            {
                switch( p_bordel[ 1 ] & 3 )
                {
                    case 0:
                        p_bordel[ 1 ] += 1;
                        /* no break */
                    case 1:
                        p_bordel[ 3 ] -= 8;
                        break;
                    case 2:
                        p_bordel[ 13 ] &= 0xFEFEFEF7;
                        break;
                    case 3:
                        p_bordel[ 8 ] |= 0x80080011;
                        break;
                }
            }
            return;
        case 10:
            p_bordel[ 4 ] -= 1;
            p_bordel[ 5 ] += 1;
            p_bordel[ 6 ] -= 1;
            p_bordel[ 7 ] += 1;
            break;
        default:
            p_bordel[ 15 ] ^= 0x18547EFF;
            break;
    }

    for( i = 3; i--; )
    {
        switch( ( p_bordel[ 12 ] + p_bordel[ 13 ] + p_bordel[ 6 ] ) % 5 )
        {
            case 0:
                p_bordel[ 12 ] -= 1;
                /* no break */
            case 1:
                p_bordel[ 12 ] -= 1;
                p_bordel[ 13 ] += 1;
                break;
            case 2:
                p_bordel[ 13 ] += 4;
                /* no break */
            case 3:
                p_bordel[ 12 ] -= 1;
                break;
            case 4:
                i_jc--;
                p_bordel[ 5 ] += 1;
                p_bordel[ 6 ] -= 1;
                p_bordel[ 7 ] += 1;
                i = 3; /* Restart the whole loop */
                break;
        }
    }

    TinyShuffle4( p_bordel );

    for( ; ; )
    {
        TinyShuffle5( p_bordel );

        switch( ( p_bordel[ 2 ] * 2 + 15 ) % 5 )
        {
            case 0:
                if( ( p_bordel[ 3 ] + i_tmp ) <=
                    ( p_bordel[ 1 ] + p_bordel[ 15 ] ) )
                {
                    p_bordel[ 3 ] += 1;
                }
                break;
            case 4:
                p_bordel[ 10 ] -= 0x13;
                break;
            case 3:
                p_bordel[ 5 ] >>= 2;
                break;
        }

        if( !( p_bordel[ 2 ] & 1 ) || i_jc == 0 )
        {
            break;
        }

        i_jc--;
        p_bordel[ 2 ] += 0x13;
        p_bordel[ 12 ] += 1;
    }

    p_bordel[ 2 ] &= 0x10076000;
}

static void ThirdPass( uint32_t * p_bordel )
{
    uint32_t i_cmd;

    i_cmd = ((p_bordel[ 7 ] + p_bordel[ 14 ] + 10) >> 1) - p_bordel[ 14 ];
    i_cmd = i_cmd % 10;

    switch( i_cmd )
    {
        case 0:
            p_bordel[ 1 ] <<= 1;
            p_bordel[ 2 ] <<= 2;
            p_bordel[ 3 ] <<= 3;
            break;
        case 6:
            p_bordel[ i_cmd + 3 ] &= 0x5EDE36B;
            p_bordel[ 5 ] += p_bordel[ 8 ];
            p_bordel[ 4 ] += p_bordel[ 7 ];
            p_bordel[ 3 ] += p_bordel[ 6 ];
            p_bordel[ 2 ] += p_bordel[ 5 ];
            /* no break */
        case 2:
            p_bordel[ 1 ] += p_bordel[ 4 ];
            p_bordel[ 0 ] += p_bordel[ 3 ];
            TinyShuffle6( p_bordel );
            return; /* jc = 4 */
        case 3:
            if( (p_bordel[ 11 ] & p_bordel[ 2 ]) > 0x211B )
            {
                p_bordel[ 6 ] += 1;
            }
            break;
        case 4:
            p_bordel[ 7 ] += 1;
            /* no break */
        case 5:
            p_bordel[ 9 ] ^= p_bordel[ 2 ];
            break;
        case 7:
            p_bordel[ 2 ] ^= (p_bordel[ 1 ] & p_bordel[ 13 ]);
            break;
        case 8:
            p_bordel[ 0 ] -= p_bordel[ 11 ] & p_bordel[ 15 ];
            return; /* jc = 4 */
        case 9:
            p_bordel[ 6 ] >>= (p_bordel[ 14 ] & 3);
            break;
    }

    SWAP( p_bordel[ 0 ], p_bordel[ 10 ] );

    TinyShuffle6( p_bordel );

    return; /* jc = 5 */
}

static void FourthPass( uint32_t * p_bordel )
{
    uint32_t i, j;

    TinyShuffle7( p_bordel );

    switch( p_bordel[ 5 ] % 5)
    {
        case 0:
            p_bordel[ 0 ] += 1;
            break;
        case 2:
            p_bordel[ 11 ] ^= (p_bordel[ 3 ] + p_bordel[ 6 ] + p_bordel[ 8 ]);
            break;
        case 3:
            for( i = 4; i < 15 && (p_bordel[ i ] & 5) == 0; i++ )
            {
                SWAP( p_bordel[ i ], p_bordel[ 15 - i ] );
            }
            break;
        case 4:
            p_bordel[ 12 ] -= 1;
            p_bordel[ 13 ] += 1;
            p_bordel[ 2 ] -= 0x64;
            p_bordel[ 3 ] += 0x64;
            TinyShuffle8( p_bordel );
            return;
    }

    for( i = 0, j = 0; i < 16; i++ )
    {
        if( p_bordel[ i ] > p_bordel[ j ] )
        {
            j = i;
        }
    }

    switch( p_bordel[ j ] % 100 )
    {
        case 0:
            SWAP( p_bordel[ 0 ], p_bordel[ j ] );
            break;
        case 8:
            p_bordel[ 1 ] >>= 1;
            p_bordel[ 2 ] <<= 1;
            p_bordel[ 14 ] >>= 3;
            p_bordel[ 15 ] <<= 4;
            break;
        case 57:
            p_bordel[ j ] += p_bordel[ 13 ];
            break;
        case 76:
            p_bordel[ 1 ] += 0x20E;
            p_bordel[ 5 ] += 0x223D;
            p_bordel[ 13 ] -= 0x576;
            p_bordel[ 15 ] += 0x576;
            return;
        case 91:
            p_bordel[ 2 ] -= 0x64;
            p_bordel[ 3 ] += 0x64;
            p_bordel[ 12 ] -= 1;
            p_bordel[ 13 ] += 1;
            break;
        case 99:
            p_bordel[ 0 ] += 1;
            p_bordel[ j ] += p_bordel[ 13 ];
            break;
    }

    TinyShuffle8( p_bordel );
}

/*****************************************************************************
 * TinyShuffle[12345678]: tiny shuffle subroutines
 *****************************************************************************
 * These standalone functions are little helpers for the shuffling process.
 *****************************************************************************/
static void TinyShuffle1( uint32_t * p_bordel )
{
    uint32_t i_cmd = (p_bordel[ 5 ] + 10) >> 2;

    if( p_bordel[ 5 ] > 0x7D0 )
    {
        i_cmd -= 0x305;
    }

    switch( i_cmd & 3 )
    {
        case 0:
            p_bordel[ 5 ] += 5;
            break;
        case 1:
            p_bordel[ 4 ] -= 1;
            break;
        case 2:
            if( p_bordel[ 4 ] & 5 )
            {
                p_bordel[ 1 ] ^= 0x4D;
            }
            /* no break */
        case 3:
            p_bordel[ 12 ] += 5;
            break;
    }
}

static void TinyShuffle2( uint32_t * p_bordel )
{
    uint32_t i, j;

    for( i = 0, j = 0; i < 16; i++ )
    {
        if( (p_bordel[ i ] & 0x777) > (p_bordel[ j ] & 0x777) )
        {
            j = i;
        }
    }

    if( j > 5 )
    {
        for( ; j < 15; j++ )
        {
            p_bordel[ j ] += p_bordel[ j + 1 ];
        }
    }
    else
    {
        p_bordel[ 2 ] &= 0xB62FC;
    }
}

static void TinyShuffle3( uint32_t * p_bordel )
{
    uint32_t i_cmd = p_bordel[ 6 ] + 0x194B;

    if( p_bordel[ 6 ] > 0x2710 )
    {
        i_cmd >>= 1;
    }

    switch( i_cmd & 3 )
    {
        case 1:
            p_bordel[ 3 ] += 0x19FE;
            break;
        case 2:
            p_bordel[ 7 ] -= p_bordel[ 3 ] >> 2;
            /* no break */
        case 0:
            p_bordel[ 5 ] ^= 0x248A;
            break;
    }
}

static void TinyShuffle4( uint32_t * p_bordel )
{
    uint32_t i, j;

    for( i = 0, j = 0; i < 16; i++ )
    {
        if( p_bordel[ i ] < p_bordel[ j ] )
        {
            j = i;
        }
    }

    if( (p_bordel[ j ] % (j + 1)) > 10 )
    {
        p_bordel[ 1 ] -= 1;
        p_bordel[ 2 ] += 0x13;
        p_bordel[ 12 ] += 1;
    }
}

static void TinyShuffle5( uint32_t * p_bordel )
{
    uint32_t i;

    p_bordel[ 2 ] &= 0x7F3F;

    for( i = 0; i < 5; i++ )
    {
        switch( ( p_bordel[ 2 ] + 10 + i ) % 5 )
        {
            case 0:
                p_bordel[ 12 ] &= p_bordel[ 2 ];
                /* no break */
            case 1:
                p_bordel[ 3 ] ^= p_bordel[ 15 ];
                break;
            case 2:
                p_bordel[ 15 ] += 0x576;
                /* no break */
            case 3:
                p_bordel[ 7 ] -= 0x2D;
                /* no break */
            case 4:
                p_bordel[ 1 ] <<= 1;
                break;
        }
    }
}

static void TinyShuffle6( uint32_t * p_bordel )
{
    uint32_t i, j;

    for( i = 0; i < 8; i++ )
    {
        j = p_bordel[ 3 ] & 0x7514 ? 5 : 7;
        SWAP( p_bordel[ i ], p_bordel[ i + j ] );
    }
}

static void TinyShuffle7( uint32_t * p_bordel )
{
    uint32_t i;

    i = (((p_bordel[ 9 ] + p_bordel[ 15 ] + 12) >> 2) - p_bordel[ 4 ]) & 7;

    while( i-- )
    {
        SWAP( p_bordel[ i ], p_bordel[ i + 3 ] );
    }

    SWAP( p_bordel[ 1 ], p_bordel[ 10 ] );
}

static void TinyShuffle8( uint32_t * p_bordel )
{
    uint32_t i;

    i = (p_bordel[ 0 ] & p_bordel[ 6 ]) & 0xF;

    switch( p_bordel[ i ] % 1000 )
    {
        case 7:
            if( (p_bordel[ i ] & 0x777) > (p_bordel[ 7 ] & 0x5555) )
            {
                p_bordel[ i ] ^= p_bordel[ 5 ] & p_bordel[ 3 ];
            }
            break;
        case 19:
            p_bordel[ 15 ] &= 0x5555;
            break;
        case 93:
            p_bordel[ i ] ^= p_bordel[ 15 ];
            break;
        case 100:
            SWAP( p_bordel[ 0 ], p_bordel[ 3 ] );
            SWAP( p_bordel[ 1 ], p_bordel[ 6 ] );
            SWAP( p_bordel[ 3 ], p_bordel[ 6 ] );
            SWAP( p_bordel[ 4 ], p_bordel[ 9 ] );
            SWAP( p_bordel[ 5 ], p_bordel[ 8 ] );
            SWAP( p_bordel[ 6 ], p_bordel[ 7 ] );
            SWAP( p_bordel[ 13 ], p_bordel[ 14 ] );
            break;
        case 329:
            p_bordel[ i ] += p_bordel[ 1 ] ^ 0x80080011;
            p_bordel[ i ] += p_bordel[ 2 ] ^ 0xBEEFDEAD;
            p_bordel[ i ] += p_bordel[ 3 ] ^ 0x8765F444;
            p_bordel[ i ] += p_bordel[ 4 ] ^ 0x78145326;
            break;
        case 567:
            p_bordel[ 12 ] -= p_bordel[ i ];
            p_bordel[ 13 ] += p_bordel[ i ];
            break;
        case 612:
            p_bordel[ i ] += p_bordel[ 1 ];
            p_bordel[ i ] -= p_bordel[ 7 ];
            p_bordel[ i ] -= p_bordel[ 8 ];
            p_bordel[ i ] += p_bordel[ 9 ];
            p_bordel[ i ] += p_bordel[ 13 ];
            break;
        case 754:
            i = __MIN( i, 12 );
            p_bordel[ i + 1 ] >>= 1;
            p_bordel[ i + 2 ] <<= 4;
            p_bordel[ i + 3 ] >>= 3;
            break;
        case 777:
            p_bordel[ 1 ] += 0x20E;
            p_bordel[ 5 ] += 0x223D;
            p_bordel[ 13 ] -= 0x576;
            p_bordel[ 15 ] += 0x576;
            break;
        case 981:
            if( (p_bordel[ i ] ^ 0x8765F441) < 0x2710 )
            {
                SWAP( p_bordel[ 0 ], p_bordel[ 1 ] );
            }
            else
            {
                SWAP( p_bordel[ 1 ], p_bordel[ 11 ] );
            }
            break;
    }
}

/*****************************************************************************
 * GetSystemKey: get the system key
 *****************************************************************************
 * Compute the system key from various system information, see HashSystemInfo.
 *****************************************************************************/
static int GetSystemKey( uint32_t *p_sys_key, vlc_bool_t b_ipod )
{
    static char const p_secret5[ 8 ] = "YuaFlafu";
    static char const p_secret6[ 8 ] = "zPif98ga";
    struct md5_s md5;
    int64_t i_ipod_id;
    uint32_t p_system_hash[ 4 ];

    /* Compute the MD5 hash of our system info */
    if( ( !b_ipod && HashSystemInfo( p_system_hash ) ) ||
        (  b_ipod && GetiPodID( &i_ipod_id ) ) )
    {
        return -1;
    }

    /* Combine our system info hash with additional secret data. The resulting
     * MD5 hash will be our system key. */
    InitMD5( &md5 );
    AddMD5( &md5, p_secret5, 8 );

    if( !b_ipod )
    {
        AddMD5( &md5, (uint8_t *)p_system_hash, 6 );
        AddMD5( &md5, (uint8_t *)p_system_hash, 6 );
        AddMD5( &md5, (uint8_t *)p_system_hash, 6 );
        AddMD5( &md5, p_secret6, 8 );
    }
    else
    {
        i_ipod_id = U64_AT(&i_ipod_id);
        AddMD5( &md5, (uint8_t *)&i_ipod_id, sizeof(i_ipod_id) );
        AddMD5( &md5, (uint8_t *)&i_ipod_id, sizeof(i_ipod_id) );
        AddMD5( &md5, (uint8_t *)&i_ipod_id, sizeof(i_ipod_id) );
    }

    EndMD5( &md5 );

    memcpy( p_sys_key, md5.p_digest, 16 );

    return 0;
}

#ifdef WIN32
#   define DRMS_DIRNAME "drms"
#else
#   define DRMS_DIRNAME ".drms"
#endif

/*****************************************************************************
 * WriteUserKey: write the user key to hard disk
 *****************************************************************************
 * Write the user key to the hard disk so that it can be reused later or used
 * on operating systems other than Win32.
 *****************************************************************************/
static int WriteUserKey( void *_p_drms, uint32_t *p_user_key )
{
    struct drms_s *p_drms = (struct drms_s *)_p_drms;
    FILE *file;
    int i_ret = -1;
    char psz_path[ PATH_MAX ];

    snprintf( psz_path, PATH_MAX - 1,
              "%s/" DRMS_DIRNAME, p_drms->psz_homedir );

#if defined( HAVE_ERRNO_H )
#   if defined( WIN32 )
    if( !mkdir( psz_path ) || errno == EEXIST )
#   else
    if( !mkdir( psz_path, 0755 ) || errno == EEXIST )
#   endif
#else
    if( !mkdir( psz_path ) )
#endif
    {
        snprintf( psz_path, PATH_MAX - 1, "%s/" DRMS_DIRNAME "/%08X.%03d",
                  p_drms->psz_homedir, p_drms->i_user, p_drms->i_key );

        file = utf8_fopen( psz_path, "wb" );
        if( file != NULL )
        {
            i_ret = fwrite( p_user_key, sizeof(uint32_t),
                            4, file ) == 4 ? 0 : -1;
            fclose( file );
        }
    }

    return i_ret;
}

/*****************************************************************************
 * ReadUserKey: read the user key from hard disk
 *****************************************************************************
 * Retrieve the user key from the hard disk if available.
 *****************************************************************************/
static int ReadUserKey( void *_p_drms, uint32_t *p_user_key )
{
    struct drms_s *p_drms = (struct drms_s *)_p_drms;
    FILE *file;
    int i_ret = -1;
    char psz_path[ PATH_MAX ];

    snprintf( psz_path, PATH_MAX - 1,
              "%s/" DRMS_DIRNAME "/%08X.%03d", p_drms->psz_homedir,
              p_drms->i_user, p_drms->i_key );

    file = utf8_fopen( psz_path, "rb" );
    if( file != NULL )
    {
        i_ret = fread( p_user_key, sizeof(uint32_t),
                       4, file ) == 4 ? 0 : -1;
        fclose( file );
    }

    return i_ret;
}

/*****************************************************************************
 * GetUserKey: get the user key
 *****************************************************************************
 * Retrieve the user key from the hard disk if available, otherwise generate
 * it from the system key. If the key could be successfully generated, write
 * it to the hard disk for future use.
 *****************************************************************************/
static int GetUserKey( void *_p_drms, uint32_t *p_user_key )
{
    static char const p_secret7[] = "mUfnpognadfgf873";
    struct drms_s *p_drms = (struct drms_s *)_p_drms;
    struct aes_s aes;
    struct shuffle_s shuffle;
    uint32_t i, y;
    uint32_t *p_sci_data;
    uint32_t i_user, i_key;
    uint32_t p_sys_key[ 4 ];
    uint32_t i_sci_size, i_blocks, i_remaining;
    uint32_t *p_sci0, *p_sci1, *p_buffer;
    uint32_t p_sci_key[ 4 ];
    char *psz_ipod;
    int i_ret = -5;

    if( ReadUserKey( p_drms, p_user_key ) == 0 )
    {
        REVERSE( p_user_key, 4 );
        return 0;
    }

    psz_ipod = getenv( "IPOD" );

    if( GetSystemKey( p_sys_key, psz_ipod ? VLC_TRUE : VLC_FALSE ) )
    {
        return -3;
    }

    if( GetSCIData( psz_ipod, &p_sci_data, &i_sci_size ) )
    {
        return -4;
    }

    /* Phase 1: unscramble the SCI data using the system key and shuffle
     *          it using DoShuffle(). */

    /* Skip the first 4 bytes (some sort of header). Decrypt the rest. */
    i_blocks = (i_sci_size - 4) / 16;
    i_remaining = (i_sci_size - 4) - (i_blocks * 16);
    p_buffer = p_sci_data + 1;

    /* Decrypt and shuffle our data at the same time */
    InitAES( &aes, p_sys_key );
    REVERSE( p_sys_key, 4 );
    REVERSE( p_sci_data, 1 );
    InitShuffle( &shuffle, p_sys_key, p_sci_data[ 0 ] );

    memcpy( p_sci_key, p_secret7, 16 );
    REVERSE( p_sci_key, 4 );

    while( i_blocks-- )
    {
        uint32_t p_tmp[ 4 ];

        REVERSE( p_buffer, 4 );
        DecryptAES( &aes, p_tmp, p_buffer );
        BlockXOR( p_tmp, p_sci_key, p_tmp );

        /* Use the previous scrambled data as the key for next block */
        memcpy( p_sci_key, p_buffer, 16 );

        /* Shuffle the decrypted data using a custom routine */
        DoShuffle( &shuffle, p_tmp, 4 );

        /* Copy this block back to p_buffer */
        memcpy( p_buffer, p_tmp, 16 );

        p_buffer += 4;
    }

    if( i_remaining >= 4 )
    {
        REVERSE( p_buffer, i_remaining / 4 );
        DoShuffle( &shuffle, p_buffer, i_remaining / 4 );
    }

    /* Phase 2: look for the user key in the generated data. I must admit I
     *          do not understand what is going on here, because it almost
     *          looks like we are browsing data that makes sense, even though
     *          the DoShuffle() part made it completely meaningless. */

    y = 0;
    REVERSE( p_sci_data + 5, 1 );
    i = U32_AT( p_sci_data + 5 );
    i_sci_size -= 22 * sizeof(uint32_t);
    p_sci1 = p_sci_data + 22;
    p_sci0 = NULL;

    while( i_sci_size >= 20 && i > 0 )
    {
        if( p_sci0 == NULL )
        {
            i_sci_size -= 18 * sizeof(uint32_t);
            if( i_sci_size < 20 )
            {
                break;
            }

            p_sci0 = p_sci1;
            REVERSE( p_sci1 + 17, 1 );
            y = U32_AT( p_sci1 + 17 );
            p_sci1 += 18;
        }

        if( !y )
        {
            i--;
            p_sci0 = NULL;
            continue;
        }

        i_user = U32_AT( p_sci0 );
        i_key = U32_AT( p_sci1 );
        REVERSE( &i_user, 1 );
        REVERSE( &i_key, 1 );
        if( i_user == p_drms->i_user && ( ( i_key == p_drms->i_key ) ||
            ( !p_drms->i_key && ( p_sci1 == (p_sci0 + 18) ) ) ) )
        {
            memcpy( p_user_key, p_sci1 + 1, 16 );
            REVERSE( p_sci1 + 1, 4 );
            WriteUserKey( p_drms, p_sci1 + 1 );
            i_ret = 0;
            break;
        }

        y--;
        p_sci1 += 5;
        i_sci_size -= 5 * sizeof(uint32_t);
    }

    free( p_sci_data );

    return i_ret;
}

/*****************************************************************************
 * GetSCIData: get SCI data from "SC Info.sidb"
 *****************************************************************************
 * Read SCI data from "\Apple Computer\iTunes\SC Info\SC Info.sidb"
 *****************************************************************************/
static int GetSCIData( char *psz_ipod, uint32_t **pp_sci,
                       uint32_t *pi_sci_size )
{
    FILE *file;
    char *psz_path = NULL;
    char p_tmp[ 4 * PATH_MAX ];
    int i_ret = -1;

    if( psz_ipod == NULL )
    {
#ifdef WIN32
        char *p_filename = "\\Apple Computer\\iTunes\\SC Info\\SC Info.sidb";
        typedef HRESULT (WINAPI *SHGETFOLDERPATH)( HWND, int, HANDLE, DWORD,
                                                   LPSTR );
        HINSTANCE shfolder_dll = NULL;
        SHGETFOLDERPATH dSHGetFolderPath = NULL;

        if( ( shfolder_dll = LoadLibrary( _T("SHFolder.dll") ) ) != NULL )
        {
            dSHGetFolderPath =
                (SHGETFOLDERPATH)GetProcAddress( shfolder_dll,
                                                 _T("SHGetFolderPathA") );
        }

        if( dSHGetFolderPath != NULL &&
            SUCCEEDED( dSHGetFolderPath( NULL, CSIDL_COMMON_APPDATA,
                                         NULL, 0, p_tmp ) ) )
        {
            strncat( p_tmp, p_filename, min( strlen( p_filename ),
                     (sizeof(p_tmp)/sizeof(p_tmp[0]) - 1) -
                     strlen( p_tmp ) ) );

            psz_path = FromLocale( p_tmp );
            strncpy( p_tmp, psz_path, sizeof( p_tmp ) - 1 );
            p_tmp[sizeof( p_tmp ) - 1] = '\0';
            LocaleFree( psz_path );
            psz_path = p_tmp;
        }

        if( shfolder_dll != NULL )
        {
            FreeLibrary( shfolder_dll );
        }
#endif
    }
    else
    {
#define ISCINFO "iSCInfo"
        if( strstr( psz_ipod, ISCINFO ) == NULL )
        {
            snprintf( p_tmp, sizeof(p_tmp)/sizeof(p_tmp[0]) - 1,
                      "%s/iPod_Control/iTunes/" ISCINFO "2", psz_ipod );
            psz_path = p_tmp;
        }
        else
        {
            psz_path = psz_ipod;
        }
    }

    if( psz_path == NULL )
    {
        return -1;
    }

    file = utf8_fopen( psz_path, "rb" );
    if( file != NULL )
    {
        struct stat st;

        if( !fstat( fileno( file ), &st ) && st.st_size >= 4 )
        {
            *pp_sci = malloc( st.st_size );
            if( *pp_sci != NULL )
            {
                if( fread( *pp_sci, 1, st.st_size,
                           file ) == (size_t)st.st_size )
                {
                    *pi_sci_size = st.st_size;
                    i_ret = 0;
                }
                else
                {
                    free( (void *)*pp_sci );
                    *pp_sci = NULL;
                }
            }
        }

        fclose( file );
    }

    return i_ret;
}

/*****************************************************************************
 * HashSystemInfo: hash system information
 *****************************************************************************
 * This function computes the MD5 hash of the C: hard drive serial number,
 * BIOS version, CPU type and Windows version.
 *****************************************************************************/
static int HashSystemInfo( uint32_t *p_system_hash )
{
    struct md5_s md5;
    int i_ret = 0;

#ifdef WIN32
    HKEY i_key;
    unsigned int i;
    DWORD i_size;
    DWORD i_serial;
    LPBYTE p_reg_buf;

    static LPCTSTR p_reg_keys[ 3 ][ 2 ] =
    {
        {
            _T("HARDWARE\\DESCRIPTION\\System"),
            _T("SystemBiosVersion")
        },

        {
            _T("HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0"),
            _T("ProcessorNameString")
        },

        {
            _T("SOFTWARE\\Microsoft\\Windows\\CurrentVersion"),
            _T("ProductId")
        }
    };

    InitMD5( &md5 );

    AddMD5( &md5, "cache-control", 13 );
    AddMD5( &md5, "Ethernet", 8 );

    GetVolumeInformation( _T("C:\\"), NULL, 0, &i_serial,
                          NULL, NULL, NULL, 0 );
    AddMD5( &md5, (uint8_t *)&i_serial, 4 );

    for( i = 0; i < sizeof(p_reg_keys) / sizeof(p_reg_keys[ 0 ]); i++ )
    {
        if( RegOpenKeyEx( HKEY_LOCAL_MACHINE, p_reg_keys[ i ][ 0 ],
                          0, KEY_READ, &i_key ) != ERROR_SUCCESS )
        {
            continue;
        }

        if( RegQueryValueEx( i_key, p_reg_keys[ i ][ 1 ],
                             NULL, NULL, NULL, &i_size ) != ERROR_SUCCESS )
        {
            RegCloseKey( i_key );
            continue;
        }

        p_reg_buf = malloc( i_size );

        if( p_reg_buf != NULL )
        {
            if( RegQueryValueEx( i_key, p_reg_keys[ i ][ 1 ],
                                 NULL, NULL, p_reg_buf,
                                 &i_size ) == ERROR_SUCCESS )
            {
                AddMD5( &md5, (uint8_t *)p_reg_buf, i_size );
            }

            free( p_reg_buf );
        }

        RegCloseKey( i_key );
    }

#else
    InitMD5( &md5 );
    i_ret = -1;
#endif

    EndMD5( &md5 );
    memcpy( p_system_hash, md5.p_digest, 16 );

    return i_ret;
}

/*****************************************************************************
 * GetiPodID: Get iPod ID
 *****************************************************************************
 * This function gets the iPod ID.
 *****************************************************************************/
static int GetiPodID( int64_t *p_ipod_id )
{
    int i_ret = -1;

#define PROD_NAME   "iPod"
#define VENDOR_NAME "Apple Computer, Inc."

    char *psz_ipod_id = getenv( "IPODID" );
    if( psz_ipod_id != NULL )
    {
        *p_ipod_id = strtoll( psz_ipod_id, NULL, 16 );
        return 0;
    }

#ifdef __APPLE__
    CFTypeRef value;
    mach_port_t port;
    io_object_t device;
    io_iterator_t iterator;
    CFMutableDictionaryRef match_dic;
    CFMutableDictionaryRef smatch_dic;

    if( IOMasterPort( MACH_PORT_NULL, &port ) == KERN_SUCCESS )
    {
        smatch_dic = IOServiceMatching( "IOFireWireUnit" );
        match_dic = CFDictionaryCreateMutable( kCFAllocatorDefault, 0,
                                           &kCFTypeDictionaryKeyCallBacks,
                                           &kCFTypeDictionaryValueCallBacks );

        if( smatch_dic != NULL && match_dic != NULL )
        {
            CFDictionarySetValue( smatch_dic,
                                  CFSTR("FireWire Vendor Name"),
                                  CFSTR(VENDOR_NAME) );
            CFDictionarySetValue( smatch_dic,
                                  CFSTR("FireWire Product Name"),
                                  CFSTR(PROD_NAME) );

            CFDictionarySetValue( match_dic,
                                  CFSTR(kIOPropertyMatchKey),
                                  smatch_dic );

            if( IOServiceGetMatchingServices( port, match_dic,
                                              &iterator ) == KERN_SUCCESS )
            {
                while( ( device = IOIteratorNext( iterator ) ) != NULL )
                {
                    value = IORegistryEntryCreateCFProperty( device,
                        CFSTR("GUID"), kCFAllocatorDefault, kNilOptions );

                    if( value != NULL )
                    {
                        if( CFGetTypeID( value ) == CFNumberGetTypeID() )
                        {
                            int64_t i_ipod_id;
                            CFNumberGetValue( (CFNumberRef)value,
                                              kCFNumberLongLongType,
                                              &i_ipod_id );
                            *p_ipod_id = i_ipod_id;
                            i_ret = 0;
                        }

                        CFRelease( value );
                    }

                    IOObjectRelease( device );

                    if( !i_ret ) break;
                }

                IOObjectRelease( iterator );
            }
        }

        mach_port_deallocate( mach_task_self(), port );
    }

#elif HAVE_SYSFS_LIBSYSFS_H
    struct sysfs_bus *bus = NULL;
    struct dlist *devlist = NULL;
    struct dlist *attributes = NULL;
    struct sysfs_device *curdev = NULL;
    struct sysfs_attribute *curattr = NULL;

    bus = sysfs_open_bus( "ieee1394" );
    if( bus != NULL )
    {
        devlist = sysfs_get_bus_devices( bus );
        if( devlist != NULL )
        {
            dlist_for_each_data( devlist, curdev, struct sysfs_device )
            {
                attributes = sysfs_get_device_attributes( curdev );
                if( attributes != NULL )
                {
                    dlist_for_each_data( attributes, curattr,
                                         struct sysfs_attribute )
                    {
                        if( ( strcmp( curattr->name, "model_name" ) == 0 ) &&
                            ( strncmp( curattr->value, PROD_NAME,
                                       sizeof(PROD_NAME) ) == 0 ) )
                        {
                            *p_ipod_id = strtoll( curdev->name, NULL, 16 );
                            i_ret = 0;
                            break;
                        }
                    }
               }

                if( !i_ret ) break;
            }
        }

        sysfs_close_bus( bus );
    }
#endif

    return i_ret;
}

#else /* !defined( UNDER_CE ) */

void *drms_alloc( char *psz_homedir ){ return 0; }
void drms_free( void *a ){}
void drms_decrypt( void *a, uint32_t *b, uint32_t c  ){}
int drms_init( void *a, uint32_t b, uint8_t *c, uint32_t d ){ return -1; }

#endif /* defined( UNDER_CE ) */
