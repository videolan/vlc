/*****************************************************************************
 * drms.c: DRMS
 *****************************************************************************
 * Copyright (C) 2004 VideoLAN
 * $Id: drms.c,v 1.5 2004/01/16 18:26:57 sam Exp $
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

#include <stdlib.h>                                      /* malloc(), free() */

#ifdef WIN32
#   include <io.h>
#else
#   include <stdio.h>
#endif

#include <vlc/vlc.h>

#ifdef HAVE_ERRNO_H
#   include <errno.h>
#endif

#ifdef WIN32
#   include <tchar.h>
#   include <shlobj.h>
#   include <windows.h>
#endif

#ifdef HAVE_SYS_STAT_H
   #include <sys/stat.h>
#endif
#ifdef HAVE_SYS_TYPES_H
   #include <sys/types.h>
#endif

#include "drms.h"
#include "drmstables.h"

#include "libmp4.h"

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

/*****************************************************************************
 * shuffle_s: shuffle structure
 *****************************************************************************
 * This structure stores the static information needed to shuffle data using
 * a custom algorithm.
 *****************************************************************************/
struct shuffle_s
{
    uint32_t p_commands[ 20 ];
    uint32_t p_bordel[ 16 ];
};

/*****************************************************************************
 * drms_s: DRMS structure
 *****************************************************************************
 * This structure stores the static information needed to decrypt DRMS data.
 *****************************************************************************/
struct drms_s
{
    uint32_t i_user;
    uint32_t i_key;
    uint8_t *p_iviv;
    uint8_t *p_name;
    uint32_t i_name_len;

    uint32_t p_key[ 4 ];
    struct aes_s aes;

    char    *psz_homedir;
};

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static void InitAES       ( struct aes_s *, uint32_t * );
static void DecryptAES    ( struct aes_s *, uint32_t *, const uint32_t * );

static void InitMD5       ( struct md5_s * );
static void AddMD5        ( struct md5_s *, const uint8_t *, uint32_t );
static void AddNativeMD5  ( struct md5_s *, uint32_t *, uint32_t );
static void EndMD5        ( struct md5_s * );
static void Digest        ( struct md5_s *, const uint32_t * );

static void InitShuffle   ( struct shuffle_s *, uint32_t * );
static void DoShuffle     ( struct shuffle_s *, uint8_t *, uint32_t );
static void Bordelize     ( uint32_t *, uint32_t );

static int GetSystemKey   ( uint32_t * );
static int WriteUserKey   ( void *, uint32_t * );
static int ReadUserKey    ( void *, uint32_t * );
static int GetUserKey     ( void *, uint32_t * );

static int GetSCIData     ( uint32_t **, uint32_t * );
static int HashSystemInfo ( struct md5_s * );

/*****************************************************************************
 * BlockXOR: XOR two 128 bit blocks
 *****************************************************************************/
static inline void BlockXOR( uint32_t *p_dest, uint32_t *p_s1, uint32_t *p_s2 )
{
    uint32_t i;

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

    p_drms->psz_homedir = malloc( PATH_MAX );
    if( p_drms->psz_homedir != NULL )
    {
        strncpy( p_drms->psz_homedir, psz_homedir, PATH_MAX );
        p_drms->psz_homedir[ PATH_MAX - 1 ] = '\0';
    }
    else
    {
        free( (void *)p_drms );
        p_drms = NULL;
    }

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

    if( p_drms->p_iviv != NULL )
    {
        free( (void *)p_drms->p_iviv );
    }

    if( p_drms->psz_homedir != NULL )
    {
        free( (void *)p_drms->psz_homedir );
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
    uint32_t i_blocks, i;

    /* AES is a block cypher, round down the byte count */
    i_blocks = i_bytes / 16;
    i_bytes = i_blocks * 16;

    /* Initialise the key */
    memcpy( p_key, p_drms->p_key, 4 * sizeof(uint32_t) );

    /* Unscramble */
    for( i = i_blocks; i--; )
    {
        uint32_t  p_tmp[ 4 ];

        DecryptAES( &p_drms->aes, p_tmp, p_buffer );
        BlockXOR( p_tmp, p_key, p_tmp );

        /* Use the previous scrambled data as the key for next block */
        memcpy( p_key, p_buffer, 4 * sizeof(uint32_t) );

        /* Copy unscrambled data back to the buffer */
        memcpy( p_buffer, p_tmp, 4 * sizeof(uint32_t) );

        p_buffer += 4;
    }
}

/*****************************************************************************
 * drms_init: initialise a DRMS structure
 *****************************************************************************/
int drms_init( void *_p_drms, uint32_t i_type,
               uint8_t *p_info, uint32_t i_len )
{
    struct drms_s *p_drms = (struct drms_s *)_p_drms;
    int i_ret = 0;

    switch( i_type )
    {
        case FOURCC_user:
        {
            if( i_len < sizeof(p_drms->i_user) )
            {
                i_ret = -1;
                break;
            }

            p_drms->i_user = U32_AT( p_info );
        }
        break;

        case FOURCC_key:
        {
            if( i_len < sizeof(p_drms->i_key) )
            {
                i_ret = -1;
                break;
            }

            p_drms->i_key = U32_AT( p_info );
        }
        break;

        case FOURCC_iviv:
        {
            if( i_len < sizeof(p_drms->p_key) )
            {
                i_ret = -1;
                break;
            }

            p_drms->p_iviv = malloc( sizeof(p_drms->p_key) );
            if( p_drms->p_iviv == NULL )
            {
                i_ret = -1;
                break;
            }

            memcpy( p_drms->p_iviv, p_info, sizeof(p_drms->p_key) );
        }
        break;

        case FOURCC_name:
        {
            p_drms->i_name_len = strlen( p_info );

            p_drms->p_name = malloc( p_drms->i_name_len );
            if( p_drms->p_name == NULL )
            {
                i_ret = -1;
                break;
            }

            memcpy( p_drms->p_name, p_info, p_drms->i_name_len );
        }
        break;

        case FOURCC_priv:
        {
            uint32_t p_priv[ 64 ];
            struct md5_s md5;

            if( i_len < 64 )
            {
                i_ret = -1;
                break;
            }

            InitMD5( &md5 );
            AddMD5( &md5, p_drms->p_name, p_drms->i_name_len );
            AddMD5( &md5, p_drms->p_iviv, sizeof(p_drms->p_key) );
            EndMD5( &md5 );

            if( GetUserKey( p_drms, p_drms->p_key ) )
            {
                i_ret = -1;
                break;
            }

            InitAES( &p_drms->aes, p_drms->p_key );

            memcpy( p_priv, p_info, 64 );
            memcpy( p_drms->p_key, md5.p_digest, sizeof(p_drms->p_key) );
            drms_decrypt( p_drms, p_priv, sizeof(p_priv) );

            InitAES( &p_drms->aes, p_priv + 6 );
            memcpy( p_drms->p_key, p_priv + 12, sizeof(p_drms->p_key) );

            free( (void *)p_drms->psz_homedir );
            p_drms->psz_homedir = NULL;
            free( (void *)p_drms->p_name );
            p_drms->p_name = NULL;
            free( (void *)p_drms->p_iviv );
            p_drms->p_iviv = NULL;
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
    uint32_t i, t, i_key, i_tmp;

    memset( p_aes->pp_enc_keys[1], 0, 4 * sizeof(uint32_t) );
    memcpy( p_aes->pp_enc_keys[0], p_key, 4 * sizeof(uint32_t) );

    /* Generate the key tables */
    i_tmp = p_aes->pp_enc_keys[ 0 ][ 3 ];

    for( i_key = 0; i_key < AES_KEY_COUNT; i_key++ )
    {
        uint32_t j;

        i_tmp = AES_ROR( i_tmp, 8 );

        j = p_aes_table[ i_key ];

        j ^= p_aes_encrypt[ (i_tmp >> 24) & 0xFF ]
              ^ AES_ROR( p_aes_encrypt[ (i_tmp >> 16) & 0xFF ], 8 )
              ^ AES_ROR( p_aes_encrypt[ (i_tmp >> 8) & 0xFF ], 16 )
              ^ AES_ROR( p_aes_encrypt[ i_tmp & 0xFF ], 24 );

        j ^= p_aes->pp_enc_keys[ i_key ][ 0 ];
        p_aes->pp_enc_keys[ i_key + 1 ][ 0 ] = j;
        j ^= p_aes->pp_enc_keys[ i_key ][ 1 ];
        p_aes->pp_enc_keys[ i_key + 1 ][ 1 ] = j;
        j ^= p_aes->pp_enc_keys[ i_key ][ 2 ];
        p_aes->pp_enc_keys[ i_key + 1 ][ 2 ] = j;
        j ^= p_aes->pp_enc_keys[ i_key ][ 3 ];
        p_aes->pp_enc_keys[ i_key + 1 ][ 3 ] = j;

        i_tmp = j;
    }

    memcpy( p_aes->pp_dec_keys[ 0 ],
            p_aes->pp_enc_keys[ 0 ], 4 * sizeof(uint32_t) );

    for( i = 1; i < AES_KEY_COUNT; i++ )
    {
        for( t = 0; t < 4; t++ )
        {
            uint32_t j, k, l, m, n;

            j = p_aes->pp_enc_keys[ i ][ t ];

            k = (((j >> 7) & 0x01010101) * 27) ^ ((j & 0xFF7F7F7F) << 1);
            l = (((k >> 7) & 0x01010101) * 27) ^ ((k & 0xFF7F7F7F) << 1);
            m = (((l >> 7) & 0x01010101) * 27) ^ ((l & 0xFF7F7F7F) << 1);

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
    uint32_t round, t;

    for( t = 0; t < 4; t++ )
    {
        /* FIXME: are there any endianness issues here? */
        p_wtxt[ t ] = p_src[ t ] ^ p_aes->pp_enc_keys[ AES_KEY_COUNT ][ t ];
    }

    /* Rounds 0 - 8 */
    for( round = 0; round < (AES_KEY_COUNT - 1); round++ )
    {
        for( t = 0; t < 4; t++ )
        {
            p_tmp[ t ] = AES_XOR_ROR( p_aes_itable, p_wtxt );
        }

        for( t = 0; t < 4; t++ )
        {
            p_wtxt[ t ] = p_tmp[ t ]
                    ^ p_aes->pp_dec_keys[ (AES_KEY_COUNT - 1) - round ][ t ];
        }
    }

    /* Final round (9) */
    for( t = 0; t < 4; t++ )
    {
        p_dest[ t ] = AES_XOR_ROR( p_aes_decrypt, p_wtxt );
        p_dest[ t ] ^= p_aes->pp_dec_keys[ (AES_KEY_COUNT - 1) - round ][ t ];
    }
}

/*****************************************************************************
 * InitMD5: initialise an MD5 message
 *****************************************************************************
 * The MD5 message-digest algorithm is described in RFC 1321
 *****************************************************************************/
static void InitMD5( struct md5_s *p_md5 )
{
    p_md5->p_digest[ 0 ] = 0x67452301;
    p_md5->p_digest[ 1 ] = 0xEFCDAB89;
    p_md5->p_digest[ 2 ] = 0x98BADCFE;
    p_md5->p_digest[ 3 ] = 0x10325476;

    memset( p_md5->p_data, 0, 16 * sizeof(uint32_t) );
    p_md5->i_bits = 0;
}

/*****************************************************************************
 * AddMD5: add i_len bytes to an MD5 message
 *****************************************************************************/
static void AddMD5( struct md5_s *p_md5, const uint8_t *p_src, uint32_t i_len )
{
    uint32_t i_current; /* Current bytes in the spare buffer */
    uint32_t i_offset = 0;

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
        memcpy( p_tmp, p_src + i_offset, 16 * sizeof(uint32_t) );
        Digest( p_md5, p_tmp );
        i_offset += 64;
        i_len -= 64;
    }

    /* Copy our remaining data to the message's spare buffer */
    memcpy( ((uint8_t *)p_md5->p_data) + i_current, p_src + i_offset, i_len );
}

/*****************************************************************************
 * AddNativeMD5: add i_len big-endian uin32_t to an MD5 message
 *****************************************************************************
 * FIXME: I don't really understand what this is supposed to do, especially
 * with big values of i_len ...
 *****************************************************************************/
static void AddNativeMD5( struct md5_s *p_md5, uint32_t *p_src, uint32_t i_len )
{
    uint32_t i, x, y;
    /* XXX: it's 32, not 16! */
    uint32_t p_tmp[ 32 ];

    /* Convert big endian p_src to native-endian p_tmp */
    for( x = i_len; x; x -= y )
    {
        /* XXX: this looks weird! */
        y = x > 32 ? 32 : x;

        for( i = 0; i < y; i++ )
        {
            p_tmp[ i ] = U32_AT(p_src + i);
        }
    }

    AddMD5( p_md5, (uint8_t *)p_tmp, i_len * sizeof(uint32_t) );
}

/*****************************************************************************
 * EndMD5: finish an MD5 message
 *****************************************************************************
 * This function adds adequate padding to the end of the message, and appends
 * the bit count so that we end at a block boundary.
 *****************************************************************************/
static void EndMD5( struct md5_s *p_md5 )
{
    uint32_t i_current;

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
static void Digest( struct md5_s *p_md5, const uint32_t *p_input )
{
    uint32_t a, b, c, d;

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
 * InitShuffle: initialise a shuffle structure
 *****************************************************************************
 * This function initialises tables in the p_shuffle structure that will be
 * used later by DoShuffle. The only external parameter is p_sys_key.
 *****************************************************************************/
static void InitShuffle( struct shuffle_s *p_shuffle, uint32_t *p_sys_key )
{
    uint32_t p_native_key[ 4 ];
    uint32_t i, i_seed = 0x5476212A; /* *!vT */

    /* Store the system key in native endianness */
    for( i = 0; i < 4; i++ )
    {
        p_native_key[ i ] = U32_AT(p_sys_key + i);
    }

    /* Fill p_commands using the native key and our seed */
    for( i = 0; i < 20; i++ )
    {
        struct md5_s md5;
        int32_t i_hash;

        InitMD5( &md5 );
        AddNativeMD5( &md5, p_native_key, 4 );
        AddNativeMD5( &md5, &i_seed, 1 );
        EndMD5( &md5 );

        i_seed++;

        i_hash = ((int32_t)U32_AT(md5.p_digest)) % 1024;

        p_shuffle->p_commands[ i ] = i_hash < 0 ? i_hash * -1 : i_hash;
    }

    /* Fill p_bordel with completely meaningless initial values.
     * FIXME: check endianness issues. */
    p_shuffle->p_bordel[  0 ] = p_native_key[ 0 ];
    p_shuffle->p_bordel[  1 ] = 0x68723876; /* v8rh */
    p_shuffle->p_bordel[  2 ] = 0x41617376; /* vsaA */
    p_shuffle->p_bordel[  3 ] = 0x4D4B4F76; /* voKM */

    p_shuffle->p_bordel[  4 ] = p_native_key[ 1 ];
    p_shuffle->p_bordel[  5 ] = 0x48556646; /* FfUH */
    p_shuffle->p_bordel[  6 ] = 0x38393725; /* %798 */
    p_shuffle->p_bordel[  7 ] = 0x2E3B5B3D; /* =[;. */

    p_shuffle->p_bordel[  8 ] = p_native_key[ 2 ];
    p_shuffle->p_bordel[  9 ] = 0x37363866; /* f867 */
    p_shuffle->p_bordel[ 10 ] = 0x30383637; /* 7680 */
    p_shuffle->p_bordel[ 11 ] = 0x34333661; /* a634 */

    p_shuffle->p_bordel[ 12 ] = p_native_key[ 3 ];
    p_shuffle->p_bordel[ 13 ] = 0x37386162; /* ba87 */
    p_shuffle->p_bordel[ 14 ] = 0x494F6E66; /* fnOI */
    p_shuffle->p_bordel[ 15 ] = 0x2A282966; /* f)(* */
}

/*****************************************************************************
 * DoShuffle: shuffle i_len bytes of a buffer
 *****************************************************************************
 * This is so ugly and uses so many MD5 checksums that it is most certainly
 * one-way, though why it needs to be so complicated is beyond me.
 *****************************************************************************/
static void DoShuffle( struct shuffle_s *p_shuffle,
                       uint8_t *p_buffer, uint32_t i_len )
{
    struct md5_s md5;
    uint32_t i;

    /* Randomize p_bordel and compute its MD5 checksum */
    for( i = 0; i < 20; i++ )
    {
        if( p_shuffle->p_commands[ i ] )
        {
            Bordelize( p_shuffle->p_bordel, p_shuffle->p_commands[ i ] );
        }
    }

    InitMD5( &md5 );
    AddNativeMD5( &md5, p_shuffle->p_bordel, 16 );
    EndMD5( &md5 );

    /* There are only 16 bytes in an MD5 hash */
    if( i_len > 16 )
    {
        i_len = 16;
    }

    /* XOR our buffer with the computed checksum */
    for( i = 0; i < i_len; i++ )
    {
        p_buffer[ i ] ^= ((uint8_t *)&md5.p_digest)[ i ];
    }
}

/*****************************************************************************
 * Bordelize: helper for DoShuffle
 *****************************************************************************
 * Using the MD5 hash of a string is probably not one-way enough. This
 * function randomises p_bordel depending on the value of i_command to make
 * things even more messy in p_bordel.
 *****************************************************************************/
static void Bordelize( uint32_t *p_bordel, uint32_t i_command )
{
    uint32_t i, x;

    i = (i_command / 16) & 15;
    x = (~(i_command & 15)) & 15;

    if( (i_command & 768) == 768 )
    {
        x = (~i) & 15;
        i = i_command & 15;

        p_bordel[ i ] = p_bordel[ ((16 - x) & 15) ] + p_bordel[ (15 - x) ];
    }
    else if( (i_command & 512) == 512 )
    {
        p_bordel[ i ] ^= p_shuffle_xor[ 15 - i ][ x ];
    }
    else if( (i_command & 256) == 256 )
    {
        p_bordel[ i ] -= p_shuffle_sub[ 15 - i ][ x ];
    }
    else
    {
        p_bordel[ i ] += p_shuffle_add[ 15 - i ][ x ];
    }
}

/*****************************************************************************
 * GetSystemKey: get the system key
 *****************************************************************************
 * Compute the system key from various system information, see HashSystemInfo.
 *****************************************************************************/
static int GetSystemKey( uint32_t *p_sys_key )
{
    struct md5_s md5;
    uint32_t p_tmp_key[ 4 ];

    InitMD5( &md5 );
    if( HashSystemInfo( &md5 ) )
    {
        return -1;
    }
    EndMD5( &md5 );

    /* Write our digest to p_tmp_key */
    memcpy( p_tmp_key, md5.p_digest, 4 * sizeof(uint32_t) );

    InitMD5( &md5 );
    AddMD5( &md5, "YuaFlafu", 8 );
    AddMD5( &md5, (uint8_t *)p_tmp_key, 6 );
    AddMD5( &md5, (uint8_t *)p_tmp_key, 6 );
    AddMD5( &md5, (uint8_t *)p_tmp_key, 6 );
    AddMD5( &md5, "zPif98ga", 8 );
    EndMD5( &md5 );

    memcpy( p_sys_key, md5.p_digest, 4 * sizeof(uint32_t) );

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

        file = fopen( psz_path, "w" );
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

    file = fopen( psz_path, "r" );
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
    struct drms_s *p_drms = (struct drms_s *)_p_drms;
    struct aes_s aes;
    struct shuffle_s shuffle;
    uint32_t i, y;
    uint32_t *p_tmp;
    uint32_t *p_cur_key;
    uint32_t p_sys_key[ 4 ];
    uint32_t i_sci_size;
    uint32_t *pp_sci[ 2 ];
    int i_ret = -1;

    uint32_t p_sci_key[ 4 ] =
    {
        0x6e66556d, /* nfUm */
        0x6e676f70, /* ngop */
        0x67666461, /* gfda */
        0x33373866  /* 378f */
    };

    if( !ReadUserKey( p_drms, p_user_key ) )
    {
        return 0;
    }

    if( GetSystemKey( p_sys_key ) )
    {
        return -1;
    }

    if( GetSCIData( pp_sci + 0, &i_sci_size ) )
    {
        return -1;
    }

    p_tmp = pp_sci[ 0 ];
    pp_sci[ 1 ] = (uint32_t *)(((uint8_t *)pp_sci[ 0 ]) + i_sci_size);
    i_sci_size -= sizeof(*pp_sci[ 0 ]);

    InitAES( &aes, p_sys_key );

    for( i = 0, p_cur_key = p_sci_key;
         i < i_sci_size / sizeof(p_drms->p_key); i++ )
    {
        y = i * sizeof(*pp_sci[ 0 ]);

        DecryptAES( &aes, pp_sci[ 1 ] + y + 1, pp_sci[ 0 ] + y + 1 );
        BlockXOR( pp_sci[ 1 ] + y + 1, p_cur_key, pp_sci[ 1 ] + y + 1 );

        p_cur_key = pp_sci[ 0 ] + y + 1;
    }

    /* Shuffle pp_sci[ 1 ] using a custom routine */
    InitShuffle( &shuffle, p_sys_key );

    for( i = 0; i < i_sci_size / sizeof(p_drms->p_key); i++ )
    {
        y = i * sizeof(*pp_sci[ 1 ]);

        DoShuffle( &shuffle, (uint8_t *)(pp_sci[ 1 ] + y + 1),
                   sizeof(p_drms->p_key) );
    }

    y = 0;
    i = U32_AT( &pp_sci[ 1 ][ 5 ] );
    i_sci_size -= 21 * sizeof(*pp_sci[ 1 ]);
    pp_sci[ 1 ] += 22;
    pp_sci[ 0 ] = NULL;

    while( i_sci_size > 0 && i > 0 )
    {
        if( pp_sci[ 0 ] == NULL )
        {
            i_sci_size -= 18 * sizeof(*pp_sci[ 1 ]);
            if( i_sci_size <= 0 )
            {
                break;
            }

            pp_sci[ 0 ] = pp_sci[ 1 ];
            y = U32_AT( &pp_sci[ 1 ][ 17 ] );
            pp_sci[ 1 ] += 18;
        }

        if( !y )
        {
            i--;
            pp_sci[ 0 ] = NULL;
            continue;
        }

        if( U32_AT( &pp_sci[ 0 ][ 0 ] ) == p_drms->i_user &&
            ( i_sci_size >=
              (sizeof(p_drms->p_key) + sizeof(pp_sci[ 1 ][ 0 ]) ) ) &&
            ( ( U32_AT( &pp_sci[ 1 ][ 0 ] ) == p_drms->i_key ) ||
              ( !p_drms->i_key ) || ( pp_sci[ 1 ] == (pp_sci[ 0 ] + 18) ) ) )
        {
            memcpy( p_user_key, &pp_sci[ 1 ][ 1 ], sizeof(p_drms->p_key) );
            WriteUserKey( p_drms, p_user_key );
            i_ret = 0;
            break;
        }

        y--;
        pp_sci[ 1 ] += 5;
        i_sci_size -= 5 * sizeof(*pp_sci[ 1 ]);
    }

    free( (void *)p_tmp );

    return i_ret;
}

/*****************************************************************************
 * GetSCIData: get SCI data from "SC Info.sidb"
 *****************************************************************************
 * Read SCI data from "\Apple Computer\iTunes\SC Info\SC Info.sidb"
 *****************************************************************************/
static int GetSCIData( uint32_t **pp_sci, uint32_t *p_sci_size )
{
    int i_ret = -1;

#ifdef WIN32
    HANDLE i_file;
    DWORD i_size, i_read;
    TCHAR p_path[ PATH_MAX ];
    TCHAR *p_filename = _T("\\Apple Computer\\iTunes\\SC Info\\SC Info.sidb");

    typedef HRESULT (WINAPI *SHGETFOLDERPATH)( HWND, int, HANDLE, DWORD,
                                               LPTSTR );

    HINSTANCE shfolder_dll = NULL;
    SHGETFOLDERPATH dSHGetFolderPath = NULL;

    if( ( shfolder_dll = LoadLibrary( _T("SHFolder.dll") ) ) != NULL )
    {
        dSHGetFolderPath =
            (SHGETFOLDERPATH)GetProcAddress( shfolder_dll,
#ifdef _UNICODE
                                             _T("SHGetFolderPathW") );
#else
                                             _T("SHGetFolderPathA") );
#endif
    }

    if( dSHGetFolderPath != NULL &&
        SUCCEEDED( dSHGetFolderPath( NULL, CSIDL_COMMON_APPDATA,
                                     NULL, 0, p_path ) ) )
    {
        _tcsncat( p_path, p_filename, min( _tcslen( p_filename ),
                  (PATH_MAX-1) - _tcslen( p_path ) ) );

        i_file = CreateFile( p_path, GENERIC_READ, 0, NULL,
                             OPEN_EXISTING, 0, NULL );
        if( i_file != INVALID_HANDLE_VALUE )
        {
            i_size = GetFileSize( i_file, NULL );
            if( i_size != INVALID_FILE_SIZE &&
                i_size > (sizeof(*pp_sci[ 0 ]) * 22) )
            {
                *pp_sci = malloc( i_size * 2 );
                if( *pp_sci != NULL )
                {
                    if( ReadFile( i_file, *pp_sci, i_size, &i_read, NULL ) &&
                        i_read == i_size )
                    {
                        *p_sci_size = i_size;
                        i_ret = 0;
                    }
                    else
                    {
                        free( (void *)*pp_sci );
                        *pp_sci = NULL;
                    }
                }
            }

            CloseHandle( i_file );
        }
    }

    if( shfolder_dll != NULL )
    {
        FreeLibrary( shfolder_dll );
    }
#endif

    return i_ret;
}

/*****************************************************************************
 * HashSystemInfo: add system information to an MD5 hash
 *****************************************************************************
 * This function adds the C: hard drive serial number, BIOS version, CPU type
 * and Windows version to an MD5 hash.
 *****************************************************************************/
static int HashSystemInfo( struct md5_s *p_md5 )
{
    int i_ret = 0;

#ifdef WIN32
    HKEY i_key;
    uint32_t i;
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

    AddMD5( p_md5, "cache-control", 13 );
    AddMD5( p_md5, "Ethernet", 8 );

    GetVolumeInformation( _T("C:\\"), NULL, 0, &i_serial,
                          NULL, NULL, NULL, 0 );
    AddMD5( p_md5, (uint8_t *)&i_serial, 4 );

    for( i = 0; i < sizeof(p_reg_keys)/sizeof(p_reg_keys[ 0 ]); i++ )
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
                AddMD5( p_md5, (uint8_t *)p_reg_buf, i_size );
            }

            free( p_reg_buf );
        }

        RegCloseKey( i_key );
    }

#else
    i_ret = -1;
#endif

    return i_ret;
}

