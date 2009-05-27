/*****************************************************************************
 * http.c : HTTP/HTTPS Remote control interface
 *****************************************************************************
 * Copyright (C) 2001-2006 the VideoLAN team
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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "http.h"
#include <vlc_plugin.h>
#include <vlc_url.h>

#include <assert.h>

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

#define HOST_TEXT N_( "Host address" )
#define HOST_LONGTEXT N_( \
    "Address and port the HTTP interface will listen on. It defaults to " \
    "all network interfaces (0.0.0.0)." \
    " If you want the HTTP interface to be available only on the local " \
    "machine, enter 127.0.0.1" )
#define SRC_TEXT N_( "Source directory" )
#define SRC_LONGTEXT N_( "Source directory" )
#define HANDLERS_TEXT N_( "Handlers" )
#define HANDLERS_LONGTEXT N_( \
        "List of handler extensions and executable paths (for instance: " \
        "php=/usr/bin/php,pl=/usr/bin/perl)." )
#define ART_TEXT N_( "Export album art as /art." )
#define ART_LONGTEXT N_( \
        "Allow exporting album art for current playlist items at the " \
        "/art and /art?id=<id> URLs." )
#define CERT_TEXT N_( "Certificate file" )
#define CERT_LONGTEXT N_( "HTTP interface x509 PEM certificate file " \
                          "(enables SSL)." )
#define KEY_TEXT N_( "Private key file" )
#define KEY_LONGTEXT N_( "HTTP interface x509 PEM private key file." )
#define CA_TEXT N_( "Root CA file" )
#define CA_LONGTEXT N_( "HTTP interface x509 PEM trusted root CA " \
                        "certificates file." )
#define CRL_TEXT N_( "CRL file" )
#define CRL_LONGTEXT N_( "HTTP interace Certificates Revocation List file." )

vlc_module_begin ()
    set_shortname( N_("HTTP"))
    set_description( N_("HTTP remote control interface") )
    set_category( CAT_INTERFACE )
    set_subcategory( SUBCAT_INTERFACE_MAIN )
        add_string ( "http-host", NULL, NULL, HOST_TEXT, HOST_LONGTEXT, true )
        add_string ( "http-src",  NULL, NULL, SRC_TEXT,  SRC_LONGTEXT,  true )
        add_obsolete_string ( "http-charset" )
#if defined( HAVE_FORK ) || defined( WIN32 )
        add_string ( "http-handlers", NULL, NULL, HANDLERS_TEXT, HANDLERS_LONGTEXT, true )
#endif
        add_bool   ( "http-album-art", false, NULL, ART_TEXT, ART_LONGTEXT, true )
        set_section( N_("HTTP SSL" ), 0 )
        add_string ( "http-intf-cert", NULL, NULL, CERT_TEXT, CERT_LONGTEXT, true )
        add_string ( "http-intf-key",  NULL, NULL, KEY_TEXT,  KEY_LONGTEXT,  true )
        add_string ( "http-intf-ca",   NULL, NULL, CA_TEXT,   CA_LONGTEXT,   true )
        add_string ( "http-intf-crl",  NULL, NULL, CRL_TEXT,  CRL_LONGTEXT,  true )
    set_capability( "interface", 0 )
    set_callbacks( Open, Close )
vlc_module_end ()


/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
int  ArtCallback( httpd_handler_sys_t *p_args,
                          httpd_handler_t *p_handler, char *_p_url,
                          uint8_t *_p_request, int i_type,
                          uint8_t *_p_in, int i_in,
                          char *psz_remote_addr, char *psz_remote_host,
                          uint8_t **pp_data, int *pi_data );

/*****************************************************************************
 * Activate: initialize and create stuff
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t*)p_this;
    intf_sys_t    *p_sys;
    char          *psz_address;
    char          *psz_cert = NULL, *psz_key = NULL, *psz_ca = NULL,
                  *psz_crl = NULL;
    int           i_port       = 0;
    char          *psz_src = NULL;

    psz_address = var_CreateGetNonEmptyString( p_intf, "http-host" );
    if( psz_address != NULL )
    {
        char *psz_parser = strrchr( psz_address, ':' );
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
        free( psz_address );
        return( VLC_ENOMEM );
    }

    p_sys->p_playlist = pl_Hold( p_this );
    p_sys->p_input    = NULL;
    p_sys->p_vlm      = NULL;
    p_sys->psz_address = psz_address;
    p_sys->i_port     = i_port;
    p_sys->p_art_handler = NULL;

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
            psz_options = FirstWord( psz_program, psz_program );
            p_handler->i_argc = 0;
            p_handler->ppsz_argv = NULL;
            TAB_APPEND( p_handler->i_argc, p_handler->ppsz_argv,
                        strdup( psz_program ) );
            while( psz_options != NULL && *psz_options )
            {
                char *psz_next = FirstWord( psz_options, psz_options );
                TAB_APPEND( p_handler->i_argc, p_handler->ppsz_argv,
                            strdup( psz_options ) );
                psz_options = psz_next;
            }
            /* NULL will be appended later on */

            TAB_APPEND( p_sys->i_handlers, p_sys->pp_handlers, p_handler );
        }
    }
    free( psz_src );
#endif

    /* determine SSL configuration */
    psz_cert = config_GetPsz( p_intf, "http-intf-cert" );
    if ( psz_cert != NULL )
    {
        msg_Dbg( p_intf, "enabling TLS for HTTP interface (cert file: %s)",
                 psz_cert );
        psz_key = var_GetNonEmptyString( p_intf, "http-intf-key" );
        psz_ca = var_GetNonEmptyString( p_intf, "http-intf-ca" );
        psz_crl = var_GetNonEmptyString( p_intf, "http-intf-crl" );

        if( i_port <= 0 )
            i_port = 8443;
    }
    else
    {
        if( i_port <= 0 )
            i_port= 8080;
    }

    msg_Dbg( p_intf, "base %s:%d", psz_address, i_port );

    p_sys->p_httpd_host = httpd_TLSHostNew( VLC_OBJECT(p_intf), psz_address,
                                            i_port, psz_cert, psz_key, psz_ca,
                                            psz_crl );
    free( psz_cert );
    free( psz_key );
    free( psz_ca );
    free( psz_crl );

    if( p_sys->p_httpd_host == NULL )
    {
        msg_Err( p_intf, "cannot listen on %s:%d", psz_address, i_port );
        pl_Release( p_this );
        free( p_sys->psz_address );
        free( p_sys );
        return VLC_EGENERIC;
    }
    else
    {
        char psz_tmp[NI_MAXHOST + 6];

        /* Ugly hack to run several HTTP servers on different ports */
        snprintf( psz_tmp, sizeof (psz_tmp), "%s:%d", psz_address, i_port + 1 );
        var_Create(p_intf->p_libvlc, "http-host", VLC_VAR_STRING );
        var_SetString( p_intf->p_libvlc, "http-host", psz_tmp );
    }

    p_sys->i_files  = 0;
    p_sys->pp_files = NULL;

    psz_src = config_GetPsz( p_intf, "http-src" );
    if( ( psz_src == NULL ) || ( *psz_src == '\0' ) )
    {
        const char *data_path = config_GetDataDir ();
        if( asprintf( &psz_src, "%s" DIR_SEP "http", data_path ) == -1 )
            psz_src = NULL;
    }

    if( !psz_src || *psz_src == '\0' )
    {
        msg_Err( p_intf, "invalid web interface source directory" );
        goto failed;
    }

    /* remove trainling \ or / */
    if( psz_src[strlen( psz_src ) - 1] == '\\' ||
        psz_src[strlen( psz_src ) - 1] == '/' )
    {
        psz_src[strlen( psz_src ) - 1] = '\0';
    }

    ParseDirectory( p_intf, psz_src, psz_src );
    if( p_sys->i_files <= 0 )
    {
        msg_Err( p_intf, "cannot find any file in directory %s", psz_src );
        goto failed;
    }

    free( psz_src );

    if( config_GetInt( p_intf, "http-album-art" ) )
    {
        /* FIXME: we're leaking h */
        httpd_handler_sys_t *h = malloc( sizeof( httpd_handler_sys_t ) );
        if( !h )
            goto failed;
        h->file.p_intf = p_intf;
        h->file.file = NULL;
        h->file.name = NULL;
        /* TODO: use ACL and login/password stuff here too */
        h->p_handler = httpd_HandlerNew( p_sys->p_httpd_host,
                                         "/art", NULL, NULL, NULL,
                                         ArtCallback, h );
        p_sys->p_art_handler = h->p_handler;
    }

    return VLC_SUCCESS;

failed:
    free( psz_src );
    free( p_sys->pp_files );
    httpd_HostDelete( p_sys->p_httpd_host );
    free( p_sys->psz_address );
    free( p_sys );
    pl_Release( p_this );
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

#ifdef ENABLE_VLM
    if( p_sys->p_vlm )
        vlm_Delete( p_sys->p_vlm );
#endif
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
    free( p_sys->pp_files );
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
    if( p_sys->p_art_handler )
        httpd_HandlerDelete( p_sys->p_art_handler );
    httpd_HostDelete( p_sys->p_httpd_host );
    free( p_sys->psz_address );
    free( p_sys );
    pl_Release( p_this );
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
    p += sprintf( p, "Content-Type: text/html\n" );
    p += sprintf( p, "<html xmlns=\"http://www.w3.org/1999/xhtml\">\n" );
    p += sprintf( p, "<head>\n" );
    p += sprintf( p, "<title>Error loading %s</title>\n", p_args->file );
    p += sprintf( p, "</head>\n" );
    p += sprintf( p, "<body>\n" );
    p += sprintf( p, "<h1><center>Error loading %s for %s</center></h1>\n", p_args->file, p_args->name );
    p += sprintf( p, "<a href=\"http://www.videolan.org/\">VideoLAN</a>\n" );
    p += sprintf( p, "</body>\n" );
    p += sprintf( p, "</html>\n" );

    *pi_data = strlen( *pp_data );
}

static void ParseExecute( httpd_file_sys_t *p_args, char *p_buffer,
                          int i_buffer, char *p_request,
                          char **pp_data, int *pi_data )
{
    intf_sys_t *p_sys = p_args->p_intf->p_sys;
    int i_request = p_request != NULL ? strlen( p_request ) : 0;
    char *dst;
    vlc_value_t val;
    char position[4]; /* percentage */
    char time[12]; /* in seconds */
    char length[12]; /* in seconds */
    audio_volume_t i_volume;
    char volume[5];
    const char *state;
    char stats[20];

    assert( p_sys->p_input == NULL );
    /* FIXME: proper locking anyone? */
    p_sys->p_input = playlist_CurrentInput( p_sys->p_playlist );
    if( p_sys->p_input )
    {
        var_Get( p_sys->p_input, "position", &val);
        sprintf( position, "%d" , (int)((val.f_float) * 100.0));
        var_Get( p_sys->p_input, "time", &val);
        sprintf( time, "%"PRIi64, (int64_t)val.i_time / INT64_C(1000000) );
        var_Get( p_sys->p_input, "length", &val);
        sprintf( length, "%"PRIi64, (int64_t)val.i_time / INT64_C(1000000) );

        var_Get( p_sys->p_input, "state", &val );
        if( val.i_int == PLAYING_S )
        {
            state = "playing";
        }
        else if( val.i_int == OPENING_S )
        {
            state = "opening/connecting";
        }
        else if( val.i_int == PAUSE_S )
        {
            state = "paused";
        }
        else
        {
            state = "stop";
        }
    }
    else
    {
        sprintf( position, "%d", 0 );
        sprintf( time, "%d", 0 );
        sprintf( length, "%d", 0 );
        state = "stop";
    }

    aout_VolumeGet( p_args->p_intf, &i_volume );
    sprintf( volume, "%d", (int)i_volume );

    p_args->vars = mvar_New( "variables", "" );
    mvar_AppendNewVar( p_args->vars, "url_param",
                           i_request > 0 ? "1" : "0" );
    mvar_AppendNewVar( p_args->vars, "url_value", p_request );
    mvar_AppendNewVar( p_args->vars, "version", VLC_Version() );
    mvar_AppendNewVar( p_args->vars, "copyright", COPYRIGHT_MESSAGE );
    mvar_AppendNewVar( p_args->vars, "vlc_compile_by", VLC_CompileBy() );
    mvar_AppendNewVar( p_args->vars, "vlc_compile_host",
                           VLC_CompileHost() );
    mvar_AppendNewVar( p_args->vars, "vlc_compile_domain",
                           VLC_CompileDomain() );
    mvar_AppendNewVar( p_args->vars, "vlc_compiler", VLC_Compiler() );
    mvar_AppendNewVar( p_args->vars, "stream_position", position );
    mvar_AppendNewVar( p_args->vars, "stream_time", time );
    mvar_AppendNewVar( p_args->vars, "stream_length", length );
    mvar_AppendNewVar( p_args->vars, "volume", volume );
    mvar_AppendNewVar( p_args->vars, "stream_state", state );
    mvar_AppendNewVar( p_args->vars, "charset", "UTF-8" );

    /* Stats */
    if( p_sys->p_input )
    {
        /* FIXME: Workarround a stupid assert in input_GetItem */
        input_item_t *p_item = p_sys->p_input && p_sys->p_input->p
                               ? input_GetItem( p_sys->p_input )
                               : NULL;

        if( p_item )
        {
            vlc_mutex_lock( &p_item->p_stats->lock );
#define STATS_INT( n ) sprintf( stats, "%d", p_item->p_stats->i_ ## n ); \
                       mvar_AppendNewVar( p_args->vars, #n, stats );
#define STATS_FLOAT( n ) sprintf( stats, "%f", p_item->p_stats->f_ ## n ); \
                       mvar_AppendNewVar( p_args->vars, #n, stats );
            STATS_INT( read_bytes )
            STATS_FLOAT( input_bitrate )
            STATS_INT( demux_read_bytes )
            STATS_FLOAT( demux_bitrate )
            STATS_INT( decoded_video )
            STATS_INT( displayed_pictures )
            STATS_INT( lost_pictures )
            STATS_INT( decoded_audio )
            STATS_INT( played_abuffers )
            STATS_INT( lost_abuffers )
            STATS_INT( sent_packets )
            STATS_INT( sent_bytes )
            STATS_FLOAT( send_bitrate )
#undef STATS_INT
#undef STATS_FLOAT
            vlc_mutex_unlock( &p_item->p_stats->lock );
        }
    }

    SSInit( &p_args->stack );

    /* allocate output */
    *pi_data = i_buffer + 1000;
    dst = *pp_data = malloc( *pi_data );

    /* we parse executing all  <vlc /> macros */
    Execute( p_args, p_request, i_request, pp_data, pi_data, &dst,
                 &p_buffer[0], &p_buffer[i_buffer] );

    *dst     = '\0';
    *pi_data = dst - *pp_data;

    if( p_sys->p_input != NULL )
    {
        vlc_object_release( p_sys->p_input );
        p_sys->p_input = NULL;
    }
    SSClean( &p_args->stack );
    mvar_Delete( p_args->vars );
}

int  HttpCallback( httpd_file_sys_t *p_args,
                       httpd_file_t *p_file,
                       uint8_t *_p_request,
                       uint8_t **_pp_data, int *pi_data )
{
    VLC_UNUSED(p_file);
    char *p_request = (char *)_p_request;
    char **pp_data = (char **)_pp_data;
    FILE *f;

    if( ( f = utf8_fopen( p_args->file, "r" ) ) == NULL )
    {
        Callback404( p_args, pp_data, pi_data );
        return VLC_SUCCESS;
    }

    if( !p_args->b_html )
    {
        FileLoad( f, pp_data, pi_data );
    }
    else
    {
        int  i_buffer;
        char *p_buffer;

        /* first we load in a temporary buffer */
        FileLoad( f, &p_buffer, &i_buffer );

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
int  HandlerCallback( httpd_handler_sys_t *p_args,
                          httpd_handler_t *p_handler, char *_p_url,
                          uint8_t *_p_request, int i_type,
                          uint8_t *_p_in, int i_in,
                          char *psz_remote_addr, char *psz_remote_host,
                          uint8_t **_pp_data, int *pi_data )
{
    VLC_UNUSED(p_handler); VLC_UNUSED(_p_in);
    char *p_url = (char *)_p_url;
    char *p_request = (char *)_p_request;
    char **pp_data = (char **)_pp_data;
    char *p_in = (char *)_p_in;
    int i_request = p_request != NULL ? strlen( p_request ) : 0;
    char *p;
    int i_env = 0;
    char **ppsz_env = NULL;
    char *psz_tmp;
    size_t i_buffer;
    char *p_buffer;
    char *psz_cwd, *psz_file = NULL;
    int i_ret;

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
        asprintf( &psz_tmp, "QUERY_STRING=%s", p_request );
        TAB_APPEND( i_env, ppsz_env, psz_tmp );

        asprintf( &psz_tmp, "REQUEST_URI=%s?%s", p_url, p_request );
        TAB_APPEND( i_env, ppsz_env, psz_tmp );
    }
    else
    {
        asprintf( &psz_tmp, "REQUEST_URI=%s", p_url );
        TAB_APPEND( i_env, ppsz_env, psz_tmp );
    }

    asprintf( &psz_tmp, "SCRIPT_NAME=%s", p_url );
    TAB_APPEND( i_env, ppsz_env, psz_tmp );

#define p_sys p_args->file.p_intf->p_sys
    asprintf( &psz_tmp, "SERVER_NAME=%s", p_sys->psz_address );
    TAB_APPEND( i_env, ppsz_env, psz_tmp );

    asprintf( &psz_tmp, "SERVER_PORT=%u", p_sys->i_port );
    TAB_APPEND( i_env, ppsz_env, psz_tmp );
#undef p_sys

    p = getenv( "PATH" );
    if( p != NULL )
    {
        asprintf( &psz_tmp, "PATH=%s", p );
        TAB_APPEND( i_env, ppsz_env, psz_tmp );
    }

#ifdef WIN32
    p = getenv( "windir" );
    if( p != NULL )
    {
        asprintf( &psz_tmp, "SYSTEMROOT=%s", p );
        TAB_APPEND( i_env, ppsz_env, psz_tmp );
    }
#endif

    if( psz_remote_addr != NULL && *psz_remote_addr )
    {
        asprintf( &psz_tmp, "REMOTE_ADDR=%s", psz_remote_addr );
        TAB_APPEND( i_env, ppsz_env, psz_tmp );
    }

    if( psz_remote_host != NULL && *psz_remote_host )
    {
        asprintf( &psz_tmp, "REMOTE_HOST=%s", psz_remote_host );
        TAB_APPEND( i_env, ppsz_env, psz_tmp );
    }

    if( i_in )
    {
        p = p_in;
        for ( ; ; )
        {
            if( !strncasecmp( p, "Content-Type: ", strlen("Content-Type: ") ) )
            {
                char *end = strchr( p, '\r' );
                if( end == NULL )
                    break;
                *end = '\0';
                asprintf( &psz_tmp, "CONTENT_TYPE=%s", p );
                TAB_APPEND( i_env, ppsz_env, psz_tmp );
                *end = '\r';
            }
            if( !strncasecmp( p, "Content-Length: ",
                              strlen("Content-Length: ") ) )
            {
                char *end = strchr( p, '\r' );
                if( end == NULL )
                    break;
                *end = '\0';
                asprintf( &psz_tmp, "CONTENT_LENGTH=%s", p );
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

    psz_file = strrchr( p_args->file.file, DIR_SEP_CHAR );
    if( psz_file != NULL )
    {
        psz_file++;
        asprintf( &psz_tmp, "SCRIPT_FILENAME=%s", psz_file );
        TAB_APPEND( i_env, ppsz_env, psz_tmp );

        TAB_APPEND( p_args->p_association->i_argc,
                    p_args->p_association->ppsz_argv, psz_file );
    }

    TAB_APPEND( i_env, ppsz_env, NULL );

    TAB_APPEND( p_args->p_association->i_argc, p_args->p_association->ppsz_argv,
                NULL );

    psz_tmp = strdup( p_args->file.file );
    p = strrchr( psz_tmp, DIR_SEP_CHAR );
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
    free( psz_cwd );
    while( i_env )
        TAB_REMOVE( i_env, ppsz_env, ppsz_env[0] );

    if( i_ret == -1 )
    {
        Callback404( (httpd_file_sys_t *)p_args, pp_data, pi_data );
        return VLC_SUCCESS;
    }
    p = p_buffer;
    while( strncasecmp( p, "Content-Type: text/html",
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

int  ArtCallback( httpd_handler_sys_t *p_args,
                          httpd_handler_t *p_handler, char *_p_url,
                          uint8_t *p_request, int i_type,
                          uint8_t *p_in, int i_in,
                          char *psz_remote_addr, char *psz_remote_host,
                          uint8_t **pp_data, int *pi_data )
{
    VLC_UNUSED(p_handler); VLC_UNUSED(_p_url); VLC_UNUSED(i_type); 
    VLC_UNUSED(p_in); VLC_UNUSED(i_in); VLC_UNUSED(psz_remote_addr); 
    VLC_UNUSED(psz_remote_host); 

    char *psz_art = NULL;
    intf_thread_t *p_intf = p_args->file.p_intf;
    intf_sys_t *p_sys = p_intf->p_sys;
    char psz_id[16];
    input_item_t *p_item = NULL;
    int i_id;

    psz_id[0] = '\0';
    if( p_request )
        ExtractURIValue( (char *)p_request, "id", psz_id, 15 );
    i_id = atoi( psz_id );
    if( i_id )
    {
        playlist_Lock( p_sys->p_playlist );
        playlist_item_t *p_pl_item = playlist_ItemGetById( p_sys->p_playlist,
                                                           i_id );
        if( p_pl_item )
            p_item = p_pl_item->p_input;
        playlist_Unlock( p_sys->p_playlist );
    }
    else
    {
        /* FIXME: Workarround a stupid assert in input_GetItem */
        if( p_sys->p_input && p_sys->p_input->p )
            p_item = input_GetItem( p_sys->p_input );
    }

    if( p_item )
    {
        psz_art = input_item_GetArtURL( p_item );
    }

    if( psz_art && !strncmp( psz_art, "file://", strlen( "file://" ) ) &&
        decode_URI( psz_art + 7 ) )
    {
        FILE *f;
        char *psz_ext;
        char *psz_header;
        char *p_data = NULL;
        int i_header_size, i_data;

        if( ( f = utf8_fopen( psz_art + strlen( "file://" ), "r" ) ) == NULL )
        {
            msg_Dbg( p_intf, "Couldn't open album art file %s",
                     psz_art + strlen( "file://" ) );
            Callback404( &p_args->file, (char**)pp_data, pi_data );
            free( psz_art );
            return VLC_SUCCESS;
        }

        FileLoad( f, &p_data, &i_data );

        fclose( f );

        psz_ext = strrchr( psz_art, '.' );
        if( psz_ext ) psz_ext++;

#define HEADER  "Content-Type: image/%s\n" \
                "Content-Length: %d\n" \
                "\n"
        i_header_size = asprintf( &psz_header, HEADER, psz_ext, i_data );
#undef HEADER

        *pi_data = i_header_size + i_data;
        *pp_data = (uint8_t*)malloc( *pi_data );
        memcpy( *pp_data, psz_header, i_header_size );
        memcpy( *pp_data+i_header_size, p_data, i_data );
        free( psz_header );
        free( p_data );
    }
    else
    {
        msg_Dbg( p_intf, "No album art found" );
        Callback404( &p_args->file, (char**)pp_data, pi_data );
    }

    free( psz_art );

    return VLC_SUCCESS;
}
