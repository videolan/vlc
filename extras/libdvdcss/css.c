/*****************************************************************************
 * css.c: Functions for DVD authentification and unscrambling
 *****************************************************************************
 * Copyright (C) 1999-2001 VideoLAN
 * $Id: css.c,v 1.15 2001/11/12 20:16:32 sam Exp $
 *
 * Author: Stéphane Borel <stef@via.ecp.fr>
 *         Håkan Hjort <d95hjort@dtek.chalmers.se>
 *
 * based on:
 *  - css-auth by Derek Fawcus <derek@spider.com>
 *  - DVD CSS ioctls example program by Andrew T. Veliath <andrewtv@usa.net>
 *  - The Divide and conquer attack by Frank A. Stevenson <frank@funcom.com>
 *  - DeCSSPlus by Ethan Hawke
 *  - DecVOB
 *  see http://www.lemuria.org/DeCSS/ by Tom Vogt for more information.
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include "defs.h"

#include <stdio.h>
#include <stdlib.h>

#include <string.h>

#include "config.h"
#include "common.h"

#include "videolan/dvdcss.h"
#include "libdvdcss.h"

#include "csstables.h"
#include "ioctl.h"

#ifdef HAVE_CSSKEYS
#  include "csskeys.h"
#endif


/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  CSSGetASF    ( dvdcss_handle dvdcss );
static void CSSCryptKey  ( int i_key_type, int i_varient,
                           u8 const * p_challenge, u8* p_key );
static void CSSDecryptKey( u8* p_crypted, u8* p_key, u8 );
static int  CSSDiscCrack ( dvdcss_handle dvdcss, u8 * p_disc_key );
static int  CSSTitleCrack( int i_start, unsigned char * p_crypted,
                           unsigned char * p_decrypted,
                           dvd_key_t * p_sector_key, dvd_key_t * p_key );

/*****************************************************************************
 * CSSTest : check if the disc is encrypted or not
 *****************************************************************************/
int CSSTest( dvdcss_handle dvdcss )
{
    int i_ret, i_copyright;

    i_ret = ioctl_ReadCopyright( dvdcss->i_fd, 0 /* i_layer */, &i_copyright );

    if( i_ret < 0 )
    {
        /* Since it's the first ioctl we try to issue, we add a notice */
        _dvdcss_error( dvdcss, "css error: ioctl_ReadCopyright failed, "
                       "make sure there is a DVD in the drive, and that "
                       "DVD ioctls were compiled in this libdvdcss version" );

        return i_ret;
    }

    return i_copyright;
}

/*****************************************************************************
 * CSSAuth : CSS Structure initialisation and DVD authentication.
 *****************************************************************************
 * It simulates the mutual authentication between logical unit and host.
 * Since we don't need the disc key to find the title key, we just run the
 * basic unavoidable commands to authenticate device and disc.
 *****************************************************************************/
int CSSAuth( dvdcss_handle dvdcss )
{
    /* structures defined in cdrom.h or dvdio.h */
    unsigned char p_buffer[10];
    char psz_warning[48];
    int  i_ret = -1;
    int  i;

    dvdcss->css.i_agid = 0;

    /* Test authentication success */
    switch( CSSGetASF( dvdcss ) )
    {
        case -1:
            return -1;

        case 1:
            _dvdcss_debug( dvdcss, "already authenticated" );
            break;

        case 0:
            _dvdcss_debug( dvdcss, "need to authenticate" );
            break;
    }

    /* Init sequence, request AGID */
    for( i = 1; i < 4 ; ++i )
    {
        snprintf( psz_warning, sizeof(psz_warning), "requesting AGID %d", i );
        _dvdcss_debug( dvdcss, psz_warning );

        i_ret = ioctl_ReportAgid( dvdcss->i_fd, &dvdcss->css.i_agid );

        if( i_ret != -1 )
        {
            /* No error during ioctl: we know the device is authenticated */
            break;
        }

        _dvdcss_error( dvdcss, "ioctl_ReportAgid failed, invalidating" );

        dvdcss->css.i_agid = 0;
        ioctl_InvalidateAgid( dvdcss->i_fd, &dvdcss->css.i_agid );
    }

    /* Unable to authenticate without AGID */
    if( i_ret == -1 )
    {
        _dvdcss_error( dvdcss, "ioctl_ReportAgid failed, fatal" );
        return -1;
    }

    for( i = 0 ; i < 10; ++i )
    {
        dvdcss->css.disc.p_challenge[i] = i;
    }

    /* Get challenge from host */
    for( i = 0 ; i < 10 ; ++i )
    {
        p_buffer[9-i] = dvdcss->css.disc.p_challenge[i];
    }

    /* Send challenge to LU */
    if( ioctl_SendChallenge( dvdcss->i_fd, &dvdcss->css.i_agid, p_buffer ) < 0 )
    {
        _dvdcss_error( dvdcss, "ioctl_SendChallenge failed" );
        return -1;
    }

    /* Get key1 from LU */
    if( ioctl_ReportKey1( dvdcss->i_fd, &dvdcss->css.i_agid, p_buffer ) < 0)
    {
        _dvdcss_error( dvdcss, "ioctl_ReportKey1 failed" );
        return -1;
    }

    /* Send key1 to host */
    for( i = 0 ; i < KEY_SIZE ; i++ )
    {
        dvdcss->css.disc.p_key1[i] = p_buffer[4-i];
    }

    for( i = 0 ; i < 32 ; ++i )
    {
        CSSCryptKey( 0, i, dvdcss->css.disc.p_challenge,
                           dvdcss->css.disc.p_key_check );

        if( memcmp( dvdcss->css.disc.p_key_check,
                    dvdcss->css.disc.p_key1, KEY_SIZE ) == 0 )
        {
            snprintf( psz_warning, sizeof(psz_warning),
                      "drive authentic, using variant %d", i );
            _dvdcss_debug( dvdcss, psz_warning );
            dvdcss->css.disc.i_varient = i;
            break;
        }
    }

    if( i == 32 )
    {
        _dvdcss_error( dvdcss, "drive would not authenticate" );
        return -1;
    }

    /* Get challenge from LU */
    if( ioctl_ReportChallenge( dvdcss->i_fd, &dvdcss->css.i_agid, p_buffer ) < 0 )
    {
        _dvdcss_error( dvdcss, "ioctl_ReportKeyChallenge failed" );
        return -1;
    }

    /* Send challenge to host */
    for( i = 0 ; i < 10 ; ++i )
    {
        dvdcss->css.disc.p_challenge[i] = p_buffer[9-i];
    }

    CSSCryptKey( 1, dvdcss->css.disc.i_varient,
                    dvdcss->css.disc.p_challenge,
                    dvdcss->css.disc.p_key2 );

    /* Get key2 from host */
    for( i = 0 ; i < KEY_SIZE ; ++i )
    {
        p_buffer[4-i] = dvdcss->css.disc.p_key2[i];
    }

    /* Send key2 to LU */
    if( ioctl_SendKey2( dvdcss->i_fd, &dvdcss->css.i_agid, p_buffer ) < 0 )
    {
        _dvdcss_error( dvdcss, "ioctl_SendKey2 failed" );
        return -1;
    }

    _dvdcss_debug( dvdcss, "authentication established" );

    memcpy( dvdcss->css.disc.p_challenge,
            dvdcss->css.disc.p_key1, KEY_SIZE );
    memcpy( dvdcss->css.disc.p_challenge+KEY_SIZE,
            dvdcss->css.disc.p_key2, KEY_SIZE );

    CSSCryptKey( 2, dvdcss->css.disc.i_varient,
                    dvdcss->css.disc.p_challenge,
                    dvdcss->css.disc.p_key_check );

    _dvdcss_debug( dvdcss, "received session key" );

    if( dvdcss->css.i_agid < 0 )
    {
        return -1;
    }

    /* Test authentication success */
    switch( CSSGetASF( dvdcss ) )
    {
        case -1:
            return -1;

        case 1:
            _dvdcss_debug( dvdcss, "already authenticated" );
            return 0;

        case 0:
            _dvdcss_debug( dvdcss, "need to get disc key" );
            return 0;
    }

    return -1;
}

/*****************************************************************************
 * CSSGetDiscKey : get disc key and optionnaly decrypts it.
 *****************************************************************************
 * This function should only be called if DVD ioctls are present.
 * Two decryption methods are then offered:
 *  -disc key hash crack,
 *  -decryption with player keys if they are available.
 *****************************************************************************/
int CSSGetDiscKey( dvdcss_handle dvdcss )
{
    unsigned char   p_buffer[2048 + 4 + 1];
#ifdef HAVE_CSSKEYS
    dvd_key_t       disc_key;
    dvd_key_t       test_key;
#endif
    int i;

    if( CSSAuth( dvdcss ) )
    {
        return -1;
    }

    /* Get encrypted disc key */
    if( ioctl_ReadDiscKey( dvdcss->i_fd, &dvdcss->css.i_agid, p_buffer ) < 0 )
    {
        _dvdcss_error( dvdcss, "ioctl_ReadDiscKey failed" );
        return -1;
    }

    /* Unencrypt disc key using bus key */
    for( i = 0 ; i < 2048 ; i++ )
    {
        p_buffer[ i ] ^= dvdcss->css.disc.p_key_check[ 4 - (i % KEY_SIZE) ];
    }
    memcpy( dvdcss->css.disc.p_disc_key, p_buffer, 2048 );

    switch( dvdcss->i_method )
    {
        case DVDCSS_METHOD_KEY:
#ifdef HAVE_CSSKEYS
            /* Decrypt disc key with player keys from csskeys.h */
            _dvdcss_debug( dvdcss, "decrypting disc key with player keys" );
            i = 0;
            do
            {
                /* Take encrypted disc key and decrypt it */
                memcpy( disc_key,
                        dvdcss->css.disc.p_disc_key
                      + playerkeys[i].i_offset,
                        KEY_SIZE );
                CSSDecryptKey( disc_key, playerkeys[i].p_key, 0 );

                /* Encrypt disc key hash with disc key to
                 * check we have disc key */
                memcpy( test_key, dvdcss->css.disc.p_disc_key, KEY_SIZE );
                CSSDecryptKey( test_key, disc_key, 0);

                i++;

            } while( ( playerkeys[i].i_offset != -1 ) &&
                     ( memcmp( test_key, disc_key, KEY_SIZE ) ) );

	    /* The decrypted disk key will replace the disk key hash */
            memcpy( dvdcss->css.disc.p_disc_key, disc_key, KEY_SIZE );
            break;
#else
            dvdcss->i_method = DVDCSS_METHOD_DISC;            
#endif
        case DVDCSS_METHOD_DISC:
            /* Crack Disc key to be able to use it */
            _dvdcss_debug( dvdcss, "cracking disc key with key hash" );
            _dvdcss_debug( dvdcss, "building 64MB table ... this will take some time" );
            CSSDiscCrack( dvdcss, dvdcss->css.disc.p_disc_key );
            break;

        default:
            _dvdcss_debug( dvdcss, "disc key won't be decrypted" );
    }

    return 0;
}


/*****************************************************************************
 * CSSGetTitleKey : get title key.
 *****************************************************************************/
int CSSGetTitleKey( dvdcss_handle dvdcss, int i_pos )
{
    dvd_key_t   p_key;
    int         i,j;

    if( ( dvdcss->i_method == DVDCSS_METHOD_TITLE )
        || ( dvdcss->b_ioctls == 0 ) )
    {
        /*
         * Title key cracking method from Ethan Hawke,
         * with Frank A. Stevenson algorithm.
         * Does not use any player key table and ioctls.
         */
        u8          p_buf[0x800];
        u8          p_packstart[4] = { 0x00, 0x00, 0x01, 0xba };
        boolean_t   b_encrypted;
        boolean_t   b_stop_scanning;
        int         i_blocks_read;
        int         i_best_plen;
        int         i_best_p;

        _dvdcss_debug( dvdcss, "cracking title key ... this may take some time" );

        for( i = 0 ; i < KEY_SIZE ; i++ )
        {
            p_key[i] = 0;
        }

        b_encrypted = 0;
        b_stop_scanning = 0;
        i_blocks_read = 0;

        do
        {
            i_pos = dvdcss_seek( dvdcss, i_pos, DVDCSS_NOFLAGS );
            if( dvdcss_read( dvdcss, p_buf, 1, DVDCSS_NOFLAGS ) != 1 ) break;

            /* Stop when we find a non MPEG stream block */
            if( memcmp( p_buf, p_packstart, 4 ) )
            {
                /* The title is unencrypted */
                if( !b_encrypted )
                    break;
                /* dvdcss some times fail to find/crack the key, 
                   hope that it's the same as the one in the next title
                   _dvdcss_debug( dvdcss, "no key found at end of title" );
                */
            }

            /* PES_scrambling_control on and make sure that the packet type 
               is one that can be scrambled */
            if( p_buf[0x14] & 0x30  && ! ( p_buf[0x11] == 0xbb 
                                           || p_buf[0x11] == 0xbe  
                                           || p_buf[0x11] == 0xbf ) )
            {
                b_encrypted = 1;
                i_best_plen = 0;
                i_best_p = 0;

                for( i = 2 ; i < 0x30 ; i++ )
                {
                    for( j = i+1 ;
                         j < 0x80 && ( p_buf[0x7F - (j%i)] == p_buf[0x7F-j] );
                         j++ );
                    {
                        if( j > i_best_plen )
                        {
                            i_best_plen = j;
                            i_best_p = i;
                        }
                    }
                }

                if( ( i_best_plen > 20 ) && ( i_best_plen / i_best_p >= 2) )
                {
                    i = CSSTitleCrack( 0,  &p_buf[0x80],
                            &p_buf[0x80 - ( i_best_plen / i_best_p) *i_best_p],
                            (dvd_key_t*)&p_buf[0x54],
                            &p_key );
                    b_stop_scanning = ( i >= 0 );
                }
            }

            i_pos += 1;
            i_blocks_read += 1;

            /* If we haven't seen any encrypted ones after 3000 blocks stop */
            if( !b_encrypted && i_blocks_read >= 1000 ) break;

        } while( !b_stop_scanning );

        if( b_stop_scanning )
        {
            memcpy( dvdcss->css.p_title_key, &p_key, sizeof(dvd_key_t) );
            _dvdcss_debug( dvdcss, "vts key initialized" );
            return 0;
        }

        if( !b_encrypted )
        {
            _dvdcss_debug( dvdcss, "file was unscrambled" );
            return 0;
        }

        return -1;
    }
    else
    {
        /* 
         * if we are here we have a decrypted disc key and ioctls are available
         * so we can read the title key and decrypt it.
         */

        _dvdcss_debug( dvdcss, "decrypting title key with disc key" );
        
        /* We need to authenticate again for every key
         * (to get a new session key ?) */
        CSSAuth( dvdcss );

        /* Get encrypted title key */
        if( ioctl_ReadTitleKey( dvdcss->i_fd, &dvdcss->css.i_agid,
                                i_pos, p_key ) < 0 )
        {
            _dvdcss_error( dvdcss, "ioctl_ReadTitleKey failed" );
            return -1;
        }
        /* Unencrypt title key using bus key */
        for( i = 0 ; i < KEY_SIZE ; i++ )
        {
            p_key[ i ] ^= dvdcss->css.disc.p_key_check[ 4 - (i % KEY_SIZE) ];
        }

        /* Title key decryption needs one inversion 0xff */
        CSSDecryptKey( p_key, dvdcss->css.disc.p_disc_key, 0xff );

        memcpy( dvdcss->css.p_title_key, p_key, sizeof(dvd_key_t) );

        return 0;
    } // (dvdcss->i_method == DVDCSS_METHOD_TITLE) || (dvdcss->b_ioctls == 0)
}

/*****************************************************************************
 * CSSDescrambleSector: does the actual descrambling of data
 *****************************************************************************
 * sec : sector to descramble
 * key : title key for this sector
 *****************************************************************************/
int CSSDescrambleSector( dvd_key_t p_key, u8* p_sec )
{
    unsigned int    i_t1, i_t2, i_t3, i_t4, i_t5, i_t6;
    u8*             p_end = p_sec + 0x800;

    /* PES_scrambling_control */
    if( p_sec[0x14] & 0x30)
    {
        i_t1 = ((p_key)[0] ^ p_sec[0x54]) | 0x100;
        i_t2 = (p_key)[1] ^ p_sec[0x55];
        i_t3 = (((p_key)[2]) | ((p_key)[3] << 8) |
               ((p_key)[4] << 16)) ^ ((p_sec[0x56]) |
               (p_sec[0x57] << 8) | (p_sec[0x58] << 16));
        i_t4 = i_t3 & 7;
        i_t3 = i_t3 * 2 + 8 - i_t4;
        p_sec += 0x80;
        i_t5 = 0;

        while( p_sec != p_end )
        {
            i_t4 = p_css_tab2[i_t2] ^ p_css_tab3[i_t1];
            i_t2 = i_t1>>1;
            i_t1 = ( ( i_t1 & 1 ) << 8 ) ^ i_t4;
            i_t4 = p_css_tab5[i_t4];
            i_t6 = ((((((( i_t3 >> 3 ) ^ i_t3 ) >> 1 ) ^
                                         i_t3 ) >> 8 ) ^ i_t3 ) >> 5 ) & 0xff;
            i_t3 = (i_t3 << 8 ) | i_t6;
            i_t6 = p_css_tab4[i_t6];
            i_t5 += i_t6 + i_t4;
            *p_sec = p_css_tab1[*p_sec] ^( i_t5 & 0xff );
            p_sec++;
            i_t5 >>= 8;
        }
    }

    return 0;
}

/* Following functions are local */

/*****************************************************************************
 * CSSGetASF : Get Authentification success flag
 *****************************************************************************
 * Returns :
 *  -1 on ioctl error,
 *  0 if the device needs to be authenticated,
 *  1 either.
 *****************************************************************************/
static int CSSGetASF( dvdcss_handle dvdcss )
{
    int i_agid;
    int i_asf = 0;

    for( i_agid = 0 ; i_agid < 4 ; i_agid++ )
    {
        if( ioctl_ReportASF( dvdcss->i_fd, &i_agid, &i_asf ) == 0 )
        {
            if( i_asf )
            {
                _dvdcss_debug( dvdcss, "GetASF authenticated" );
            }
            else
            {
                _dvdcss_debug( dvdcss, "GetASF not authenticated" );
            }

            return i_asf;
        }
    }

    /* The ioctl process has failed */
    _dvdcss_error( dvdcss, "GetASF fatal error" );
    return -1;
}

/*****************************************************************************
 * CSSCryptKey : shuffles bits and unencrypt keys.
 *****************************************************************************
 * Used during authentication and disc key negociation in CSSAuth.
 * i_key_type : 0->key1, 1->key2, 2->buskey.
 * i_varient : between 0 and 31.
 *****************************************************************************/
static void CSSCryptKey( int i_key_type, int i_varient,
                         u8 const * p_challenge, u8* p_key )
{
    /* Permutation table for challenge */
    u8      pp_perm_challenge[3][10] =
            { { 1, 3, 0, 7, 5, 2, 9, 6, 4, 8 },
              { 6, 1, 9, 3, 8, 5, 7, 4, 0, 2 },
              { 4, 0, 3, 5, 7, 2, 8, 6, 1, 9 } };

    /* Permutation table for varient table for key2 and buskey */
    u8      pp_perm_varient[2][32] =
            { { 0x0a, 0x08, 0x0e, 0x0c, 0x0b, 0x09, 0x0f, 0x0d,
                0x1a, 0x18, 0x1e, 0x1c, 0x1b, 0x19, 0x1f, 0x1d,
                0x02, 0x00, 0x06, 0x04, 0x03, 0x01, 0x07, 0x05,
                0x12, 0x10, 0x16, 0x14, 0x13, 0x11, 0x17, 0x15 },
              { 0x12, 0x1a, 0x16, 0x1e, 0x02, 0x0a, 0x06, 0x0e,
                0x10, 0x18, 0x14, 0x1c, 0x00, 0x08, 0x04, 0x0c,
                0x13, 0x1b, 0x17, 0x1f, 0x03, 0x0b, 0x07, 0x0f,
                0x11, 0x19, 0x15, 0x1d, 0x01, 0x09, 0x05, 0x0d } };

    u8      p_varients[32] =
            {   0xB7, 0x74, 0x85, 0xD0, 0xCC, 0xDB, 0xCA, 0x73,
                0x03, 0xFE, 0x31, 0x03, 0x52, 0xE0, 0xB7, 0x42,
                0x63, 0x16, 0xF2, 0x2A, 0x79, 0x52, 0xFF, 0x1B,
                0x7A, 0x11, 0xCA, 0x1A, 0x9B, 0x40, 0xAD, 0x01 };

    /* The "secret" key */
    u8      p_secret[5] = { 0x55, 0xD6, 0xC4, 0xC5, 0x28 };

    u8      p_bits[30];
    u8      p_scratch[10];
    u8      p_tmp1[5];
    u8      p_tmp2[5];
    u8      i_lfsr0_o;  /* 1 bit used */
    u8      i_lfsr1_o;  /* 1 bit used */
    u32     i_lfsr0;
    u32     i_lfsr1;
    u8      i_css_varient;
    u8      i_cse;
    u8      i_index;
    u8      i_combined;
    u8      i_carry;
    u8      i_val = 0;
    int     i_term = 0;
    int     i_bit;
    int     i;

    for (i = 9; i >= 0; --i)
        p_scratch[i] = p_challenge[pp_perm_challenge[i_key_type][i]];

    i_css_varient = ( i_key_type == 0 ) ? i_varient :
                    pp_perm_varient[i_key_type-1][i_varient];

    /*
     * This encryption engine implements one of 32 variations
     * one the same theme depending upon the choice in the
     * varient parameter (0 - 31).
     *
     * The algorithm itself manipulates a 40 bit input into
     * a 40 bit output.
     * The parameter 'input' is 80 bits.  It consists of
     * the 40 bit input value that is to be encrypted followed
     * by a 40 bit seed value for the pseudo random number
     * generators.
     */

    /* Feed the secret into the input values such that
     * we alter the seed to the LFSR's used above,  then
     * generate the bits to play with.
     */
    for( i = 5 ; --i >= 0 ; )
    {
        p_tmp1[i] = p_scratch[5 + i] ^ p_secret[i] ^ p_crypt_tab2[i];
    }

    /*
     * We use two LFSR's (seeded from some of the input data bytes) to
     * generate two streams of pseudo-random bits.  These two bit streams
     * are then combined by simply adding with carry to generate a final
     * sequence of pseudo-random bits which is stored in the buffer that
     * 'output' points to the end of - len is the size of this buffer.
     *
     * The first LFSR is of degree 25,  and has a polynomial of:
     * x^13 + x^5 + x^4 + x^1 + 1
     *
     * The second LSFR is of degree 17,  and has a (primitive) polynomial of:
     * x^15 + x^1 + 1
     *
     * I don't know if these polynomials are primitive modulo 2,  and thus
     * represent maximal-period LFSR's.
     *
     *
     * Note that we take the output of each LFSR from the new shifted in
     * bit,  not the old shifted out bit.  Thus for ease of use the LFSR's
     * are implemented in bit reversed order.
     *
     */
    
    /* In order to ensure that the LFSR works we need to ensure that the
     * initial values are non-zero.  Thus when we initialise them from
     * the seed,  we ensure that a bit is set.
     */
    i_lfsr0 = ( p_tmp1[0] << 17 ) | ( p_tmp1[1] << 9 ) |
              (( p_tmp1[2] & ~7 ) << 1 ) | 8 | ( p_tmp1[2] & 7 );
    i_lfsr1 = ( p_tmp1[3] << 9 ) | 0x100 | p_tmp1[4];

    i_index = sizeof(p_bits);
    i_carry = 0;

    do
    {
        for( i_bit = 0, i_val = 0 ; i_bit < 8 ; ++i_bit )
        {

            i_lfsr0_o = ( ( i_lfsr0 >> 24 ) ^ ( i_lfsr0 >> 21 ) ^
                        ( i_lfsr0 >> 20 ) ^ ( i_lfsr0 >> 12 ) ) & 1;
            i_lfsr0 = ( i_lfsr0 << 1 ) | i_lfsr0_o;

            i_lfsr1_o = ( ( i_lfsr1 >> 16 ) ^ ( i_lfsr1 >> 2 ) ) & 1;
            i_lfsr1 = ( i_lfsr1 << 1 ) | i_lfsr1_o;

            i_combined = !i_lfsr1_o + i_carry + !i_lfsr0_o;
            /* taking bit 1 */
            i_carry = ( i_combined >> 1 ) & 1;
            i_val |= ( i_combined & 1 ) << i_bit;
        }
    
        p_bits[--i_index] = i_val;
    } while( i_index > 0 );

    /* This term is used throughout the following to
     * select one of 32 different variations on the
     * algorithm.
     */
    i_cse = p_varients[i_css_varient] ^ p_crypt_tab2[i_css_varient];

    /* Now the actual blocks doing the encryption.  Each
     * of these works on 40 bits at a time and are quite
     * similar.
     */
    i_index = 0;
    for( i = 5, i_term = 0 ; --i >= 0 ; i_term = p_scratch[i] )
    {
        i_index = p_bits[25 + i] ^ p_scratch[i];
        i_index = p_crypt_tab1[i_index] ^ ~p_crypt_tab2[i_index] ^ i_cse;

        p_tmp1[i] = p_crypt_tab2[i_index] ^ p_crypt_tab3[i_index] ^ i_term;
    }
    p_tmp1[4] ^= p_tmp1[0];

    for( i = 5, i_term = 0 ; --i >= 0 ; i_term = p_tmp1[i] )
    {
        i_index = p_bits[20 + i] ^ p_tmp1[i];
        i_index = p_crypt_tab1[i_index] ^ ~p_crypt_tab2[i_index] ^ i_cse;

        p_tmp2[i] = p_crypt_tab2[i_index] ^ p_crypt_tab3[i_index] ^ i_term;
    }
    p_tmp2[4] ^= p_tmp2[0];

    for( i = 5, i_term = 0 ; --i >= 0 ; i_term = p_tmp2[i] )
    {
        i_index = p_bits[15 + i] ^ p_tmp2[i];
        i_index = p_crypt_tab1[i_index] ^ ~p_crypt_tab2[i_index] ^ i_cse;
        i_index = p_crypt_tab2[i_index] ^ p_crypt_tab3[i_index] ^ i_term;

        p_tmp1[i] = p_crypt_tab0[i_index] ^ p_crypt_tab2[i_index];
    }
    p_tmp1[4] ^= p_tmp1[0];

    for( i = 5, i_term = 0 ; --i >= 0 ; i_term = p_tmp1[i] )
    {
        i_index = p_bits[10 + i] ^ p_tmp1[i];
        i_index = p_crypt_tab1[i_index] ^ ~p_crypt_tab2[i_index] ^ i_cse;

        i_index = p_crypt_tab2[i_index] ^ p_crypt_tab3[i_index] ^ i_term;

        p_tmp2[i] = p_crypt_tab0[i_index] ^ p_crypt_tab2[i_index];
    }
    p_tmp2[4] ^= p_tmp2[0];

    for( i = 5, i_term = 0 ; --i >= 0 ; i_term = p_tmp2[i] )
    {
        i_index = p_bits[5 + i] ^ p_tmp2[i];
        i_index = p_crypt_tab1[i_index] ^ ~p_crypt_tab2[i_index] ^ i_cse;

        p_tmp1[i] = p_crypt_tab2[i_index] ^ p_crypt_tab3[i_index] ^ i_term;
    }
    p_tmp1[4] ^= p_tmp1[0];

    for(i = 5, i_term = 0 ; --i >= 0 ; i_term = p_tmp1[i] )
    {
        i_index = p_bits[i] ^ p_tmp1[i];
        i_index = p_crypt_tab1[i_index] ^ ~p_crypt_tab2[i_index] ^ i_cse;

        p_key[i] = p_crypt_tab2[i_index] ^ p_crypt_tab3[i_index] ^ i_term;
    }

    return;
}

/*****************************************************************************
 * CSSDecryptKey: decrypt p_crypted with p_key.
 *****************************************************************************
 * Decryption is slightly dependant on the type of key:
 *  -for disc key, invert is 0x00,
 *  -for title key, invert if 0xff. 
 *****************************************************************************/
static void CSSDecryptKey( u8* p_crypted, u8* p_key, u8 invert )
{
    unsigned int    i_lfsr1_lo;
    unsigned int    i_lfsr1_hi;
    unsigned int    i_lfsr0;
    unsigned int    i_combined;
    byte_t          o_lfsr0;
    byte_t          o_lfsr1;
    byte_t          k[5];
    int             i;

    i_lfsr1_lo = p_key[0] | 0x100;
    i_lfsr1_hi = p_key[1];

    i_lfsr0    = ( ( p_key[4] << 17 )
                 | ( p_key[3] << 9 )
                 | ( p_key[2] << 1 ) )
                 + 8 - ( p_key[2] & 7 );
    i_lfsr0    = ( p_css_tab4[i_lfsr0 & 0xff] << 24 ) |
                 ( p_css_tab4[( i_lfsr0 >> 8 ) & 0xff] << 16 ) |
                 ( p_css_tab4[( i_lfsr0 >> 16 ) & 0xff] << 8 ) |
                   p_css_tab4[( i_lfsr0 >> 24 ) & 0xff];

    i_combined = 0;
    for( i = 0 ; i < KEY_SIZE ; ++i )
    {
        o_lfsr1     = p_css_tab2[i_lfsr1_hi] ^ p_css_tab3[i_lfsr1_lo];
        i_lfsr1_hi  = i_lfsr1_lo >> 1;
        i_lfsr1_lo  = ( ( i_lfsr1_lo & 1 ) << 8 ) ^ o_lfsr1;
        o_lfsr1     = p_css_tab4[o_lfsr1];

        o_lfsr0 = ((((((( i_lfsr0 >> 8 ) ^ i_lfsr0 ) >> 1 )
                        ^ i_lfsr0 ) >> 3 ) ^ i_lfsr0 ) >> 7 );
        i_lfsr0 = ( i_lfsr0 >> 8 ) | ( o_lfsr0 << 24 );

        i_combined += ( o_lfsr0 ^ invert ) + o_lfsr1;
        k[i] = i_combined & 0xff;
        i_combined >>= 8;
    }

    p_crypted[4] = k[4] ^ p_css_tab1[p_crypted[4]] ^ p_crypted[3];
    p_crypted[3] = k[3] ^ p_css_tab1[p_crypted[3]] ^ p_crypted[2];
    p_crypted[2] = k[2] ^ p_css_tab1[p_crypted[2]] ^ p_crypted[1];
    p_crypted[1] = k[1] ^ p_css_tab1[p_crypted[1]] ^ p_crypted[0];
    p_crypted[0] = k[0] ^ p_css_tab1[p_crypted[0]] ^ p_crypted[4];

    p_crypted[4] = k[4] ^ p_css_tab1[p_crypted[4]] ^ p_crypted[3];
    p_crypted[3] = k[3] ^ p_css_tab1[p_crypted[3]] ^ p_crypted[2];
    p_crypted[2] = k[2] ^ p_css_tab1[p_crypted[2]] ^ p_crypted[1];
    p_crypted[1] = k[1] ^ p_css_tab1[p_crypted[1]] ^ p_crypted[0];
    p_crypted[0] = k[0] ^ p_css_tab1[p_crypted[0]];

    return;
}

/*****************************************************************************
 * CSSDiscCrack: brute force disc key
 * CSS hash reversal function designed by Frank Stevenson
 *****************************************************************************
 * This function uses a big amount of memory to crack the disc key from the   
 * disc key hash, if player keys are not available.
 *****************************************************************************/
#define K1TABLEWIDTH 10

/*
 * Simple function to test if a candidate key produces the given hash
 */
static int investigate( unsigned char* hash, unsigned char *ckey )
{
    unsigned char key[5];
    unsigned char pkey[5];

    memcpy( key, hash, 5 );
    memcpy( pkey, ckey, 5 );

    CSSDecryptKey( key, pkey, 0 );

    return memcmp( key, pkey, 5 );
}

static int CSSDiscCrack( dvdcss_handle dvdcss, u8 * p_disc_key )
{
    unsigned char B[5] = { 0,0,0,0,0 }; /* Second Stage of mangle cipher */
    unsigned char C[5] = { 0,0,0,0,0 }; /* Output Stage of mangle cipher
                                         * IntermediateKey */
    unsigned char k[5] = { 0,0,0,0,0 }; /* Mangling cipher key
                                         * Also output from CSS( C ) */
    unsigned char out1[5];              /* five first output bytes of LFSR1 */
    unsigned char out2[5];              /* five first output bytes of LFSR2 */
    unsigned int lfsr1a;                /* upper 9 bits of LFSR1 */
    unsigned int lfsr1b;                /* lower 8 bits of LFSR1 */
    unsigned int tmp, tmp2, tmp3, tmp4,tmp5;
    int i,j;
    unsigned int nStepA;        /* iterator for LFSR1 start state */
    unsigned int nStepB;        /* iterator for possible B[0]     */
    unsigned int nTry;          /* iterator for K[1] possibilities */
    unsigned int nPossibleK1;   /* #of possible K[1] values */
    unsigned char* K1table;     /* Lookup table for possible K[1] */
    unsigned int*  BigTable;    /* LFSR2 startstate indexed by 
                                 * 1,2,5 output byte */

    /*
     * Prepare tables for hash reversal
     */

    
    /* initialize lookup tables for k[1] */
    K1table = malloc( 65536 * K1TABLEWIDTH );
    memset( K1table, 0 , 65536 * K1TABLEWIDTH );
    if( K1table == NULL )
    {
        return -1;
    }

    tmp = p_disc_key[0] ^ p_css_tab1[ p_disc_key[1] ];
    for( i = 0 ; i < 256 ; i++ ) /* k[1] */
    {
        tmp2 = p_css_tab1[ tmp ^ i ]; /* p_css_tab1[ B[1] ]*/

        for( j = 0 ; j < 256 ; j++ ) /* B[0] */
        {
            tmp3 = j ^ tmp2 ^ i; /* C[1] */
            tmp4 = K1table[ K1TABLEWIDTH * ( 256 * j + tmp3 ) ]; /* count of entries  here */
            tmp4++;
/*
            if( tmp4 == K1TABLEWIDTH )
            {
                _dvdcss_debug( dvdcss, "Table disaster %d", tmp4 );
            }
*/
            if( tmp4 < K1TABLEWIDTH )
            {
                K1table[ K1TABLEWIDTH * ( 256 * j + tmp3 ) +    tmp4 ] = i;
            }
            K1table[ K1TABLEWIDTH * ( 256 * j + tmp3 ) ] = tmp4;
        }
    }

    /* Initing our Really big table */
    BigTable = malloc( 16777216 * sizeof(int) );
    memset( BigTable, 0 , 16777216 * sizeof(int) );
    if( BigTable == NULL )
    {
        return -1;
    }

    tmp3 = 0;

    _dvdcss_debug( dvdcss, "initializing the big table" );

    for( i = 0 ; i < 16777216 ; i++ )
    {
/*
        if( ( i & 0x07ffff ) == 0 )
        {
            fprintf( stderr, "#" );
        }
*/
        tmp = (( i + i ) & 0x1fffff0 ) | 0x8 | ( i & 0x7 );

        for( j = 0 ; j < 5 ; j++ )
        {
            tmp2=((((((( tmp >> 3 ) ^ tmp ) >> 1 ) ^ tmp ) >> 8 )
                                    ^ tmp ) >> 5 ) & 0xff;
            tmp = ( tmp << 8) | tmp2;
            out2[j] = p_css_tab4[ tmp2 ];
        }

        j = ( out2[0] << 16 ) | ( out2[1] << 8 ) | out2[4];
        BigTable[j] = i;
    }

/*    fprintf( stderr, "\n" ); */

    /*
     * We are done initing, now reverse hash
     */
    tmp5 = p_disc_key[0] ^ p_css_tab1[ p_disc_key[1] ];

    for( nStepA = 0 ; nStepA < 65536 ; nStepA ++ )
    {
        lfsr1a = 0x100 | ( nStepA >> 8 );
        lfsr1b = nStepA & 0xff;

        /* Generate 5 first output bytes from lfsr1 */
        for( i = 0 ; i < 5 ; i++ )
        {
            tmp = p_css_tab2[ lfsr1b ] ^ p_css_tab3[ lfsr1a ];
            lfsr1b = lfsr1a >> 1;
            lfsr1a = ((lfsr1a&1)<<8) ^ tmp;
            out1[ i ] = p_css_tab4[ tmp ];
        }

        /* cumpute and cache some variables */
        C[0] = nStepA >> 8;
        C[1] = nStepA & 0xff;
        tmp = p_disc_key[3] ^ p_css_tab1[ p_disc_key[4] ];
        tmp2 = p_css_tab1[ p_disc_key[0] ];

        /* Search through all possible B[0] */
        for( nStepB = 0 ; nStepB < 256 ; nStepB++ )
        {
            /* reverse parts of the mangling cipher */
            B[0] = nStepB;
            k[0] = p_css_tab1[ B[0] ] ^ C[0];
            B[4] = B[0] ^ k[0] ^ tmp2;
            k[4] = B[4] ^ tmp;
            nPossibleK1 = K1table[ K1TABLEWIDTH * (256 * B[0] + C[1]) ];

            /* Try out all possible values for k[1] */
            for( nTry = 0 ; nTry < nPossibleK1 ; nTry++ )
            {
                k[1] = K1table[ K1TABLEWIDTH * (256 * B[0] + C[1]) + nTry + 1 ];
                B[1] = tmp5 ^ k[1];

                /* reconstruct output from LFSR2 */
                tmp3 = ( 0x100 + k[0] - out1[0] );
                out2[0] = tmp3 & 0xff;
                tmp3 = tmp3 & 0x100 ? 0x100 : 0xff;
                tmp3 = ( tmp3 + k[1] - out1[1] );
                out2[1] = tmp3 & 0xff;
                tmp3 = ( 0x100 + k[4] - out1[4] );
                out2[4] = tmp3 & 0xff;  /* Can be 1 off  */

                /* test first possible out2[4] */
                tmp4 = ( out2[0] << 16 ) | ( out2[1] << 8 ) | out2[4];
                tmp4 = BigTable[ tmp4 ];
                C[2] = tmp4 & 0xff;
                C[3] = ( tmp4 >> 8 ) & 0xff;
                C[4] = ( tmp4 >> 16 ) & 0xff;
                B[3] = p_css_tab1[ B[4] ] ^ k[4] ^ C[4];
                k[3] = p_disc_key[2] ^ p_css_tab1[ p_disc_key[3] ] ^ B[3];
                B[2] = p_css_tab1[ B[3] ] ^ k[3] ^ C[3];
                k[2] = p_disc_key[1] ^ p_css_tab1[ p_disc_key[2] ] ^ B[2];

                if( ( B[1] ^ p_css_tab1[ B[2] ] ^ k[ 2 ]  ) == C[ 2 ] )
                {
                    if( ! investigate( &p_disc_key[0] , &C[0] ) )
                    {
                        goto end;
                    }
                }

                /* Test second possible out2[4] */
                out2[4] = ( out2[4] + 0xff ) & 0xff;
                tmp4 = ( out2[0] << 16 ) | ( out2[1] << 8 ) | out2[4];
                tmp4 = BigTable[ tmp4 ];
                C[2] = tmp4 & 0xff;
                C[3] = ( tmp4 >> 8 ) & 0xff;
                C[4] = ( tmp4 >> 16 ) & 0xff;
                B[3] = p_css_tab1[ B[4] ] ^ k[4] ^ C[4];
                k[3] = p_disc_key[2] ^ p_css_tab1[ p_disc_key[3] ] ^ B[3];
                B[2] = p_css_tab1[ B[3] ] ^ k[3] ^ C[3];
                k[2] = p_disc_key[1] ^ p_css_tab1[ p_disc_key[2] ] ^ B[2];

                if( ( B[1] ^ p_css_tab1[ B[2] ] ^ k[ 2 ]  ) == C[ 2 ] )
                {
                    if( ! investigate( &p_disc_key[0] , &C[0] ) )
                    {
                        goto end;
                    }
                }
            }
        }
    }

end:

    memcpy( p_disc_key, &C[0], KEY_SIZE );

    free( K1table );
    free( BigTable );

    return( 0 );
}

/*****************************************************************************
 * CSSTitleCrack : title key decryption by cracking
 * Function designed by Frank Stevenson
 *****************************************************************************
 * This function is called by CSSGetTitleKey to find a title key, if we've
 * chosen to crack title key instead of decrypting it with the disc key.
 *****************************************************************************/
static int CSSTitleCrack( int i_start,
                          unsigned char * p_crypted,
                          unsigned char * p_decrypted,
                          dvd_key_t * p_sector_key,
                          dvd_key_t * p_key )
{
    unsigned char p_buffer[10];
    unsigned int i_t1, i_t2, i_t3, i_t4, i_t5, i_t6;
    unsigned int i_try;
    unsigned int i_candidate;
    unsigned int i, j;
    int i_exit = -1;


    for( i = 0 ; i < 10 ; i++ )
    {
        p_buffer[i] = p_css_tab1[p_crypted[i]] ^ p_decrypted[i];
    }

    for( i_try = i_start ; i_try < 0x10000 ; i_try++ )
    {
        i_t1 = i_try >> 8 | 0x100;
        i_t2 = i_try & 0xff;
        i_t3 = 0;               /* not needed */
        i_t5 = 0;

        /* iterate cipher 4 times to reconstruct LFSR2 */
        for( i = 0 ; i < 4 ; i++ )
        {
            /* advance LFSR1 normaly */
            i_t4 = p_css_tab2[i_t2] ^ p_css_tab3[i_t1];
            i_t2 = i_t1 >> 1;
            i_t1 = ( ( i_t1 & 1 ) << 8 ) ^ i_t4;
            i_t4 = p_css_tab5[i_t4];
            /* deduce i_t6 & i_t5 */
            i_t6 = p_buffer[i];
            if( i_t5 )
            {
                i_t6 = ( i_t6 + 0xff ) & 0x0ff;
            }
            if( i_t6 < i_t4 )
            {
                i_t6 += 0x100;
            }
            i_t6 -= i_t4;
            i_t5 += i_t6 + i_t4;
            i_t6 = p_css_tab4[ i_t6 ];
            /* feed / advance i_t3 / i_t5 */
            i_t3 = ( i_t3 << 8 ) | i_t6;
            i_t5 >>= 8;
        }

        i_candidate = i_t3;

        /* iterate 6 more times to validate candidate key */
        for( ; i < 10 ; i++ )
        {
            i_t4 = p_css_tab2[i_t2] ^ p_css_tab3[i_t1];
            i_t2 = i_t1 >> 1;
            i_t1 = ( ( i_t1 & 1 ) << 8 ) ^ i_t4;
            i_t4 = p_css_tab5[i_t4];
            i_t6 = ((((((( i_t3 >> 3 ) ^ i_t3 ) >> 1 ) ^
                                         i_t3 ) >> 8 ) ^ i_t3 ) >> 5 ) & 0xff;
            i_t3 = ( i_t3 << 8 ) | i_t6;
            i_t6 = p_css_tab4[i_t6];
            i_t5 += i_t6 + i_t4;
            if( ( i_t5 & 0xff ) != p_buffer[i] )
            {
                break;
            }

            i_t5 >>= 8;
        }

        if( i == 10 )
        {
            /* Do 4 backwards steps of iterating t3 to deduce initial state */
            i_t3 = i_candidate;
            for( i = 0 ; i < 4 ; i++ )
            {
                i_t1 = i_t3 & 0xff;
                i_t3 = ( i_t3 >> 8 );
                /* easy to code, and fast enough bruteforce
                 * search for byte shifted in */
                for( j = 0 ; j < 256 ; j++ )
                {
                    i_t3 = ( i_t3 & 0x1ffff) | ( j << 17 );
                    i_t6 = ((((((( i_t3 >> 3 ) ^ i_t3 ) >> 1 ) ^
                                   i_t3 ) >> 8 ) ^ i_t3 ) >> 5 ) & 0xff;
                    if( i_t6 == i_t1 )
                    {
                        break;
                    }
                }
            }

            i_t4 = ( i_t3 >> 1 ) - 4;
            for( i_t5 = 0 ; i_t5 < 8; i_t5++ )
            {
                if( ( ( i_t4 + i_t5 ) * 2 + 8 - ( (i_t4 + i_t5 ) & 7 ) )
                                                                      == i_t3 )
                {
                    (*p_key)[0] = i_try>>8;
                    (*p_key)[1] = i_try & 0xFF;
                    (*p_key)[2] = ( ( i_t4 + i_t5 ) >> 0) & 0xFF;
                    (*p_key)[3] = ( ( i_t4 + i_t5 ) >> 8) & 0xFF;
                    (*p_key)[4] = ( ( i_t4 + i_t5 ) >> 16) & 0xFF;
                    i_exit = i_try + 1;
                }
            }
        }
    }

    if( i_exit >= 0 )
    {
        (*p_key)[0] ^= (*p_sector_key)[0];
        (*p_key)[1] ^= (*p_sector_key)[1];
        (*p_key)[2] ^= (*p_sector_key)[2];
        (*p_key)[3] ^= (*p_sector_key)[3];
        (*p_key)[4] ^= (*p_sector_key)[4];
    }

    return i_exit;
}
