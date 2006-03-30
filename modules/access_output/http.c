/*****************************************************************************
 * http.c
 *****************************************************************************
 * Copyright (C) 2001-2005 the VideoLAN team
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Jon Lech Johansen <jon@nanocrew.net>
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
#include <stdlib.h>

#include <vlc/vlc.h>
#include <vlc/sout.h>

#ifdef HAVE_AVAHI_CLIENT
    #include <vlc/intf.h>

    #include "bonjour.h"

    #if defined( WIN32 )
        #define DIRECTORY_SEPARATOR '\\'
    #else
        #define DIRECTORY_SEPARATOR '/'
    #endif
#endif

#include "vlc_httpd.h"

#define FREE( p ) if( p ) { free( p); (p) = NULL; }

#define DEFAULT_PORT        8080
#define DEFAULT_SSL_PORT    8443

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

#define SOUT_CFG_PREFIX "sout-http-"

#define USER_TEXT N_("Username")
#define USER_LONGTEXT N_("User name that will be " \
                         "requested to access the stream." )
#define PASS_TEXT N_("Password")
#define PASS_LONGTEXT N_("Password that will be " \
                         "requested to access the stream." )
#define MIME_TEXT N_("Mime")
#define MIME_LONGTEXT N_("MIME returned by the server (autodetected " \
                        "if not specified." )

#define CERT_TEXT N_( "Certificate file" )
#define CERT_LONGTEXT N_( "Path to the x509 PEM certificate file that will "\
                          "be used for HTTPS." )
#define KEY_TEXT N_( "Private key file" )
#define KEY_LONGTEXT N_( "Path to the x509 PEM private key file that will " \
                         " be used for HTTPS. Leave " \
                         "empty if you don't have one." )
#define CA_TEXT N_( "Root CA file" )
#define CA_LONGTEXT N_( "Path to the x509 PEM trusted root CA certificates " \
                        "(certificate authority) file that will be used for" \
                        "HTTPS. Leave empty if you " \
                        "don't have one." )
#define CRL_TEXT N_( "CRL file" )
#define CRL_LONGTEXT N_( "Path to the x509 PEM Certificates Revocation List " \
                         "file that will be used for SSL. Leave " \
                         "empty if you don't have one." )
#define BONJOUR_TEXT N_( "Advertise with Bonjour")
#define BONJOUR_LONGTEXT N_( "Advertise the stream with the Bonjour protocol." )


vlc_module_begin();
    set_description( _("HTTP stream output") );
    set_capability( "sout access", 0 );
    set_shortname( N_("HTTP" ) );
    add_shortcut( "http" );
    add_shortcut( "https" );
    add_shortcut( "mmsh" );
    set_category( CAT_SOUT );
    set_subcategory( SUBCAT_SOUT_ACO );
    add_string( SOUT_CFG_PREFIX "user", "", NULL,
                USER_TEXT, USER_LONGTEXT, VLC_TRUE );
    add_string( SOUT_CFG_PREFIX "pwd", "", NULL,
                PASS_TEXT, PASS_LONGTEXT, VLC_TRUE );
    add_string( SOUT_CFG_PREFIX "mime", "", NULL,
                MIME_TEXT, MIME_LONGTEXT, VLC_TRUE );
    add_string( SOUT_CFG_PREFIX "cert", "vlc.pem", NULL,
                CERT_TEXT, CERT_LONGTEXT, VLC_TRUE );
    add_string( SOUT_CFG_PREFIX "key", NULL, NULL,
                KEY_TEXT, KEY_LONGTEXT, VLC_TRUE );
    add_string( SOUT_CFG_PREFIX "ca", NULL, NULL,
                CA_TEXT, CA_LONGTEXT, VLC_TRUE );
    add_string( SOUT_CFG_PREFIX "crl", NULL, NULL,
                CRL_TEXT, CRL_LONGTEXT, VLC_TRUE );
    add_bool( SOUT_CFG_PREFIX "bonjour", VLC_FALSE, NULL,
              BONJOUR_TEXT, BONJOUR_LONGTEXT,VLC_TRUE);
    set_callbacks( Open, Close );
vlc_module_end();


/*****************************************************************************
 * Exported prototypes
 *****************************************************************************/
static const char *ppsz_sout_options[] = {
    "user", "pwd", "mime", "cert", "key", "ca", "crl", NULL
};

static int Write( sout_access_out_t *, block_t * );
static int Seek ( sout_access_out_t *, off_t  );

struct sout_access_out_sys_t
{
    /* host */
    httpd_host_t        *p_httpd_host;

    /* stream */
    httpd_stream_t      *p_httpd_stream;

    /* gather header from stream */
    int                 i_header_allocated;
    int                 i_header_size;
    uint8_t             *p_header;
    vlc_bool_t          b_header_complete;

#ifdef HAVE_AVAHI_CLIENT
    void                *p_bonjour;
#endif
};

/*****************************************************************************
 * Open: open the file
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    sout_access_out_t       *p_access = (sout_access_out_t*)p_this;
    sout_access_out_sys_t   *p_sys;

    char                *psz_parser;

    char                *psz_bind_addr;
    int                 i_bind_port;
    char                *psz_file_name;
    char                *psz_user = NULL;
    char                *psz_pwd = NULL;
    char                *psz_mime = NULL;
    const char          *psz_cert = NULL, *psz_key = NULL, *psz_ca = NULL,
                        *psz_crl = NULL;
    vlc_value_t         val;

    if( !( p_sys = p_access->p_sys =
                malloc( sizeof( sout_access_out_sys_t ) ) ) )
    {
        msg_Err( p_access, "Not enough memory" );
        return VLC_ENOMEM ;
    }

    sout_CfgParse( p_access, SOUT_CFG_PREFIX, ppsz_sout_options, p_access->p_cfg );

    /* p_access->psz_name = "hostname:port/filename" */
    psz_bind_addr = psz_parser = strdup( p_access->psz_name );

    i_bind_port = 0;
    psz_file_name = "";

    while( *psz_parser && *psz_parser != ':' && *psz_parser != '/' )
    {
        psz_parser++;
    }
    if( *psz_parser == ':' )
    {
        *psz_parser = '\0';
        psz_parser++;
        i_bind_port = atoi( psz_parser );

        while( *psz_parser && *psz_parser != '/' )
        {
            psz_parser++;
        }
    }
    if( *psz_parser == '/' )
    {
        *psz_parser = '\0';
        psz_parser++;
        psz_file_name = psz_parser;
    }

    if( !*psz_file_name )
    {
        psz_file_name = strdup( "/" );
    }
    else if( *psz_file_name != '/' )
    {
        char *p = psz_file_name;

        psz_file_name = malloc( strlen( p ) + 2 );
        strcpy( psz_file_name, "/" );
        strcat( psz_file_name, p );
    }
    else
    {
        psz_file_name = strdup( psz_file_name );
    }

    /* SSL support */
    if( p_access->psz_access && !strcmp( p_access->psz_access, "https" ) )
    {
        psz_cert = config_GetPsz( p_this, SOUT_CFG_PREFIX"cert" );
        psz_key = config_GetPsz( p_this, SOUT_CFG_PREFIX"key" );
        psz_ca = config_GetPsz( p_this, SOUT_CFG_PREFIX"ca" );
        psz_crl = config_GetPsz( p_this, SOUT_CFG_PREFIX"crl" );

        if( i_bind_port <= 0 )
            i_bind_port = DEFAULT_SSL_PORT;
    }
    else
    {
        if( i_bind_port <= 0 )
            i_bind_port = DEFAULT_PORT;
    }

    p_sys->p_httpd_host = httpd_TLSHostNew( VLC_OBJECT(p_access),
                                            psz_bind_addr, i_bind_port,
                                            psz_cert, psz_key, psz_ca,
                                            psz_crl );
    if( p_sys->p_httpd_host == NULL )
    {
        msg_Err( p_access, "cannot listen on %s:%d",
                 psz_bind_addr, i_bind_port );
        free( psz_file_name );
        free( psz_bind_addr );
        free( p_sys );
        return VLC_EGENERIC;
    }
    free( psz_bind_addr );

    if( p_access->psz_access && !strcmp( p_access->psz_access, "mmsh" ) )
    {
        psz_mime = strdup( "video/x-ms-asf-stream" );
    }
    else
    {
        var_Get( p_access, SOUT_CFG_PREFIX "mime", &val );
        if( *val.psz_string )
            psz_mime = val.psz_string;
        else
            free( val.psz_string );
    }

    var_Get( p_access, SOUT_CFG_PREFIX "user", &val );
    if( *val.psz_string )
        psz_user = val.psz_string;
    else
        free( val.psz_string );

    var_Get( p_access, SOUT_CFG_PREFIX "pwd", &val );
    if( *val.psz_string )
        psz_pwd = val.psz_string;
    else
        free( val.psz_string );

    p_sys->p_httpd_stream =
        httpd_StreamNew( p_sys->p_httpd_host, psz_file_name, psz_mime,
                         psz_user, psz_pwd, NULL );
    if( psz_user ) free( psz_user );
    if( psz_pwd ) free( psz_pwd );
    if( psz_mime ) free( psz_mime );

    if( p_sys->p_httpd_stream == NULL )
    {
        msg_Err( p_access, "cannot add stream %s", psz_file_name );
        httpd_HostDelete( p_sys->p_httpd_host );

        free( psz_file_name );
        free( p_sys );
        return VLC_EGENERIC;
    }

#ifdef HAVE_AVAHI_CLIENT
    if( config_GetInt(p_this, SOUT_CFG_PREFIX "bonjour") )
    {
        playlist_t          *p_playlist;
        char                *psz_txt, *psz_name;

        p_playlist = (playlist_t *)vlc_object_find( p_access,
                                                    VLC_OBJECT_PLAYLIST,
                                                    FIND_ANYWHERE );
        if( p_playlist == NULL )
        {
            msg_Err( p_access, "unable to find playlist" );
            httpd_StreamDelete( p_sys->p_httpd_stream );
            httpd_HostDelete( p_sys->p_httpd_host );
            free( (void *)p_sys );
            return VLC_EGENERIC;
        }

        psz_name = strrchr( p_playlist->status.p_item->input.psz_uri,
                            DIRECTORY_SEPARATOR );
        if( psz_name != NULL ) psz_name++;
        else psz_name = p_playlist->status.p_item->input.psz_uri;

        asprintf( &psz_txt, "path=%s", psz_file_name );

        p_sys->p_bonjour = bonjour_start_service( (vlc_object_t *)p_access,
                                    strcmp( p_access->psz_access, "https" )
                                       ? "_vlc-http._tcp" : "_vlc-https._tcp",
                                                  psz_name, i_bind_port, psz_txt );
        free( (void *)psz_txt );

        if( p_sys->p_bonjour == NULL )
        {
            vlc_object_release( p_playlist );
            httpd_StreamDelete( p_sys->p_httpd_stream );
            httpd_HostDelete( p_sys->p_httpd_host );
            free( (void *)p_sys );
            return VLC_EGENERIC;
        }
        vlc_object_release( p_playlist );
    }
    else
        p_sys->p_bonjour = NULL;
#endif

    free( psz_file_name );

    p_sys->i_header_allocated = 1024;
    p_sys->i_header_size      = 0;
    p_sys->p_header           = malloc( p_sys->i_header_allocated );
    p_sys->b_header_complete  = VLC_FALSE;

    p_access->pf_write       = Write;
    p_access->pf_seek        = Seek;


    /* update p_sout->i_out_pace_nocontrol */
    p_access->p_sout->i_out_pace_nocontrol++;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close: close the target
 *****************************************************************************/
static void Close( vlc_object_t * p_this )
{
    sout_access_out_t       *p_access = (sout_access_out_t*)p_this;
    sout_access_out_sys_t   *p_sys = p_access->p_sys;

#ifdef HAVE_AVAHI_CLIENT
    if( p_sys->p_bonjour != NULL )
        bonjour_stop_service( p_sys->p_bonjour );
#endif

    /* update p_sout->i_out_pace_nocontrol */
    p_access->p_sout->i_out_pace_nocontrol--;

    httpd_StreamDelete( p_sys->p_httpd_stream );
    httpd_HostDelete( p_sys->p_httpd_host );

    FREE( p_sys->p_header );

    msg_Dbg( p_access, "Close" );

    free( p_sys );
}

/*****************************************************************************
 * Write:
 *****************************************************************************/
static int Write( sout_access_out_t *p_access, block_t *p_buffer )
{
    sout_access_out_sys_t *p_sys = p_access->p_sys;
    int i_err = 0;

    while( p_buffer )
    {
        block_t *p_next;

        if( p_buffer->i_flags & BLOCK_FLAG_HEADER )
        {
            /* gather header */
            if( p_sys->b_header_complete )
            {
                /* free previously gathered header */
                p_sys->i_header_size = 0;
                p_sys->b_header_complete = VLC_FALSE;
            }
            if( (int)(p_buffer->i_buffer + p_sys->i_header_size) >
                p_sys->i_header_allocated )
            {
                p_sys->i_header_allocated =
                    p_buffer->i_buffer + p_sys->i_header_size + 1024;
                p_sys->p_header =
                    realloc( p_sys->p_header, p_sys->i_header_allocated );
            }
            memcpy( &p_sys->p_header[p_sys->i_header_size],
                    p_buffer->p_buffer,
                    p_buffer->i_buffer );
            p_sys->i_header_size += p_buffer->i_buffer;
        }
        else if( !p_sys->b_header_complete )
        {
            p_sys->b_header_complete = VLC_TRUE;

            httpd_StreamHeader( p_sys->p_httpd_stream, p_sys->p_header,
                                p_sys->i_header_size );
        }

        /* send data */
        i_err = httpd_StreamSend( p_sys->p_httpd_stream, p_buffer->p_buffer,
                                  p_buffer->i_buffer );

        p_next = p_buffer->p_next;
        block_Release( p_buffer );
        p_buffer = p_next;

        if( i_err < 0 )
        {
            break;
        }
    }

    if( i_err < 0 )
    {
        block_ChainRelease( p_buffer );
    }

    return( i_err < 0 ? VLC_EGENERIC : VLC_SUCCESS );
}

/*****************************************************************************
 * Seek: seek to a specific location in a file
 *****************************************************************************/
static int Seek( sout_access_out_t *p_access, off_t i_pos )
{
    msg_Warn( p_access, "HTTP sout access cannot seek" );
    return VLC_EGENERIC;
}
