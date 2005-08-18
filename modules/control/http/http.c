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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
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
#if !defined(SYS_DARWIN) && !defined(SYS_BEOS) && !defined(WIN32)
static int DirectoryCheck( char *psz_dir )
{
    DIR           *p_dir;

#ifdef HAVE_SYS_STAT_H
    struct stat   stat_info;

    if( stat( psz_dir, &stat_info ) == -1 || !S_ISDIR( stat_info.st_mode ) )
    {
        return VLC_EGENERIC;
    }
#endif

    if( ( p_dir = opendir( psz_dir ) ) == NULL )
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
    char          *psz_host;
    char          *psz_address = "";
    const char    *psz_cert = NULL, *psz_key = NULL, *psz_ca = NULL,
                  *psz_crl = NULL;
    int           i_port       = 0;
    char          *psz_src;

    psz_host = config_GetPsz( p_intf, "http-host" );
    if( psz_host )
    {
        char *psz_parser;
        psz_address = psz_host;

        psz_parser = strchr( psz_host, ':' );
        if( psz_parser )
        {
            *psz_parser++ = '\0';
            i_port = atoi( psz_parser );
        }
    }

    p_intf->p_sys = p_sys = malloc( sizeof( intf_sys_t ) );
    if( !p_intf->p_sys )
    {
        return( VLC_ENOMEM );
    }
    p_sys->p_playlist = NULL;
    p_sys->p_input    = NULL;
    p_sys->p_vlm      = NULL;

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

    msg_Dbg( p_intf, "base %s:%d", psz_address, i_port );

    p_sys->p_httpd_host = httpd_TLSHostNew( VLC_OBJECT(p_intf), psz_address,
                                            i_port, psz_cert, psz_key, psz_ca,
                                            psz_crl );
    if( p_sys->p_httpd_host == NULL )
    {
        msg_Err( p_intf, "cannot listen on %s:%d", psz_address, i_port );
        free( p_sys->psz_html_type );
        free( p_sys );
        return VLC_EGENERIC;
    }

    if( psz_host )
    {
        free( psz_host );
    }

    p_sys->i_files  = 0;
    p_sys->pp_files = NULL;

#if defined(SYS_DARWIN) || defined(SYS_BEOS) || defined(WIN32)
    if ( ( psz_src = config_GetPsz( p_intf, "http-src" )) == NULL )
    {
        char * psz_vlcpath = p_intf->p_libvlc->psz_vlcpath;
        psz_src = malloc( strlen(psz_vlcpath) + strlen("/share/http" ) + 1 );
        if( !psz_src ) return VLC_ENOMEM;
#if defined(WIN32)
        sprintf( psz_src, "%s/http", psz_vlcpath);
#else
        sprintf( psz_src, "%s/share/http", psz_vlcpath);
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
    httpd_HostDelete( p_sys->p_httpd_host );
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
 * <vlc id="macro name" [param1="" [param2=""]] />
 * valid id are
 *
 ****************************************************************************/
int  E_(HttpCallback)( httpd_file_sys_t *p_args,
                       httpd_file_t *p_file,
                       uint8_t *_p_request,
                       uint8_t **_pp_data, int *pi_data )
{
    char *p_request = (char *)_p_request;
    char **pp_data = (char **)_pp_data;
    int i_request = p_request ? strlen( p_request ) : 0;
    char *p;
    FILE *f;

    if( ( f = fopen( p_args->file, "r" ) ) == NULL )
    {
        p = *pp_data = malloc( 10240 );
        if( !p )
        {
                return VLC_EGENERIC;
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

        aout_VolumeGet( p_args->p_intf , &i_volume );
        sprintf( volume , "%d" , (int)i_volume );

        p_args->vars = mvar_New( "variables", "" );
        mvar_AppendNewVar( p_args->vars, "url_param",
                           i_request > 0 ? "1" : "0" );
        mvar_AppendNewVar( p_args->vars, "url_value", p_request );
        mvar_AppendNewVar( p_args->vars, "version", VLC_Version() );
        mvar_AppendNewVar( p_args->vars, "copyright", COPYRIGHT_MESSAGE );
        mvar_AppendNewVar( p_args->vars, "vlc_compile_time",
                           VLC_CompileTime() );
        mvar_AppendNewVar( p_args->vars, "vlc_compile_by", VLC_CompileBy() );
        mvar_AppendNewVar( p_args->vars, "vlc_compile_host",
                           VLC_CompileHost() );
        mvar_AppendNewVar( p_args->vars, "vlc_compile_domain",
                           VLC_CompileDomain() );
        mvar_AppendNewVar( p_args->vars, "vlc_compiler", VLC_Compiler() );
        mvar_AppendNewVar( p_args->vars, "vlc_changeset", VLC_Changeset() );
        mvar_AppendNewVar( p_args->vars, "stream_position", position );
        mvar_AppendNewVar( p_args->vars, "stream_time", time );
        mvar_AppendNewVar( p_args->vars, "stream_length", length );
        mvar_AppendNewVar( p_args->vars, "volume", volume );
        mvar_AppendNewVar( p_args->vars, "stream_state", state );

        SSInit( &p_args->stack );

        /* first we load in a temporary buffer */
        E_(FileLoad)( f, &p_buffer, &i_buffer );

        /* allocate output */
        *pi_data = i_buffer + 1000;
        dst = *pp_data = malloc( *pi_data );

        /* we parse executing all  <vlc /> macros */
        E_(Execute)( p_args, p_request, i_request, pp_data, pi_data, &dst,
                     &p_buffer[0], &p_buffer[i_buffer] );

        *dst     = '\0';
        *pi_data = dst - *pp_data;

        SSClean( &p_args->stack );
        mvar_Delete( p_args->vars );
        free( p_buffer );
    }

    fclose( f );

    return VLC_SUCCESS;
}

