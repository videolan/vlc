/*****************************************************************************
 * update.c: VLC update checking and downloading
 *****************************************************************************
 * Copyright © 2005-2007 the VideoLAN team
 * $Id$
 *
 * Authors: Antoine Cellerier <dionoea -at- videolan -dot- org>
 *          Rémi Duraffort <ivoire at via.ecp.fr>
            Rafaël Carré <funman@videolanorg>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either release 2 of the License, or
 * (at your option) any later release.
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

/**
 *   \file
 *   This file contains functions related to VLC and plugins update management
 */

/* TODO: pgp verification of the status file, and downloaded binaries */

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#include <vlc/vlc.h>

#ifdef UPDATE_CHECK

#include <ctype.h>                                              /* tolower() */
#include <assert.h>


#include <vlc_update.h>

#include <vlc_block.h>
#include <vlc_stream.h>
#include <vlc_interface.h>
#include <vlc_charset.h>

/*****************************************************************************
 * Misc defines
 *****************************************************************************/

/*
 * Here is the format of these "status files" :
 * First line is the last version: "X.Y.Ze" where:
 *      * X is the major number
 *      * Y is the minor number
 *      * Z is the revision number
 *      * e is an OPTIONAL extra letter
 *      * AKA "0.8.6d" or "0.9.0"
 * Second line is an url to the last binary
 * Third line is a description of the update (it MAY be extended to several lines, but for now it is only one line)
 */

#if defined( UNDER_CE )
#   define UPDATE_VLC_STATUS_URL "http://update.videolan.org/vlc/status-ce"
#elif defined( WIN32 )
#   define UPDATE_VLC_STATUS_URL "http://update.videolan.org/vlc/status-win-x86"
#elif defined( __APPLE__ )
#   define UPDATE_VLC_OS "macosx"
#   if defined( __powerpc__ ) || defined( __ppc__ ) || defined( __ppc64__ )
#       define UPDATE_VLC_STATUS_URL "http://update.videolan.org/vlc/status-mac-ppc"
#   else
#       define UPDATE_VLC_STATUS_URL "http://update.videolan.org/vlc/status-mac-x86"
#   endif
#elif defined( SYS_BEOS )
#       define UPDATE_VLC_STATUS_URL "http://update.videolan.org/vlc/status-beos-x86"
#else
#   define UPDATE_VLC_STATUS_URL "http://update.videolan.org/vlc/status"
#endif

#define STRDUP( a ) ( a ? strdup( a ) : NULL )


/*****************************************************************************
 * Local Prototypes
 *****************************************************************************/
static void EmptyRelease( update_t *p_update );
static void GetUpdateFile( update_t *p_update );
static int CompareReleases( const struct update_release_t *p1,
                            const struct update_release_t *p2 );
static char * size_str( long int l_size );


/*****************************************************************************
 * OpenPGP functions
 *****************************************************************************/

#define packet_type( c ) ( ( c & 0x3c ) >> 2 )      /* 0x3C = 00111100 */
#define packet_header_len( c ) ( ( c & 0x03 ) + 1 ) /* number of bytes in a packet header */

static inline int scalar_number( uint8_t *p, int header_len )
{
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

/* 
 * fill a public_key_packet_t structure from public key packet data
 * verify that it is a version 4 public key packet, using DSA
 */
static int parse_public_key_packet( public_key_packet_t *p_key, uint8_t *p_buf,
                                    size_t i_packet_len )
{
    if( i_packet_len != 418 )
        return VLC_EGENERIC;

    p_key->version   = *p_buf++;
    if( p_key->version != 4 )
        return VLC_EGENERIC;

    /* warn when timestamp is > date ? */
    memcpy( p_key->timestamp, p_buf, 4 ); p_buf += 4;

    p_key->algo      = *p_buf++;
    if( p_key->algo != PUBLIC_KEY_ALGO_DSA )
        return VLC_EGENERIC;

    memcpy( p_key->p, p_buf, 2+128 ); p_buf += 2+128;
    if( mpi_len( p_key->p ) != 128 )
        return VLC_EGENERIC;

    memcpy( p_key->q, p_buf, 2+20 );  p_buf += 2+20;
    if( mpi_len( p_key->q ) != 20 )
        return VLC_EGENERIC;

    memcpy( p_key->g, p_buf, 2+128 ); p_buf += 2+128;
    if( mpi_len( p_key->g ) != 128 )
        return VLC_EGENERIC;

    memcpy( p_key->y, p_buf, 2+128 ); p_buf += 2+128;
    if( mpi_len( p_key->y ) != 128 )
        return VLC_EGENERIC;

    return VLC_SUCCESS;
}

/*
 * fill a signature_packet_v4_t from signature packet data
 * verify that it was used with a DSA public key, using SHA-1 digest
 */
static int parse_signature_v4_packet( signature_packet_v4_t *p_sig,
                                      uint8_t *p_buf, size_t i_sig_len )
{
    if( i_sig_len < 54 )
        return VLC_EGENERIC;

    p_sig->version = *p_buf++;
    if( p_sig->version != 4 )
        return VLC_EGENERIC;

    p_sig->type = *p_buf++;
    if( p_sig->type < GENERIC_KEY_SIGNATURE ||
        p_sig->type > POSITIVE_KEY_SIGNATURE )
        return VLC_EGENERIC;

    p_sig->public_key_algo = *p_buf++;
    if( p_sig->public_key_algo != PUBLIC_KEY_ALGO_DSA )
        return VLC_EGENERIC;

    p_sig->digest_algo = *p_buf++;
    if( p_sig->digest_algo != DIGEST_ALGO_SHA1 )
        return VLC_EGENERIC;

    memcpy( p_sig->hashed_data_len, p_buf, 2 ); p_buf += 2;

    size_t i_pos = 6;
    size_t i_hashed_data_len = scalar_number( p_sig->hashed_data_len, 2 );
    i_pos += i_hashed_data_len;
    if( i_pos > i_sig_len - 48 ) /* r & s are 44 bytes in total, 
                              * + the unhashed data length (2 bytes)
                              * + the hash verification (2 bytes) */
        return VLC_EGENERIC;

    p_sig->hashed_data = (uint8_t*) malloc( i_hashed_data_len );
    if( !p_sig->hashed_data )
        return VLC_ENOMEM;
    memcpy( p_sig->hashed_data, p_buf, i_hashed_data_len );
    p_buf += i_hashed_data_len;

    memcpy( p_sig->unhashed_data_len, p_buf, 2 ); p_buf += 2;

    size_t i_unhashed_data_len = scalar_number( p_sig->unhashed_data_len, 2 );
    i_pos += 2 + i_unhashed_data_len;
    if( i_pos != i_sig_len - 46 )
    {
        free( p_sig->hashed_data );
        return VLC_EGENERIC;
    }

    p_sig->unhashed_data = (uint8_t*) malloc( i_unhashed_data_len );
    if( !p_sig->unhashed_data )
    {
        free( p_sig->hashed_data );
        return VLC_ENOMEM;
    }
    memcpy( p_sig->unhashed_data, p_buf, i_unhashed_data_len );
    p_buf += i_unhashed_data_len;

    memcpy( p_sig->hash_verification, p_buf, 2 ); p_buf += 2;

    memcpy( p_sig->r, p_buf, 22 ); p_buf += 22;
    if( mpi_len( p_sig->r ) != 20 )
    {
        free( p_sig->hashed_data );
        free( p_sig->unhashed_data );
        return VLC_EGENERIC;
    }

    memcpy( p_sig->s, p_buf, 22 );
    if( mpi_len( p_sig->s ) != 20 )
    {
        free( p_sig->hashed_data );
        free( p_sig->unhashed_data );
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
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
static int pgp_unarmor( char *p_ibuf, size_t i_ibuf_len,
                        uint8_t *p_obuf, size_t i_obuf_len )
{
    char *p_ipos = p_ibuf;
    uint8_t *p_opos = p_obuf;
    int i_end = 0;

    int i_header_skipped = 0;

    while( !i_end && p_ipos < p_ibuf + i_ibuf_len )
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
            p_ipos[i_line_len - 1] = '\0';
        }
        else
            p_ipos[i_line_len] = '\0';

        p_opos += vlc_b64_decode_binary_to_buffer(  p_opos,
                                                    p_obuf - p_opos + i_obuf_len,
                                                    p_ipos );

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
 * Download the signature associated to a document or a binary file.
 * We're given the file's url, we just append ".asc" to it and download 
 */
static int download_signature(  vlc_object_t *p_this,
                                signature_packet_v3_t *p_sig, char *psz_url )
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
    if( i_size < 65 )
    {
        stream_Delete( p_stream );
        return VLC_EGENERIC;
    }
    else if( i_size == 65 ) /* binary format signature */
    {
        int i_read = stream_Read( p_stream, p_sig, (int)i_size );
        stream_Delete( p_stream );
        if( i_read != i_size )
            return VLC_EGENERIC;
        else
            return VLC_SUCCESS;
    }

    char *p_buf = (char*)malloc( i_size );
    if( !p_buf )
    {
        stream_Delete( p_stream );
        return VLC_ENOMEM;
    }
    
    int i_read = stream_Read( p_stream, p_buf, (int)i_size );

    stream_Delete( p_stream );

    if( i_read != i_size )
    {
        free( p_buf );
        return VLC_EGENERIC;
    }
    
    int i_bytes = pgp_unarmor( p_buf, i_size, (uint8_t*)p_sig, 65 );
    free( p_buf );

    if( i_bytes != 65 )
        return VLC_EGENERIC;
    else
        return VLC_SUCCESS;
}

/*
 * Verify an OpenPGP signature made on some SHA-1 hash, with some DSA public key
 */
static int verify_signature( vlc_object_t *p_this, uint8_t *p_r, uint8_t *p_s,
        public_key_packet_t *p_key, uint8_t *p_hash )
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

    if( gcry_mpi_scan( &p, GCRYMPI_FMT_USG, p_key->p + 2, 128, NULL ) ||
        gcry_mpi_scan( &q, GCRYMPI_FMT_USG, p_key->q + 2, 20, NULL ) ||
        gcry_mpi_scan( &g, GCRYMPI_FMT_USG, p_key->g + 2, 128, NULL ) ||
        gcry_mpi_scan( &y, GCRYMPI_FMT_USG, p_key->y + 2, 128, NULL ) ||
        gcry_sexp_build( &key_sexp, &erroff, key_sexp_s, p, q, g, y ) )
        goto problem;

    if( gcry_mpi_scan( &r, GCRYMPI_FMT_USG, p_r + 2, 20, NULL ) ||
        gcry_mpi_scan( &s, GCRYMPI_FMT_USG, p_s + 2, 20, NULL ) ||
        gcry_sexp_build( &sig_sexp, &erroff, sig_sexp_s, r, s ) )
        goto problem;

    if( gcry_mpi_scan( &hash, GCRYMPI_FMT_USG, p_hash, 20, NULL ) ||
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
 * Return the long id (8 bytes) of the public key used to generate a signature
 */
static uint8_t *get_issuer_from_signature_v4( signature_packet_v4_t *p_sig )
{
    uint8_t *p = p_sig->unhashed_data;
    uint8_t *max_pos = p + scalar_number( p_sig->unhashed_data_len, 2 );

    while( p < max_pos )
    {
        int i_subpacket_len = *p < 192 ? *p++ :
                *p < 255 ? ((*p++ - 192) << 8) + *p++ + 192 :
                ((*++p) << 24) + (*++p << 16) + (*++p << 8) + *++p;

        if( p >= max_pos - 1 )
            return NULL;

        if( *p == ISSUER_SUBPACKET )
            return p+1;
        else
            p += i_subpacket_len;
    }
    return NULL;
}

/*
 * fill a public_key_t with public key data, including:
 *   * public key packet
 *   * signature packet issued by key which long id is p_sig_issuer
 *   * user id packet
 */
static int parse_public_key( const uint8_t *p_key_data, size_t i_key_len, public_key_t *p_key, const uint8_t *p_sig_issuer )
{
    uint8_t *pos = (uint8_t*) p_key_data;
    uint8_t *max_pos = pos + i_key_len;

    int i_status = 0;
#define PUBLIC_KEY_FOUND    0x01
#define USER_ID_FOUND       0x02
#define SIGNATURE_FOUND     0X04

    uint8_t *p_key_unarmored = NULL;

    signature_packet_v4_t sig;

    p_key->psz_username = NULL;
    p_key->sig.hashed_data = p_key->sig.unhashed_data = NULL;

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
        if( pos + i_header_len > max_pos )
            goto error;

        int i_packet_len = scalar_number( pos, i_header_len );
        pos += i_header_len;

        if( pos + i_packet_len > max_pos )
            goto error;

        switch( i_type )
        {
            uint8_t *p_issuer;

            case PUBLIC_KEY_PACKET:
                i_status |= PUBLIC_KEY_FOUND;
                if( parse_public_key_packet( &p_key->key, pos, i_packet_len ) != VLC_SUCCESS )
                    goto error;
                break;

            case SIGNATURE_PACKET:
                if( !p_sig_issuer || i_status & SIGNATURE_FOUND ||
                    parse_signature_v4_packet( &sig, pos, i_packet_len ) != VLC_SUCCESS )
                    break;
                p_issuer = get_issuer_from_signature_v4( &sig );
                if( memcmp( p_issuer, p_sig_issuer, 8 ) == 0 )
                {
                    memcpy( &p_key->sig, &sig, sizeof( signature_packet_v4_t ) );
                    i_status |= SIGNATURE_FOUND;
                }
                else
                {
                    free( sig.hashed_data );
                    free( sig.unhashed_data );
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

    if( !( i_status & ( PUBLIC_KEY_FOUND + USER_ID_FOUND ) ) )
        return VLC_EGENERIC;

    if( p_sig_issuer && !( i_status & SIGNATURE_FOUND ) )
        return VLC_EGENERIC;

    return VLC_SUCCESS;

error:
    free( p_key->sig.hashed_data );
    free( p_key->sig.unhashed_data );
    free( p_key->psz_username );
    free( p_key_unarmored );
    return VLC_EGENERIC;
}

/*
 * return a sha1 hash of a file
 */
static uint8_t *hash_sha1_from_file( const char *psz_file,
                            signature_packet_v3_t *p_sig )
{
    FILE *f = utf8_fopen( psz_file, "r" );
    if( !f )
        return NULL;

    uint8_t buffer[4096]; //FIXME

    gcry_md_hd_t hd;
    if( gcry_md_open( &hd, GCRY_MD_SHA1, 0 ) )
    {
        fclose( f );
        return NULL;
    } 

    size_t i_read;
    while( ( i_read = fread( buffer, 1, sizeof(buffer), f ) ) > 0 )
        gcry_md_write( hd, buffer, i_read );

    gcry_md_putc( hd, p_sig->type );
    gcry_md_write( hd, &p_sig->timestamp, 4 );

    fclose( f );
    gcry_md_final( hd );

    return( (uint8_t*) gcry_md_read( hd, GCRY_MD_SHA1) );
}

/*
 * download a public key (the last one) from videolan server, and parse it
 */
static public_key_t *download_key( vlc_object_t *p_this, const uint8_t *p_longid, const uint8_t *p_signature_issuer )
{
    char *psz_url;
    if( asprintf( &psz_url, "http://download.videolan.org/pub/keys/%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x.asc",
                            p_longid[0], p_longid[1],
                            p_longid[2], p_longid[3],
                            p_longid[4], p_longid[5],
                            p_longid[6], p_longid[7] ) == -1 )
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
        free( p_buf );
        return NULL;
    }

    public_key_t *p_pkey = (public_key_t*) malloc( sizeof( public_key_t ) );
    if( !p_pkey )
    {
        free( p_buf );
        return NULL;
    }

    int i_error = parse_public_key( p_buf, i_read, p_pkey, p_signature_issuer );
    free( p_buf );

    if( i_error != VLC_SUCCESS )
    {
        free( p_pkey );
        return NULL;
    }

    return p_pkey;
}

/*
 * Generate a SHA-1 hash on a public key, to verify a signature made on that hash
 * Note that we need the signature to compute the hash
 */
static uint8_t *key_sign_hash( public_key_t *p_pkey )
{
    gcry_error_t error = 0;
    gcry_md_hd_t hd;

    error = gcry_md_open( &hd, GCRY_MD_SHA1, 0 );
    if( error )
        return NULL;

    gcry_md_putc( hd, 0x99 );

    gcry_md_putc( hd, (418 >> 8) & 0xff );
    gcry_md_putc( hd, 418 & 0xff );

    gcry_md_write( hd, (uint8_t*)&p_pkey->key, 418 );

    gcry_md_putc( hd, 0xb4 );

    int i_len = strlen((char*)p_pkey->psz_username);

    gcry_md_putc( hd, (i_len << 24) & 0xff );
    gcry_md_putc( hd, (i_len << 16) & 0xff );
    gcry_md_putc( hd, (i_len << 8) & 0xff );
    gcry_md_putc( hd, (i_len) & 0xff );

    gcry_md_write( hd, p_pkey->psz_username, i_len );

    size_t i_hashed_data_len = scalar_number( p_pkey->sig.hashed_data_len, 2 );

    gcry_md_putc( hd, p_pkey->sig.version );
    gcry_md_putc( hd, p_pkey->sig.type );
    gcry_md_putc( hd, p_pkey->sig.public_key_algo );
    gcry_md_putc( hd, p_pkey->sig.digest_algo );
    gcry_md_write( hd, p_pkey->sig.hashed_data_len, 2 );
    gcry_md_write( hd, p_pkey->sig.hashed_data, i_hashed_data_len );

    gcry_md_putc( hd, 0x04 );
    gcry_md_putc( hd, 0xff );

    i_hashed_data_len += 6; /* hashed data + 6 bytes header */

    gcry_md_putc( hd, (i_hashed_data_len << 24) & 0xff);
    gcry_md_putc( hd, (i_hashed_data_len << 16) &0xff );
    gcry_md_putc( hd, (i_hashed_data_len << 8) & 0xff );
    gcry_md_putc( hd, (i_hashed_data_len) & 0xff );

    gcry_md_final( hd );

    uint8_t *p_hash = gcry_md_read( hd, GCRY_MD_SHA1);

    if( p_hash[0] != p_pkey->sig.hash_verification[0] ||
        p_hash[1] != p_pkey->sig.hash_verification[1] )
    {
        free( p_hash );
        return NULL;
    }

    return p_hash;
}


/*****************************************************************************
 * Update_t functions
 *****************************************************************************/

/**
 * Create a new update VLC struct
 *
 * \param p_this the calling vlc_object
 * \return pointer to new update_t or NULL
 */
update_t *__update_New( vlc_object_t *p_this )
{
    update_t *p_update;

    if( p_this == NULL ) return NULL;

    p_update = (update_t *)malloc( sizeof( update_t ) );
    if( !p_update ) return NULL;

    vlc_mutex_init( p_this, &p_update->lock );

    p_update->p_libvlc = p_this->p_libvlc;

    p_update->release.psz_url = NULL;
    p_update->release.psz_desc = NULL;

    var_Create( p_this->p_libvlc, "update-notify", VLC_VAR_INTEGER |
                VLC_VAR_ISCOMMAND );

    return p_update;
}

/**
 * Delete an update_t struct
 *
 * \param p_update update_t* pointer
 * \return nothing
 */
void update_Delete( update_t *p_update )
{
    assert( p_update );

    vlc_mutex_destroy( &p_update->lock );

    var_Destroy( p_update->p_libvlc, "update-notify" );

    FREENULL( p_update->release.psz_url );
    FREENULL( p_update->release.psz_desc );

    free( p_update );
}

/**
 * Empty the release struct
 *
 * \param p_update update_t* pointer
 * \return nothing
 */
static void EmptyRelease( update_t *p_update )
{
    p_update->release.i_major = 0;
    p_update->release.i_minor = 0;
    p_update->release.i_revision = 0;

    FREENULL( p_update->release.psz_url );
    FREENULL( p_update->release.psz_desc );
}

/**
 * Get the update file and parse it
 * *p_update has to be locked when calling this function
 *
 * \param p_update pointer to update struct
 * \return nothing
 */
static void GetUpdateFile( update_t *p_update )
{
    stream_t *p_stream = NULL;
    int i_major = 0;
    int i_minor = 0;
    int i_revision = 0;
    unsigned char extra;
    char *psz_line = NULL;

    p_stream = stream_UrlNew( p_update->p_libvlc, UPDATE_VLC_STATUS_URL );
    if( !p_stream )
    {
        msg_Err( p_update->p_libvlc, "Failed to open %s for reading",
                 UPDATE_VLC_STATUS_URL );
        goto error;
    }

    /* Try to read three lines */
    if( !( psz_line = stream_ReadLine( p_stream ) ) )
    {
        msg_Err( p_update->p_libvlc, "Update file %s is corrupted : missing version",
                 UPDATE_VLC_STATUS_URL );
        goto error;
    }

    /* first line : version number */
    p_update->release.extra = 0;
    switch( sscanf( psz_line, "%i.%i.%i%c", &i_major, &i_minor, &i_revision, &extra ) )
    {
        case 4:
            p_update->release.extra = extra;
        case 3:
            p_update->release.i_major = i_major;
            p_update->release.i_minor = i_minor;
            p_update->release.i_revision = i_revision;
            break;
        default:
            msg_Err( p_update->p_libvlc, "Update version false formated" );
            free( psz_line );
            goto error;
    }

    /* Second line : URL */
    if( !( psz_line = stream_ReadLine( p_stream ) ) )
    {
        msg_Err( p_update->p_libvlc, "Update file %s is corrupted : URL missing",
                 UPDATE_VLC_STATUS_URL );
        goto error;
    }
    p_update->release.psz_url = psz_line;


    /* Third line : description */
    if( !( psz_line = stream_ReadLine( p_stream ) ) )
    {
        msg_Err( p_update->p_libvlc, "Update file %s is corrupted : description missing",
                 UPDATE_VLC_STATUS_URL );
        goto error;
    }
    p_update->release.psz_desc = psz_line;

    error:
        if( p_stream )
            stream_Delete( p_stream );
}


/**
 * Struct to launch the check in an other thread
 */
typedef struct
{
    VLC_COMMON_MEMBERS
    update_t *p_update;
} update_check_thread_t;

void update_CheckReal( update_check_thread_t *p_uct );

/**
 * Check for updates
 *
 * \param p_update pointer to update struct
 * \returns nothing
 */
void update_Check( update_t *p_update )
{
    assert( p_update );

    update_check_thread_t *p_uct = vlc_object_create( p_update->p_libvlc,
                                            sizeof( update_check_thread_t ) );
    p_uct->p_update = p_update;

    vlc_thread_create( p_uct, "check for update", update_CheckReal,
                       VLC_THREAD_PRIORITY_LOW, VLC_FALSE );
}

void update_CheckReal( update_check_thread_t *p_uct )
{
    vlc_mutex_lock( &p_uct->p_update->lock );

    EmptyRelease( p_uct->p_update );
    GetUpdateFile( p_uct->p_update );

    vlc_mutex_unlock( &p_uct->p_update->lock );

    var_TriggerCallback( p_uct->p_update->p_libvlc, "update-notify" );
}

/**
 * Compare two release numbers
 *
 * \param p1 first release
 * \param p2 second release
 * \return UpdateReleaseStatus(Older|Equal|Newer)
 */
static int CompareReleases( const struct update_release_t *p1,
                            const struct update_release_t *p2 )
{
    int32_t d;
    d = ( p1->i_major << 24 ) + ( p1->i_minor << 16 ) + ( p1->i_revision << 8 )
      - ( p2->i_major << 24 ) - ( p2->i_minor << 16 ) - ( p2->i_revision << 8 )
      + ( p1->extra ) - ( p2->extra );

    if( d < 0 )
        return UpdateReleaseStatusOlder;
    else if( d == 0 )
        return UpdateReleaseStatusEqual;
    else
        return UpdateReleaseStatusNewer;
}

/**
 * Compare a given release's version number to the current VLC's one
 *
 * \param p_update structure
 * \return UpdateReleaseStatus(Older|Equal|Newer)
 */
int update_CompareReleaseToCurrent( update_t *p_update )
{
    assert( p_update );

    struct update_release_t c;

    /* get the current version number */
    c.i_major = *PACKAGE_VERSION_MAJOR - '0';
    c.i_minor = *PACKAGE_VERSION_MINOR - '0';
    c.i_revision = *PACKAGE_VERSION_REVISION - '0';
    c.extra = *PACKAGE_VERSION_EXTRA;

    return CompareReleases( &p_update->release, &c );
}

/**
 * Convert a long int size in bytes to a string
 *
 * \param l_size the size in bytes
 * \return the size as a string
 */
static char *size_str( long int l_size )
{
    char *psz_tmp = NULL;
    if( l_size >> 30 )
        asprintf( &psz_tmp, "%.1f GB", (float)l_size/(1<<30) );
    else if( l_size >> 20 )
        asprintf( &psz_tmp, "%.1f MB", (float)l_size/(1<<20) );
    else if( l_size >> 10 )
        asprintf( &psz_tmp, "%.1f kB", (float)l_size/(1<<10) );
    else
        asprintf( &psz_tmp, "%ld B", l_size );
    return psz_tmp;
}


/*
 * Struct to launch the download in a thread
 */
typedef struct
{
    VLC_COMMON_MEMBERS
    update_t *p_update;
    char *psz_destdir;
} update_download_thread_t;

void update_DownloadReal( update_download_thread_t *p_udt );

/**
 * Download the file given in the update_t
 *
 * \param p_update structure
 * \param dir to store the download file
 * \return nothing
 */
void update_Download( update_t *p_update, char *psz_destdir )
{
    assert( p_update );

    update_download_thread_t *p_udt = vlc_object_create( p_update->p_libvlc,
                                                      sizeof( update_download_thread_t ) );

    p_udt->p_update = p_update;
    p_udt->psz_destdir = STRDUP( psz_destdir );

    vlc_thread_create( p_udt, "download update", update_DownloadReal,
                       VLC_THREAD_PRIORITY_LOW, VLC_FALSE );
}
void update_DownloadReal( update_download_thread_t *p_udt )
{
    int i_progress = 0;
    long int l_size;
    long int l_downloaded = 0;
    char *psz_status;
    char *psz_downloaded;
    char *psz_size;
    char *psz_destfile;
    char *psz_tmpdestfile;

    FILE *p_file = NULL;
    stream_t *p_stream;
    void* p_buffer;
    int i_read;

    update_t *p_update = p_udt->p_update;
    char *psz_destdir = p_udt->psz_destdir;

    /* Open the stream */
    p_stream = stream_UrlNew( p_update->p_libvlc, p_update->release.psz_url );

    if( !p_stream )
    {
        msg_Err( p_update->p_libvlc, "Failed to open %s for reading", p_update->release.psz_url );
    }
    else
    {
        /* Get the stream size and open the output file */
        l_size = stream_Size( p_stream );

        psz_tmpdestfile = strrchr( p_update->release.psz_url, '/' );
        psz_tmpdestfile++;
        asprintf( &psz_destfile, "%s%s", psz_destdir, psz_tmpdestfile );

        p_file = utf8_fopen( psz_destfile, "w" );
        if( !p_file )
        {
            msg_Err( p_update->p_libvlc, "Failed to open %s for writing", psz_destfile );
        }
        else
        {
            /* Create a buffer and fill it with the downloaded file */
            p_buffer = (void *)malloc( 1 << 10 );
            if( p_buffer )
            {
                psz_size = size_str( l_size );
                asprintf( &psz_status, "%s\nDownloading... O.O/%s %.1f%% done",  p_update->release.psz_url, psz_size, 0.0 );
                i_progress = intf_UserProgress( p_update->p_libvlc, "Downloading ...", psz_status, 0.0, 0 );
                free( psz_status );

                while( ( i_read = stream_Read( p_stream, p_buffer, 1 << 10 ) ) &&
                         !intf_ProgressIsCancelled( p_update->p_libvlc, i_progress ) )
                {
                    fwrite( p_buffer, i_read, 1, p_file );

                    l_downloaded += i_read;
                    psz_downloaded = size_str( l_downloaded );
                    asprintf( &psz_status, "%s\nDonwloading... %s/%s %.1f%% done", p_update->release.psz_url,
                              psz_size, psz_downloaded, 100.0*(float)l_downloaded/(float)l_size );
                    intf_ProgressUpdate( p_update->p_libvlc, i_progress, psz_status, 10.0, 0 );
                    free( psz_downloaded );
                    free( psz_status );
                }

                /* If the user cancelled the download */
                if( !intf_ProgressIsCancelled( p_update->p_libvlc, i_progress ) )
                {
                    asprintf( &psz_status, "%s\nDone %s (100.0%%)", p_update->release.psz_url, psz_size );
                    intf_ProgressUpdate( p_update->p_libvlc, i_progress, psz_status, 100.0, 0 );
                    free( psz_status );
                }
                free( p_buffer );
                free( psz_size );
            }
            fclose( p_file );
            if( intf_ProgressIsCancelled( p_update->p_libvlc, i_progress ) )
                remove( psz_destfile );
        }
        stream_Delete( p_stream );
    }
    free( psz_destdir );
}

#endif
