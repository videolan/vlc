/*****************************************************************************
 * update_crypto.c: DSA/SHA1 related functions used for updating
 *****************************************************************************
 * Copyright © 2008-2009 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Rafaël Carré <funman@videolanorg>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either release 2 of the License, or
 * (at your option) any later release.
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

/**
 *   \file
 *   This file contains functions related to OpenPGP in VLC update management
 */

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef UPDATE_CHECK

#include <gcrypt.h>
#include <assert.h>

#include "vlc_common.h"
#include <vlc_stream.h>
#include <vlc_strings.h>
#include <vlc_fs.h>

#include "update.h"


/*****************************************************************************
 * OpenPGP functions
 *****************************************************************************/

#define packet_type( c ) ( ( c & 0x3c ) >> 2 )      /* 0x3C = 00111100 */
#define packet_header_len( c ) ( ( c & 0x03 ) + 1 ) /* number of bytes in a packet header */


static inline int scalar_number( const uint8_t *p, int header_len )
{
    assert( header_len == 1 || header_len == 2 || header_len == 4 );

    if( header_len == 1 )
        return( p[0] );
    else if( header_len == 2 )
        return( (p[0] << 8) + p[1] );
    else if( header_len == 4 )
        return( (p[0] << 24) + (p[1] << 16) + (p[2] << 8) + p[3] );
    else
        abort();
}


/* number of data bytes in a MPI */
#define mpi_len( mpi ) ( ( scalar_number( mpi, 2 ) + 7 ) / 8 )

#define READ_MPI(n, bits) do { \
    if( i_read + 2 > i_packet_len ) \
        goto error; \
    int len = mpi_len( p_buf ); \
    if( len > (bits)/8 || i_read + 2 + len > i_packet_len ) \
        goto error; \
    len += 2; \
    memcpy( n, p_buf, len ); \
    p_buf += len; i_read += len; \
    } while(0)

/*
 * fill a public_key_packet_t structure from public key packet data
 * verify that it is a version 4 public key packet, using DSA
 */
static int parse_public_key_packet( public_key_packet_t *p_key,
                                    const uint8_t *p_buf, size_t i_packet_len )
{

    if( i_packet_len > 418 || i_packet_len < 6 )
        return VLC_EGENERIC;

    size_t i_read = 0;

    p_key->version   = *p_buf++; i_read++;
    if( p_key->version != 4 )
        return VLC_EGENERIC;

    /* XXX: warn when timestamp is > date ? */
    memcpy( p_key->timestamp, p_buf, 4 ); p_buf += 4; i_read += 4;

    p_key->algo      = *p_buf++; i_read++;
    if( p_key->algo != PUBLIC_KEY_ALGO_DSA )
        return VLC_EGENERIC;

    READ_MPI(p_key->p, 1024);
    READ_MPI(p_key->q, 160);
    READ_MPI(p_key->g, 1024);
    READ_MPI(p_key->y, 1024);

    if( i_read != i_packet_len ) /* some extra data eh ? */
        return VLC_EGENERIC;

    return VLC_SUCCESS;

error:
    return VLC_EGENERIC;
}


static size_t parse_signature_v3_packet( signature_packet_t *p_sig,
                                      const uint8_t *p_buf, size_t i_sig_len )
{
    size_t i_read = 1; /* we already read the version byte */

    if( i_sig_len < 19 ) /* signature is at least 19 bytes + the 2 MPIs */
        return 0;

    p_sig->specific.v3.hashed_data_len = *p_buf++; i_read++;
    if( p_sig->specific.v3.hashed_data_len != 5 )
        return 0;

    p_sig->type = *p_buf++; i_read++;

    memcpy( p_sig->specific.v3.timestamp, p_buf, 4 );
    p_buf += 4; i_read += 4;

    memcpy( p_sig->issuer_longid, p_buf, 8 );
    p_buf += 8; i_read += 8;

    p_sig->public_key_algo = *p_buf++; i_read++;

    p_sig->digest_algo = *p_buf++; i_read++;

    p_sig->hash_verification[0] = *p_buf++; i_read++;
    p_sig->hash_verification[1] = *p_buf++; i_read++;

    assert( i_read == 19 );

    return i_read;
}


/*
 * fill a signature_packet_v4_t from signature packet data
 * verify that it was used with a DSA public key, using SHA-1 digest
 */
static size_t parse_signature_v4_packet( signature_packet_t *p_sig,
                                      const uint8_t *p_buf, size_t i_sig_len )
{
    size_t i_read = 1; /* we already read the version byte */

    if( i_sig_len < 10 ) /* signature is at least 10 bytes + the 2 MPIs */
        return 0;

    p_sig->type = *p_buf++; i_read++;

    p_sig->public_key_algo = *p_buf++; i_read++;

    p_sig->digest_algo = *p_buf++; i_read++;

    memcpy( p_sig->specific.v4.hashed_data_len, p_buf, 2 );
    p_buf += 2; i_read += 2;

    size_t i_hashed_data_len =
        scalar_number( p_sig->specific.v4.hashed_data_len, 2 );
    i_read += i_hashed_data_len;
    if( i_read + 4 > i_sig_len )
        return 0;

    p_sig->specific.v4.hashed_data = (uint8_t*) malloc( i_hashed_data_len );
    if( !p_sig->specific.v4.hashed_data )
        return 0;
    memcpy( p_sig->specific.v4.hashed_data, p_buf, i_hashed_data_len );
    p_buf += i_hashed_data_len;

    memcpy( p_sig->specific.v4.unhashed_data_len, p_buf, 2 );
    p_buf += 2; i_read += 2;

    size_t i_unhashed_data_len =
        scalar_number( p_sig->specific.v4.unhashed_data_len, 2 );
    i_read += i_unhashed_data_len;
    if( i_read + 2 > i_sig_len )
        return 0;

    p_sig->specific.v4.unhashed_data = (uint8_t*) malloc( i_unhashed_data_len );
    if( !p_sig->specific.v4.unhashed_data )
        return 0;

    memcpy( p_sig->specific.v4.unhashed_data, p_buf, i_unhashed_data_len );
    p_buf += i_unhashed_data_len;

    memcpy( p_sig->hash_verification, p_buf, 2 );
    p_buf += 2; i_read += 2;

    uint8_t *p, *max_pos;
    p = p_sig->specific.v4.unhashed_data;
    max_pos = p + scalar_number( p_sig->specific.v4.unhashed_data_len, 2 );

    for( ;; )
    {
        if( p > max_pos )
            return 0;

        size_t i_subpacket_len;
        if( *p < 192 )
        {
            if( p + 1 > max_pos )
                return 0;
            i_subpacket_len = *p++;
        }
        else if( *p < 255 )
        {
            if( p + 2 > max_pos )
                return 0;
            i_subpacket_len = (*p++ - 192) << 8;
            i_subpacket_len += *p++ + 192;
        }
        else
        {
            if( p + 4 > max_pos )
                return 0;
            i_subpacket_len = *++p << 24;
            i_subpacket_len += *++p << 16;
            i_subpacket_len += *++p << 8;
            i_subpacket_len += *++p;
        }

        if( *p == ISSUER_SUBPACKET )
        {
            if( p + 9 > max_pos )
                return 0;

            memcpy( &p_sig->issuer_longid, p+1, 8 );

            return i_read;
        }

        p += i_subpacket_len;
    }
}


static int parse_signature_packet( signature_packet_t *p_sig,
                                   const uint8_t *p_buf, size_t i_packet_len )
{
    if( !i_packet_len ) /* 1st sanity check, we need at least the version */
        return VLC_EGENERIC;

    p_sig->version = *p_buf++;

    size_t i_read;
    switch( p_sig->version )
    {
        case 3:
            i_read = parse_signature_v3_packet( p_sig, p_buf, i_packet_len );
            break;
        case 4:
            p_sig->specific.v4.hashed_data = NULL;
            p_sig->specific.v4.unhashed_data = NULL;
            i_read = parse_signature_v4_packet( p_sig, p_buf, i_packet_len );
            break;
        default:
            return VLC_EGENERIC;
    }

    if( i_read == 0 ) /* signature packet parsing has failed */
        goto error;

    if( p_sig->public_key_algo != PUBLIC_KEY_ALGO_DSA )
        goto error;

    if( p_sig->digest_algo != DIGEST_ALGO_SHA1 )
        goto error;

    switch( p_sig->type )
    {
        case BINARY_SIGNATURE:
        case TEXT_SIGNATURE:
        case GENERIC_KEY_SIGNATURE:
        case PERSONA_KEY_SIGNATURE:
        case CASUAL_KEY_SIGNATURE:
        case POSITIVE_KEY_SIGNATURE:
            break;
        default:
            goto error;
    }

    p_buf--; /* rewind to the version byte */
    p_buf += i_read;

    READ_MPI(p_sig->r, 160);
    READ_MPI(p_sig->s, 160);

    assert( i_read == i_packet_len );
    if( i_read < i_packet_len ) /* some extra data, hm ? */
        goto error;

    return VLC_SUCCESS;

error:

    if( p_sig->version == 4 )
    {
        free( p_sig->specific.v4.hashed_data );
        free( p_sig->specific.v4.unhashed_data );
    }

    return VLC_EGENERIC;
}


/*
 * crc_octets() was lamely copied from rfc 2440
 * Copyright (C) The Internet Society (1998).  All Rights Reserved.
 */
#define CRC24_INIT 0xB704CEL
#define CRC24_POLY 0x1864CFBL

static long crc_octets( uint8_t *octets, size_t len )
{
    long crc = CRC24_INIT;
    int i;
    while (len--)
    {
        crc ^= (*octets++) << 16;
        for (i = 0; i < 8; i++)
        {
            crc <<= 1;
            if (crc & 0x1000000)
                crc ^= CRC24_POLY;
        }
    }
    return crc & 0xFFFFFFL;
}


/*
 * Transform an armored document in binary format
 * Used on public keys and signatures
 */
static int pgp_unarmor( const char *p_ibuf, size_t i_ibuf_len,
                        uint8_t *p_obuf, size_t i_obuf_len )
{
    const char *p_ipos = p_ibuf;
    uint8_t *p_opos = p_obuf;
    int i_end = 0;
    int i_header_skipped = 0;

    while( !i_end && p_ipos < p_ibuf + i_ibuf_len && *p_ipos != '=' )
    {
        if( *p_ipos == '\r' || *p_ipos == '\n' )
        {
            p_ipos++;
            continue;
        }

        size_t i_line_len = strcspn( p_ipos, "\r\n" );
        if( i_line_len == 0 )
            continue;

        if( !i_header_skipped )
        {
            if( !strncmp( p_ipos, "-----BEGIN PGP", 14 ) )
                i_header_skipped = 1;

            p_ipos += i_line_len + 1;
            continue;
        }

        if( !strncmp( p_ipos, "Version:", 8 ) )
        {
            p_ipos += i_line_len + 1;
            continue;
        }

        if( p_ipos[i_line_len - 1] == '=' )
        {
            i_end = 1;
        }

        p_opos += vlc_b64_decode_binary_to_buffer(  p_opos,
                        p_obuf - p_opos + i_obuf_len, p_ipos );
        p_ipos += i_line_len + 1;
    }

    /* XXX: the CRC is OPTIONAL, really require it ? */
    if( p_ipos + 5 > p_ibuf + i_ibuf_len || *p_ipos++ != '=' )
        return 0;

    uint8_t p_crc[3];
    if( vlc_b64_decode_binary_to_buffer( p_crc, 3, p_ipos ) != 3 )
        return 0;

    long l_crc = crc_octets( p_obuf, p_opos - p_obuf );
    long l_crc2 = ( 0 << 24 ) + ( p_crc[0] << 16 ) + ( p_crc[1] << 8 ) + p_crc[2];

    return l_crc2 == l_crc ? p_opos - p_obuf : 0;
}


/*
 * Verify an OpenPGP signature made on some SHA-1 hash, with some DSA public key
 */
int verify_signature( uint8_t *p_r, uint8_t *p_s, public_key_packet_t *p_key,
                      uint8_t *p_hash )
{
    /* the data to be verified (a SHA-1 hash) */
    const char *hash_sexp_s = "(data(flags raw)(value %m))";
    /* the public key */
    const char *key_sexp_s = "(public-key(dsa(p %m)(q %m)(g %m)(y %m)))";
    /* the signature */
    const char *sig_sexp_s = "(sig-val(dsa(r %m )(s %m )))";

    size_t erroff;
    gcry_mpi_t p, q, g, y, r, s, hash;
    p = q = g = y = r = s = hash = NULL;
    gcry_sexp_t key_sexp, hash_sexp, sig_sexp;
    key_sexp = hash_sexp = sig_sexp = NULL;

    int i_p_len = mpi_len( p_key->p );
    int i_q_len = mpi_len( p_key->q );
    int i_g_len = mpi_len( p_key->g );
    int i_y_len = mpi_len( p_key->y );
    if( gcry_mpi_scan( &p, GCRYMPI_FMT_USG, p_key->p + 2, i_p_len, NULL ) ||
        gcry_mpi_scan( &q, GCRYMPI_FMT_USG, p_key->q + 2, i_q_len, NULL ) ||
        gcry_mpi_scan( &g, GCRYMPI_FMT_USG, p_key->g + 2, i_g_len, NULL ) ||
        gcry_mpi_scan( &y, GCRYMPI_FMT_USG, p_key->y + 2, i_y_len, NULL ) ||
        gcry_sexp_build( &key_sexp, &erroff, key_sexp_s, p, q, g, y ) )
        goto problem;

    int i_r_len = mpi_len( p_r );
    int i_s_len = mpi_len( p_s );
    if( gcry_mpi_scan( &r, GCRYMPI_FMT_USG, p_r + 2, i_r_len, NULL ) ||
        gcry_mpi_scan( &s, GCRYMPI_FMT_USG, p_s + 2, i_s_len, NULL ) ||
        gcry_sexp_build( &sig_sexp, &erroff, sig_sexp_s, r, s ) )
        goto problem;

    int i_hash_len = 20;
    if( gcry_mpi_scan( &hash, GCRYMPI_FMT_USG, p_hash, i_hash_len, NULL ) ||
        gcry_sexp_build( &hash_sexp, &erroff, hash_sexp_s, hash ) )
        goto problem;

    if( gcry_pk_verify( sig_sexp, hash_sexp, key_sexp ) )
        goto problem;

    return VLC_SUCCESS;

problem:
    if( p ) gcry_mpi_release( p );
    if( q ) gcry_mpi_release( q );
    if( g ) gcry_mpi_release( g );
    if( y ) gcry_mpi_release( y );
    if( r ) gcry_mpi_release( r );
    if( s ) gcry_mpi_release( s );
    if( hash ) gcry_mpi_release( hash );
    if( key_sexp ) gcry_sexp_release( key_sexp );
    if( sig_sexp ) gcry_sexp_release( sig_sexp );
    if( hash_sexp ) gcry_sexp_release( hash_sexp );
    return VLC_EGENERIC;
}


/*
 * fill a public_key_t with public key data, including:
 *   * public key packet
 *   * signature packet issued by key which long id is p_sig_issuer
 *   * user id packet
 */
int parse_public_key( const uint8_t *p_key_data, size_t i_key_len,
                      public_key_t *p_key, const uint8_t *p_sig_issuer )
{
    const uint8_t *pos = p_key_data;
    const uint8_t *max_pos = pos + i_key_len;

    int i_status = 0;
#define PUBLIC_KEY_FOUND    0x01
#define USER_ID_FOUND       0x02
#define SIGNATURE_FOUND     0X04

    uint8_t *p_key_unarmored = NULL;

    p_key->psz_username = NULL;
    p_key->sig.specific.v4.hashed_data = NULL;
    p_key->sig.specific.v4.unhashed_data = NULL;

    if( !( *pos & 0x80 ) )
    {   /* first byte is ASCII, unarmoring */
        p_key_unarmored = (uint8_t*)malloc( i_key_len );
        if( !p_key_unarmored )
            return VLC_ENOMEM;
        int i_len = pgp_unarmor( (char*)p_key_data, i_key_len,
                                 p_key_unarmored, i_key_len );

        if( i_len == 0 )
            goto error;

        pos = p_key_unarmored;
        max_pos = pos + i_len;
    }

    while( pos < max_pos )
    {
        if( !(*pos & 0x80) || *pos & 0x40 )
            goto error;

        int i_type = packet_type( *pos );

        int i_header_len = packet_header_len( *pos++ );
        if( pos + i_header_len > max_pos ||
            ( i_header_len != 1 && i_header_len != 2 && i_header_len != 4 ) )
            goto error;

        int i_packet_len = scalar_number( pos, i_header_len );
        pos += i_header_len;

        if( pos + i_packet_len > max_pos )
            goto error;

        switch( i_type )
        {
            case PUBLIC_KEY_PACKET:
                i_status |= PUBLIC_KEY_FOUND;
                if( parse_public_key_packet( &p_key->key, pos, i_packet_len ) != VLC_SUCCESS )
                    goto error;
                break;

            case SIGNATURE_PACKET: /* we accept only v4 signatures here */
                if( i_status & SIGNATURE_FOUND || !p_sig_issuer )
                    break;
                int i_ret = parse_signature_packet( &p_key->sig, pos,
                                                    i_packet_len );
                if( i_ret == VLC_SUCCESS )
                {
                    if( p_key->sig.version != 4 )
                        break;
                    if( memcmp( p_key->sig.issuer_longid, p_sig_issuer, 8 ) )
                    {
                        free( p_key->sig.specific.v4.hashed_data );
                        free( p_key->sig.specific.v4.unhashed_data );
                        p_key->sig.specific.v4.hashed_data = NULL;
                        p_key->sig.specific.v4.unhashed_data = NULL;
                        break;
                    }
                    i_status |= SIGNATURE_FOUND;
                }
                break;

            case USER_ID_PACKET:
                if( p_key->psz_username ) /* save only the first User ID */
                    break;
                i_status |= USER_ID_FOUND;
                p_key->psz_username = (uint8_t*)malloc( i_packet_len + 1);
                if( !p_key->psz_username )
                    goto error;

                memcpy( p_key->psz_username, pos, i_packet_len );
                p_key->psz_username[i_packet_len] = '\0';
                break;

            default:
                break;
        }
        pos += i_packet_len;
    }
    free( p_key_unarmored );

    if( !( i_status & ( PUBLIC_KEY_FOUND | USER_ID_FOUND ) ) )
        return VLC_EGENERIC;

    if( p_sig_issuer && !( i_status & SIGNATURE_FOUND ) )
        return VLC_EGENERIC;

    return VLC_SUCCESS;

error:
    if( p_key->sig.version == 4 )
    {
        free( p_key->sig.specific.v4.hashed_data );
        free( p_key->sig.specific.v4.unhashed_data );
    }
    free( p_key->psz_username );
    free( p_key_unarmored );
    return VLC_EGENERIC;
}


/* hash a binary file */
static int hash_from_binary_file( const char *psz_file, gcry_md_hd_t hd )
{
    uint8_t buffer[4096];
    size_t i_read;

    FILE *f = vlc_fopen( psz_file, "r" );
    if( !f )
        return -1;

    while( ( i_read = fread( buffer, 1, sizeof(buffer), f ) ) > 0 )
        gcry_md_write( hd, buffer, i_read );

    fclose( f );

    return 0;
}


/* final part of the hash */
static uint8_t *hash_finish( gcry_md_hd_t hd, signature_packet_t *p_sig )
{
    if( p_sig->version == 3 )
    {
        gcry_md_putc( hd, p_sig->type );
        gcry_md_write( hd, &p_sig->specific.v3.timestamp, 4 );
    }
    else if( p_sig->version == 4 )
    {
        gcry_md_putc( hd, p_sig->version );
        gcry_md_putc( hd, p_sig->type );
        gcry_md_putc( hd, p_sig->public_key_algo );
        gcry_md_putc( hd, p_sig->digest_algo );
        gcry_md_write( hd, p_sig->specific.v4.hashed_data_len, 2 );
        size_t i_len = scalar_number( p_sig->specific.v4.hashed_data_len, 2 );
        gcry_md_write( hd, p_sig->specific.v4.hashed_data, i_len );

        gcry_md_putc( hd, 0x04 );
        gcry_md_putc( hd, 0xFF );

        i_len += 6; /* hashed data + 6 bytes header */

        gcry_md_putc( hd, (i_len >> 24) & 0xff );
        gcry_md_putc( hd, (i_len >> 16) & 0xff );
        gcry_md_putc( hd, (i_len >> 8) & 0xff );
        gcry_md_putc( hd, (i_len) & 0xff );
    }
    else
    {   /* RFC 4880 only tells about versions 3 and 4 */
        return NULL;
    }

    gcry_md_final( hd );

    uint8_t *p_tmp = (uint8_t*) gcry_md_read( hd, GCRY_MD_SHA1);
    uint8_t *p_hash = malloc( 20 );
    if( p_hash )
        memcpy( p_hash, p_tmp, 20 );
    gcry_md_close( hd );
    return p_hash;
}


/*
 * return a sha1 hash of a text
 */
uint8_t *hash_sha1_from_text( const char *psz_string,
        signature_packet_t *p_sig )
{
    gcry_md_hd_t hd;
    if( gcry_md_open( &hd, GCRY_MD_SHA1, 0 ) )
        return NULL;

    if( p_sig->type == TEXT_SIGNATURE )
    while( *psz_string )
    {
        size_t i_len = strcspn( psz_string, "\r\n" );

        if( i_len )
        {
            gcry_md_write( hd, psz_string, i_len );
            psz_string += i_len;
        }
        gcry_md_putc( hd, '\r' );
        gcry_md_putc( hd, '\n' );

        if( *psz_string == '\r' )
            psz_string++;
        if( *psz_string == '\n' )
            psz_string++;
    }
    else
        gcry_md_write( hd, psz_string, strlen( psz_string ) );

    return hash_finish( hd, p_sig );
}


/*
 * return a sha1 hash of a file
 */
uint8_t *hash_sha1_from_file( const char *psz_file, signature_packet_t *p_sig )
{
    gcry_md_hd_t hd;
    if( gcry_md_open( &hd, GCRY_MD_SHA1, 0 ) )
        return NULL;

    if( hash_from_binary_file( psz_file, hd ) < 0 )
    {
        gcry_md_close( hd );
        return NULL;
    }

    return hash_finish( hd, p_sig );
}


/*
 * Generate a SHA1 hash on a public key, to verify a signature made on that hash
 * Note that we need the signature (v4) to compute the hash
 */
uint8_t *hash_sha1_from_public_key( public_key_t *p_pkey )
{
    if( p_pkey->sig.version != 4 )
        return NULL;

    if( p_pkey->sig.type < GENERIC_KEY_SIGNATURE ||
        p_pkey->sig.type > POSITIVE_KEY_SIGNATURE )
        return NULL;

    gcry_error_t error = 0;
    gcry_md_hd_t hd;

    error = gcry_md_open( &hd, GCRY_MD_SHA1, 0 );
    if( error )
        return NULL;

    gcry_md_putc( hd, 0x99 );

    size_t i_p_len = mpi_len( p_pkey->key.p );
    size_t i_g_len = mpi_len( p_pkey->key.g );
    size_t i_q_len = mpi_len( p_pkey->key.q );
    size_t i_y_len = mpi_len( p_pkey->key.y );

    size_t i_size = 6 + 2*4 + i_p_len + i_g_len + i_q_len + i_y_len;

    gcry_md_putc( hd, (i_size >> 8) & 0xff );
    gcry_md_putc( hd, i_size & 0xff );

    gcry_md_putc( hd, p_pkey->key.version );
    gcry_md_write( hd, p_pkey->key.timestamp, 4 );
    gcry_md_putc( hd, p_pkey->key.algo );

    gcry_md_write( hd, (uint8_t*)&p_pkey->key.p, 2 );
    gcry_md_write( hd, (uint8_t*)&p_pkey->key.p + 2, i_p_len );

    gcry_md_write( hd, (uint8_t*)&p_pkey->key.q, 2 );
    gcry_md_write( hd, (uint8_t*)&p_pkey->key.q + 2, i_q_len );

    gcry_md_write( hd, (uint8_t*)&p_pkey->key.g, 2 );
    gcry_md_write( hd, (uint8_t*)&p_pkey->key.g + 2, i_g_len );

    gcry_md_write( hd, (uint8_t*)&p_pkey->key.y, 2 );
    gcry_md_write( hd, (uint8_t*)&p_pkey->key.y + 2, i_y_len );

    gcry_md_putc( hd, 0xb4 );

    size_t i_len = strlen((char*)p_pkey->psz_username);

    gcry_md_putc( hd, (i_len >> 24) & 0xff );
    gcry_md_putc( hd, (i_len >> 16) & 0xff );
    gcry_md_putc( hd, (i_len >> 8) & 0xff );
    gcry_md_putc( hd, (i_len) & 0xff );

    gcry_md_write( hd, p_pkey->psz_username, i_len );

    uint8_t *p_hash = hash_finish( hd, &p_pkey->sig );
    if( !p_hash ||
        p_hash[0] != p_pkey->sig.hash_verification[0] ||
        p_hash[1] != p_pkey->sig.hash_verification[1] )
    {
        free(p_hash);
        return NULL;
    }

    return p_hash;
}


/*
 * download a public key (the last one) from videolan server, and parse it
 */
public_key_t *download_key( vlc_object_t *p_this,
                    const uint8_t *p_longid, const uint8_t *p_signature_issuer )
{
    char *psz_url;
    if( asprintf( &psz_url, "http://download.videolan.org/pub/keys/%.2X%.2X%.2X%.2X%.2X%.2X%.2X%.2X.asc",
                    p_longid[0], p_longid[1], p_longid[2], p_longid[3],
                    p_longid[4], p_longid[5], p_longid[6], p_longid[7] ) == -1 )
        return NULL;

    stream_t *p_stream = stream_UrlNew( p_this, psz_url );
    free( psz_url );
    if( !p_stream )
        return NULL;

    int64_t i_size = stream_Size( p_stream );
    if( i_size < 0 )
    {
        stream_Delete( p_stream );
        return NULL;
    }

    uint8_t *p_buf = (uint8_t*)malloc( i_size );
    if( !p_buf )
    {
        stream_Delete( p_stream );
        return NULL;
    }

    int i_read = stream_Read( p_stream, p_buf, (int)i_size );
    stream_Delete( p_stream );

    if( i_read != (int)i_size )
    {
        msg_Dbg( p_this, "Couldn't read full GPG key" );
        free( p_buf );
        return NULL;
    }

    public_key_t *p_pkey = (public_key_t*) malloc( sizeof( public_key_t ) );
    if( !p_pkey )
    {
        free( p_buf );
        return NULL;
    }

    memcpy( p_pkey->longid, p_longid, 8 );

    int i_error = parse_public_key( p_buf, i_read, p_pkey, p_signature_issuer );
    free( p_buf );

    if( i_error != VLC_SUCCESS )
    {
        msg_Dbg( p_this, "Couldn't parse GPG key" );
        free( p_pkey );
        return NULL;
    }

    return p_pkey;
}


/*
 * Download the signature associated to a document or a binary file.
 * We're given the file's url, we just append ".asc" to it and download
 */
int download_signature( vlc_object_t *p_this, signature_packet_t *p_sig,
                        const char *psz_url )
{
    char *psz_sig = (char*) malloc( strlen( psz_url ) + 4 + 1 ); /* ".asc" + \0 */
    if( !psz_sig )
        return VLC_ENOMEM;

    strcpy( psz_sig, psz_url );
    strcat( psz_sig, ".asc" );

    stream_t *p_stream = stream_UrlNew( p_this, psz_sig );
    free( psz_sig );

    if( !p_stream )
        return VLC_ENOMEM;

    int64_t i_size = stream_Size( p_stream );

    msg_Dbg( p_this, "Downloading signature (%"PRId64" bytes)", i_size );
    uint8_t *p_buf = (uint8_t*)malloc( i_size );
    if( !p_buf )
    {
        stream_Delete( p_stream );
        return VLC_ENOMEM;
    }

    int i_read = stream_Read( p_stream, p_buf, (int)i_size );

    stream_Delete( p_stream );

    if( i_read != (int)i_size )
    {
        msg_Dbg( p_this,
            "Couldn't download full signature (only %d bytes)", i_read );
        free( p_buf );
        return VLC_EGENERIC;
    }

    if( (uint8_t)*p_buf < 0x80 ) /* ASCII */
    {
        msg_Dbg( p_this, "Unarmoring signature" );

        uint8_t* p_unarmored = (uint8_t*) malloc( ( i_size * 3 ) / 4 + 1 );
        if( !p_unarmored )
        {
            free( p_buf );
            return VLC_EGENERIC;
        }

        int i_bytes = pgp_unarmor( (char*)p_buf, i_size, p_unarmored, i_size );
        free( p_buf );

        p_buf = p_unarmored;
        i_size = i_bytes;

        if( i_bytes < 2 )
        {
            free( p_buf );
            msg_Dbg( p_this, "Unarmoring failed : corrupted signature ?" );
            return VLC_EGENERIC;
        }
    }

    if( packet_type( *p_buf ) != SIGNATURE_PACKET )
    {
        free( p_buf );
        msg_Dbg( p_this, "Not a signature: %d", *p_buf );
        return VLC_EGENERIC;
    }

    size_t i_header_len = packet_header_len( *p_buf );
    if( ( i_header_len != 1 && i_header_len != 2 && i_header_len != 4 ) ||
        i_header_len + 1 > (size_t)i_size )
    {
        free( p_buf );
        msg_Dbg( p_this, "Invalid signature packet header" );
        return VLC_EGENERIC;
    }

    size_t i_len = scalar_number( p_buf+1, i_header_len );
    if( i_len + i_header_len + 1 != (size_t)i_size )
    {
        free( p_buf );
        msg_Dbg( p_this, "Invalid signature packet" );
        return VLC_EGENERIC;
    }

    int i_ret = parse_signature_packet( p_sig, p_buf+1+i_header_len, i_len );
    free( p_buf );
    if( i_ret != VLC_SUCCESS )
    {
        msg_Dbg( p_this, "Couldn't parse signature" );
        return i_ret;
    }

    if( p_sig->type != BINARY_SIGNATURE && p_sig->type != TEXT_SIGNATURE )
    {
        msg_Dbg( p_this, "Invalid signature type: %d", p_sig->type );
        if( p_sig->version == 4 )
        {
            free( p_sig->specific.v4.hashed_data );
            free( p_sig->specific.v4.unhashed_data );
        }
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

#endif /* UPDATE_CHECK */
