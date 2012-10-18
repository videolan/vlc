/*****************************************************************************
 * raop.c: Remote Audio Output Protocol streaming support
 *****************************************************************************
 * Copyright (C) 2008 the VideoLAN team
 * $Id$
 *
 * Author: Michael Hanselmann
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <assert.h>

#include <gcrypt.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_sout.h>
#include <vlc_block.h>
#include <vlc_network.h>
#include <vlc_strings.h>
#include <vlc_charset.h>
#include <vlc_fs.h>
#include <vlc_gcrypt.h>
#include <vlc_es.h>
#include <vlc_http.h>
#include <vlc_memory.h>

#define RAOP_PORT 5000
#define RAOP_USER_AGENT "VLC " VERSION


static const char ps_raop_rsa_pubkey[] =
    "\xe7\xd7\x44\xf2\xa2\xe2\x78\x8b\x6c\x1f\x55\xa0\x8e\xb7\x05\x44"
    "\xa8\xfa\x79\x45\xaa\x8b\xe6\xc6\x2c\xe5\xf5\x1c\xbd\xd4\xdc\x68"
    "\x42\xfe\x3d\x10\x83\xdd\x2e\xde\xc1\xbf\xd4\x25\x2d\xc0\x2e\x6f"
    "\x39\x8b\xdf\x0e\x61\x48\xea\x84\x85\x5e\x2e\x44\x2d\xa6\xd6\x26"
    "\x64\xf6\x74\xa1\xf3\x04\x92\x9a\xde\x4f\x68\x93\xef\x2d\xf6\xe7"
    "\x11\xa8\xc7\x7a\x0d\x91\xc9\xd9\x80\x82\x2e\x50\xd1\x29\x22\xaf"
    "\xea\x40\xea\x9f\x0e\x14\xc0\xf7\x69\x38\xc5\xf3\x88\x2f\xc0\x32"
    "\x3d\xd9\xfe\x55\x15\x5f\x51\xbb\x59\x21\xc2\x01\x62\x9f\xd7\x33"
    "\x52\xd5\xe2\xef\xaa\xbf\x9b\xa0\x48\xd7\xb8\x13\xa2\xb6\x76\x7f"
    "\x6c\x3c\xcf\x1e\xb4\xce\x67\x3d\x03\x7b\x0d\x2e\xa3\x0c\x5f\xff"
    "\xeb\x06\xf8\xd0\x8a\xdd\xe4\x09\x57\x1a\x9c\x68\x9f\xef\x10\x72"
    "\x88\x55\xdd\x8c\xfb\x9a\x8b\xef\x5c\x89\x43\xef\x3b\x5f\xaa\x15"
    "\xdd\xe6\x98\xbe\xdd\xf3\x59\x96\x03\xeb\x3e\x6f\x61\x37\x2b\xb6"
    "\x28\xf6\x55\x9f\x59\x9a\x78\xbf\x50\x06\x87\xaa\x7f\x49\x76\xc0"
    "\x56\x2d\x41\x29\x56\xf8\x98\x9e\x18\xa6\x35\x5b\xd8\x15\x97\x82"
    "\x5e\x0f\xc8\x75\x34\x3e\xc7\x82\x11\x76\x25\xcd\xbf\x98\x44\x7b";

static const char ps_raop_rsa_exp[] = "\x01\x00\x01";

static const char psz_delim_space[] = " ";
static const char psz_delim_colon[] = ":";
static const char psz_delim_equal[] = "=";
static const char psz_delim_semicolon[] = ";";


/*****************************************************************************
 * Prototypes
 *****************************************************************************/
static int Open( vlc_object_t * );
static void Close( vlc_object_t * );

static sout_stream_id_t *Add( sout_stream_t *, es_format_t * );
static int Del( sout_stream_t *, sout_stream_id_t * );
static int Send( sout_stream_t *, sout_stream_id_t *, block_t* );

static int VolumeCallback( vlc_object_t *p_this, char const *psz_cmd,
                           vlc_value_t oldval, vlc_value_t newval,
                           void *p_data );

typedef enum
{
    JACK_TYPE_NONE = 0,
    JACK_TYPE_ANALOG,
    JACK_TYPE_DIGITAL,
} jack_type_t;

struct sout_stream_sys_t
{
    /* Input parameters */
    char *psz_host;
    char *psz_password;
    int i_volume;

    /* Plugin status */
    sout_stream_id_t *p_audio_stream;
    bool b_alac_warning;
    bool b_volume_callback;

    /* Connection state */
    int i_control_fd;
    int i_stream_fd;

    uint8_t ps_aes_key[16];
    uint8_t ps_aes_iv[16];
    gcry_cipher_hd_t aes_ctx;

    char *psz_url;
    char *psz_client_instance;
    char *psz_session;
    char *psz_last_status_line;

    int i_cseq;
    int i_server_port;
    int i_audio_latency;
    int i_jack_type;

    http_auth_t auth;

    /* Send buffer */
    size_t i_sendbuf_len;
    uint8_t *p_sendbuf;
};

struct sout_stream_id_t
{
    es_format_t fmt;
};


/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define SOUT_CFG_PREFIX "sout-raop-"

#define HOST_TEXT N_("Host")
#define HOST_LONGTEXT N_("Hostname or IP address of target device")

#define VOLUME_TEXT N_("Volume")
#define VOLUME_LONGTEXT N_("Output volume for analog output: 0 for silence, " \
                           "1..255 from almost silent to very loud.")

#define PASSWORD_TEXT N_("Password")
#define PASSWORD_LONGTEXT N_("Password for target device.")

#define PASSWORD_FILE_TEXT N_("Password file")
#define PASSWORD_FILE_LONGTEXT N_("Read password for target device from file.")

vlc_module_begin();
    set_shortname( N_("RAOP") )
    set_description( N_("Remote Audio Output Protocol stream output") )
    set_capability( "sout stream", 0 )
    add_shortcut( "raop" )
    set_category( CAT_SOUT )
    set_subcategory( SUBCAT_SOUT_STREAM )
    add_string( SOUT_CFG_PREFIX "host", "",
                HOST_TEXT, HOST_LONGTEXT, false )
    add_password( SOUT_CFG_PREFIX "password", NULL,
                  PASSWORD_TEXT, PASSWORD_LONGTEXT, false )
    add_loadfile( SOUT_CFG_PREFIX "password-file", NULL,
              PASSWORD_FILE_TEXT, PASSWORD_FILE_LONGTEXT, false )
    add_integer_with_range( SOUT_CFG_PREFIX "volume", 100, 0, 255,
                            VOLUME_TEXT, VOLUME_LONGTEXT, false )
    set_callbacks( Open, Close )
vlc_module_end()

static const char *const ppsz_sout_options[] = {
    "host",
    "password",
    "password-file",
    "volume",
    NULL
};


/*****************************************************************************
 * Utilities:
 *****************************************************************************/
static void FreeSys( vlc_object_t *p_this, sout_stream_sys_t *p_sys )
{
    sout_stream_t *p_stream = (sout_stream_t*)p_this;

    if ( p_sys->i_control_fd >= 0 )
        net_Close( p_sys->i_control_fd );
    if ( p_sys->i_stream_fd >= 0 )
        net_Close( p_sys->i_stream_fd );
    if ( p_sys->b_volume_callback )
        var_DelCallback( p_stream, SOUT_CFG_PREFIX "volume",
                         VolumeCallback, NULL );

    gcry_cipher_close( p_sys->aes_ctx );

    free( p_sys->p_sendbuf );
    free( p_sys->psz_host );
    free( p_sys->psz_password );
    free( p_sys->psz_url );
    free( p_sys->psz_session );
    free( p_sys->psz_client_instance );
    free( p_sys->psz_last_status_line );
    free( p_sys );
}

static void FreeId( sout_stream_id_t *id )
{
    free( id );
}

static void RemoveBase64Padding( char *str )
{
    char *ps_pos = strchr( str, '=' );
    if ( ps_pos != NULL )
        *ps_pos = '\0';
}

static int CheckForGcryptErrorWithLine( sout_stream_t *p_stream,
                                        gcry_error_t i_gcrypt_err,
                                        unsigned int i_line )
{
    if ( i_gcrypt_err != GPG_ERR_NO_ERROR )
    {
        msg_Err( p_stream, "gcrypt error (line %d): %s", i_line,
                 gpg_strerror( i_gcrypt_err ) );
        return 1;
    }

    return 0;
}

/* Wrapper to pass line number for easier debugging */
#define CheckForGcryptError( p_this, i_gcrypt_err ) \
    CheckForGcryptErrorWithLine( p_this, i_gcrypt_err, __LINE__ )

/* MGF1 is specified in RFC2437, section 10.2.1. Variables are named after the
 * specification.
 */
static int MGF1( vlc_object_t *p_this,
                 unsigned char *mask, size_t l,
                 const unsigned char *Z, const size_t zLen,
                 const int Hash )
{
    sout_stream_t *p_stream = (sout_stream_t*)p_this;
    gcry_error_t i_gcrypt_err;
    gcry_md_hd_t md_handle = NULL;
    unsigned int hLen;
    unsigned char *ps_md;
    uint32_t counter = 0;
    uint8_t C[4];
    size_t i_copylen;
    int i_err = VLC_SUCCESS;

    assert( mask != NULL );
    assert( Z != NULL );

    hLen = gcry_md_get_algo_dlen( Hash );

    i_gcrypt_err = gcry_md_open( &md_handle, Hash, 0 );
    if ( CheckForGcryptError( p_stream, i_gcrypt_err ) )
    {
        i_err = VLC_EGENERIC;
        goto error;
    }

    while ( l > 0 )
    {
        /* 3. For counter from 0 to \lceil{l / hLen}\rceil-1, do the following:
         * a. Convert counter to an octet string C of length 4 with the
         *    primitive I2OSP: C = I2OSP (counter, 4)
         */
        C[0] = (counter >> 24) & 0xff;
        C[1] = (counter >> 16) & 0xff;
        C[2] = (counter >> 8) & 0xff;
        C[3] = counter & 0xff;
        ++counter;

        /* b. Concatenate the hash of the seed Z and C to the octet string T:
         *    T = T || Hash (Z || C)
         */
        gcry_md_reset( md_handle );
        gcry_md_write( md_handle, Z, zLen );
        gcry_md_write( md_handle, C, 4 );
        ps_md = gcry_md_read( md_handle, Hash );

        /* 4. Output the leading l octets of T as the octet string mask. */
        i_copylen = __MIN( l, hLen );
        memcpy( mask, ps_md, i_copylen );
        mask += i_copylen;
        l -= i_copylen;
    }

error:
    gcry_md_close( md_handle );

    return i_err;
}

/* EME-OAEP-ENCODE is specified in RFC2437, section 9.1.1.1. Variables are
 * named after the specification.
 */
static int AddOaepPadding( vlc_object_t *p_this,
                           unsigned char *EM, const size_t emLenWithPrefix,
                           const unsigned char *M, const size_t mLen,
                           const unsigned char *P, const size_t pLen )
{
    const int Hash = GCRY_MD_SHA1;
    const unsigned int hLen = gcry_md_get_algo_dlen( Hash );
    unsigned char *seed = NULL;
    unsigned char *DB = NULL;
    unsigned char *dbMask = NULL;
    unsigned char *seedMask = NULL;
    size_t emLen;
    size_t psLen;
    size_t i;
    int i_err = VLC_SUCCESS;

    /* Space for 0x00 prefix in EM. */
    emLen = emLenWithPrefix - 1;

    /* Step 2:
     * If ||M|| > emLen-2hLen-1 then output "message too long" and stop.
     */
    if ( mLen > (emLen - (2 * hLen) - 1) )
    {
        msg_Err( p_this , "Message too long" );
        goto error;
    }

    /* Step 3:
     * Generate an octet string PS consisting of emLen-||M||-2hLen-1 zero
     * octets. The length of PS may be 0.
     */
    psLen = emLen - mLen - (2 * hLen) - 1;

    /*
     * Step 5:
     * Concatenate pHash, PS, the message M, and other padding to form a data
     * block DB as: DB = pHash || PS || 01 || M
     */
    DB = calloc( 1, hLen + psLen + 1 + mLen );
    dbMask = calloc( 1, emLen - hLen );
    seedMask = calloc( 1, hLen );

    if ( DB == NULL || dbMask == NULL || seedMask == NULL )
    {
        i_err = VLC_ENOMEM;
        goto error;
    }

    /* Step 4:
     * Let pHash = Hash(P), an octet string of length hLen.
     */
    gcry_md_hash_buffer( Hash, DB, P, pLen );

    /* Step 3:
     * Generate an octet string PS consisting of emLen-||M||-2hLen-1 zero
     * octets. The length of PS may be 0.
     */
    memset( DB + hLen, 0, psLen );

    /* Step 5:
     * Concatenate pHash, PS, the message M, and other padding to form a data
     * block DB as: DB = pHash || PS || 01 || M
     */
    DB[hLen + psLen] = 0x01;
    memcpy( DB + hLen + psLen + 1, M, mLen );

    /* Step 6:
     * Generate a random octet string seed of length hLen
     */
    seed = gcry_random_bytes( hLen, GCRY_STRONG_RANDOM );
    if ( seed == NULL )
    {
        i_err = VLC_ENOMEM;
        goto error;
    }

    /* Step 7:
     * Let dbMask = MGF(seed, emLen-hLen).
     */
    i_err = MGF1( p_this, dbMask, emLen - hLen, seed, hLen, Hash );
    if ( i_err != VLC_SUCCESS )
        goto error;

    /* Step 8:
     * Let maskedDB = DB \xor dbMask.
     */
    for ( i = 0; i < (emLen - hLen); ++i )
        DB[i] ^= dbMask[i];

    /* Step 9:
     * Let seedMask = MGF(maskedDB, hLen).
     */
    i_err = MGF1( p_this, seedMask, hLen, DB, emLen - hLen, Hash );
    if ( i_err != VLC_SUCCESS )
        goto error;

    /* Step 10:
     * Let maskedSeed = seed \xor seedMask.
     */
    for ( i = 0; i < hLen; ++i )
        seed[i] ^= seedMask[i];

    /* Step 11:
     * Let EM = maskedSeed || maskedDB.
     */
    assert( (1 + hLen + (hLen + psLen + 1 + mLen)) == emLenWithPrefix );
    EM[0] = 0x00;
    memcpy( EM + 1, seed, hLen );
    memcpy( EM + 1 + hLen, DB, hLen + psLen + 1 + mLen );

    /* Step 12:
     * Output EM.
     */

error:
    free( DB );
    free( dbMask );
    free( seedMask );
    free( seed );

    return i_err;
}

static int EncryptAesKeyBase64( vlc_object_t *p_this, char **result )
{
    sout_stream_t *p_stream = (sout_stream_t*)p_this;
    sout_stream_sys_t *p_sys = p_stream->p_sys;
    gcry_error_t i_gcrypt_err;
    gcry_sexp_t sexp_rsa_params = NULL;
    gcry_sexp_t sexp_input = NULL;
    gcry_sexp_t sexp_encrypted = NULL;
    gcry_sexp_t sexp_token_a = NULL;
    gcry_mpi_t mpi_pubkey = NULL;
    gcry_mpi_t mpi_exp = NULL;
    gcry_mpi_t mpi_input = NULL;
    gcry_mpi_t mpi_output = NULL;
    unsigned char ps_padded_key[256];
    unsigned char *ps_value;
    size_t i_value_size;
    int i_err;

    /* Add RSA-OAES-SHA1 padding */
    i_err = AddOaepPadding( p_this,
                            ps_padded_key, sizeof( ps_padded_key ),
                            p_sys->ps_aes_key, sizeof( p_sys->ps_aes_key ),
                            NULL, 0 );
    if ( i_err != VLC_SUCCESS )
        goto error;
    i_err = VLC_EGENERIC;

    /* Read public key */
    i_gcrypt_err = gcry_mpi_scan( &mpi_pubkey, GCRYMPI_FMT_USG,
                                  ps_raop_rsa_pubkey,
                                  sizeof( ps_raop_rsa_pubkey ) - 1, NULL );
    if ( CheckForGcryptError( p_stream, i_gcrypt_err ) )
        goto error;

    /* Read exponent */
    i_gcrypt_err = gcry_mpi_scan( &mpi_exp, GCRYMPI_FMT_USG, ps_raop_rsa_exp,
                                  sizeof( ps_raop_rsa_exp ) - 1, NULL );
    if ( CheckForGcryptError( p_stream, i_gcrypt_err ) )
        goto error;

    /* If the input data starts with a set bit (0x80), gcrypt thinks it's a
     * signed integer and complains. Prefixing it with a zero byte (\0)
     * works, but involves more work. Converting it to an MPI in our code is
     * cleaner.
     */
    i_gcrypt_err = gcry_mpi_scan( &mpi_input, GCRYMPI_FMT_USG,
                                  ps_padded_key, sizeof( ps_padded_key ),
                                  NULL);
    if ( CheckForGcryptError( p_stream, i_gcrypt_err ) )
        goto error;

    /* Build S-expression with RSA parameters */
    i_gcrypt_err = gcry_sexp_build( &sexp_rsa_params, NULL,
                                    "(public-key(rsa(n %m)(e %m)))",
                                    mpi_pubkey, mpi_exp );
    if ( CheckForGcryptError( p_stream, i_gcrypt_err ) )
        goto error;

    /* Build S-expression for data */
    i_gcrypt_err = gcry_sexp_build( &sexp_input, NULL, "(data(value %m))",
                                    mpi_input );
    if ( CheckForGcryptError( p_stream, i_gcrypt_err ) )
        goto error;

    /* Encrypt data */
    i_gcrypt_err = gcry_pk_encrypt( &sexp_encrypted, sexp_input,
                                    sexp_rsa_params );
    if ( CheckForGcryptError( p_stream, i_gcrypt_err ) )
        goto error;

    /* Extract encrypted data */
    sexp_token_a = gcry_sexp_find_token( sexp_encrypted, "a", 0 );
    if ( !sexp_token_a )
    {
        msg_Err( p_this , "Token 'a' not found in result S-expression" );
        goto error;
    }

    mpi_output = gcry_sexp_nth_mpi( sexp_token_a, 1, GCRYMPI_FMT_USG );
    if ( !mpi_output )
    {
        msg_Err( p_this, "Unable to extract MPI from result" );
        goto error;
    }

    /* Copy encrypted data into char array */
    i_gcrypt_err = gcry_mpi_aprint( GCRYMPI_FMT_USG, &ps_value, &i_value_size,
                                    mpi_output );
    if ( CheckForGcryptError( p_stream, i_gcrypt_err ) )
    {
        goto error;
    }

    /* Encode in Base64 */
    *result = vlc_b64_encode_binary( ps_value, i_value_size );
    i_err = VLC_SUCCESS;

error:
    gcry_sexp_release( sexp_rsa_params );
    gcry_sexp_release( sexp_input );
    gcry_sexp_release( sexp_encrypted );
    gcry_sexp_release( sexp_token_a );
    gcry_mpi_release( mpi_pubkey );
    gcry_mpi_release( mpi_exp );
    gcry_mpi_release( mpi_input );
    gcry_mpi_release( mpi_output );

    return i_err;
}

static char *ReadPasswordFile( vlc_object_t *p_this, const char *psz_path )
{
    FILE *p_file = NULL;
    char *psz_password = NULL;
    char *psz_newline;
    char ps_buffer[256];

    p_file = vlc_fopen( psz_path, "rt" );
    if ( p_file == NULL )
    {
        msg_Err( p_this, "Unable to open password file '%s': %m", psz_path );
        goto error;
    }

    /* Read one line only */
    if ( fgets( ps_buffer, sizeof( ps_buffer ), p_file ) == NULL )
    {
        if ( ferror( p_file ) )
        {
            msg_Err( p_this, "Error reading '%s': %m", psz_path );
            goto error;
        }

        /* Nothing was read, but there was no error either. Maybe the file is
         * empty. Not all implementations of fgets(3) write \0 to the output
         * buffer in this case.
         */
        ps_buffer[0] = '\0';

    } else {
        /* Replace first newline with '\0' */
        psz_newline = strchr( ps_buffer, '\n' );
        if ( psz_newline != NULL )
            *psz_newline = '\0';
    }

    if ( *ps_buffer == '\0' ) {
        msg_Err( p_this, "No password could be read from '%s'", psz_path );
        goto error;
    }

    psz_password = strdup( ps_buffer );

error:
    if ( p_file != NULL )
        fclose( p_file );

    return psz_password;
}

/* Splits the value of a received header.
 *
 * Example: "Transport: RTP/AVP/TCP;unicast;mode=record;server_port=6000"
 */
static int SplitHeader( char **ppsz_next, char **ppsz_name,
                        char **ppsz_value )
{
    /* Find semicolon (separator between assignments) */
    *ppsz_name = strsep( ppsz_next, psz_delim_semicolon );
    if ( *ppsz_name )
    {
        /* Skip spaces */
        *ppsz_name += strspn( *ppsz_name, psz_delim_space );

        /* Get value */
        *ppsz_value = *ppsz_name;
        strsep( ppsz_value, psz_delim_equal );
    }
    else
        *ppsz_value = NULL;

    return !!*ppsz_name;
}

static void FreeHeader( void *p_value, void *p_data )
{
    VLC_UNUSED( p_data );
    free( p_value );
}

static int ReadStatusLine( vlc_object_t *p_this )
{
    sout_stream_t *p_stream = (sout_stream_t*)p_this;
    sout_stream_sys_t *p_sys = p_stream->p_sys;
    char *psz_line = NULL;
    char *psz_token;
    char *psz_next;
    int i_result = VLC_EGENERIC;

    p_sys->psz_last_status_line = net_Gets( p_this, p_sys->i_control_fd,
                                            NULL );
    if ( !p_sys->psz_last_status_line )
        goto error;

    /* Create working copy */
    psz_line = strdup( p_sys->psz_last_status_line );
    psz_next = psz_line;

    /* Protocol field */
    psz_token = strsep( &psz_next, psz_delim_space );
    if ( !psz_token || strncmp( psz_token, "RTSP/1.", 7 ) != 0 )
    {
        msg_Err( p_this, "Unknown protocol (%s)",
                 p_sys->psz_last_status_line );
        goto error;
    }

    /* Status field */
    psz_token = strsep( &psz_next, psz_delim_space );
    if ( !psz_token )
    {
        msg_Err( p_this, "Request failed (%s)",
                 p_sys->psz_last_status_line );
        goto error;
    }

    i_result = atoi( psz_token );

error:
    free( psz_line );

    return i_result;
}

static int ReadHeader( vlc_object_t *p_this,
                       vlc_dictionary_t *p_resp_headers,
                       int *done )
{
    sout_stream_t *p_stream = (sout_stream_t*)p_this;
    sout_stream_sys_t *p_sys = p_stream->p_sys;
    char *psz_original = NULL;
    char *psz_line = NULL;
    char *psz_token;
    char *psz_next;
    char *psz_name;
    char *psz_value;
    int i_err = VLC_SUCCESS;

    psz_line = net_Gets( p_this, p_sys->i_control_fd, NULL );
    if ( !psz_line )
    {
        i_err = VLC_EGENERIC;
        goto error;
    }

    /* Empty line for response end */
    if ( psz_line[0] == '\0' )
        *done = 1;
    else
    {
        psz_original = strdup( psz_line );
        psz_next = psz_line;

        psz_token = strsep( &psz_next, psz_delim_colon );
        if ( !psz_token || psz_next[0] != ' ' )
        {
            msg_Err( p_this, "Invalid header format (%s)", psz_original );
            i_err = VLC_EGENERIC;
            goto error;
        }

        psz_name = psz_token;
        psz_value = psz_next + 1;

        vlc_dictionary_insert( p_resp_headers, psz_name, strdup( psz_value ) );
    }

error:
    free( psz_original );
    free( psz_line );

    return i_err;
}

static int WriteAuxHeaders( vlc_object_t *p_this,
                            vlc_dictionary_t *p_req_headers )
{
    sout_stream_t *p_stream = (sout_stream_t*)p_this;
    sout_stream_sys_t *p_sys = p_stream->p_sys;
    char **ppsz_keys = NULL;
    char *psz_key;
    char *psz_value;
    int i_err = VLC_SUCCESS;
    int i_rc;
    size_t i;

    ppsz_keys = vlc_dictionary_all_keys( p_req_headers );
    for ( i = 0; ppsz_keys[i]; ++i )
    {
        psz_key = ppsz_keys[i];
        psz_value = vlc_dictionary_value_for_key( p_req_headers, psz_key );

        i_rc = net_Printf( p_this, p_sys->i_control_fd, NULL,
                           "%s: %s\r\n", psz_key, psz_value );
        if ( i_rc < 0 )
        {
            i_err = VLC_EGENERIC;
            goto error;
        }
    }

error:
    for ( i = 0; ppsz_keys[i]; ++i )
        free( ppsz_keys[i] );
    free( ppsz_keys );

    return i_err;
}

static int SendRequest( vlc_object_t *p_this, const char *psz_method,
                        const char *psz_content_type, const char *psz_body,
                        vlc_dictionary_t *p_req_headers )
{
    sout_stream_t *p_stream = (sout_stream_t*)p_this;
    sout_stream_sys_t *p_sys = p_stream->p_sys;
    const unsigned char psz_headers_end[] = "\r\n";
    size_t i_body_length = 0;
    int i_err = VLC_SUCCESS;
    int i_rc;

    i_rc = net_Printf( p_this, p_sys->i_control_fd, NULL,
                       "%s %s RTSP/1.0\r\n"
                       "User-Agent: " RAOP_USER_AGENT "\r\n"
                       "Client-Instance: %s\r\n"
                       "CSeq: %d\r\n",
                       psz_method, p_sys->psz_url,
                       p_sys->psz_client_instance,
                       ++p_sys->i_cseq );
    if ( i_rc < 0 )
    {
        i_err = VLC_EGENERIC;
        goto error;
    }

    if ( psz_content_type )
    {
        i_rc = net_Printf( p_this, p_sys->i_control_fd, NULL,
                           "Content-Type: %s\r\n", psz_content_type );
        if ( i_rc < 0 )
        {
            i_err = VLC_ENOMEM;
            goto error;
        }
    }

    if ( psz_body )
    {
        i_body_length = strlen( psz_body );

        i_rc = net_Printf( p_this, p_sys->i_control_fd, NULL,
                           "Content-Length: %u\r\n",
                           (unsigned int)i_body_length );
        if ( i_rc < 0 )
        {
            i_err = VLC_ENOMEM;
            goto error;
        }
    }

    i_err = WriteAuxHeaders( p_this, p_req_headers );
    if ( i_err != VLC_SUCCESS )
        goto error;

    i_rc = net_Write( p_this, p_sys->i_control_fd, NULL,
                      psz_headers_end, sizeof( psz_headers_end ) - 1 );
    if ( i_rc < 0 )
    {
        i_err = VLC_ENOMEM;
        goto error;
    }

    if ( psz_body )
        net_Write( p_this, p_sys->i_control_fd, NULL,
                   psz_body, i_body_length );

error:
    return i_err;
}

static int ParseAuthenticateHeader( vlc_object_t *p_this,
                                    vlc_dictionary_t *p_resp_headers )
{
    sout_stream_t *p_stream = (sout_stream_t*)p_this;
    sout_stream_sys_t *p_sys = p_stream->p_sys;
    char *psz_auth;
    int i_err = VLC_SUCCESS;

    psz_auth = vlc_dictionary_value_for_key( p_resp_headers,
                                             "WWW-Authenticate" );
    if ( psz_auth == NULL )
    {
        msg_Err( p_this, "HTTP 401 response missing "
                         "WWW-Authenticate header" );
        i_err = VLC_EGENERIC;
        goto error;
    }

    http_auth_ParseWwwAuthenticateHeader( p_this, &p_sys->auth, psz_auth );

error:
    return i_err;
}

static int ExecRequest( vlc_object_t *p_this, const char *psz_method,
                        const char *psz_content_type, const char *psz_body,
                        vlc_dictionary_t *p_req_headers,
                        vlc_dictionary_t *p_resp_headers )
{
    sout_stream_t *p_stream = (sout_stream_t*)p_this;
    sout_stream_sys_t *p_sys = p_stream->p_sys;
    char *psz_authorization = NULL;
    int headers_done;
    int i_err = VLC_SUCCESS;
    int i_status;
    int i_auth_state;

    if ( p_sys->i_control_fd < 0 )
    {
        msg_Err( p_this, "Control connection not open" );
        i_err = VLC_EGENERIC;
        goto error;
    }

    i_auth_state = 0;
    while ( 1 )
    {
        /* Send header only when Digest authentication is used */
        if ( p_sys->psz_password != NULL && p_sys->auth.psz_nonce != NULL )
        {
            FREENULL( psz_authorization );

            psz_authorization =
                http_auth_FormatAuthorizationHeader( p_this, &p_sys->auth,
                                                     psz_method,
                                                     p_sys->psz_url, "",
                                                     p_sys->psz_password );
            if ( psz_authorization == NULL )
            {
                i_err = VLC_EGENERIC;
                goto error;
            }

            vlc_dictionary_insert( p_req_headers, "Authorization",
                                   psz_authorization );
        }

        /* Send request */
        i_err = SendRequest( p_this, psz_method, psz_content_type, psz_body,
                             p_req_headers);
        if ( i_err != VLC_SUCCESS )
            goto error;

        /* Read status line */
        i_status = ReadStatusLine( p_this );
        if ( i_status < 0 )
        {
            i_err = i_status;
            goto error;
        }

        vlc_dictionary_clear( p_resp_headers, FreeHeader, NULL );

        /* Read headers */
        headers_done = 0;
        while ( !headers_done )
        {
            i_err = ReadHeader( p_this, p_resp_headers, &headers_done );
            if ( i_err != VLC_SUCCESS )
                goto error;
        }

        if ( i_status == 200 )
            /* Request successful */
            break;
        else if ( i_status == 401 )
        {
            /* Authorization required */
            if ( i_auth_state == 1 || p_sys->psz_password == NULL )
            {
                msg_Err( p_this, "Access denied, password invalid" );
                i_err = VLC_EGENERIC;
                goto error;
            }

            i_err = ParseAuthenticateHeader( p_this, p_resp_headers );
            if ( i_err != VLC_SUCCESS )
                goto error;

            i_auth_state = 1;
        }
        else
        {
            msg_Err( p_this, "Request failed (%s), status is %d",
                     p_sys->psz_last_status_line, i_status );
            i_err = VLC_EGENERIC;
            goto error;
        }
    }

error:
    FREENULL( p_sys->psz_last_status_line );
    free( psz_authorization );

    return i_err;
}

static int AnnounceSDP( vlc_object_t *p_this, char *psz_local,
                        uint32_t i_session_id )
{
    sout_stream_t *p_stream = (sout_stream_t*)p_this;
    sout_stream_sys_t *p_sys = p_stream->p_sys;
    vlc_dictionary_t req_headers;
    vlc_dictionary_t resp_headers;
    unsigned char ps_sac[16];
    char *psz_sdp = NULL;
    char *psz_sac_base64 = NULL;
    char *psz_aes_key_base64 = NULL;
    char *psz_aes_iv_base64 = NULL;
    int i_err = VLC_SUCCESS;
    int i_rc;

    vlc_dictionary_init( &req_headers, 0 );
    vlc_dictionary_init( &resp_headers, 0 );

    /* Encrypt AES key and encode it in Base64 */
    i_rc = EncryptAesKeyBase64( p_this, &psz_aes_key_base64 );
    if ( i_rc != VLC_SUCCESS || psz_aes_key_base64 == NULL )
    {
        i_err = VLC_EGENERIC;
        goto error;
    }
    RemoveBase64Padding( psz_aes_key_base64 );

    /* Encode AES IV in Base64 */
    psz_aes_iv_base64 = vlc_b64_encode_binary( p_sys->ps_aes_iv,
                                               sizeof( p_sys->ps_aes_iv ) );
    if ( psz_aes_iv_base64 == NULL )
    {
        i_err = VLC_EGENERIC;
        goto error;
    }
    RemoveBase64Padding( psz_aes_iv_base64 );

    /* Random bytes for Apple-Challenge header */
    gcry_randomize( ps_sac, sizeof( ps_sac ), GCRY_STRONG_RANDOM );

    psz_sac_base64 = vlc_b64_encode_binary( ps_sac, sizeof( ps_sac ) );
    if ( psz_sac_base64 == NULL )
    {
        i_err = VLC_EGENERIC;
        goto error;
    }
    RemoveBase64Padding( psz_sac_base64 );

    /* Build SDP
     * Note: IPv6 addresses also use "IP4". Make sure not to include the
     * scope ID.
     */
    i_rc = asprintf( &psz_sdp,
                     "v=0\r\n"
                     "o=iTunes %u 0 IN IP4 %s\r\n"
                     "s=iTunes\r\n"
                     "c=IN IP4 %s\r\n"
                     "t=0 0\r\n"
                     "m=audio 0 RTP/AVP 96\r\n"
                     "a=rtpmap:96 AppleLossless\r\n"
                     "a=fmtp:96 4096 0 16 40 10 14 2 255 0 0 44100\r\n"
                     "a=rsaaeskey:%s\r\n"
                     "a=aesiv:%s\r\n",
                     i_session_id, psz_local, p_sys->psz_host,
                     psz_aes_key_base64, psz_aes_iv_base64 );

    if ( i_rc < 0 )
    {
        i_err = VLC_ENOMEM;
        goto error;
    }

    /* Build and send request */
    vlc_dictionary_insert( &req_headers, "Apple-Challenge", psz_sac_base64 );

    i_err = ExecRequest( p_this, "ANNOUNCE", "application/sdp", psz_sdp,
                         &req_headers, &resp_headers);
    if ( i_err != VLC_SUCCESS )
        goto error;

error:
    vlc_dictionary_clear( &req_headers, NULL, NULL );
    vlc_dictionary_clear( &resp_headers, FreeHeader, NULL );

    free( psz_sdp );
    free( psz_sac_base64 );
    free( psz_aes_key_base64 );
    free( psz_aes_iv_base64 );

    return i_err;
}

static int SendSetup( vlc_object_t *p_this )
{
    sout_stream_t *p_stream = (sout_stream_t*)p_this;
    sout_stream_sys_t *p_sys = p_stream->p_sys;
    vlc_dictionary_t req_headers;
    vlc_dictionary_t resp_headers;
    int i_err = VLC_SUCCESS;
    char *psz_tmp;
    char *psz_next;
    char *psz_name;
    char *psz_value;

    vlc_dictionary_init( &req_headers, 0 );
    vlc_dictionary_init( &resp_headers, 0 );

    vlc_dictionary_insert( &req_headers, "Transport",
                           ((void*)"RTP/AVP/TCP;unicast;interleaved=0-1;"
                            "mode=record") );

    i_err = ExecRequest( p_this, "SETUP", NULL, NULL,
                         &req_headers, &resp_headers );
    if ( i_err != VLC_SUCCESS )
        goto error;

    psz_tmp = vlc_dictionary_value_for_key( &resp_headers, "Session" );
    if ( !psz_tmp )
    {
        msg_Err( p_this, "Missing 'Session' header during setup" );
        i_err = VLC_EGENERIC;
        goto error;
    }

    free( p_sys->psz_session );
    p_sys->psz_session = strdup( psz_tmp );

    /* Get server_port */
    psz_next = vlc_dictionary_value_for_key( &resp_headers, "Transport" );
    while ( SplitHeader( &psz_next, &psz_name, &psz_value ) )
    {
        if ( psz_value && strcmp( psz_name, "server_port" ) == 0 )
        {
            p_sys->i_server_port = atoi( psz_value );
            break;
        }
    }

    if ( !p_sys->i_server_port )
    {
        msg_Err( p_this, "Missing 'server_port' during setup" );
        i_err = VLC_EGENERIC;
        goto error;
    }

    /* Get jack type */
    psz_next = vlc_dictionary_value_for_key( &resp_headers,
                                             "Audio-Jack-Status" );
    while ( SplitHeader( &psz_next, &psz_name, &psz_value ) )
    {
        if ( strcmp( psz_name, "type" ) != 0 )
            continue;

        if ( strcmp( psz_value, "analog" ) == 0 )
            p_sys->i_jack_type = JACK_TYPE_ANALOG;

        else if ( strcmp( psz_value, "digital" ) == 0 )
            p_sys->i_jack_type = JACK_TYPE_DIGITAL;

        break;
    }

error:
    vlc_dictionary_clear( &req_headers, NULL, NULL );
    vlc_dictionary_clear( &resp_headers, FreeHeader, NULL );

    return i_err;
}

static int SendRecord( vlc_object_t *p_this )
{
    sout_stream_t *p_stream = (sout_stream_t*)p_this;
    sout_stream_sys_t *p_sys = p_stream->p_sys;
    vlc_dictionary_t req_headers;
    vlc_dictionary_t resp_headers;
    int i_err = VLC_SUCCESS;
    char *psz_value;

    vlc_dictionary_init( &req_headers, 0 );
    vlc_dictionary_init( &resp_headers, 0 );

    vlc_dictionary_insert( &req_headers, "Range", (void *)"npt=0-" );
    vlc_dictionary_insert( &req_headers, "RTP-Info",
                           (void *)"seq=0;rtptime=0" );
    vlc_dictionary_insert( &req_headers, "Session",
                           (void *)p_sys->psz_session );

    i_err = ExecRequest( p_this, "RECORD", NULL, NULL,
                         &req_headers, &resp_headers );
    if ( i_err != VLC_SUCCESS )
        goto error;

    psz_value = vlc_dictionary_value_for_key( &resp_headers, "Audio-Latency" );
    if ( psz_value )
        p_sys->i_audio_latency = atoi( psz_value );
    else
        p_sys->i_audio_latency = 0;

error:
    vlc_dictionary_clear( &req_headers, NULL, NULL );
    vlc_dictionary_clear( &resp_headers, FreeHeader, NULL );

    return i_err;
}

static int SendFlush( vlc_object_t *p_this )
{
    VLC_UNUSED( p_this );
    vlc_dictionary_t resp_headers;
    vlc_dictionary_t req_headers;
    int i_err = VLC_SUCCESS;

    vlc_dictionary_init( &req_headers, 0 );
    vlc_dictionary_init( &resp_headers, 0 );

    vlc_dictionary_insert( &req_headers, "RTP-Info",
                           (void *)"seq=0;rtptime=0" );

    i_err = ExecRequest( p_this, "FLUSH", NULL, NULL,
                         &req_headers, &resp_headers );
    if ( i_err != VLC_SUCCESS )
        goto error;

error:
    vlc_dictionary_clear( &req_headers, NULL, NULL );
    vlc_dictionary_clear( &resp_headers, FreeHeader, NULL );

    return i_err;
}

static int SendTeardown( vlc_object_t *p_this )
{
    vlc_dictionary_t resp_headers;
    vlc_dictionary_t req_headers;
    int i_err = VLC_SUCCESS;

    vlc_dictionary_init( &req_headers, 0 );
    vlc_dictionary_init( &resp_headers, 0 );

    i_err = ExecRequest( p_this, "TEARDOWN", NULL, NULL,
                         &req_headers, &resp_headers );
    if ( i_err != VLC_SUCCESS )
        goto error;

error:
    vlc_dictionary_clear( &req_headers, NULL, NULL );
    vlc_dictionary_clear( &resp_headers, FreeHeader, NULL );

    return i_err;
}

static int UpdateVolume( vlc_object_t *p_this )
{
    sout_stream_t *p_stream = (sout_stream_t*)p_this;
    sout_stream_sys_t *p_sys = p_stream->p_sys;
    vlc_dictionary_t req_headers;
    vlc_dictionary_t resp_headers;
    char *psz_parameters = NULL;
    double d_volume;
    int i_err = VLC_SUCCESS;
    int i_rc;

    vlc_dictionary_init( &req_headers, 0 );
    vlc_dictionary_init( &resp_headers, 0 );

    /* Our volume is 0..255, RAOP is -144..0 (-144 off, -30..0 on) */

    /* Limit range */
    p_sys->i_volume = VLC_CLIP( p_sys->i_volume, 0, 255 );

    if ( p_sys->i_volume == 0 )
        d_volume = -144.0;
    else
        d_volume = -30 + ( ( (double)p_sys->i_volume ) * 30.0 / 255.0 );

    /* Format without using locales */
    i_rc = us_asprintf( &psz_parameters, "volume: %0.6f\r\n", d_volume );
    if ( i_rc < 0 )
    {
        i_err = VLC_ENOMEM;
        goto error;
    }

    vlc_dictionary_insert( &req_headers, "Session",
                           (void *)p_sys->psz_session );

    i_err = ExecRequest( p_this, "SET_PARAMETER",
                         "text/parameters", psz_parameters,
                         &req_headers, &resp_headers );
    if ( i_err != VLC_SUCCESS )
        goto error;

error:
    vlc_dictionary_clear( &req_headers, NULL, NULL );
    vlc_dictionary_clear( &resp_headers, FreeHeader, NULL );
    free( psz_parameters );

    return i_err;
}

static void LogInfo( vlc_object_t *p_this )
{
    sout_stream_t *p_stream = (sout_stream_t*)p_this;
    sout_stream_sys_t *p_sys = p_stream->p_sys;
    const char *psz_jack_name;

    msg_Info( p_this, "Audio latency: %d", p_sys->i_audio_latency );

    switch ( p_sys->i_jack_type )
    {
        case JACK_TYPE_ANALOG:
            psz_jack_name = "analog";
            break;

        case JACK_TYPE_DIGITAL:
            psz_jack_name = "digital";
            break;

        case JACK_TYPE_NONE:
        default:
            psz_jack_name = "none";
            break;
    }

    msg_Info( p_this, "Jack type: %s", psz_jack_name );
}

static void SendAudio( sout_stream_t *p_stream, block_t *p_buffer )
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;
    gcry_error_t i_gcrypt_err;
    block_t *p_next;
    size_t i_len;
    size_t i_payload_len;
    size_t i_realloc_len;
    int rc;

    const uint8_t header[16] = {
        0x24, 0x00, 0x00, 0x00,
        0xf0, 0xff, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
    };

    while ( p_buffer )
    {
        i_len = sizeof( header ) + p_buffer->i_buffer;

        /* Buffer resize needed? */
        if ( i_len > p_sys->i_sendbuf_len || p_sys->p_sendbuf == NULL )
        {
            /* Grow in blocks of 4K */
            i_realloc_len = (1 + (i_len / 4096)) * 4096;

            p_sys->p_sendbuf = realloc_or_free( p_sys->p_sendbuf, i_realloc_len );
            if ( p_sys->p_sendbuf == NULL )
                goto error;

            p_sys->i_sendbuf_len = i_realloc_len;
        }

        /* Fill buffer */
        memcpy( p_sys->p_sendbuf, header, sizeof( header ) );
        memcpy( p_sys->p_sendbuf + sizeof( header ),
                p_buffer->p_buffer, p_buffer->i_buffer );

        /* Calculate payload length and update header */
        i_payload_len = i_len - 4;
        if ( i_payload_len > 0xffff )
        {
            msg_Err( p_stream, "Buffer is too long (%u bytes)",
                     (unsigned int)i_payload_len );
            goto error;
        }

        p_sys->p_sendbuf[2] = ( i_payload_len >> 8 ) & 0xff;
        p_sys->p_sendbuf[3] = i_payload_len & 0xff;

        /* Reset cipher */
        i_gcrypt_err = gcry_cipher_reset( p_sys->aes_ctx );
        if ( CheckForGcryptError( p_stream, i_gcrypt_err ) )
            goto error;

        /* Set IV */
        i_gcrypt_err = gcry_cipher_setiv( p_sys->aes_ctx, p_sys->ps_aes_iv,
                                          sizeof( p_sys->ps_aes_iv ) );
        if ( CheckForGcryptError( p_stream, i_gcrypt_err ) )
            goto error;

        /* Encrypt in place. Only full blocks of 16 bytes are encrypted,
         * the rest (0-15 bytes) is left unencrypted.
         */
        i_gcrypt_err =
            gcry_cipher_encrypt( p_sys->aes_ctx,
                                 p_sys->p_sendbuf + sizeof( header ),
                                 ( p_buffer->i_buffer / 16 ) * 16,
                                 NULL, 0 );
        if ( CheckForGcryptError( p_stream, i_gcrypt_err ) )
            goto error;

        /* Send data */
        rc = net_Write( p_stream, p_sys->i_stream_fd, NULL,
                        p_sys->p_sendbuf, i_len );
        if ( rc < 0 )
            goto error;

        p_next = p_buffer->p_next;
        block_Release( p_buffer );
        p_buffer = p_next;
    }

error:
    block_ChainRelease( p_buffer );
    return;
}


/*****************************************************************************
 * Open:
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    sout_stream_t *p_stream = (sout_stream_t*)p_this;
    sout_stream_sys_t *p_sys;
    char psz_local[NI_MAXNUMERICHOST];
    char *psz_pwfile = NULL;
    gcry_error_t i_gcrypt_err;
    int i_err = VLC_SUCCESS;
    uint32_t i_session_id;
    uint64_t i_client_instance;

    vlc_gcrypt_init();

    config_ChainParse( p_stream, SOUT_CFG_PREFIX, ppsz_sout_options,
                       p_stream->p_cfg );

    p_sys = calloc( 1, sizeof( *p_sys ) );
    if ( p_sys == NULL )
    {
        i_err = VLC_ENOMEM;
        goto error;
    }

    p_stream->pf_add = Add;
    p_stream->pf_del = Del;
    p_stream->pf_send = Send;
    p_stream->p_sys = p_sys;
    p_stream->pace_nocontrol = true;

    p_sys->i_control_fd = -1;
    p_sys->i_stream_fd = -1;
    p_sys->i_volume = var_GetInteger( p_stream, SOUT_CFG_PREFIX "volume");
    p_sys->i_jack_type = JACK_TYPE_NONE;

    http_auth_Init( &p_sys->auth );

    p_sys->psz_host = var_GetNonEmptyString( p_stream,
                                             SOUT_CFG_PREFIX "host" );
    if ( p_sys->psz_host == NULL )
    {
        msg_Err( p_this, "Missing host" );
        i_err = VLC_EGENERIC;
        goto error;
    }

    p_sys->psz_password = var_GetNonEmptyString( p_stream,
                                                 SOUT_CFG_PREFIX "password" );
    if ( p_sys->psz_password == NULL )
    {
        /* Try password file instead */
        psz_pwfile = var_GetNonEmptyString( p_stream,
                                            SOUT_CFG_PREFIX "password-file" );
        if ( psz_pwfile != NULL )
        {
            p_sys->psz_password = ReadPasswordFile( p_this, psz_pwfile );
            if ( p_sys->psz_password == NULL )
            {
                i_err = VLC_EGENERIC;
                goto error;
            }
        }
    }

    if ( p_sys->psz_password != NULL )
        msg_Info( p_this, "Using password authentication" );

    var_AddCallback( p_stream, SOUT_CFG_PREFIX "volume",
                     VolumeCallback, NULL );
    p_sys->b_volume_callback = true;

    /* Open control connection */
    p_sys->i_control_fd = net_ConnectTCP( p_stream, p_sys->psz_host,
                                          RAOP_PORT );
    if ( p_sys->i_control_fd < 0 )
    {
        msg_Err( p_this, "Cannot establish control connection to %s:%d (%m)",
                 p_sys->psz_host, RAOP_PORT );
        i_err = VLC_EGENERIC;
        goto error;
    }

    /* Get local IP address */
    if ( net_GetSockAddress( p_sys->i_control_fd, psz_local, NULL ) )
    {
        msg_Err( p_this, "cannot get local IP address" );
        i_err = VLC_EGENERIC;
        goto error;
    }

    /* Random session ID */
    gcry_randomize( &i_session_id, sizeof( i_session_id ),
                    GCRY_STRONG_RANDOM );

    /* Random client instance */
    gcry_randomize( &i_client_instance, sizeof( i_client_instance ),
                    GCRY_STRONG_RANDOM );
    if ( asprintf( &p_sys->psz_client_instance, "%016"PRIX64,
                   i_client_instance ) < 0 )
    {
        i_err = VLC_ENOMEM;
        goto error;
    }

    /* Build session URL */
    if ( asprintf( &p_sys->psz_url, "rtsp://%s/%u",
                   psz_local, i_session_id ) < 0 )
    {
        i_err = VLC_ENOMEM;
        goto error;
    }

    /* Generate AES key and IV */
    gcry_randomize( p_sys->ps_aes_key, sizeof( p_sys->ps_aes_key ),
                    GCRY_STRONG_RANDOM );
    gcry_randomize( p_sys->ps_aes_iv, sizeof( p_sys->ps_aes_iv ),
                    GCRY_STRONG_RANDOM );

    /* Setup AES */
    i_gcrypt_err = gcry_cipher_open( &p_sys->aes_ctx, GCRY_CIPHER_AES,
                                     GCRY_CIPHER_MODE_CBC, 0 );
    if ( CheckForGcryptError( p_stream, i_gcrypt_err ) )
    {
        i_err = VLC_EGENERIC;
        goto error;
    }

    /* Set key */
    i_gcrypt_err = gcry_cipher_setkey( p_sys->aes_ctx, p_sys->ps_aes_key,
                                       sizeof( p_sys->ps_aes_key ) );
    if ( CheckForGcryptError( p_stream, i_gcrypt_err ) )
    {
        i_err = VLC_EGENERIC;
        goto error;
    }

    /* Protocol handshake */
    i_err = AnnounceSDP( p_this, psz_local, i_session_id );
    if ( i_err != VLC_SUCCESS )
        goto error;

    i_err = SendSetup( p_this );
    if ( i_err != VLC_SUCCESS )
        goto error;

    i_err = SendRecord( p_this );
    if ( i_err != VLC_SUCCESS )
        goto error;

    i_err = UpdateVolume( p_this );
    if ( i_err != VLC_SUCCESS )
        goto error;

    LogInfo( p_this );

    /* Open stream connection */
    p_sys->i_stream_fd = net_ConnectTCP( p_stream, p_sys->psz_host,
                                         p_sys->i_server_port );
    if ( p_sys->i_stream_fd < 0 )
    {
        msg_Err( p_this, "Cannot establish stream connection to %s:%d (%m)",
                 p_sys->psz_host, p_sys->i_server_port );
        i_err = VLC_EGENERIC;
        goto error;
    }

error:
    free( psz_pwfile );

    if ( i_err != VLC_SUCCESS )
        FreeSys( p_this, p_sys );

    return i_err;
}


/*****************************************************************************
 * Close:
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    sout_stream_t *p_stream = (sout_stream_t*)p_this;
    sout_stream_sys_t *p_sys = p_stream->p_sys;

    SendFlush( p_this );
    SendTeardown( p_this );

    FreeSys( p_this, p_sys );
}


/*****************************************************************************
 * Add:
 *****************************************************************************/
static sout_stream_id_t *Add( sout_stream_t *p_stream, es_format_t *p_fmt )
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;
    sout_stream_id_t *id = NULL;

    id = calloc( 1, sizeof( *id ) );
    if ( id == NULL )
        goto error;

    es_format_Copy( &id->fmt, p_fmt );

    switch ( id->fmt.i_cat )
    {
    case AUDIO_ES:
        if ( id->fmt.i_codec == VLC_CODEC_ALAC )
        {
            if ( p_sys->p_audio_stream )
            {
                msg_Warn( p_stream, "Only the first Apple Lossless audio "
                                    "stream is used" );
            }
            else if ( id->fmt.audio.i_rate != 44100 ||
                      id->fmt.audio.i_channels != 2 )
            {
                msg_Err( p_stream, "The Apple Lossless audio stream must be "
                                   "encoded with 44100 Hz and 2 channels" );
            }
            else
            {
                /* Use this stream */
                p_sys->p_audio_stream = id;
            }
        }
        else if ( !p_sys->b_alac_warning )
        {
            msg_Err( p_stream, "Apple Lossless is the only codec supported. "
                               "Use the \"transcode\" module for conversion "
                               "(e.g. \"transcode{acodec=alac,"
                               "channels=2}\")." );
            p_sys->b_alac_warning = true;
        }

        break;

    default:
        /* Leave other stream types alone */
        break;
    }

    return id;

error:
    FreeId( id );

    return NULL;
}


/*****************************************************************************
 * Del:
 *****************************************************************************/
static int Del( sout_stream_t *p_stream, sout_stream_id_t *id )
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;
    int i_err = VLC_SUCCESS;

    if ( p_sys->p_audio_stream == id )
        p_sys->p_audio_stream = NULL;

    FreeId( id );

    return i_err;
}


/*****************************************************************************
 * Send:
 *****************************************************************************/
static int Send( sout_stream_t *p_stream, sout_stream_id_t *id,
                 block_t *p_buffer )
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;

    if ( id->fmt.i_cat == AUDIO_ES && id == p_sys->p_audio_stream )
    {
        /* SendAudio takes care of releasing the buffers */
        SendAudio( p_stream, p_buffer );
    }
    else
    {
        block_ChainRelease( p_buffer );
    }

    return VLC_SUCCESS;
}


/*****************************************************************************
 * VolumeCallback: called when the volume is changed on the fly.
 *****************************************************************************/
static int VolumeCallback( vlc_object_t *p_this, char const *psz_cmd,
                           vlc_value_t oldval, vlc_value_t newval,
                           void *p_data )
{
    VLC_UNUSED(psz_cmd);
    VLC_UNUSED(oldval);
    VLC_UNUSED(p_data);
    VLC_UNUSED(newval);
    sout_stream_t *p_stream = (sout_stream_t*)p_this;
    sout_stream_sys_t *p_sys = p_stream->p_sys;

    /* TODO: Implement volume change */
    VLC_UNUSED(p_sys);

    return VLC_SUCCESS;
}
