/*****************************************************************************
 * http_auth.c: HTTP authentication for clients as per RFC2617
 *****************************************************************************
 * Copyright (C) 2001-2008 VLC authors and VideoLAN
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Christophe Massiot <massiot@via.ecp.fr>
 *          Rémi Denis-Courmont
 *          Antoine Cellerier <dionoea at videolan dot org>
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_http.h>
#include <vlc_hash.h>
#include <vlc_rand.h>
#include <vlc_strings.h>

#include "libvlc.h"


/*****************************************************************************
 * "RFC 2617: Basic and Digest Access Authentication" header parsing
 *****************************************************************************/
static char *AuthGetParam( const char *psz_header, const char *psz_param )
{
    char psz_what[strlen(psz_param)+3];
    sprintf( psz_what, "%s=\"", psz_param );
    psz_header = strstr( psz_header, psz_what );
    if ( psz_header )
    {
        const char *psz_end;
        psz_header += strlen( psz_what );
        psz_end = strchr( psz_header, '"' );
        if ( !psz_end ) /* Invalid since we should have a closing quote */
            return strdup( psz_header );
        return strndup( psz_header, psz_end - psz_header );
    }
    else
    {
        return NULL;
    }
}

static char *AuthGetParamNoQuotes( const char *psz_header, const char *psz_param )
{
    char psz_what[strlen(psz_param)+2];
    sprintf( psz_what, "%s=", psz_param );
    psz_header = strstr( psz_header, psz_what );
    if ( psz_header )
    {
        const char *psz_end;
        psz_header += strlen( psz_what );
        psz_end = strchr( psz_header, ',' );
        /* XXX: Do we need to filter out trailing space between the value and
         * the comma/end of line? */
        if ( !psz_end ) /* Can be valid if this is the last parameter */
            return strdup( psz_header );
        return strndup( psz_header, psz_end - psz_header );
    }
    else
    {
        return NULL;
    }
}

static char *GenerateCnonce()
{
    char ps_random[32];
    char *md5_hex;
    vlc_hash_md5_t md5;

    md5_hex = malloc( VLC_HASH_MD5_DIGEST_HEX_SIZE );
    if (unlikely( md5_hex == NULL ))
        return NULL;

    vlc_rand_bytes( ps_random, sizeof( ps_random ) );

    vlc_hash_md5_Init( &md5 );
    vlc_hash_md5_Update( &md5, ps_random, sizeof( ps_random ) );

    vlc_hash_FinishHex( &md5, md5_hex );
    return md5_hex;
}

static char *AuthDigest( vlc_object_t *p_this, vlc_http_auth_t *p_auth,
                         const char *psz_method, const char *psz_path,
                         const char *psz_username, const char *psz_password )
{
    char psz_HA1[VLC_HASH_MD5_DIGEST_HEX_SIZE];
    char psz_HA2[VLC_HASH_MD5_DIGEST_HEX_SIZE];
    char psz_ent[VLC_HASH_MD5_DIGEST_HEX_SIZE];
    char *psz_result = NULL;
    char psz_inonce[9];
    vlc_hash_md5_t md5;
    vlc_hash_md5_t ent;

    if ( p_auth->psz_realm == NULL )
    {
        msg_Warn( p_this, "Digest Authentication: "
                  "Mandatory 'realm' value not available" );
        return NULL;
    }

    /* H(A1) */
    if ( p_auth->psz_HA1 )
    {
        memcpy( psz_HA1, p_auth->psz_HA1, sizeof(psz_HA1) );
    }
    else
    {
        vlc_hash_md5_Init( &md5 );
        vlc_hash_md5_Update( &md5, psz_username, strlen( psz_username ) );
        vlc_hash_md5_Update( &md5, ":", 1 );
        vlc_hash_md5_Update( &md5, p_auth->psz_realm, strlen( p_auth->psz_realm ) );
        vlc_hash_md5_Update( &md5, ":", 1 );
        vlc_hash_md5_Update( &md5, psz_password, strlen( psz_password ) );
        vlc_hash_FinishHex( &md5, psz_HA1 );

        if ( p_auth->psz_algorithm &&
             strcmp( p_auth->psz_algorithm, "MD5-sess" ) == 0 )
        {
            vlc_hash_md5_Init( &md5 );
            vlc_hash_md5_Update( &md5, psz_HA1, sizeof(psz_HA1) - 1 );
            vlc_hash_md5_Update( &md5, ":", 1 );
            vlc_hash_md5_Update( &md5, p_auth->psz_nonce, strlen( p_auth->psz_nonce ) );
            vlc_hash_md5_Update( &md5, ":", 1 );
            vlc_hash_md5_Update( &md5, p_auth->psz_cnonce, strlen( p_auth->psz_cnonce ) );
            vlc_hash_FinishHex( &md5, psz_HA1 );

            p_auth->psz_HA1 = strdup( psz_HA1 );
            if ( p_auth->psz_HA1 == NULL )
                return NULL;
        }
    }

    /* H(A2) */
    vlc_hash_md5_Init( &md5 );
    if ( *psz_method )
        vlc_hash_md5_Update( &md5, psz_method, strlen( psz_method ) );
    vlc_hash_md5_Update( &md5, ":", 1 );
    if ( psz_path )
        vlc_hash_md5_Update( &md5, psz_path, strlen( psz_path ) );
    else
        vlc_hash_md5_Update( &md5, "/", 1 );
    if ( p_auth->psz_qop && strcmp( p_auth->psz_qop, "auth-int" ) == 0 )
    {
        vlc_hash_md5_Init( &ent );
        /* TODO: Support for "qop=auth-int" */
        vlc_hash_md5_Update( &ent, "", 0 );
        vlc_hash_FinishHex( &ent, psz_ent );
        vlc_hash_md5_Update( &md5, ":", 1 );
        vlc_hash_md5_Update( &md5, psz_ent, sizeof(psz_ent) - 1 );
    }

    vlc_hash_FinishHex( &md5, psz_HA2 );

    /* Request digest */
    vlc_hash_md5_Init( &md5 );
    vlc_hash_md5_Update( &md5, psz_HA1, sizeof(psz_HA1) - 1 );
    vlc_hash_md5_Update( &md5, ":", 1 );
    vlc_hash_md5_Update( &md5, p_auth->psz_nonce, strlen( p_auth->psz_nonce ) );
    vlc_hash_md5_Update( &md5, ":", 1 );
    if ( p_auth->psz_qop &&
         ( strcmp( p_auth->psz_qop, "auth" ) == 0 ||
           strcmp( p_auth->psz_qop, "auth-int" ) == 0 ) )
    {
        snprintf( psz_inonce, sizeof( psz_inonce ), "%08x", p_auth->i_nonce );
        vlc_hash_md5_Update( &md5, psz_inonce, 8 );
        vlc_hash_md5_Update( &md5, ":", 1 );
        vlc_hash_md5_Update( &md5, p_auth->psz_cnonce, strlen( p_auth->psz_cnonce ) );
        vlc_hash_md5_Update( &md5, ":", 1 );
        vlc_hash_md5_Update( &md5, p_auth->psz_qop, strlen( p_auth->psz_qop ) );
        vlc_hash_md5_Update( &md5, ":", 1 );
    }
    vlc_hash_md5_Update( &md5, psz_HA2, sizeof(psz_HA2) - 1 );

    psz_result = malloc(VLC_HASH_MD5_DIGEST_HEX_SIZE);
    if (psz_result == NULL)
        return NULL;

    vlc_hash_FinishHex( &md5, psz_result );
    return psz_result;
}

/* RFC2617, section 3.2.1 The WWW-Authenticate Response Header
 *
 * If a server receives a request for an access-protected object, and an
 * acceptable Authorization header is not sent, the server responds with a "401
 * Unauthorized" status code, and a WWW-Authenticate header [...]
 */
void vlc_http_auth_ParseWwwAuthenticateHeader(
        vlc_object_t *p_this, vlc_http_auth_t *p_auth,
        const char *psz_header )
{
    static const char psz_basic_prefix[] = "Basic ";
    static const char psz_digest_prefix[] = "Digest ";

    /* FIXME: multiple auth methods can be listed (comma separated) */

    if ( strncasecmp( psz_header, psz_basic_prefix,
                      sizeof( psz_basic_prefix ) - 1 ) == 0 )
    {
        /* 2 Basic Authentication Scheme */
        msg_Dbg( p_this, "Using Basic Authentication" );
        psz_header += sizeof( psz_basic_prefix ) - 1;
        p_auth->psz_realm = AuthGetParam( psz_header, "realm" );
        if ( p_auth->psz_realm == NULL )
            msg_Warn( p_this, "Basic Authentication: "
                      "Mandatory 'realm' parameter is missing" );
    }
    else if ( strncasecmp( psz_header, psz_digest_prefix,
                           sizeof( psz_digest_prefix ) - 1 ) == 0 )
    {
        /* 3 Digest Access Authentication Scheme */
        msg_Dbg( p_this, "Using Digest Access Authentication" );

        if ( p_auth->psz_nonce )
            /* FIXME */
            return;

        psz_header += sizeof( psz_digest_prefix ) - 1;
        p_auth->psz_realm = AuthGetParam( psz_header, "realm" );
        p_auth->psz_domain = AuthGetParam( psz_header, "domain" );
        p_auth->psz_nonce = AuthGetParam( psz_header, "nonce" );
        p_auth->psz_opaque = AuthGetParam( psz_header, "opaque" );
        p_auth->psz_stale = AuthGetParamNoQuotes( psz_header, "stale" );
        p_auth->psz_algorithm = AuthGetParamNoQuotes( psz_header, "algorithm" );
        p_auth->psz_qop = AuthGetParam( psz_header, "qop" );
        p_auth->i_nonce = 0;

        /* printf("realm: |%s|\ndomain: |%s|\nnonce: |%s|\nopaque: |%s|\n"
                  "stale: |%s|\nalgorithm: |%s|\nqop: |%s|\n",
                  p_auth->psz_realm,p_auth->psz_domain,p_auth->psz_nonce,
                  p_auth->psz_opaque,p_auth->psz_stale,p_auth->psz_algorithm,
                  p_auth->psz_qop); */

        if ( p_auth->psz_realm == NULL )
            msg_Warn( p_this, "Digest Access Authentication: "
                      "Mandatory 'realm' parameter is missing" );
        if ( p_auth->psz_nonce == NULL )
            msg_Warn( p_this, "Digest Access Authentication: "
                      "Mandatory 'nonce' parameter is missing" );

        /* FIXME: parse the qop list */
        if ( p_auth->psz_qop )
        {
            char *psz_tmp = strchr( p_auth->psz_qop, ',' );
            if ( psz_tmp )
                *psz_tmp = '\0';
        }
    }
    else
    {
        const char *psz_end = strchr( psz_header, ' ' );
        if ( psz_end )
            msg_Warn( p_this, "Unknown authentication scheme: '%*s'",
                      (int)(psz_end - psz_header), psz_header );
        else
            msg_Warn( p_this, "Unknown authentication scheme: '%s'",
                      psz_header );
    }
}

/* RFC2617, section 3.2.3: The Authentication-Info Header
 *
 * The Authentication-Info header is used by the server to communicate some
 * information regarding the successful authentication in the response.
 */
int vlc_http_auth_ParseAuthenticationInfoHeader(
        vlc_object_t *p_this, vlc_http_auth_t *p_auth,
        const char *psz_header, const char *psz_method, const char *psz_path,
        const char *psz_username, const char *psz_password )
{
    char *psz_nextnonce = AuthGetParam( psz_header, "nextnonce" );
    char *psz_qop = AuthGetParamNoQuotes( psz_header, "qop" );
    char *psz_rspauth = AuthGetParam( psz_header, "rspauth" );
    char *psz_cnonce = AuthGetParam( psz_header, "cnonce" );
    char *psz_nc = AuthGetParamNoQuotes( psz_header, "nc" );
    char *psz_digest = NULL;
    int i_err = VLC_SUCCESS;
    int i_nonce;

    if ( psz_cnonce )
    {
        if ( strcmp( psz_cnonce, p_auth->psz_cnonce ) != 0 )
        {
            msg_Err( p_this, "HTTP Digest Access Authentication: server "
                             "replied with a different client nonce value." );
            i_err = VLC_EGENERIC;
            goto error;
        }

        if ( psz_nc )
        {
            i_nonce = strtol( psz_nc, NULL, 16 );

            if ( i_nonce != p_auth->i_nonce )
            {
                msg_Err( p_this, "HTTP Digest Access Authentication: server "
                                 "replied with a different nonce count "
                                 "value." );
                i_err = VLC_EGENERIC;
                goto error;
            }
        }

        if ( psz_qop && p_auth->psz_qop &&
             strcmp( psz_qop, p_auth->psz_qop ) != 0 )
            msg_Warn( p_this, "HTTP Digest Access Authentication: server "
                              "replied using a different 'quality of "
                              "protection' option" );

        /* All the clear text values match, let's now check the response
         * digest.
         *
         * TODO: Support for "qop=auth-int"
         */
        psz_digest = AuthDigest( p_this, p_auth, psz_method, psz_path,
                                 psz_username, psz_password );
        if( psz_digest == NULL || strcmp( psz_digest, psz_rspauth ) != 0 )
        {
            msg_Err( p_this, "HTTP Digest Access Authentication: server "
                             "replied with an invalid response digest "
                             "(expected value: %s).", psz_digest );
            i_err = VLC_EGENERIC;
            goto error;
        }
    }

    if ( psz_nextnonce )
    {
        free( p_auth->psz_nonce );
        p_auth->psz_nonce = psz_nextnonce;
        psz_nextnonce = NULL;
    }

error:
    free( psz_nextnonce );
    free( psz_qop );
    free( psz_rspauth );
    free( psz_cnonce );
    free( psz_nc );
    free( psz_digest );

    return i_err;
}

char *vlc_http_auth_FormatAuthorizationHeader(
        vlc_object_t *p_this, vlc_http_auth_t *p_auth,
        const char *psz_method, const char *psz_path,
        const char *psz_username, const char *psz_password )
{
    char *psz_result = NULL;
    char *psz_buffer = NULL;
    char *psz_base64 = NULL;
    int i_rc;

    if ( p_auth->psz_nonce )
    {
        /* Digest Access Authentication */
        if ( p_auth->psz_algorithm &&
             strcmp( p_auth->psz_algorithm, "MD5" ) != 0 &&
             strcmp( p_auth->psz_algorithm, "MD5-sess" ) != 0 )
        {
            msg_Err( p_this, "Digest Access Authentication: "
                     "Unknown algorithm '%s'", p_auth->psz_algorithm );
            goto error;
        }

        if ( p_auth->psz_qop != NULL || p_auth->psz_cnonce == NULL )
        {
            free( p_auth->psz_cnonce );

            p_auth->psz_cnonce = GenerateCnonce();
            if ( p_auth->psz_cnonce == NULL )
                goto error;
        }

        ++p_auth->i_nonce;

        psz_buffer = AuthDigest( p_this, p_auth, psz_method, psz_path,
                                 psz_username, psz_password );
        if ( psz_buffer == NULL )
            goto error;

        i_rc = asprintf( &psz_result,
            "Digest "
            /* Mandatory parameters */
            "username=\"%s\", "
            "realm=\"%s\", "
            "nonce=\"%s\", "
            "uri=\"%s\", "
            "response=\"%s\", "
            /* Optional parameters */
            "%s%s%s" /* algorithm */
            "%s%s%s" /* cnonce */
            "%s%s%s" /* opaque */
            "%s%s%s" /* message qop */
            "%s=\"%08x\"", /* nonce count */
            /* Mandatory parameters */
            psz_username,
            p_auth->psz_realm,
            p_auth->psz_nonce,
            psz_path ? psz_path : "/",
            psz_buffer,
            /* Optional parameters */
            p_auth->psz_algorithm ? "algorithm=\"" : "",
            p_auth->psz_algorithm ? p_auth->psz_algorithm : "",
            p_auth->psz_algorithm ? "\", " : "",
            p_auth->psz_cnonce ? "cnonce=\"" : "",
            p_auth->psz_cnonce ? p_auth->psz_cnonce : "",
            p_auth->psz_cnonce ? "\", " : "",
            p_auth->psz_opaque ? "opaque=\"" : "",
            p_auth->psz_opaque ? p_auth->psz_opaque : "",
            p_auth->psz_opaque ? "\", " : "",
            p_auth->psz_qop ? "qop=\"" : "",
            p_auth->psz_qop ? p_auth->psz_qop : "",
            p_auth->psz_qop ? "\", " : "",
            /* "uglyhack" will be parsed as an unhandled extension */
            p_auth->i_nonce ? "nc" : "uglyhack",
            p_auth->i_nonce
        );
        if ( i_rc < 0 )
            goto error;
    }
    else
    {
        /* Basic Access Authentication */
        i_rc = asprintf( &psz_buffer, "%s:%s", psz_username, psz_password );
        if ( i_rc < 0 )
            goto error;

        psz_base64 = vlc_b64_encode( psz_buffer );
        if ( psz_base64 == NULL )
            goto error;

        i_rc = asprintf( &psz_result, "Basic %s", psz_base64 );
        if ( i_rc < 0 )
            goto error;
    }

error:
    free( psz_buffer );
    free( psz_base64 );

    return psz_result;
}

void vlc_http_auth_Init( vlc_http_auth_t *p_auth )
{
    memset( p_auth, 0, sizeof( *p_auth ) );
}

void vlc_http_auth_Deinit( vlc_http_auth_t *p_auth )
{
    free( p_auth->psz_realm );
    free( p_auth->psz_domain );
    free( p_auth->psz_nonce );
    free( p_auth->psz_opaque );
    free( p_auth->psz_stale );
    free( p_auth->psz_algorithm );
    free( p_auth->psz_qop );
    free( p_auth->psz_cnonce );
    free( p_auth->psz_HA1 );
}
