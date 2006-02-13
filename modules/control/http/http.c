/*****************************************************************************
 * http.c : HTTP/HTTPS Remote control interface
 *****************************************************************************
 * Copyright (C) 2001-2005 the VideoLAN team
 * $Id$
 *
 * Authors: Gildas Bazin <gbazin@netcourrier.com>
 *          Laurent Aimar <fenrir@via.ecp.fr>
 *          Christophe Massiot <massiot@via.ecp.fr>
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

#include "http.h"

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

#define HOST_TEXT N_( "Host address" )
#define HOST_LONGTEXT N_( \
    "You can set the address and port the http interface will bind to." )
#define SRC_TEXT N_( "Source directory" )
#define SRC_LONGTEXT N_( "Source directory" )
#define CHARSET_TEXT N_( "Charset" )
#define CHARSET_LONGTEXT N_( \
        "Charset declared in Content-Type header (default UTF-8)." )
#define HANDLERS_TEXT N_( "Handlers" )
#define HANDLERS_LONGTEXT N_( \
        "List of extensions and executable paths (for instance: " \
        "php=/usr/bin/php,pl=/usr/bin/perl)." )
#define CERT_TEXT N_( "Certificate file" )
#define CERT_LONGTEXT N_( "HTTP interface x509 PEM certificate file " \
                          "(enables SSL)" )
#define KEY_TEXT N_( "Private key file" )
#define KEY_LONGTEXT N_( "HTTP interface x509 PEM private key file" )
#define CA_TEXT N_( "Root CA file" )
#define CA_LONGTEXT N_( "HTTP interface x509 PEM trusted root CA " \
                        "certificates file" )
#define CRL_TEXT N_( "CRL file" )
#define CRL_LONGTEXT N_( "HTTP interace Certificates Revocation List file" )

vlc_module_begin();
    set_shortname( _("HTTP"));
    set_description( _("HTTP remote control interface") );
    set_category( CAT_INTERFACE );
    set_subcategory( SUBCAT_INTERFACE_GENERAL );
        add_string ( "http-host", NULL, NULL, HOST_TEXT, HOST_LONGTEXT, VLC_TRUE );
        add_string ( "http-src",  NULL, NULL, SRC_TEXT,  SRC_LONGTEXT,  VLC_TRUE );
        add_string ( "http-charset", "UTF-8", NULL, CHARSET_TEXT, CHARSET_LONGTEXT, VLC_TRUE );
#if defined( HAVE_FORK ) || defined( WIN32 )
        add_string ( "http-handlers", NULL, NULL, HANDLERS_TEXT, HANDLERS_LONGTEXT, VLC_TRUE );
#endif
        set_section( N_("HTTP SSL" ), 0 );
        add_string ( "http-intf-cert", NULL, NULL, CERT_TEXT, CERT_LONGTEXT, VLC_TRUE );
        add_string ( "http-intf-key",  NULL, NULL, KEY_TEXT,  KEY_LONGTEXT,  VLC_TRUE );
        add_string ( "http-intf-ca",   NULL, NULL, CA_TEXT,   CA_LONGTEXT,   VLC_TRUE );
        add_string ( "http-intf-crl",  NULL, NULL, CRL_TEXT,  CRL_LONGTEXT,  VLC_TRUE );
    set_capability( "interface", 0 );
    set_callbacks( Open, Close );
vlc_module_end();


/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static void Run          ( intf_thread_t *p_intf );

/*****************************************************************************
 * Local functions
 *****************************************************************************/
#if !defined(__APPLE__) && !defined(SYS_BEOS) && !defined(WIN32)
static int DirectoryCheck( char *psz_dir )
{
    DIR           *p_dir;

#ifdef HAVE_SYS_STAT_H
    struct stat   stat_info;

    if( utf8_stat( psz_dir, &stat_info ) == -1 || !S_ISDIR( stat_info.st_mode ) )
    {
        return VLC_EGENERIC;
    }
#endif

    if( ( p_dir = utf8_opendir( psz_dir ) ) == NULL )
    {
        return VLC_EGENERIC;
    }
    closedir( p_dir );

    return VLC_SUCCESS;
}
#endif


/*****************************************************************************
 * Activate: initialize and create stuff
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t*)p_this;
    intf_sys_t    *p_sys;
    char          *psz_address;
    const char    *psz_cert = NULL, *psz_key = NULL, *psz_ca = NULL,
                  *psz_crl = NULL;
    int           i_port       = 0;
    char          *psz_src;
    char          psz_tmp[10];

    psz_address = config_GetPsz( p_intf, "http-host" );
    if( psz_address != NULL )
    {
        char *psz_parser = strchr( psz_address, ':' );
        if( psz_parser )
        {
            *psz_parser++ = '\0';
            i_port = atoi( psz_parser );
        }
    }
    else
        psz_address = strdup("");

    p_intf->p_sys = p_sys = malloc( sizeof( intf_sys_t ) );
    if( !p_intf->p_sys )
    {
        return( VLC_ENOMEM );
    }
    p_sys->p_playlist = NULL;
    p_sys->p_input    = NULL;
    p_sys->p_vlm      = NULL;
    p_sys->psz_address = psz_address;
    p_sys->i_port     = i_port;

    /* determine Content-Type value for HTML pages */
    psz_src = config_GetPsz( p_intf, "http-charset" );
    if( psz_src == NULL || !*psz_src )
    {
        if( psz_src != NULL ) free( psz_src );
        psz_src = strdup("UTF-8");
    }

    p_sys->psz_html_type = malloc( 20 + strlen( psz_src ) );
    if( p_sys->psz_html_type == NULL )
    {
        free( p_sys->psz_address );
        free( p_sys );
        free( psz_src );
        return VLC_ENOMEM ;
    }
    sprintf( p_sys->psz_html_type, "text/html; charset=%s", psz_src );
    msg_Dbg( p_intf, "using charset=%s", psz_src );

    if( strcmp( psz_src, "UTF-8" ) )
    {
        p_sys->iconv_from_utf8 = vlc_iconv_open( psz_src, "UTF-8" );
        if( p_sys->iconv_from_utf8 == (vlc_iconv_t)-1 )
            msg_Warn( p_intf, "unable to perform charset conversion to %s",
                      psz_src );
        else
        {
            p_sys->iconv_to_utf8 = vlc_iconv_open( "UTF-8", psz_src );
            if( p_sys->iconv_to_utf8 == (vlc_iconv_t)-1 )
                msg_Warn( p_intf,
                          "unable to perform charset conversion from %s",
                          psz_src );
        }
    }
    else
    {
        p_sys->iconv_from_utf8 = p_sys->iconv_to_utf8 = (vlc_iconv_t)-1;
    }

    free( psz_src );

    /* determine file handler associations */
    p_sys->i_handlers = 0;
    p_sys->pp_handlers = NULL;
#if defined( HAVE_FORK ) || defined( WIN32 )
    psz_src = config_GetPsz( p_intf, "http-handlers" );
    if( psz_src != NULL && *psz_src )
    {
        char *p = psz_src;
        while( p != NULL )
        {
            http_association_t *p_handler;
            char *psz_ext = p;
            char *psz_program, *psz_options;
            p = strchr( p, '=' );
            if( p == NULL ) break;
            *p++ = '\0';
            psz_program = p;
            p = strchr( p, ',' );
            if( p != NULL )
                *p++ = '\0';

            p_handler = malloc( sizeof( http_association_t ) );
            p_handler->psz_ext = strdup( psz_ext );
            psz_options = E_(FirstWord)( psz_program, psz_program );
            p_handler->i_argc = 0;
            p_handler->ppsz_argv = NULL;
            TAB_APPEND( p_handler->i_argc, p_handler->ppsz_argv,
                        strdup( psz_program ) );
            while( psz_options != NULL && *psz_options )
            {
                char *psz_next = E_(FirstWord)( psz_options, psz_options );
                TAB_APPEND( p_handler->i_argc, p_handler->ppsz_argv,
                            strdup( psz_options ) );
                psz_options = psz_next;
            }
            /* NULL will be appended later on */

            TAB_APPEND( p_sys->i_handlers, p_sys->pp_handlers, p_handler );
        }
    }
    if( psz_src != NULL )
        free( psz_src );
#endif

    /* determine SSL configuration */
    psz_cert = config_GetPsz( p_intf, "http-intf-cert" );
    if ( psz_cert != NULL )
    {
        msg_Dbg( p_intf, "enabling TLS for HTTP interface (cert file: %s)",
                 psz_cert );
        psz_key = config_GetPsz( p_intf, "http-intf-key" );
        psz_ca = config_GetPsz( p_intf, "http-intf-ca" );
        psz_crl = config_GetPsz( p_intf, "http-intf-crl" );

        if( i_port <= 0 )
            i_port = 8443;
    }
    else
    {
        if( i_port <= 0 )
            i_port= 8080;
    }

    /* Ugly hack to allow to run several HTTP servers on different ports. */
    sprintf( psz_tmp, ":%d", i_port + 1 );
    config_PutPsz( p_intf, "http-host", psz_tmp );

    msg_Dbg( p_intf, "base %s:%d", psz_address, i_port );

    p_sys->p_httpd_host = httpd_TLSHostNew( VLC_OBJECT(p_intf), psz_address,
                                            i_port, psz_cert, psz_key, psz_ca,
                                            psz_crl );
    if( p_sys->p_httpd_host == NULL )
    {
        msg_Err( p_intf, "cannot listen on %s:%d", psz_address, i_port );
        free( p_sys->psz_html_type );
        free( p_sys->psz_address );
        free( p_sys );
        return VLC_EGENERIC;
    }

    p_sys->i_files  = 0;
    p_sys->pp_files = NULL;

#if defined(__APPLE__) || defined(SYS_BEOS) || defined(WIN32)
    if ( ( psz_src = config_GetPsz( p_intf, "http-src" )) == NULL )
    {
        char * psz_vlcpath = p_intf->p_libvlc->psz_vlcpath;
        psz_src = malloc( strlen(psz_vlcpath) + strlen("/share/http" ) + 1 );
        if( !psz_src ) return VLC_ENOMEM;
#if defined(WIN32)
        sprintf( psz_src, "%s/http", psz_vlcpath );
#else
        sprintf( psz_src, "%s/share/http", psz_vlcpath );
#endif
    }
#else
    psz_src = config_GetPsz( p_intf, "http-src" );

    if( !psz_src || *psz_src == '\0' )
    {
        if( !DirectoryCheck( "share/http" ) )
        {
            psz_src = strdup( "share/http" );
        }
        else if( !DirectoryCheck( DATA_PATH "/http" ) )
        {
            psz_src = strdup( DATA_PATH "/http" );
        }
    }
#endif

    if( !psz_src || *psz_src == '\0' )
    {
        msg_Err( p_intf, "invalid src dir" );
        goto failed;
    }

    /* remove trainling \ or / */
    if( psz_src[strlen( psz_src ) - 1] == '\\' ||
        psz_src[strlen( psz_src ) - 1] == '/' )
    {
        psz_src[strlen( psz_src ) - 1] = '\0';
    }

    E_(ParseDirectory)( p_intf, psz_src, psz_src );


    if( p_sys->i_files <= 0 )
    {
        msg_Err( p_intf, "cannot find any files (%s)", psz_src );
        goto failed;
    }
    p_intf->pf_run = Run;
    free( psz_src );

    return VLC_SUCCESS;

failed:
    if( psz_src ) free( psz_src );
    if( p_sys->pp_files )
    {
        free( p_sys->pp_files );
    }
    httpd_HostDelete( p_sys->p_httpd_host );
    free( p_sys->psz_address );
    free( p_sys->psz_html_type ); 
    if( p_sys->iconv_from_utf8 != (vlc_iconv_t)-1 )
        vlc_iconv_close( p_sys->iconv_from_utf8 );
    if( p_sys->iconv_to_utf8 != (vlc_iconv_t)-1 )
        vlc_iconv_close( p_sys->iconv_to_utf8 );
    free( p_sys );
    return VLC_EGENERIC;
}

/*****************************************************************************
 * Close: destroy interface
 *****************************************************************************/
static void Close ( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_this;
    intf_sys_t    *p_sys = p_intf->p_sys;

    int i;

    if( p_sys->p_vlm )
    {
        vlm_Delete( p_sys->p_vlm );
    }
    for( i = 0; i < p_sys->i_files; i++ )
    {
        if( p_sys->pp_files[i]->b_handler )
            httpd_HandlerDelete( ((httpd_handler_sys_t *)p_sys->pp_files[i])->p_handler );
        else
            httpd_FileDelete( p_sys->pp_files[i]->p_file );
        if( p_sys->pp_files[i]->p_redir )
            httpd_RedirectDelete( p_sys->pp_files[i]->p_redir );
        if( p_sys->pp_files[i]->p_redir2 )
            httpd_RedirectDelete( p_sys->pp_files[i]->p_redir2 );

        free( p_sys->pp_files[i]->file );
        free( p_sys->pp_files[i]->name );
        free( p_sys->pp_files[i] );
    }
    if( p_sys->pp_files )
    {
        free( p_sys->pp_files );
    }
    for( i = 0; i < p_sys->i_handlers; i++ )
    {
        http_association_t *p_handler = p_sys->pp_handlers[i];
        int j;
        free( p_handler->psz_ext );
        for( j = 0; j < p_handler->i_argc; j++ )
            free( p_handler->ppsz_argv[j] );
        if( p_handler->i_argc )
            free( p_handler->ppsz_argv );
        free( p_handler );
    }
    if( p_sys->i_handlers )
        free( p_sys->pp_handlers );
    httpd_HostDelete( p_sys->p_httpd_host );
    free( p_sys->psz_address );
    free( p_sys->psz_html_type );

    if( p_sys->iconv_from_utf8 != (vlc_iconv_t)-1 )
        vlc_iconv_close( p_sys->iconv_from_utf8 );
    if( p_sys->iconv_to_utf8 != (vlc_iconv_t)-1 )
        vlc_iconv_close( p_sys->iconv_to_utf8 );
    free( p_sys );
}

/*****************************************************************************
 * Run: http interface thread
 *****************************************************************************/
static void Run( intf_thread_t *p_intf )
{
    intf_sys_t     *p_sys = p_intf->p_sys;

    while( !p_intf->b_die )
    {
        /* get the playlist */
        if( p_sys->p_playlist == NULL )
        {
            p_sys->p_playlist = vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST, FIND_ANYWHERE );
        }

        /* Manage the input part */
        if( p_sys->p_input == NULL )
        {
            if( p_sys->p_playlist )
            {
                p_sys->p_input =
                    vlc_object_find( p_sys->p_playlist,
                                     VLC_OBJECT_INPUT,
                                     FIND_CHILD );
            }
        }
        else if( p_sys->p_input->b_dead )
        {
            vlc_object_release( p_sys->p_input );
            p_sys->p_input = NULL;
        }


        /* Wait a bit */
        msleep( INTF_IDLE_SLEEP );
    }

    if( p_sys->p_input )
    {
        vlc_object_release( p_sys->p_input );
        p_sys->p_input = NULL;
    }

    if( p_sys->p_playlist )
    {
        vlc_object_release( p_sys->p_playlist );
        p_sys->p_playlist = NULL;
    }
}

/****************************************************************************
 * HttpCallback:
 ****************************************************************************
 * a file with b_html is parsed and all "macro" replaced
 ****************************************************************************/
static void Callback404( httpd_file_sys_t *p_args, char **pp_data,
                         int *pi_data )
{
    char *p = *pp_data = malloc( 10240 );
    if( !p )
    {
        return;
    }
    p += sprintf( p, "<html>\n" );
    p += sprintf( p, "<head>\n" );
    p += sprintf( p, "<title>Error loading %s</title>\n", p_args->file );
    p += sprintf( p, "</head>\n" );
    p += sprintf( p, "<body>\n" );
    p += sprintf( p, "<h1><center>Error loading %s for %s</center></h1>\n", p_args->file, p_args->name );
    p += sprintf( p, "<hr />\n" );
    p += sprintf( p, "<a href=\"http://www.videolan.org/\">VideoLAN</a>\n" );
    p += sprintf( p, "</body>\n" );
    p += sprintf( p, "</html>\n" );

    *pi_data = strlen( *pp_data );
}

static void ParseExecute( httpd_file_sys_t *p_args, char *p_buffer,
                          int i_buffer, char *p_request,
                          char **pp_data, int *pi_data )
{
    int i_request = p_request != NULL ? strlen( p_request ) : 0;
    char *dst;
    vlc_value_t val;
    char position[4]; /* percentage */
    char time[12]; /* in seconds */
    char length[12]; /* in seconds */
    audio_volume_t i_volume;
    char volume[5];
    char state[8];

#define p_sys p_args->p_intf->p_sys
    if( p_sys->p_input )
    {
        var_Get( p_sys->p_input, "position", &val);
        sprintf( position, "%d" , (int)((val.f_float) * 100.0));
        var_Get( p_sys->p_input, "time", &val);
        sprintf( time, "%d" , (int)(val.i_time / 1000000) );
        var_Get( p_sys->p_input, "length", &val);
        sprintf( length, "%d" , (int)(val.i_time / 1000000) );

        var_Get( p_sys->p_input, "state", &val );
        if( val.i_int == PLAYING_S )
        {
            sprintf( state, "playing" );
        }
        else if( val.i_int == PAUSE_S )
        {
            sprintf( state, "paused" );
        }
        else
        {
            sprintf( state, "stop" );
        }
    }
    else
    {
        sprintf( position, "%d", 0 );
        sprintf( time, "%d", 0 );
        sprintf( length, "%d", 0 );
        sprintf( state, "stop" );
    }
#undef p_sys

    aout_VolumeGet( p_args->p_intf, &i_volume );
    sprintf( volume, "%d", (int)i_volume );

    p_args->vars = E_(mvar_New)( "variables", "" );
    E_(mvar_AppendNewVar)( p_args->vars, "url_param",
                           i_request > 0 ? "1" : "0" );
    E_(mvar_AppendNewVar)( p_args->vars, "url_value", p_request );
    E_(mvar_AppendNewVar)( p_args->vars, "version", VLC_Version() );
    E_(mvar_AppendNewVar)( p_args->vars, "copyright", COPYRIGHT_MESSAGE );
    E_(mvar_AppendNewVar)( p_args->vars, "vlc_compile_by", VLC_CompileBy() );
    E_(mvar_AppendNewVar)( p_args->vars, "vlc_compile_host",
                           VLC_CompileHost() );
    E_(mvar_AppendNewVar)( p_args->vars, "vlc_compile_domain",
                           VLC_CompileDomain() );
    E_(mvar_AppendNewVar)( p_args->vars, "vlc_compiler", VLC_Compiler() );
    E_(mvar_AppendNewVar)( p_args->vars, "vlc_changeset", VLC_Changeset() );
    E_(mvar_AppendNewVar)( p_args->vars, "stream_position", position );
    E_(mvar_AppendNewVar)( p_args->vars, "stream_time", time );
    E_(mvar_AppendNewVar)( p_args->vars, "stream_length", length );
    E_(mvar_AppendNewVar)( p_args->vars, "volume", volume );
    E_(mvar_AppendNewVar)( p_args->vars, "stream_state", state );

    E_(SSInit)( &p_args->stack );

    /* allocate output */
    *pi_data = i_buffer + 1000;
    dst = *pp_data = malloc( *pi_data );

    /* we parse executing all  <vlc /> macros */
    E_(Execute)( p_args, p_request, i_request, pp_data, pi_data, &dst,
                 &p_buffer[0], &p_buffer[i_buffer] );

    *dst     = '\0';
    *pi_data = dst - *pp_data;

    E_(SSClean)( &p_args->stack );
    E_(mvar_Delete)( p_args->vars );
}

int  E_(HttpCallback)( httpd_file_sys_t *p_args,
                       httpd_file_t *p_file,
                       uint8_t *_p_request,
                       uint8_t **_pp_data, int *pi_data )
{
    char *p_request = (char *)_p_request;
    char **pp_data = (char **)_pp_data;
    FILE *f;

    /* FIXME: do we need character encoding translation here ? */
    if( ( f = fopen( p_args->file, "r" ) ) == NULL )
    {
        Callback404( p_args, pp_data, pi_data );
        return VLC_SUCCESS;
    }

    if( !p_args->b_html )
    {
        E_(FileLoad)( f, pp_data, pi_data );
    }
    else
    {
        int  i_buffer;
        char *p_buffer;

        /* first we load in a temporary buffer */
        E_(FileLoad)( f, &p_buffer, &i_buffer );

        ParseExecute( p_args, p_buffer, i_buffer, p_request, pp_data, pi_data );

        free( p_buffer );
    }

    fclose( f );

    return VLC_SUCCESS;
}

/****************************************************************************
 * HandlerCallback:
 ****************************************************************************
 * call the external handler and parse vlc macros if Content-Type is HTML
 ****************************************************************************/
int  E_(HandlerCallback)( httpd_handler_sys_t *p_args,
                          httpd_handler_t *p_handler, char *_p_url,
                          uint8_t *_p_request, int i_type,
                          uint8_t *_p_in, int i_in,
                          char *psz_remote_addr, char *psz_remote_host,
                          uint8_t **_pp_data, int *pi_data )
{
    char *p_url = (char *)_p_url;
    char *p_request = (char *)_p_request;
    char **pp_data = (char **)_pp_data;
    char *p_in = (char *)p_in;
    int i_request = p_request != NULL ? strlen( p_request ) : 0;
    char *p;
    int i_env = 0;
    char **ppsz_env = NULL;
    char *psz_tmp;
    char sep;
    int  i_buffer;
    char *p_buffer;
    char *psz_cwd, *psz_file = NULL;
    int i_ret;

#ifdef WIN32
    sep = '\\';
#else
    sep = '/';
#endif

    /* Create environment for the CGI */
    TAB_APPEND( i_env, ppsz_env, strdup("GATEWAY_INTERFACE=CGI/1.1") );
    TAB_APPEND( i_env, ppsz_env, strdup("SERVER_PROTOCOL=HTTP/1.1") );
    TAB_APPEND( i_env, ppsz_env, strdup("SERVER_SOFTWARE=" COPYRIGHT_MESSAGE) );

    switch( i_type )
    {
    case HTTPD_MSG_GET:
        TAB_APPEND( i_env, ppsz_env, strdup("REQUEST_METHOD=GET") );
        break;
    case HTTPD_MSG_POST:
        TAB_APPEND( i_env, ppsz_env, strdup("REQUEST_METHOD=POST") );
        break;
    case HTTPD_MSG_HEAD:
        TAB_APPEND( i_env, ppsz_env, strdup("REQUEST_METHOD=HEAD") );
        break;
    default:
        break;
    }

    if( i_request )
    {
        psz_tmp = malloc( sizeof("QUERY_STRING=") + i_request );
        sprintf( psz_tmp, "QUERY_STRING=%s", p_request );
        TAB_APPEND( i_env, ppsz_env, psz_tmp );

        psz_tmp = malloc( sizeof("REQUEST_URI=?") + strlen(p_url)
                           + i_request );
        sprintf( psz_tmp, "REQUEST_URI=%s?%s", p_url, p_request );
        TAB_APPEND( i_env, ppsz_env, psz_tmp );
    }
    else
    {
        psz_tmp = malloc( sizeof("REQUEST_URI=") + strlen(p_url) );
        sprintf( psz_tmp, "REQUEST_URI=%s", p_url );
        TAB_APPEND( i_env, ppsz_env, psz_tmp );
    }

    psz_tmp = malloc( sizeof("SCRIPT_NAME=") + strlen(p_url) );
    sprintf( psz_tmp, "SCRIPT_NAME=%s", p_url );
    TAB_APPEND( i_env, ppsz_env, psz_tmp );

#define p_sys p_args->file.p_intf->p_sys
    psz_tmp = malloc( sizeof("SERVER_NAME=") + strlen(p_sys->psz_address) );
    sprintf( psz_tmp, "SERVER_NAME=%s", p_sys->psz_address );
    TAB_APPEND( i_env, ppsz_env, psz_tmp );

    psz_tmp = malloc( sizeof("SERVER_PORT=") + 5 );
    sprintf( psz_tmp, "SERVER_PORT=%u", p_sys->i_port );
    TAB_APPEND( i_env, ppsz_env, psz_tmp );
#undef p_sys

    p = getenv( "PATH" );
    if( p != NULL )
    {
        psz_tmp = malloc( sizeof("PATH=") + strlen(p) );
        sprintf( psz_tmp, "PATH=%s", p );
        TAB_APPEND( i_env, ppsz_env, psz_tmp );
    }

#ifdef WIN32
    p = getenv( "windir" );
    if( p != NULL )
    {
        psz_tmp = malloc( sizeof("SYSTEMROOT=") + strlen(p) );
        sprintf( psz_tmp, "SYSTEMROOT=%s", p );
        TAB_APPEND( i_env, ppsz_env, psz_tmp );
    }
#endif

    if( psz_remote_addr != NULL && *psz_remote_addr )
    {
        psz_tmp = malloc( sizeof("REMOTE_ADDR=") + strlen(psz_remote_addr) );
        sprintf( psz_tmp, "REMOTE_ADDR=%s", psz_remote_addr );
        TAB_APPEND( i_env, ppsz_env, psz_tmp );
    }

    if( psz_remote_host != NULL && *psz_remote_host )
    {
        psz_tmp = malloc( sizeof("REMOTE_HOST=") + strlen(psz_remote_host) );
        sprintf( psz_tmp, "REMOTE_HOST=%s", psz_remote_host );
        TAB_APPEND( i_env, ppsz_env, psz_tmp );
    }

    if( i_in )
    {
        p = p_in;
        for ( ; ; )
        {
            if( !strncmp( p, "Content-Type: ", strlen("Content-Type: ") ) )
            {
                char *end = strchr( p, '\r' );
                if( end == NULL )
                    break;
                *end = '\0';
                psz_tmp = malloc( sizeof("CONTENT_TYPE=") + strlen(p) );
                sprintf( psz_tmp, "CONTENT_TYPE=%s", p );
                TAB_APPEND( i_env, ppsz_env, psz_tmp );
                *end = '\r';
            }
            if( !strncmp( p, "Content-Length: ", strlen("Content-Length: ") ) )
            {
                char *end = strchr( p, '\r' );
                if( end == NULL )
                    break;
                *end = '\0';
                psz_tmp = malloc( sizeof("CONTENT_LENGTH=") + strlen(p) );
                sprintf( psz_tmp, "CONTENT_LENGTH=%s", p );
                TAB_APPEND( i_env, ppsz_env, psz_tmp );
                *end = '\r';
            }

            p = strchr( p, '\n' );
            if( p == NULL || p[1] == '\r' )
            {
                p = NULL;
                break;
            }
            p++;
        }
    }

    psz_file = strrchr( p_args->file.file, sep );
    if( psz_file != NULL )
    {
        psz_file++;
        psz_tmp = malloc( sizeof("SCRIPT_FILENAME=") + strlen(psz_file) );
        sprintf( psz_tmp, "SCRIPT_FILENAME=%s", psz_file );
        TAB_APPEND( i_env, ppsz_env, psz_tmp );

        TAB_APPEND( p_args->p_association->i_argc,
                    p_args->p_association->ppsz_argv, psz_file );
    }

    TAB_APPEND( i_env, ppsz_env, NULL );

    TAB_APPEND( p_args->p_association->i_argc, p_args->p_association->ppsz_argv,
                NULL );

    psz_tmp = strdup( p_args->file.file );
    p = strrchr( psz_tmp, sep );
    if( p != NULL )
    {
        *p = '\0';
        psz_cwd = psz_tmp;
    }
    else
    {
        free( psz_tmp );
        psz_cwd = NULL;
    }

    i_ret = vlc_execve( p_args->file.p_intf, p_args->p_association->i_argc,
                        p_args->p_association->ppsz_argv, ppsz_env, psz_cwd,
                        (char *)p_in, i_in, &p_buffer, &i_buffer );
    TAB_REMOVE( p_args->p_association->i_argc, p_args->p_association->ppsz_argv,
                NULL );
    TAB_REMOVE( p_args->p_association->i_argc, p_args->p_association->ppsz_argv,
                psz_file );
    if( psz_cwd != NULL )
        free( psz_cwd );
    while( i_env )
        TAB_REMOVE( i_env, ppsz_env, ppsz_env[0] );

    if( i_ret == -1 )
    {
        Callback404( (httpd_file_sys_t *)p_args, pp_data, pi_data );
        return VLC_SUCCESS;
    }
    p = p_buffer;
    while( strncmp( p, "Content-Type: text/html",
                    strlen("Content-Type: text/html") ) )
    {
        p = strchr( p, '\n' );
        if( p == NULL || p[1] == '\r' )
        {
            p = NULL;
            break;
        }
        p++;
    }

    if( p == NULL )
    {
        *pp_data = p_buffer;
        *pi_data = i_buffer;
    }
    else
    {
        ParseExecute( (httpd_file_sys_t *)p_args, p_buffer, i_buffer,
                      p_request, pp_data, pi_data );

        free( p_buffer );
    }

    return VLC_SUCCESS;
}
