/*****************************************************************************
 * http.c :  http mini-server ;)
 *****************************************************************************
 * Copyright (C) 2001-2005 the VideoLAN team
 * $Id$
 *
 * Authors: Gildas Bazin <gbazin@netcourrier.com>
 *          Laurent Aimar <fenrir@via.ecp.fr>
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
/* TODO:
 * - clean up ?
 * - doc ! (mouarf ;)
 */

#include <stdlib.h>
#include <ctype.h>
#include <vlc/vlc.h>
#include <vlc/intf.h>

#include <vlc/aout.h>
#include <vlc/vout.h> /* for fullscreen */

#include "vlc_httpd.h"
#include "vlc_vlm.h"
#include "vlc_tls.h"
#include "vlc_acl.h"
#include "charset.h"

#ifdef HAVE_SYS_STAT_H
#   include <sys/stat.h>
#endif
#ifdef HAVE_ERRNO_H
#   include <errno.h>
#endif
#ifdef HAVE_FCNTL_H
#   include <fcntl.h>
#endif

#ifdef HAVE_UNISTD_H
#   include <unistd.h>
#elif defined( WIN32 ) && !defined( UNDER_CE )
#   include <io.h>
#endif

#ifdef HAVE_DIRENT_H
#   include <dirent.h>
#endif

/* stat() support for large files on win32 */
#if defined( WIN32 ) && !defined( UNDER_CE )
#   define stat _stati64
#endif

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

static int ParseDirectory( intf_thread_t *p_intf, char *psz_root,
                           char *psz_dir );

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

static int  HttpCallback( httpd_file_sys_t *p_args,
                          httpd_file_t *,
                          uint8_t *p_request,
                          uint8_t **pp_data, int *pi_data );

static char *uri_extract_value( char *psz_uri, const char *psz_name,
                                char *psz_value, int i_value_max );
static int uri_test_param( char *psz_uri, const char *psz_name );

static void uri_decode_url_encoded( char *psz );

static char *Find_end_MRL( char *psz );
static playlist_item_t *parse_MRL( intf_thread_t *, char *psz, char *psz_name );

/*****************************************************************************
 *
 *****************************************************************************/
typedef struct mvar_s
{
    char *name;
    char *value;

    int           i_field;
    struct mvar_s **field;
} mvar_t;

#define STACK_MAX 100
typedef struct
{
    char *stack[STACK_MAX];
    int  i_stack;
} rpn_stack_t;

struct httpd_file_sys_t
{
    intf_thread_t    *p_intf;
    httpd_file_t     *p_file;
    httpd_redirect_t *p_redir;
    httpd_redirect_t *p_redir2;

    char          *file;
    char          *name;

    vlc_bool_t    b_html;

    /* inited for each access */
    rpn_stack_t   stack;
    mvar_t        *vars;
};

struct intf_sys_t
{
    httpd_host_t        *p_httpd_host;

    int                 i_files;
    httpd_file_sys_t    **pp_files;

    playlist_t          *p_playlist;
    input_thread_t      *p_input;
    vlm_t               *p_vlm;
    char                *psz_html_type;
    vlc_iconv_t         iconv_from_utf8, iconv_to_utf8;
};



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

    ParseDirectory( p_intf, psz_src, psz_src );


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
void Close ( vlc_object_t *p_this )
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


/*****************************************************************************
 * Local functions
 *****************************************************************************/
#define MAX_DIR_SIZE 10240

/****************************************************************************
 * FileToUrl: create a good name for an url from filename
 ****************************************************************************/
static char *FileToUrl( char *name, vlc_bool_t *pb_index )
{
    char *url, *p;

    url = p = malloc( strlen( name ) + 1 );

    *pb_index = VLC_FALSE;
    if( !url || !p )
    {
        return NULL;
    }

#ifdef WIN32
    while( *name == '\\' || *name == '/' )
#else
    while( *name == '/' )
#endif
    {
        name++;
    }

    *p++ = '/';
    strcpy( p, name );

#ifdef WIN32
    /* convert '\\' into '/' */
    name = p;
    while( *name )
    {
        if( *name == '\\' )
        {
            *p++ = '/';
        }
        name++;
    }
#endif

    /* index.* -> / */
    if( ( p = strrchr( url, '/' ) ) != NULL )
    {
        if( !strncmp( p, "/index.", 7 ) )
        {
            p[1] = '\0';
            *pb_index = VLC_TRUE;
        }
    }
    return url;
}

static char *FromUTF8( intf_thread_t *p_intf, char *psz_utf8 )
{
    intf_sys_t    *p_sys = p_intf->p_sys;

    if ( p_sys->iconv_from_utf8 != (vlc_iconv_t)-1 )
    {
        char *psz_in = psz_utf8;
        size_t i_in = strlen(psz_in);
        size_t i_out = i_in * 2;
        char *psz_local = malloc(i_out + 1);
        char *psz_out = psz_local;

        size_t i_ret = vlc_iconv( p_sys->iconv_from_utf8, &psz_in, &i_in,
                                  &psz_out, &i_out );
        if( i_ret == (size_t)-1 || i_in )
        {
            msg_Warn( p_intf,
                      "failed to convert \"%s\" to desired charset (%s)",
                      psz_utf8, strerror(errno) );
            free( psz_local );
            return strdup( psz_utf8 );
        }

        *psz_out = '\0';
        return psz_local;
    }
    else
        return strdup( psz_utf8 );
}

static char *ToUTF8( intf_thread_t *p_intf, char *psz_local )
{
    intf_sys_t    *p_sys = p_intf->p_sys;

    if ( p_sys->iconv_to_utf8 != (vlc_iconv_t)-1 )
    {
        char *psz_in = psz_local;
        size_t i_in = strlen(psz_in);
        size_t i_out = i_in * 6;
        char *psz_utf8 = malloc(i_out + 1);
        char *psz_out = psz_utf8;

        size_t i_ret = vlc_iconv( p_sys->iconv_to_utf8, &psz_in, &i_in,
                                  &psz_out, &i_out );
        if( i_ret == (size_t)-1 || i_in )
        {
            msg_Warn( p_intf,
                      "failed to convert \"%s\" to desired charset (%s)",
                      psz_local, strerror(errno) );
            free( psz_utf8 );
            return strdup( psz_local );
        }

        *psz_out = '\0';
        return psz_utf8;
    }
    else
        return strdup( psz_local );
}

/****************************************************************************
 * ParseDirectory: parse recursively a directory, adding each file
 ****************************************************************************/
static int ParseDirectory( intf_thread_t *p_intf, char *psz_root,
                           char *psz_dir )
{
    intf_sys_t     *p_sys = p_intf->p_sys;
    char           dir[MAX_DIR_SIZE];
#ifdef HAVE_SYS_STAT_H
    struct stat   stat_info;
#endif
    DIR           *p_dir;
    struct dirent *p_dir_content;
    vlc_acl_t     *p_acl;
    FILE          *file;

    char          *user = NULL;
    char          *password = NULL;

    int           i_dirlen;

#ifdef HAVE_SYS_STAT_H
    if( stat( psz_dir, &stat_info ) == -1 || !S_ISDIR( stat_info.st_mode ) )
    {
        return VLC_EGENERIC;
    }
#endif

    if( ( p_dir = opendir( psz_dir ) ) == NULL )
    {
        msg_Err( p_intf, "cannot open dir (%s)", psz_dir );
        return VLC_EGENERIC;
    }

    i_dirlen = strlen( psz_dir );
    if( i_dirlen + 10 > MAX_DIR_SIZE )
    {
        msg_Warn( p_intf, "skipping too deep dir (%s)", psz_dir );
        return 0;
    }

    msg_Dbg( p_intf, "dir=%s", psz_dir );

    sprintf( dir, "%s/.access", psz_dir );
    if( ( file = fopen( dir, "r" ) ) != NULL )
    {
        char line[1024];
        int  i_size;

        msg_Dbg( p_intf, "find .access in dir=%s", psz_dir );

        i_size = fread( line, 1, 1023, file );
        if( i_size > 0 )
        {
            char *p;
            while( i_size > 0 && ( line[i_size-1] == '\n' ||
                   line[i_size-1] == '\r' ) )
            {
                i_size--;
            }

            line[i_size] = '\0';

            p = strchr( line, ':' );
            if( p )
            {
                *p++ = '\0';
                user = strdup( line );
                password = strdup( p );
            }
        }
        msg_Dbg( p_intf, "using user=%s password=%s (read=%d)",
                 user, password, i_size );

        fclose( file );
    }

    sprintf( dir, "%s/.hosts", psz_dir );
    p_acl = ACL_Create( p_intf, VLC_FALSE );
    if( ACL_LoadFile( p_acl, dir ) )
    {
        ACL_Destroy( p_acl );
        p_acl = NULL;
    }

    for( ;; )
    {
        /* parse psz_src dir */
        if( ( p_dir_content = readdir( p_dir ) ) == NULL )
        {
            break;
        }

        if( ( p_dir_content->d_name[0] == '.' )
         || ( i_dirlen + strlen( p_dir_content->d_name ) > MAX_DIR_SIZE ) )
            continue;
        
        sprintf( dir, "%s/%s", psz_dir, p_dir_content->d_name );
        if( ParseDirectory( p_intf, psz_root, dir ) )
        {
            httpd_file_sys_t *f = malloc( sizeof( httpd_file_sys_t ) );
            vlc_bool_t b_index;
            char *psz_tmp;

            f->p_intf  = p_intf;
            f->p_file = NULL;
            f->p_redir = NULL;
            f->p_redir2 = NULL;
            psz_tmp = vlc_fix_readdir_charset( p_intf, dir );
            f->file = FromUTF8( p_intf, psz_tmp );
            free( psz_tmp );
            psz_tmp = vlc_fix_readdir_charset( p_intf,
                                               &dir[strlen( psz_root )] );
            f->name = FileToUrl( psz_tmp, &b_index );
            free( psz_tmp );
            f->b_html = strstr( &dir[strlen( psz_root )], ".htm" ) ? VLC_TRUE : VLC_FALSE;

            if( !f->name )
            {
                msg_Err( p_intf , "unable to parse directory" );
                closedir( p_dir );
                free( f );
                return( VLC_ENOMEM );
            }
            msg_Dbg( p_intf, "file=%s (url=%s)",
                     f->file, f->name );

            f->p_file = httpd_FileNew( p_sys->p_httpd_host,
                                       f->name,
                                       f->b_html ? p_sys->psz_html_type : NULL,
                                       user, password, p_acl,
                                       HttpCallback, f );

            if( f->p_file )
            {
                TAB_APPEND( p_sys->i_files, p_sys->pp_files, f );
            }
            /* for url that ends by / add
             *  - a redirect from rep to rep/
             *  - in case of index.* rep/index.html to rep/ */
            if( f && f->name[strlen(f->name) - 1] == '/' )
            {
                char *psz_redir = strdup( f->name );
                char *p;
                psz_redir[strlen( psz_redir ) - 1] = '\0';

                msg_Dbg( p_intf, "redir=%s -> %s", psz_redir, f->name );
                f->p_redir = httpd_RedirectNew( p_sys->p_httpd_host, f->name, psz_redir );
                free( psz_redir );

                if( b_index && ( p = strstr( f->file, "index." ) ) )
                {
                    asprintf( &psz_redir, "%s%s", f->name, p );

                    msg_Dbg( p_intf, "redir=%s -> %s", psz_redir, f->name );
                    f->p_redir2 = httpd_RedirectNew( p_sys->p_httpd_host,
                                                     f->name, psz_redir );

                    free( psz_redir );
                }
            }
        }
    }

    if( user )
    {
        free( user );
    }
    if( password )
    {
        free( password );
    }

    ACL_Destroy( p_acl );
    closedir( p_dir );

    return VLC_SUCCESS;
}

/****************************************************************************
 * var and set handling
 ****************************************************************************/

static mvar_t *mvar_New( const char *name, const char *value )
{
    mvar_t *v = malloc( sizeof( mvar_t ) );

    if( !v ) return NULL;
    v->name = strdup( name );
    v->value = strdup( value ? value : "" );

    v->i_field = 0;
    v->field = malloc( sizeof( mvar_t * ) );
    v->field[0] = NULL;

    return v;
}

static void mvar_Delete( mvar_t *v )
{
    int i;

    free( v->name );
    free( v->value );

    for( i = 0; i < v->i_field; i++ )
    {
        mvar_Delete( v->field[i] );
    }
    free( v->field );
    free( v );
}

static void mvar_AppendVar( mvar_t *v, mvar_t *f )
{
    v->field = realloc( v->field, sizeof( mvar_t * ) * ( v->i_field + 2 ) );
    v->field[v->i_field] = f;
    v->i_field++;
}

static mvar_t *mvar_Duplicate( const mvar_t *v )
{
    int i;
    mvar_t *n;

    n = mvar_New( v->name, v->value );
    for( i = 0; i < v->i_field; i++ )
    {
        mvar_AppendVar( n, mvar_Duplicate( v->field[i] ) );
    }

    return n;
}

static void mvar_PushVar( mvar_t *v, mvar_t *f )
{
    v->field = realloc( v->field, sizeof( mvar_t * ) * ( v->i_field + 2 ) );
    if( v->i_field > 0 )
    {
        memmove( &v->field[1], &v->field[0], sizeof( mvar_t * ) * v->i_field );
    }
    v->field[0] = f;
    v->i_field++;
}

static void mvar_RemoveVar( mvar_t *v, mvar_t *f )
{
    int i;
    for( i = 0; i < v->i_field; i++ )
    {
        if( v->field[i] == f )
        {
            break;
        }
    }
    if( i >= v->i_field )
    {
        return;
    }

    if( i + 1 < v->i_field )
    {
        memmove( &v->field[i], &v->field[i+1], sizeof( mvar_t * ) * ( v->i_field - i - 1 ) );
    }
    v->i_field--;
    /* FIXME should do a realloc */
}

static mvar_t *mvar_GetVar( mvar_t *s, const char *name )
{
    int i;
    char base[512], *field, *p;
    int  i_index;

    /* format: name[index].field */

    field = strchr( name, '.' );
    if( field )
    {
        int i = field - name;
        strncpy( base, name, i );
        base[i] = '\0';
        field++;
    }
    else
    {
        strcpy( base, name );
    }

    if( ( p = strchr( base, '[' ) ) )
    {
        *p++ = '\0';
        sscanf( p, "%d]", &i_index );
        if( i_index < 0 )
        {
            return NULL;
        }
    }
    else
    {
        i_index = 0;
    }

    for( i = 0; i < s->i_field; i++ )
    {
        if( !strcmp( s->field[i]->name, base ) )
        {
            if( i_index > 0 )
            {
                i_index--;
            }
            else
            {
                if( field )
                {
                    return mvar_GetVar( s->field[i], field );
                }
                else
                {
                    return s->field[i];
                }
            }
        }
    }
    return NULL;
}



static char *mvar_GetValue( mvar_t *v, char *field )
{
    if( *field == '\0' )
    {
        return v->value;
    }
    else
    {
        mvar_t *f = mvar_GetVar( v, field );
        if( f )
        {
            return f->value;
        }
        else
        {
            return field;
        }
    }
}

static void mvar_PushNewVar( mvar_t *vars, const char *name,
                             const char *value )
{
    mvar_t *f = mvar_New( name, value );
    mvar_PushVar( vars, f );
}

static void mvar_AppendNewVar( mvar_t *vars, const char *name,
                               const char *value )
{
    mvar_t *f = mvar_New( name, value );
    mvar_AppendVar( vars, f );
}


/* arg= start[:stop[:step]],.. */
static mvar_t *mvar_IntegerSetNew( const char *name, const char *arg )
{
    char *dup = strdup( arg );
    char *str = dup;
    mvar_t *s = mvar_New( name, "set" );


    while( str )
    {
        char *p;
        int  i_start,i_stop,i_step;
        int  i_match;

        p = strchr( str, ',' );
        if( p )
        {
            *p++ = '\0';
        }

        i_step = 0;
        i_match = sscanf( str, "%d:%d:%d", &i_start, &i_stop, &i_step );

        if( i_match == 1 )
        {
            i_stop = i_start;
            i_step = 1;
        }
        else if( i_match == 2 )
        {
            i_step = i_start < i_stop ? 1 : -1;
        }

        if( i_match >= 1 )
        {
            int i;

            if( ( i_start <= i_stop && i_step > 0 ) ||
                ( i_start >= i_stop && i_step < 0 ) )
            {
                for( i = i_start; ; i += i_step )
                {
                    char   value[79];

                    if( ( i_step > 0 && i > i_stop ) ||
                        ( i_step < 0 && i < i_stop ) )
                    {
                        break;
                    }

                    sprintf( value, "%d", i );

                    mvar_PushNewVar( s, name, value );
                }
            }
        }
        str = p;
    }

    free( dup );
    return s;
}

static void PlaylistListNode( intf_thread_t *p_intf, playlist_t *p_pl,
                              playlist_item_t *p_node, char *name, mvar_t *s,
                              int i_depth )
{
    if( p_node != NULL )
    {
        if( p_node->i_children == -1 )
        {
            char value[512];
            char *psz;
            mvar_t *itm = mvar_New( name, "set" );

            sprintf( value, "%d", ( p_pl->status.p_item == p_node )? 1 : 0 );
            mvar_AppendNewVar( itm, "current", value );

            sprintf( value, "%d", p_node->input.i_id );
            mvar_AppendNewVar( itm, "index", value );

            psz = FromUTF8( p_intf, p_node->input.psz_name );
            mvar_AppendNewVar( itm, "name", psz );
            free( psz );

            psz = FromUTF8( p_intf, p_node->input.psz_uri );
            mvar_AppendNewVar( itm, "uri", psz );
            free( psz );

            sprintf( value, "Item");
            mvar_AppendNewVar( itm, "type", value );

            sprintf( value, "%d", i_depth );
            mvar_AppendNewVar( itm, "depth", value );

            mvar_AppendVar( s, itm );
        }
        else
        {
            char value[512];
            char *psz;
            int i_child;
            mvar_t *itm = mvar_New( name, "set" );

            psz = FromUTF8( p_intf, p_node->input.psz_name );
            mvar_AppendNewVar( itm, "name", psz );
            mvar_AppendNewVar( itm, "uri", psz );
            free( psz );

            sprintf( value, "Node" );
            mvar_AppendNewVar( itm, "type", value );

            sprintf( value, "%d", p_node->input.i_id );
            mvar_AppendNewVar( itm, "index", value );

            sprintf( value, "%d", p_node->i_children);
            mvar_AppendNewVar( itm, "i_children", value );

            sprintf( value, "%d", i_depth );
            mvar_AppendNewVar( itm, "depth", value );

            mvar_AppendVar( s, itm );

            for (i_child = 0 ; i_child < p_node->i_children ; i_child++)
                PlaylistListNode( p_intf, p_pl, p_node->pp_children[i_child],
                                  name, s, i_depth + 1);

        }
    }
}

static mvar_t *mvar_PlaylistSetNew( intf_thread_t *p_intf, char *name,
                                    playlist_t *p_pl )
{
    playlist_view_t *p_view;
    mvar_t *s = mvar_New( name, "set" );


    vlc_mutex_lock( &p_pl->object_lock );

    p_view = playlist_ViewFind( p_pl, VIEW_CATEGORY ); /* FIXME */

    if( p_view != NULL )
        PlaylistListNode( p_intf, p_pl, p_view->p_root, name, s, 0 );

    vlc_mutex_unlock( &p_pl->object_lock );

    return s;
}

static mvar_t *mvar_InfoSetNew( intf_thread_t *p_intf, char *name,
                                input_thread_t *p_input )
{
    mvar_t *s = mvar_New( name, "set" );
    int i, j;

    if( p_input == NULL )
    {
        return s;
    }

    vlc_mutex_lock( &p_input->input.p_item->lock );
    for ( i = 0; i < p_input->input.p_item->i_categories; i++ )
    {
        info_category_t *p_category = p_input->input.p_item->pp_categories[i];
        char *psz;

        mvar_t *cat  = mvar_New( name, "set" );
        mvar_t *iset = mvar_New( "info", "set" );

        psz = FromUTF8( p_intf, p_category->psz_name );
        mvar_AppendNewVar( cat, "name", psz );
        free( psz );
        mvar_AppendVar( cat, iset );

        for ( j = 0; j < p_category->i_infos; j++ )
        {
            info_t *p_info = p_category->pp_infos[j];
            mvar_t *info = mvar_New( "info", "" );
            char *psz_name = FromUTF8( p_intf, p_info->psz_name );
            char *psz_value = FromUTF8( p_intf, p_info->psz_value );

            msg_Dbg( p_input, "adding info name=%s value=%s",
                     psz_name, psz_value );
            mvar_AppendNewVar( info, "name",  psz_name );
            mvar_AppendNewVar( info, "value", psz_value );
            free( psz_name );
            free( psz_value );
            mvar_AppendVar( iset, info );
        }
        mvar_AppendVar( s, cat );
    }
    vlc_mutex_unlock( &p_input->input.p_item->lock );

    return s;
}
#if 0
static mvar_t *mvar_HttpdInfoSetNew( char *name, httpd_t *p_httpd, int i_type )
{
    mvar_t       *s = mvar_New( name, "set" );
    httpd_info_t info;
    int          i;

    if( !p_httpd->pf_control( p_httpd, i_type, &info, NULL ) )
    {
        for( i= 0; i < info.i_count; )
        {
            mvar_t *inf;

            inf = mvar_New( name, "set" );
            do
            {
                /* fprintf( stderr," mvar_HttpdInfoSetNew: append name=`%s' value=`%s'\n",
                            info.info[i].psz_name, info.info[i].psz_value ); */
                mvar_AppendNewVar( inf,
                                   info.info[i].psz_name,
                                   info.info[i].psz_value );
                i++;
            } while( i < info.i_count && strcmp( info.info[i].psz_name, "id" ) );
            mvar_AppendVar( s, inf );
        }
    }

    /* free mem */
    for( i = 0; i < info.i_count; i++ )
    {
        free( info.info[i].psz_name );
        free( info.info[i].psz_value );
    }
    if( info.i_count > 0 )
    {
        free( info.info );
    }

    return s;
}
#endif

/* Utility function for scandir */
static int Filter( const struct dirent *foo )
{
    return VLC_TRUE;
}

static mvar_t *mvar_FileSetNew( intf_thread_t *p_intf, char *name,
                                char *psz_dir )
{
    mvar_t *s = mvar_New( name, "set" );
    char          tmp[MAX_DIR_SIZE], dir[MAX_DIR_SIZE], *p, *src;
#ifdef HAVE_SYS_STAT_H
    struct stat   stat_info;
#endif
    struct dirent **pp_dir_content;
    int           i_dir_content, i;
    char          sep;

    /* convert all / to native separator */
#if defined( WIN32 )
    while( (p = strchr( psz_dir, '/' )) )
    {
        *p = '\\';
    }
    sep = '\\';
#else
    sep = '/';
#endif

    /* remove trailling separator */
    while( strlen( psz_dir ) > 1 &&
#if defined( WIN32 )
           !( strlen(psz_dir)==3 && psz_dir[1]==':' && psz_dir[2]==sep ) &&
#endif
           psz_dir[strlen( psz_dir ) -1 ] == sep )
    {
        psz_dir[strlen( psz_dir ) -1 ]  ='\0';
    }
    /* remove double separator */
    for( p = src = psz_dir; *src != '\0'; src++, p++ )
    {
        if( src[0] == sep && src[1] == sep )
        {
            src++;
        }
        *p = *src;
    }
    *p = '\0';

    if( *psz_dir == '\0' )
    {
        return s;
    }

    if( *psz_dir == '~' )
    {
        /* This is incomplete : we should also support the ~cmassiot/ syntax. */
        snprintf( dir, sizeof(dir), "%s/%s", p_intf->p_vlc->psz_homedir,
                  psz_dir + 1 );
        psz_dir = dir;
    }

    /* first fix all .. dir */
    p = src = psz_dir;
    while( *src )
    {
        if( src[0] == '.' && src[1] == '.' )
        {
            src += 2;
            if( p <= &psz_dir[1] )
            {
                continue;
            }

            p -= 2;

            while( p > &psz_dir[1] && *p != sep )
            {
                p--;
            }
        }
        else if( *src == sep )
        {
            if( p > psz_dir && p[-1] == sep )
            {
                src++;
            }
            else
            {
                *p++ = *src++;
            }
        }
        else
        {
            do
            {
                *p++ = *src++;
            } while( *src && *src != sep );
        }
    }
    *p = '\0';


#ifdef HAVE_SYS_STAT_H
    if( stat( psz_dir, &stat_info ) == -1 || !S_ISDIR( stat_info.st_mode ) )
    {
        return s;
    }
#endif

    /* parse psz_src dir */
    if( ( i_dir_content = scandir( psz_dir, &pp_dir_content, Filter,
                                   alphasort ) ) == -1 )
    {
        msg_Warn( p_intf, "scandir error on %s (%s)", psz_dir,
                  strerror(errno) );
        return s;
    }

    /* remove trailing / or \ */
    for( p = &psz_dir[strlen( psz_dir) - 1];
         p >= psz_dir && ( *p =='/' || *p =='\\' ); p-- )
    {
        *p = '\0';
    }

    for( i = 0; i < i_dir_content; i++ )
    {
        struct dirent *p_dir_content = pp_dir_content[i];
        mvar_t *f;
        const char *psz_ext;
        char *psz_name, *psz_tmp;

        if( !strcmp( p_dir_content->d_name, "." ) )
        {
            continue;
        }

        snprintf( tmp, sizeof(tmp), "%s/%s", psz_dir, p_dir_content->d_name );

#ifdef HAVE_SYS_STAT_H
        if( stat( tmp, &stat_info ) == -1 )
        {
            continue;
        }
#endif
        f = mvar_New( name, "set" );

        psz_tmp = vlc_fix_readdir_charset( p_intf, p_dir_content->d_name );
        psz_name = FromUTF8( p_intf, psz_tmp );
        free( psz_tmp );
        snprintf( tmp, sizeof(tmp), "%s/%s", psz_dir, psz_name );
        mvar_AppendNewVar( f, "name", tmp );
        mvar_AppendNewVar( f, "basename", psz_name );

        /* put file extension in 'ext' */
        psz_ext = strrchr( psz_name, '.' );
        mvar_AppendNewVar( f, "ext", psz_ext != NULL ? psz_ext + 1 : "" );

        free( psz_name );

#ifdef HAVE_SYS_STAT_H
        if( S_ISDIR( stat_info.st_mode ) )
        {
            mvar_AppendNewVar( f, "type", "directory" );
        }
        else if( S_ISREG( stat_info.st_mode ) )
        {
            mvar_AppendNewVar( f, "type", "file" );
        }
        else
        {
            mvar_AppendNewVar( f, "type", "unknown" );
        }

        sprintf( tmp, I64Fd, (int64_t)stat_info.st_size );
        mvar_AppendNewVar( f, "size", tmp );

        /* FIXME memory leak FIXME */
#ifdef HAVE_CTIME_R
        ctime_r( &stat_info.st_mtime, tmp );
        mvar_AppendNewVar( f, "date", tmp );
#else
        mvar_AppendNewVar( f, "date", ctime( &stat_info.st_mtime ) );
#endif

#else
        mvar_AppendNewVar( f, "type", "unknown" );
        mvar_AppendNewVar( f, "size", "unknown" );
        mvar_AppendNewVar( f, "date", "unknown" );
#endif
        mvar_AppendVar( s, f );
    }

    return s;
}

static mvar_t *mvar_VlmSetNew( char *name, vlm_t *vlm )
{
    mvar_t        *s = mvar_New( name, "set" );
    vlm_message_t *msg;
    int    i;

    if( vlm == NULL ) return s;

    if( vlm_ExecuteCommand( vlm, "show", &msg ) )
    {
        return s;
    }

    for( i = 0; i < msg->i_child; i++ )
    {
        /* Over media, schedule */
        vlm_message_t *ch = msg->child[i];
        int j;

        for( j = 0; j < ch->i_child; j++ )
        {
            /* Over name */
            vlm_message_t *el = ch->child[j];
            vlm_message_t *inf, *desc;
            mvar_t        *set;
            char          psz[500];
            int k;

            sprintf( psz, "show %s", el->psz_name );
            if( vlm_ExecuteCommand( vlm, psz, &inf ) )
                continue;
            desc = inf->child[0];

            /* Add a node with name and info */
            set = mvar_New( name, "set" );
            mvar_AppendNewVar( set, "name", el->psz_name );

            for( k = 0; k < desc->i_child; k++ )
            {
                vlm_message_t *ch = desc->child[k];
                if( ch->i_child > 0 )
                {
                    int c;
                    mvar_t *n = mvar_New( ch->psz_name, "set" );

                    for( c = 0; c < ch->i_child; c++ )
                    {
                        if( ch->child[c]->psz_value )
                        {
                            mvar_AppendNewVar( n, ch->child[c]->psz_name, ch->child[c]->psz_value );
                        }
                        else
                        {
                            mvar_t *in = mvar_New( ch->psz_name, ch->child[c]->psz_name );
                            mvar_AppendVar( n, in );
                        }
                    }
                    mvar_AppendVar( set, n );
                }
                else
                {
                    mvar_AppendNewVar( set, ch->psz_name, ch->psz_value );
                }
            }
            vlm_MessageDelete( inf );

            mvar_AppendVar( s, set );
        }
    }
    vlm_MessageDelete( msg );

    return s;
}


static void SSInit( rpn_stack_t * );
static void SSClean( rpn_stack_t * );
static void EvaluateRPN( intf_thread_t *p_intf, mvar_t  *, rpn_stack_t *,
                         char * );

static void SSPush  ( rpn_stack_t *, char * );
static char *SSPop  ( rpn_stack_t * );

static void SSPushN ( rpn_stack_t *, int );
static int  SSPopN  ( rpn_stack_t *, mvar_t  * );


/****************************************************************************
 * Macro handling
 ****************************************************************************/
typedef struct
{
    char *id;
    char *param1;
    char *param2;
} macro_t;

static int FileLoad( FILE *f, char **pp_data, int *pi_data )
{
    int i_read;

    /* just load the file */
    *pi_data = 0;
    *pp_data = malloc( 1025 );  /* +1 for \0 */
    while( ( i_read = fread( &(*pp_data)[*pi_data], 1, 1024, f ) ) == 1024 )
    {
        *pi_data += 1024;
        *pp_data = realloc( *pp_data, *pi_data  + 1025 );
    }
    if( i_read > 0 )
    {
        *pi_data += i_read;
    }
    (*pp_data)[*pi_data] = '\0';

    return VLC_SUCCESS;
}

static int MacroParse( macro_t *m, char *psz_src )
{
    char *dup = strdup( (char *)psz_src );
    char *src = dup;
    char *p;
    int     i_skip;

#define EXTRACT( name, l ) \
        src += l;    \
        p = strchr( src, '"' );             \
        if( p )                             \
        {                                   \
            *p++ = '\0';                    \
        }                                   \
        m->name = strdup( src );            \
        if( !p )                            \
        {                                   \
            break;                          \
        }                                   \
        src = p;

    /* init m */
    m->id = NULL;
    m->param1 = NULL;
    m->param2 = NULL;

    /* parse */
    src += 4;

    while( *src )
    {
        while( *src == ' ')
        {
            src++;
        }
        if( !strncmp( src, "id=\"", 4 ) )
        {
            EXTRACT( id, 4 );
        }
        else if( !strncmp( src, "param1=\"", 8 ) )
        {
            EXTRACT( param1, 8 );
        }
        else if( !strncmp( src, "param2=\"", 8 ) )
        {
            EXTRACT( param2, 8 );
        }
        else
        {
            break;
        }
    }
    if( strstr( src, "/>" ) )
    {
        src = strstr( src, "/>" ) + 2;
    }
    else
    {
        src += strlen( src );
    }

    if( m->id == NULL )
    {
        m->id = strdup( "" );
    }
    if( m->param1 == NULL )
    {
        m->param1 = strdup( "" );
    }
    if( m->param2 == NULL )
    {
        m->param2 = strdup( "" );
    }
    i_skip = src - dup;

    free( dup );
    return i_skip;
#undef EXTRACT
}

static void MacroClean( macro_t *m )
{
    free( m->id );
    free( m->param1 );
    free( m->param2 );
}

enum macroType
{
    MVLC_UNKNOWN = 0,
    MVLC_CONTROL,
        MVLC_PLAY,
        MVLC_STOP,
        MVLC_PAUSE,
        MVLC_NEXT,
        MVLC_PREVIOUS,
        MVLC_ADD,
        MVLC_DEL,
        MVLC_EMPTY,
        MVLC_SEEK,
        MVLC_KEEP,
        MVLC_SORT,
        MVLC_MOVE,
        MVLC_VOLUME,
        MVLC_FULLSCREEN,

        MVLC_CLOSE,
        MVLC_SHUTDOWN,

        MVLC_VLM_NEW,
        MVLC_VLM_SETUP,
        MVLC_VLM_DEL,
        MVLC_VLM_PLAY,
        MVLC_VLM_PAUSE,
        MVLC_VLM_STOP,
        MVLC_VLM_SEEK,
        MVLC_VLM_LOAD,
        MVLC_VLM_SAVE,

    MVLC_FOREACH,
    MVLC_IF,
    MVLC_RPN,
    MVLC_STACK,
    MVLC_ELSE,
    MVLC_END,
    MVLC_GET,
    MVLC_SET,
        MVLC_INT,
        MVLC_FLOAT,
        MVLC_STRING,

    MVLC_VALUE
};

static struct
{
    char *psz_name;
    int  i_type;
}
StrToMacroTypeTab [] =
{
    { "control",    MVLC_CONTROL },
        /* player control */
        { "play",           MVLC_PLAY },
        { "stop",           MVLC_STOP },
        { "pause",          MVLC_PAUSE },
        { "next",           MVLC_NEXT },
        { "previous",       MVLC_PREVIOUS },
        { "seek",           MVLC_SEEK },
        { "keep",           MVLC_KEEP },
        { "fullscreen",     MVLC_FULLSCREEN },
        { "volume",         MVLC_VOLUME },

        /* playlist management */
        { "add",            MVLC_ADD },
        { "delete",         MVLC_DEL },
        { "empty",          MVLC_EMPTY },
        { "sort",           MVLC_SORT },
        { "move",           MVLC_MOVE },

        /* admin control */
        { "close",          MVLC_CLOSE },
        { "shutdown",       MVLC_SHUTDOWN },

        /* vlm control */
        { "vlm_new",        MVLC_VLM_NEW },
        { "vlm_setup",      MVLC_VLM_SETUP },
        { "vlm_del",        MVLC_VLM_DEL },
        { "vlm_play",       MVLC_VLM_PLAY },
        { "vlm_pause",      MVLC_VLM_PAUSE },
        { "vlm_stop",       MVLC_VLM_STOP },
        { "vlm_seek",       MVLC_VLM_SEEK },
        { "vlm_load",       MVLC_VLM_LOAD },
        { "vlm_save",       MVLC_VLM_SAVE },

    { "rpn",        MVLC_RPN },
    { "stack",        MVLC_STACK },

    { "foreach",    MVLC_FOREACH },
    { "value",      MVLC_VALUE },

    { "if",         MVLC_IF },
    { "else",       MVLC_ELSE },
    { "end",        MVLC_END },
    { "get",         MVLC_GET },
    { "set",         MVLC_SET },
        { "int",            MVLC_INT },
        { "float",          MVLC_FLOAT },
        { "string",         MVLC_STRING },

    /* end */
    { NULL,         MVLC_UNKNOWN }
};

static int StrToMacroType( char *name )
{
    int i;

    if( !name || *name == '\0')
    {
        return MVLC_UNKNOWN;
    }
    for( i = 0; StrToMacroTypeTab[i].psz_name != NULL; i++ )
    {
        if( !strcmp( name, StrToMacroTypeTab[i].psz_name ) )
        {
            return StrToMacroTypeTab[i].i_type;
        }
    }
    return MVLC_UNKNOWN;
}

static void MacroDo( httpd_file_sys_t *p_args,
                     macro_t *m,
                     char *p_request, int i_request,
                     char **pp_data,  int *pi_data,
                     char **pp_dst )
{
    intf_thread_t  *p_intf = p_args->p_intf;
    intf_sys_t     *p_sys = p_args->p_intf->p_sys;
    char control[512];

#define ALLOC( l ) \
    {               \
        int __i__ = *pp_dst - *pp_data; \
        *pi_data += (l);                  \
        *pp_data = realloc( *pp_data, *pi_data );   \
        *pp_dst = (*pp_data) + __i__;   \
    }
#define PRINT( str ) \
    ALLOC( strlen( str ) + 1 ); \
    *pp_dst += sprintf( *pp_dst, str );

#define PRINTS( str, s ) \
    ALLOC( strlen( str ) + strlen( s ) + 1 ); \
    { \
        char * psz_cur = *pp_dst; \
        *pp_dst += sprintf( *pp_dst, str, s ); \
        while( psz_cur && *psz_cur ) \
        {  \
            /* Prevent script injection */ \
            if( *psz_cur == '<' ) *psz_cur = '*'; \
            if( *psz_cur == '>' ) *psz_cur = '*'; \
            psz_cur++ ; \
        } \
    }

    switch( StrToMacroType( m->id ) )
    {
        case MVLC_CONTROL:
            if( i_request <= 0 )
            {
                break;
            }
            uri_extract_value( p_request, "control", control, 512 );
            if( *m->param1 && !strstr( m->param1, control ) )
            {
                msg_Warn( p_intf, "unauthorized control=%s", control );
                break;
            }
            switch( StrToMacroType( control ) )
            {
                case MVLC_PLAY:
                {
                    int i_item;
                    char item[512];

                    uri_extract_value( p_request, "item", item, 512 );
                    i_item = atoi( item );
                    playlist_Control( p_sys->p_playlist, PLAYLIST_ITEMPLAY,
                                      playlist_ItemGetById( p_sys->p_playlist,
                                      i_item ) );
                    msg_Dbg( p_intf, "requested playlist item: %i", i_item );
                    break;
                }
                case MVLC_STOP:
                    playlist_Control( p_sys->p_playlist, PLAYLIST_STOP);
                    msg_Dbg( p_intf, "requested playlist stop" );
                    break;
                case MVLC_PAUSE:
                    playlist_Control( p_sys->p_playlist, PLAYLIST_PAUSE );
                    msg_Dbg( p_intf, "requested playlist pause" );
                    break;
                case MVLC_NEXT:
                    playlist_Control( p_sys->p_playlist, PLAYLIST_SKIP, 1 );
                    msg_Dbg( p_intf, "requested playlist next" );
                    break;
                case MVLC_PREVIOUS:
                    playlist_Control( p_sys->p_playlist, PLAYLIST_SKIP, -1);
                    msg_Dbg( p_intf, "requested playlist next" );
                    break;
                case MVLC_FULLSCREEN:
                {
                    if( p_sys->p_input )
                    {
                        vout_thread_t *p_vout;
                        p_vout = vlc_object_find( p_sys->p_input,
                                                  VLC_OBJECT_VOUT, FIND_CHILD );

                        if( p_vout )
                        {
                            p_vout->i_changes |= VOUT_FULLSCREEN_CHANGE;
                            vlc_object_release( p_vout );
                            msg_Dbg( p_intf, "requested fullscreen toggle" );
                        }
                    }
                }
                break;
                case MVLC_SEEK:
                {
                    vlc_value_t val;
                    char value[30];
                    char * p_value;
                    int i_stock = 0;
                    uint64_t i_length;
                    int i_value = 0;
                    int i_relative = 0;
#define POSITION_ABSOLUTE 12
#define POSITION_REL_FOR 13
#define POSITION_REL_BACK 11
#define VL_TIME_ABSOLUTE 0
#define VL_TIME_REL_FOR 1
#define VL_TIME_REL_BACK -1
                    if( p_sys->p_input )
                    {
                        uri_extract_value( p_request, "seek_value", value, 20 );
                        uri_decode_url_encoded( value );
                        p_value = value;
                        var_Get( p_sys->p_input, "length", &val);
                        i_length = val.i_time;

                        while( p_value[0] != '\0' )
                        {
                            switch(p_value[0])
                            {
                                case '+':
                                {
                                    i_relative = VL_TIME_REL_FOR;
                                    p_value++;
                                    break;
                                }
                                case '-':
                                {
                                    i_relative = VL_TIME_REL_BACK;
                                    p_value++;
                                    break;
                                }
                                case '0': case '1': case '2': case '3': case '4':
                                case '5': case '6': case '7': case '8': case '9':
                                {
                                    i_stock = strtol( p_value , &p_value , 10 );
                                    break;
                                }
                                case '%': /* for percentage ie position */
                                {
                                    i_relative += POSITION_ABSOLUTE;
                                    i_value = i_stock;
                                    i_stock = 0;
                                    p_value[0] = '\0';
                                    break;
                                }
                                case ':':
                                {
                                    i_value = 60 * (i_value + i_stock) ;
                                    i_stock = 0;
                                    p_value++;
                                    break;
                                }
                                case 'h': case 'H': /* hours */
                                {
                                    i_value += 3600 * i_stock;
                                    i_stock = 0;
                                    /* other characters which are not numbers are not important */
                                    while( ((p_value[0] < '0') || (p_value[0] > '9')) && (p_value[0] != '\0') )
                                    {
                                        p_value++;
                                    }
                                    break;
                                }
                                case 'm': case 'M': case '\'': /* minutes */
                                {
                                    i_value += 60 * i_stock;
                                    i_stock = 0;
                                    p_value++;
                                    while( ((p_value[0] < '0') || (p_value[0] > '9')) && (p_value[0] != '\0') )
                                    {
                                        p_value++;
                                    }
                                    break;
                                }
                                case 's': case 'S': case '"':  /* seconds */
                                {
                                    i_value += i_stock;
                                    i_stock = 0;
                                    while( ((p_value[0] < '0') || (p_value[0] > '9')) && (p_value[0] != '\0') )
                                    {
                                        p_value++;
                                    }
                                    break;
                                }
                                default:
                                {
                                    p_value++;
                                    break;
                                }
                            }
                        }

                        /* if there is no known symbol, I consider it as seconds. Otherwise, i_stock = 0 */
                        i_value += i_stock;

                        switch(i_relative)
                        {
                            case VL_TIME_ABSOLUTE:
                            {
                                if( (uint64_t)( i_value ) * 1000000 <= i_length )
                                    val.i_time = (uint64_t)( i_value ) * 1000000;
                                else
                                    val.i_time = i_length;

                                var_Set( p_sys->p_input, "time", val );
                                msg_Dbg( p_intf, "requested seek position: %dsec", i_value );
                                break;
                            }
                            case VL_TIME_REL_FOR:
                            {
                                var_Get( p_sys->p_input, "time", &val );
                                if( (uint64_t)( i_value ) * 1000000 + val.i_time <= i_length )
                                {
                                    val.i_time = ((uint64_t)( i_value ) * 1000000) + val.i_time;
                                } else
                                {
                                    val.i_time = i_length;
                                }
                                var_Set( p_sys->p_input, "time", val );
                                msg_Dbg( p_intf, "requested seek position forward: %dsec", i_value );
                                break;
                            }
                            case VL_TIME_REL_BACK:
                            {
                                var_Get( p_sys->p_input, "time", &val );
                                if( (int64_t)( i_value ) * 1000000 > val.i_time )
                                {
                                    val.i_time = 0;
                                } else
                                {
                                    val.i_time = val.i_time - ((uint64_t)( i_value ) * 1000000);
                                }
                                var_Set( p_sys->p_input, "time", val );
                                msg_Dbg( p_intf, "requested seek position backward: %dsec", i_value );
                                break;
                            }
                            case POSITION_ABSOLUTE:
                            {
                                val.f_float = __MIN( __MAX( ((float) i_value ) / 100.0 , 0.0 ) , 100.0 );
                                var_Set( p_sys->p_input, "position", val );
                                msg_Dbg( p_intf, "requested seek percent: %d", i_value );
                                break;
                            }
                            case POSITION_REL_FOR:
                            {
                                var_Get( p_sys->p_input, "position", &val );
                                val.f_float += __MIN( __MAX( ((float) i_value ) / 100.0 , 0.0 ) , 100.0 );
                                var_Set( p_sys->p_input, "position", val );
                                msg_Dbg( p_intf, "requested seek percent forward: %d", i_value );
                                break;
                            }
                            case POSITION_REL_BACK:
                            {
                                var_Get( p_sys->p_input, "position", &val );
                                val.f_float -= __MIN( __MAX( ((float) i_value ) / 100.0 , 0.0 ) , 100.0 );
                                var_Set( p_sys->p_input, "position", val );
                                msg_Dbg( p_intf, "requested seek percent backward: %d", i_value );
                                break;
                            }
                            default:
                            {
                                msg_Dbg( p_intf, "requested seek: what the f*** is going on here ?" );
                                break;
                            }
                        }
                    }
#undef POSITION_ABSOLUTE
#undef POSITION_REL_FOR
#undef POSITION_REL_BACK
#undef VL_TIME_ABSOLUTE
#undef VL_TIME_REL_FOR
#undef VL_TIME_REL_BACK
                    break;
                }
                case MVLC_VOLUME:
                {
                    char vol[8];
                    audio_volume_t i_volume;
                    int i_value;

                    uri_extract_value( p_request, "value", vol, 8 );
                    aout_VolumeGet( p_intf, &i_volume );
                    uri_decode_url_encoded( vol );

                    if( vol[0] == '+' )
                    {
                        i_value = atoi( vol + 1 );
                        if( (i_volume + i_value) > AOUT_VOLUME_MAX )
                        {
                            aout_VolumeSet( p_intf , AOUT_VOLUME_MAX );
                            msg_Dbg( p_intf, "requested volume set: max" );
                        } else
                        {
                            aout_VolumeSet( p_intf , (i_volume + i_value) );
                            msg_Dbg( p_intf, "requested volume set: +%i", (i_volume + i_value) );
                        }
                    } else
                    if( vol[0] == '-' )
                    {
                        i_value = atoi( vol + 1 );
                        if( (i_volume - i_value) < AOUT_VOLUME_MIN )
                        {
                            aout_VolumeSet( p_intf , AOUT_VOLUME_MIN );
                            msg_Dbg( p_intf, "requested volume set: min" );
                        } else
                        {
                            aout_VolumeSet( p_intf , (i_volume - i_value) );
                            msg_Dbg( p_intf, "requested volume set: -%i", (i_volume - i_value) );
                        }
                    } else
                    if( strstr(vol, "%") != NULL )
                    {
                        i_value = atoi( vol );
                        if( (i_value <= 400) && (i_value>=0) ){
                            aout_VolumeSet( p_intf, (i_value * (AOUT_VOLUME_MAX - AOUT_VOLUME_MIN))/400+AOUT_VOLUME_MIN);
                            msg_Dbg( p_intf, "requested volume set: %i%%", atoi( vol ));
                        }
                    } else
                    {
                        i_value = atoi( vol );
                        if( ( i_value <= AOUT_VOLUME_MAX ) && ( i_value >= AOUT_VOLUME_MIN ) )
                        {
                            aout_VolumeSet( p_intf , atoi( vol ) );
                            msg_Dbg( p_intf, "requested volume set: %i", atoi( vol ) );
                        }
                    }
                    break;
                }

                /* playlist management */
                case MVLC_ADD:
                {
                    char mrl[1024], psz_name[1024];
                    playlist_item_t *p_item;

                    uri_extract_value( p_request, "mrl", mrl, 1024 );
                    uri_decode_url_encoded( mrl );
                    uri_extract_value( p_request, "name", psz_name, 1024 );
                    uri_decode_url_encoded( psz_name );
                    if( !*psz_name )
                    {
                        memcpy( psz_name, mrl, 1024 );
                    }
                    p_item = parse_MRL( p_intf, mrl, psz_name );

                    if( !p_item || !p_item->input.psz_uri ||
                        !*p_item->input.psz_uri )
                    {
                        msg_Dbg( p_intf, "invalid requested mrl: %s", mrl );
                    }
                    else
                    {
                        playlist_AddItem( p_sys->p_playlist, p_item,
                                          PLAYLIST_APPEND, PLAYLIST_END );
                        msg_Dbg( p_intf, "requested mrl add: %s", mrl );
                    }

                    break;
                }
                case MVLC_DEL:
                {
                    int i_item, *p_items = NULL, i_nb_items = 0;
                    char item[512], *p_parser = p_request;

                    /* Get the list of items to delete */
                    while( (p_parser =
                            uri_extract_value( p_parser, "item", item, 512 )) )
                    {
                        if( !*item ) continue;

                        i_item = atoi( item );
                        p_items = realloc( p_items, (i_nb_items + 1) *
                                           sizeof(int) );
                        p_items[i_nb_items] = i_item;
                        i_nb_items++;
                    }

                    if( i_nb_items )
                    {
                        int i;
                        for( i = 0; i < i_nb_items; i++ )
                        {
                            playlist_LockDelete( p_sys->p_playlist, p_items[i] );
                            msg_Dbg( p_intf, "requested playlist delete: %d",
                                     p_items[i] );
                            p_items[i] = -1;
                        }
                    }

                    if( p_items ) free( p_items );
                    break;
                }
                case MVLC_KEEP:
                {
                    int i_item, *p_items = NULL, i_nb_items = 0;
                    char item[512], *p_parser = p_request;
                    int i,j;

                    /* Get the list of items to keep */
                    while( (p_parser =
                            uri_extract_value( p_parser, "item", item, 512 )) )
                    {
                        if( !*item ) continue;

                        i_item = atoi( item );
                        p_items = realloc( p_items, (i_nb_items + 1) *
                                           sizeof(int) );
                        p_items[i_nb_items] = i_item;
                        i_nb_items++;
                    }

                    for( i = p_sys->p_playlist->i_size - 1 ; i >= 0; i-- )
                    {
                        /* Check if the item is in the keep list */
                        for( j = 0 ; j < i_nb_items ; j++ )
                        {
                            if( p_items[j] ==
                                p_sys->p_playlist->pp_items[i]->input.i_id ) break;
                        }
                        if( j == i_nb_items )
                        {
                            playlist_LockDelete( p_sys->p_playlist, p_sys->p_playlist->pp_items[i]->input.i_id );
                            msg_Dbg( p_intf, "requested playlist delete: %d",
                                     i );
                        }
                    }

                    if( p_items ) free( p_items );
                    break;
                }
                case MVLC_EMPTY:
                {
                    playlist_LockClear( p_sys->p_playlist );
                    msg_Dbg( p_intf, "requested playlist empty" );
                    break;
                }
                case MVLC_SORT:
                {
                    char type[12];
                    char order[2];
                    char item[512];
                    int i_order;
                    int i_item;

                    uri_extract_value( p_request, "type", type, 12 );
                    uri_extract_value( p_request, "order", order, 2 );
                    uri_extract_value( p_request, "item", item, 512 );
                    i_item = atoi( item );

                    if( order[0] == '0' ) i_order = ORDER_NORMAL;
                    else i_order = ORDER_REVERSE;

                    if( !strcmp( type , "title" ) )
                    {
                        playlist_RecursiveNodeSort( p_sys->p_playlist, /*playlist_ItemGetById( p_sys->p_playlist, i_item ),*/
                                                    p_sys->p_playlist->pp_views[0]->p_root,
                                                    SORT_TITLE_NODES_FIRST,
                                                    ( i_order == 0 ) ? ORDER_NORMAL : ORDER_REVERSE );
                        msg_Dbg( p_intf, "requested playlist sort by title (%d)" , i_order );
                    } else if( !strcmp( type , "author" ) )
                    {
                        playlist_RecursiveNodeSort( p_sys->p_playlist, /*playlist_ItemGetById( p_sys->p_playlist, i_item ),*/
                                                    p_sys->p_playlist->pp_views[0]->p_root,
                                                    SORT_AUTHOR,
                                                    ( i_order == 0 ) ? ORDER_NORMAL : ORDER_REVERSE );
                        msg_Dbg( p_intf, "requested playlist sort by author (%d)" , i_order );
                    } else if( !strcmp( type , "shuffle" ) )
                    {
                        playlist_RecursiveNodeSort( p_sys->p_playlist, /*playlist_ItemGetById( p_sys->p_playlist, i_item ),*/
                                                    p_sys->p_playlist->pp_views[0]->p_root,
                                                    SORT_RANDOM,
                                                    ( i_order == 0 ) ? ORDER_NORMAL : ORDER_REVERSE );
                        msg_Dbg( p_intf, "requested playlist shuffle");
                    }

                    break;
                }
                case MVLC_MOVE:
                {
                    char psz_pos[6];
                    char psz_newpos[6];
                    int i_pos;
                    int i_newpos;
                    uri_extract_value( p_request, "psz_pos", psz_pos, 6 );
                    uri_extract_value( p_request, "psz_newpos", psz_newpos, 6 );
                    i_pos = atoi( psz_pos );
                    i_newpos = atoi( psz_newpos );
                    if ( i_pos < i_newpos )
                    {
                        playlist_Move( p_sys->p_playlist, i_pos, i_newpos + 1 );
                    } else {
                        playlist_Move( p_sys->p_playlist, i_pos, i_newpos );
                    }
                    msg_Dbg( p_intf, "requested move playlist item %d to %d", i_pos, i_newpos);
                    break;
                }

                /* admin function */
                case MVLC_CLOSE:
                {
                    char id[512];
                    uri_extract_value( p_request, "id", id, 512 );
                    msg_Dbg( p_intf, "requested close id=%s", id );
#if 0
                    if( p_sys->p_httpd->pf_control( p_sys->p_httpd, HTTPD_SET_CLOSE, id, NULL ) )
                    {
                        msg_Warn( p_intf, "close failed for id=%s", id );
                    }
#endif
                    break;
                }
                case MVLC_SHUTDOWN:
                {
                    msg_Dbg( p_intf, "requested shutdown" );
                    p_intf->p_vlc->b_die = VLC_TRUE;
                    break;
                }
                /* vlm */
                case MVLC_VLM_NEW:
                case MVLC_VLM_SETUP:
                {
                    static const char *vlm_properties[11] =
                    {
                        /* no args */
                        "enabled", "disabled", "loop", "unloop",
                        /* args required */
                        "input", "output", "option", "date", "period", "repeat", "append",
                    };
                    vlm_message_t *vlm_answer;
                    char name[512];
                    char *psz = malloc( strlen( p_request ) + 1000 );
                    char *p = psz;
                    char *vlm_error;
                    int i;

                    if( p_intf->p_sys->p_vlm == NULL )
                        p_intf->p_sys->p_vlm = vlm_New( p_intf );

                    if( p_intf->p_sys->p_vlm == NULL ) break;

                    uri_extract_value( p_request, "name", name, 512 );
                    if( StrToMacroType( control ) == MVLC_VLM_NEW )
                    {
                        char type[20];
                        uri_extract_value( p_request, "type", type, 20 );
                        p += sprintf( psz, "new %s %s", name, type );
                    }
                    else
                    {
                        p += sprintf( psz, "setup %s", name );
                    }
                    /* Parse the request */
                    for( i = 0; i < 11; i++ )
                    {
                        char val[512];
                        uri_extract_value( p_request, vlm_properties[i], val, 512 );
                        uri_decode_url_encoded( val );
                        if( strlen( val ) > 0 && i >= 4 )
                        {
                            p += sprintf( p, " %s %s", vlm_properties[i], val );
                        }
                        else if( uri_test_param( p_request, vlm_properties[i] ) && i < 4 )
                        {
                            p += sprintf( p, " %s", vlm_properties[i] );
                        }
                    }
                    vlm_ExecuteCommand( p_intf->p_sys->p_vlm, psz, &vlm_answer );
                    if( vlm_answer->psz_value == NULL ) /* there is no error */
                    {
                        vlm_error = strdup( "" );
                    }
                    else
                    {
                        vlm_error = malloc( strlen(vlm_answer->psz_name) +
                                            strlen(vlm_answer->psz_value) +
                                            strlen( " : ") + 1 );
                        sprintf( vlm_error , "%s : %s" , vlm_answer->psz_name,
                                                         vlm_answer->psz_value );
                    }

                    mvar_AppendNewVar( p_args->vars, "vlm_error", vlm_error );

                    vlm_MessageDelete( vlm_answer );
                    free( vlm_error );
                    free( psz );
                    break;
                }

                case MVLC_VLM_DEL:
                {
                    vlm_message_t *vlm_answer;
                    char name[512];
                    char psz[512+10];
                    if( p_intf->p_sys->p_vlm == NULL )
                        p_intf->p_sys->p_vlm = vlm_New( p_intf );

                    if( p_intf->p_sys->p_vlm == NULL ) break;

                    uri_extract_value( p_request, "name", name, 512 );
                    sprintf( psz, "del %s", name );

                    vlm_ExecuteCommand( p_intf->p_sys->p_vlm, psz, &vlm_answer );
                    /* FIXME do a vlm_answer -> var stack conversion */
                    vlm_MessageDelete( vlm_answer );
                    break;
                }

                case MVLC_VLM_PLAY:
                case MVLC_VLM_PAUSE:
                case MVLC_VLM_STOP:
                case MVLC_VLM_SEEK:
                {
                    vlm_message_t *vlm_answer;
                    char name[512];
                    char psz[512+10];
                    if( p_intf->p_sys->p_vlm == NULL )
                        p_intf->p_sys->p_vlm = vlm_New( p_intf );

                    if( p_intf->p_sys->p_vlm == NULL ) break;

                    uri_extract_value( p_request, "name", name, 512 );
                    if( StrToMacroType( control ) == MVLC_VLM_PLAY )
                        sprintf( psz, "control %s play", name );
                    else if( StrToMacroType( control ) == MVLC_VLM_PAUSE )
                        sprintf( psz, "control %s pause", name );
                    else if( StrToMacroType( control ) == MVLC_VLM_STOP )
                        sprintf( psz, "control %s stop", name );
                    else if( StrToMacroType( control ) == MVLC_VLM_SEEK )
                    {
                        char percent[20];
                        uri_extract_value( p_request, "percent", percent, 512 );
                        sprintf( psz, "control %s seek %s", name, percent );
                    }

                    vlm_ExecuteCommand( p_intf->p_sys->p_vlm, psz, &vlm_answer );
                    /* FIXME do a vlm_answer -> var stack conversion */
                    vlm_MessageDelete( vlm_answer );
                    break;
                }
                case MVLC_VLM_LOAD:
                case MVLC_VLM_SAVE:
                {
                    vlm_message_t *vlm_answer;
                    char file[512];
                    char psz[512];

                    if( p_intf->p_sys->p_vlm == NULL )
                        p_intf->p_sys->p_vlm = vlm_New( p_intf );

                    if( p_intf->p_sys->p_vlm == NULL ) break;

                    uri_extract_value( p_request, "file", file, 512 );
                    uri_decode_url_encoded( file );

                    if( StrToMacroType( control ) == MVLC_VLM_LOAD )
                        sprintf( psz, "load %s", file );
                    else
                        sprintf( psz, "save %s", file );

                    vlm_ExecuteCommand( p_intf->p_sys->p_vlm, psz, &vlm_answer );
                    /* FIXME do a vlm_answer -> var stack conversion */
                    vlm_MessageDelete( vlm_answer );
                    break;
                }

                default:
                    if( *control )
                    {
                        PRINTS( "<!-- control param(%s) unsupported -->", control );
                    }
                    break;
            }
            break;

        case MVLC_SET:
        {
            char    value[512];
            int     i;
            float   f;

            if( i_request <= 0 ||
                *m->param1  == '\0' ||
                strstr( p_request, m->param1 ) == NULL )
            {
                break;
            }
            uri_extract_value( p_request, m->param1,  value, 512 );
            uri_decode_url_encoded( value );

            switch( StrToMacroType( m->param2 ) )
            {
                case MVLC_INT:
                    i = atoi( value );
                    config_PutInt( p_intf, m->param1, i );
                    break;
                case MVLC_FLOAT:
                    f = atof( value );
                    config_PutFloat( p_intf, m->param1, f );
                    break;
                case MVLC_STRING:
                    config_PutPsz( p_intf, m->param1, value );
                    break;
                default:
                    PRINTS( "<!-- invalid type(%s) in set -->", m->param2 )
            }
            break;
        }
        case MVLC_GET:
        {
            char    value[512];
            int     i;
            float   f;
            char    *psz;

            if( *m->param1  == '\0' )
            {
                break;
            }

            switch( StrToMacroType( m->param2 ) )
            {
                case MVLC_INT:
                    i = config_GetInt( p_intf, m->param1 );
                    sprintf( value, "%d", i );
                    break;
                case MVLC_FLOAT:
                    f = config_GetFloat( p_intf, m->param1 );
                    sprintf( value, "%f", f );
                    break;
                case MVLC_STRING:
                    psz = config_GetPsz( p_intf, m->param1 );
                    if( psz != NULL )
                    {
                        strncpy( value, psz,sizeof( value ) );
                        free( psz );
                        value[sizeof( value ) - 1] = '\0';
                    }
                    else
                        *value = '\0';
                    msg_Dbg( p_intf, "%d: value = \"%s\"", __LINE__, value );
                    break;
                default:
                    snprintf( value, sizeof( value ),
                              "invalid type(%s) in set", m->param2 );
                    value[sizeof( value ) - 1] = '\0';
                    break;
            }
            PRINTS( "%s", value );
            break;
        }
        case MVLC_VALUE:
        {
            char *s, *v;

            if( m->param1 )
            {
                EvaluateRPN( p_intf, p_args->vars, &p_args->stack, m->param1 );
                s = SSPop( &p_args->stack );
                v = mvar_GetValue( p_args->vars, s );
            }
            else
            {
                v = s = SSPop( &p_args->stack );
            }

            PRINTS( "%s", v );
            free( s );
            break;
        }
        case MVLC_RPN:
            EvaluateRPN( p_intf, p_args->vars, &p_args->stack, m->param1 );
            break;

/* Usefull for learning stack management */
        case MVLC_STACK:
        {
            int i;
            msg_Dbg( p_intf, "stack" );
            for (i=0;i<(&p_args->stack)->i_stack;i++)
                msg_Dbg( p_intf, "%d -> %s", i, (&p_args->stack)->stack[i] );
            break;
        }

        case MVLC_UNKNOWN:
        default:
            PRINTS( "<!-- invalid macro id=`%s' -->", m->id );
            msg_Dbg( p_intf, "invalid macro id=`%s'", m->id );
            break;
    }
#undef PRINTS
#undef PRINT
#undef ALLOC
}

static char *MacroSearch( char *src, char *end, int i_mvlc, vlc_bool_t b_after )
{
    int     i_id;
    int     i_level = 0;

    while( src < end )
    {
        if( src + 4 < end  && !strncmp( (char *)src, "<vlc", 4 ) )
        {
            int i_skip;
            macro_t m;

            i_skip = MacroParse( &m, src );

            i_id = StrToMacroType( m.id );

            switch( i_id )
            {
                case MVLC_IF:
                case MVLC_FOREACH:
                    i_level++;
                    break;
                case MVLC_END:
                    i_level--;
                    break;
                default:
                    break;
            }

            MacroClean( &m );

            if( ( i_mvlc == MVLC_END && i_level == -1 ) ||
                ( i_mvlc != MVLC_END && i_level == 0 && i_mvlc == i_id ) )
            {
                return src + ( b_after ? i_skip : 0 );
            }
            else if( i_level < 0 )
            {
                return NULL;
            }

            src += i_skip;
        }
        else
        {
            src++;
        }
    }

    return NULL;
}

static void Execute( httpd_file_sys_t *p_args,
                     char *p_request, int i_request,
                     char **pp_data, int *pi_data,
                     char **pp_dst,
                     char *_src, char *_end )
{
    intf_thread_t  *p_intf = p_args->p_intf;

    char *src, *dup, *end;
    char *dst = *pp_dst;

    src = dup = malloc( _end - _src + 1 );
    end = src +( _end - _src );

    memcpy( src, _src, _end - _src );
    *end = '\0';

    /* we parse searching <vlc */
    while( src < end )
    {
        char *p;
        int i_copy;

        p = (char *)strstr( (char *)src, "<vlc" );
        if( p < end && p == src )
        {
            macro_t m;

            src += MacroParse( &m, src );

            //msg_Dbg( p_intf, "macro_id=%s", m.id );

            switch( StrToMacroType( m.id ) )
            {
                case MVLC_IF:
                {
                    vlc_bool_t i_test;
                    char    *endif;

                    EvaluateRPN( p_intf, p_args->vars, &p_args->stack, m.param1 );
                    if( SSPopN( &p_args->stack, p_args->vars ) )
                    {
                        i_test = 1;
                    }
                    else
                    {
                        i_test = 0;
                    }
                    endif = MacroSearch( src, end, MVLC_END, VLC_TRUE );

                    if( i_test == 0 )
                    {
                        char *start = MacroSearch( src, endif, MVLC_ELSE, VLC_TRUE );

                        if( start )
                        {
                            char *stop  = MacroSearch( start, endif, MVLC_END, VLC_FALSE );
                            if( stop )
                            {
                                Execute( p_args, p_request, i_request,
                                         pp_data, pi_data, &dst, start, stop );
                            }
                        }
                    }
                    else if( i_test == 1 )
                    {
                        char *stop;
                        if( ( stop = MacroSearch( src, endif, MVLC_ELSE, VLC_FALSE ) ) == NULL )
                        {
                            stop = MacroSearch( src, endif, MVLC_END, VLC_FALSE );
                        }
                        if( stop )
                        {
                            Execute( p_args, p_request, i_request,
                                     pp_data, pi_data, &dst, src, stop );
                        }
                    }

                    src = endif;
                    break;
                }
                case MVLC_FOREACH:
                {
                    char *endfor = MacroSearch( src, end, MVLC_END, VLC_TRUE );
                    char *start = src;
                    char *stop = MacroSearch( src, end, MVLC_END, VLC_FALSE );

                    if( stop )
                    {
                        mvar_t *index;
                        int    i_idx;
                        mvar_t *v;
                        if( !strcmp( m.param2, "integer" ) )
                        {
                            char *arg = SSPop( &p_args->stack );
                            index = mvar_IntegerSetNew( m.param1, arg );
                            free( arg );
                        }
                        else if( !strcmp( m.param2, "directory" ) )
                        {
                            char *arg = SSPop( &p_args->stack );
                            index = mvar_FileSetNew( p_intf, m.param1, arg );
                            free( arg );
                        }
                        else if( !strcmp( m.param2, "playlist" ) )
                        {
                            index = mvar_PlaylistSetNew( p_intf, m.param1,
                                                    p_intf->p_sys->p_playlist );
                        }
                        else if( !strcmp( m.param2, "information" ) )
                        {
                            index = mvar_InfoSetNew( p_intf, m.param1,
                                                     p_intf->p_sys->p_input );
                        }
                        else if( !strcmp( m.param2, "vlm" ) )
                        {
                            if( p_intf->p_sys->p_vlm == NULL )
                                p_intf->p_sys->p_vlm = vlm_New( p_intf );
                            index = mvar_VlmSetNew( m.param1, p_intf->p_sys->p_vlm );
                        }
#if 0
                        else if( !strcmp( m.param2, "hosts" ) )
                        {
                            index = mvar_HttpdInfoSetNew( m.param1, p_intf->p_sys->p_httpd, HTTPD_GET_HOSTS );
                        }
                        else if( !strcmp( m.param2, "urls" ) )
                        {
                            index = mvar_HttpdInfoSetNew( m.param1, p_intf->p_sys->p_httpd, HTTPD_GET_URLS );
                        }
                        else if( !strcmp( m.param2, "connections" ) )
                        {
                            index = mvar_HttpdInfoSetNew(m.param1, p_intf->p_sys->p_httpd, HTTPD_GET_CONNECTIONS);
                        }
#endif
                        else if( ( v = mvar_GetVar( p_args->vars, m.param2 ) ) )
                        {
                            index = mvar_Duplicate( v );
                        }
                        else
                        {
                            msg_Dbg( p_intf, "invalid index constructor (%s)", m.param2 );
                            src = endfor;
                            break;
                        }

                        for( i_idx = 0; i_idx < index->i_field; i_idx++ )
                        {
                            mvar_t *f = mvar_Duplicate( index->field[i_idx] );

                            //msg_Dbg( p_intf, "foreach field[%d] name=%s value=%s", i_idx, f->name, f->value );

                            free( f->name );
                            f->name = strdup( m.param1 );


                            mvar_PushVar( p_args->vars, f );
                            Execute( p_args, p_request, i_request,
                                     pp_data, pi_data, &dst, start, stop );
                            mvar_RemoveVar( p_args->vars, f );

                            mvar_Delete( f );
                        }
                        mvar_Delete( index );

                        src = endfor;
                    }
                    break;
                }
                default:
                    MacroDo( p_args, &m, p_request, i_request,
                             pp_data, pi_data, &dst );
                    break;
            }

            MacroClean( &m );
            continue;
        }

        i_copy =   ( (p == NULL || p > end ) ? end : p  ) - src;
        if( i_copy > 0 )
        {
            int i_index = dst - *pp_data;

            *pi_data += i_copy;
            *pp_data = realloc( *pp_data, *pi_data );
            dst = (*pp_data) + i_index;

            memcpy( dst, src, i_copy );
            dst += i_copy;
            src += i_copy;
        }
    }

    *pp_dst = dst;
    free( dup );
}

/****************************************************************************
 * HttpCallback:
 ****************************************************************************
 * a file with b_html is parsed and all "macro" replaced
 * <vlc id="macro name" [param1="" [param2=""]] />
 * valid id are
 *
 ****************************************************************************/
static int  HttpCallback( httpd_file_sys_t *p_args,
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
        FileLoad( f, pp_data, pi_data );
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
            } else if( val.i_int == PAUSE_S )
            {
                sprintf( state, "paused" );
            } else
            {
                sprintf( state, "stop" );
            }
        } else
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
        mvar_AppendNewVar( p_args->vars, "url_param", i_request > 0 ? "1" : "0" );
        mvar_AppendNewVar( p_args->vars, "url_value", p_request );
        mvar_AppendNewVar( p_args->vars, "version",   VERSION_MESSAGE );
        mvar_AppendNewVar( p_args->vars, "copyright", COPYRIGHT_MESSAGE );
        mvar_AppendNewVar( p_args->vars, "stream_position", position );
        mvar_AppendNewVar( p_args->vars, "stream_time", time );
        mvar_AppendNewVar( p_args->vars, "stream_length", length );
        mvar_AppendNewVar( p_args->vars, "volume", volume );
        mvar_AppendNewVar( p_args->vars, "stream_state", state );

        SSInit( &p_args->stack );

        /* first we load in a temporary buffer */
        FileLoad( f, &p_buffer, &i_buffer );

        /* allocate output */
        *pi_data = i_buffer + 1000;
        dst = *pp_data = malloc( *pi_data );

        /* we parse executing all  <vlc /> macros */
        Execute( p_args, p_request, i_request, pp_data, pi_data, &dst,
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

/****************************************************************************
 * uri parser
 ****************************************************************************/
static int uri_test_param( char *psz_uri, const char *psz_name )
{
    char *p = psz_uri;

    while( (p = strstr( p, psz_name )) )
    {
        /* Verify that we are dealing with a post/get argument */
        if( p == psz_uri || *(p - 1) == '&' || *(p - 1) == '\n' )
        {
            return VLC_TRUE;
        }
        p++;
    }

    return VLC_FALSE;
}
static char *uri_extract_value( char *psz_uri, const char *psz_name,
                                char *psz_value, int i_value_max )
{
    char *p = psz_uri;

    while( (p = strstr( p, psz_name )) )
    {
        /* Verify that we are dealing with a post/get argument */
        if( p == psz_uri || *(p - 1) == '&' || *(p - 1) == '\n' )
            break;
        p++;
    }

    if( p )
    {
        int i_len;

        p += strlen( psz_name );
        if( *p == '=' ) p++;

        if( strchr( p, '&' ) )
        {
            i_len = strchr( p, '&' ) - p;
        }
        else
        {
            /* for POST method */
            if( strchr( p, '\n' ) )
            {
                i_len = strchr( p, '\n' ) - p;
                if( i_len && *(p+i_len-1) == '\r' ) i_len--;
            }
            else
            {
                i_len = strlen( p );
            }
        }
        i_len = __MIN( i_value_max - 1, i_len );
        if( i_len > 0 )
        {
            strncpy( psz_value, p, i_len );
            psz_value[i_len] = '\0';
        }
        else
        {
            strncpy( psz_value, "", i_value_max );
        }
        p += i_len;
    }
    else
    {
        strncpy( psz_value, "", i_value_max );
    }

    return p;
}

static void uri_decode_url_encoded( char *psz )
{
    char *dup = strdup( psz );
    char *p = dup;

    while( *p )
    {
        if( *p == '%' )
        {
            char val[3];
            p++;
            if( !*p )
            {
                break;
            }

            val[0] = *p++;
            val[1] = *p++;
            val[2] = '\0';

            *psz++ = strtol( val, NULL, 16 );
        }
        else if( *p == '+' )
        {
            *psz++ = ' ';
            p++;
        }
        else
        {
            *psz++ = *p++;
        }
    }
    *psz++ = '\0';
    free( dup );
}

/****************************************************************************
 * Light RPN evaluator
 ****************************************************************************/
static void SSInit( rpn_stack_t *st )
{
    st->i_stack = 0;
}

static void SSClean( rpn_stack_t *st )
{
    while( st->i_stack > 0 )
    {
        free( st->stack[--st->i_stack] );
    }
}

static void SSPush( rpn_stack_t *st, char *s )
{
    if( st->i_stack < STACK_MAX )
    {
        st->stack[st->i_stack++] = strdup( s );
    }
}

static char * SSPop( rpn_stack_t *st )
{
    if( st->i_stack <= 0 )
    {
        return strdup( "" );
    }
    else
    {
        return st->stack[--st->i_stack];
    }
}

static int SSPopN( rpn_stack_t *st, mvar_t  *vars )
{
    char *name;
    char *value;

    char *end;
    int  i;

    name = SSPop( st );
    i = strtol( name, &end, 0 );
    if( end == name )
    {
        value = mvar_GetValue( vars, name );
        i = atoi( value );
    }
    free( name );

    return( i );
}

static void SSPushN( rpn_stack_t *st, int i )
{
    char v[512];

    sprintf( v, "%d", i );
    SSPush( st, v );
}

static void  EvaluateRPN( intf_thread_t *p_intf, mvar_t  *vars,
                          rpn_stack_t *st, char *exp )
{
    intf_sys_t    *p_sys = p_intf->p_sys;

    for( ;; )
    {
        char s[100], *p;

        /* skip spcae */
        while( *exp == ' ' )
        {
            exp++;
        }

        if( *exp == '\'' )
        {
            /* extract string */
            p = &s[0];
            exp++;
            while( *exp && *exp != '\'' )
            {
                *p++ = *exp++;
            }
            *p = '\0';
            exp++;
            SSPush( st, s );
            continue;
        }

        /* extract token */
        p = strchr( exp, ' ' );
        if( !p )
        {
            strcpy( s, exp );

            exp += strlen( exp );
        }
        else
        {
            int i = p -exp;
            strncpy( s, exp, i );
            s[i] = '\0';

            exp = p + 1;
        }

        if( *s == '\0' )
        {
            break;
        }

        /* 1. Integer function */
        if( !strcmp( s, "!" ) )
        {
            SSPushN( st, ~SSPopN( st, vars ) );
        }
        else if( !strcmp( s, "^" ) )
        {
            SSPushN( st, SSPopN( st, vars ) ^ SSPopN( st, vars ) );
        }
        else if( !strcmp( s, "&" ) )
        {
            SSPushN( st, SSPopN( st, vars ) & SSPopN( st, vars ) );
        }
        else if( !strcmp( s, "|" ) )
        {
            SSPushN( st, SSPopN( st, vars ) | SSPopN( st, vars ) );
        }
        else if( !strcmp( s, "+" ) )
        {
            SSPushN( st, SSPopN( st, vars ) + SSPopN( st, vars ) );
        }
        else if( !strcmp( s, "-" ) )
        {
            int j = SSPopN( st, vars );
            int i = SSPopN( st, vars );
            SSPushN( st, i - j );
        }
        else if( !strcmp( s, "*" ) )
        {
            SSPushN( st, SSPopN( st, vars ) * SSPopN( st, vars ) );
        }
        else if( !strcmp( s, "/" ) )
        {
            int i, j;

            j = SSPopN( st, vars );
            i = SSPopN( st, vars );

            SSPushN( st, j != 0 ? i / j : 0 );
        }
        else if( !strcmp( s, "%" ) )
        {
            int i, j;

            j = SSPopN( st, vars );
            i = SSPopN( st, vars );

            SSPushN( st, j != 0 ? i % j : 0 );
        }
        /* 2. integer tests */
        else if( !strcmp( s, "=" ) )
        {
            SSPushN( st, SSPopN( st, vars ) == SSPopN( st, vars ) ? -1 : 0 );
        }
        else if( !strcmp( s, "!=" ) )
        {
            SSPushN( st, SSPopN( st, vars ) != SSPopN( st, vars ) ? -1 : 0 );
        }
        else if( !strcmp( s, "<" ) )
        {
            int j = SSPopN( st, vars );
            int i = SSPopN( st, vars );

            SSPushN( st, i < j ? -1 : 0 );
        }
        else if( !strcmp( s, ">" ) )
        {
            int j = SSPopN( st, vars );
            int i = SSPopN( st, vars );

            SSPushN( st, i > j ? -1 : 0 );
        }
        else if( !strcmp( s, "<=" ) )
        {
            int j = SSPopN( st, vars );
            int i = SSPopN( st, vars );

            SSPushN( st, i <= j ? -1 : 0 );
        }
        else if( !strcmp( s, ">=" ) )
        {
            int j = SSPopN( st, vars );
            int i = SSPopN( st, vars );

            SSPushN( st, i >= j ? -1 : 0 );
        }
        /* 3. string functions */
        else if( !strcmp( s, "strcat" ) )
        {
            char *s2 = SSPop( st );
            char *s1 = SSPop( st );
            char *str = malloc( strlen( s1 ) + strlen( s2 ) + 1 );

            strcpy( str, s1 );
            strcat( str, s2 );

            SSPush( st, str );
            free( s1 );
            free( s2 );
            free( str );
        }
        else if( !strcmp( s, "strcmp" ) )
        {
            char *s2 = SSPop( st );
            char *s1 = SSPop( st );

            SSPushN( st, strcmp( s1, s2 ) );
            free( s1 );
            free( s2 );
        }
        else if( !strcmp( s, "strncmp" ) )
        {
            int n = SSPopN( st, vars );
            char *s2 = SSPop( st );
            char *s1 = SSPop( st );

            SSPushN( st, strncmp( s1, s2 , n ) );
            free( s1 );
            free( s2 );
        }
        else if( !strcmp( s, "strsub" ) )
        {
            int n = SSPopN( st, vars );
            int m = SSPopN( st, vars );
            int i_len;
            char *s = SSPop( st );
            char *str;

            if( n >= m )
            {
                i_len = n - m + 1;
            }
            else
            {
                i_len = 0;
            }

            str = malloc( i_len + 1 );

            memcpy( str, s + m - 1, i_len );
            str[ i_len ] = '\0';

            SSPush( st, str );
            free( s );
            free( str );
        }
       else if( !strcmp( s, "strlen" ) )
        {
            char *str = SSPop( st );

            SSPushN( st, strlen( str ) );
            free( str );
        }
        /* 4. stack functions */
        else if( !strcmp( s, "dup" ) )
        {
            char *str = SSPop( st );
            SSPush( st, str );
            SSPush( st, str );
            free( str );
        }
        else if( !strcmp( s, "drop" ) )
        {
            char *str = SSPop( st );
            free( str );
        }
        else if( !strcmp( s, "swap" ) )
        {
            char *s1 = SSPop( st );
            char *s2 = SSPop( st );

            SSPush( st, s1 );
            SSPush( st, s2 );
            free( s1 );
            free( s2 );
        }
        else if( !strcmp( s, "flush" ) )
        {
            SSClean( st );
            SSInit( st );
        }
        else if( !strcmp( s, "store" ) )
        {
            char *value = SSPop( st );
            char *name  = SSPop( st );

            mvar_PushNewVar( vars, name, value );
            free( name );
            free( value );
        }
        else if( !strcmp( s, "value" ) )
        {
            char *name  = SSPop( st );
            char *value = mvar_GetValue( vars, name );

            SSPush( st, value );

            free( name );
        }
        else if( !strcmp( s, "url_extract" ) )
        {
            char *url = mvar_GetValue( vars, "url_value" );
            char *name = SSPop( st );
            char value[512];
            char *tmp;

            uri_extract_value( url, name, value, 512 );
            uri_decode_url_encoded( value );
            tmp = FromUTF8( p_intf, value );
            SSPush( st, tmp );
            free( tmp );
        }
        else if( !strcmp( s, "url_encode" ) )
        {
            char *url = SSPop( st );
            char *value;

            url = ToUTF8( p_intf, url );
            value = vlc_UrlEncode( url );
            free( url );
            SSPush( st, value );
            free( value );
        }
        /* 5. player control */
        else if( !strcmp( s, "play" ) )
        {
            int i_id = SSPopN( st, vars );
            int i_ret;

            i_ret = playlist_Control( p_sys->p_playlist, PLAYLIST_ITEMPLAY,
                                      playlist_ItemGetById( p_sys->p_playlist,
                                      i_id ) );
            msg_Dbg( p_intf, "requested playlist item: %i", i_id );
            SSPushN( st, i_ret );
        }
        /* 6. playlist functions */
        else if( !strcmp( s, "playlist_add" ) )
        {
            char *psz_name = SSPop( st );
            char *mrl = SSPop( st );
            playlist_item_t *p_item;
            int i_id;

            if( !*psz_name )
            {
                p_item = parse_MRL( p_intf, mrl, mrl );
            }
            else
            {
                p_item = parse_MRL( p_intf, mrl, psz_name );
            }

            if( p_item == NULL || p_item->input.psz_uri == NULL ||
                !*p_item->input.psz_uri )
            {
                i_id = VLC_EGENERIC;
                msg_Dbg( p_intf, "invalid requested mrl: %s", mrl );
            }
            else
            {
                i_id = playlist_AddItem( p_sys->p_playlist, p_item,
                                         PLAYLIST_APPEND, PLAYLIST_END );
                msg_Dbg( p_intf, "requested mrl add: %s", mrl );
            }
            SSPushN( st, i_id );
        }
        else
        {
            SSPush( st, s );
        }
    }
}

/**********************************************************************
 * Find_end_MRL: Find the end of the sentence :
 * this function parses the string psz and find the end of the item
 * and/or option with detecting the " and ' problems.
 * returns NULL if an error is detected, otherwise, returns a pointer
 * of the end of the sentence (after the last character)
 **********************************************************************/
static char *Find_end_MRL( char *psz )
{
    char *s_sent = psz;

    switch( *s_sent )
    {
        case '\"':
        {
            s_sent++;

            while( ( *s_sent != '\"' ) && ( *s_sent != '\0' ) )
            {
                if( *s_sent == '\'' )
                {
                    s_sent = Find_end_MRL( s_sent );

                    if( s_sent == NULL )
                    {
                        return NULL;
                    }
                } else
                {
                    s_sent++;
                }
            }

            if( *s_sent == '\"' )
            {
                s_sent++;
                return s_sent;
            } else  /* *s_sent == '\0' , which means the number of " is incorrect */
            {
                return NULL;
            }
            break;
        }
        case '\'':
        {
            s_sent++;

            while( ( *s_sent != '\'' ) && ( *s_sent != '\0' ) )
            {
                if( *s_sent == '\"' )
                {
                    s_sent = Find_end_MRL( s_sent );

                    if( s_sent == NULL )
                    {
                        return NULL;
                    }
                } else
                {
                    s_sent++;
                }
            }

            if( *s_sent == '\'' )
            {
                s_sent++;
                return s_sent;
            } else  /* *s_sent == '\0' , which means the number of ' is incorrect */
            {
                return NULL;
            }
            break;
        }
        default: /* now we can look for spaces */
        {
            while( ( *s_sent != ' ' ) && ( *s_sent != '\0' ) )
            {
                if( ( *s_sent == '\'' ) || ( *s_sent == '\"' ) )
                {
                    s_sent = Find_end_MRL( s_sent );
                } else
                {
                    s_sent++;
                }
            }
            return s_sent;
        }
    }
}

/**********************************************************************
 * parse_MRL: parse the MRL, find the mrl string and the options,
 * create an item with all information in it, and return the item.
 * return NULL if there is an error.
 **********************************************************************/
static playlist_item_t *parse_MRL( intf_thread_t *p_intf, char *psz,
                                   char *psz_name )
{
    char **ppsz_options = NULL;
    char *mrl;
    char *s_mrl = psz;
    int i_error = 0;
    char *s_temp;
    int i = 0;
    int i_options = 0;
    playlist_item_t * p_item = NULL;

    /* In case there is spaces before the mrl */
    while( ( *s_mrl == ' ' ) && ( *s_mrl != '\0' ) )
    {
        s_mrl++;
    }

    /* extract the mrl */
    s_temp = strstr( s_mrl , " :" );
    if( s_temp == NULL )
    {
        s_temp = s_mrl + strlen( s_mrl );
    } else
    {
        while( (*s_temp == ' ') && (s_temp != s_mrl ) )
        {
            s_temp--;
        }
        s_temp++;
    }

    /* if the mrl is between " or ', we must remove them */
    if( (*s_mrl == '\'') || (*s_mrl == '\"') )
    {
        mrl = (char *)malloc( (s_temp - s_mrl - 1) * sizeof( char ) );
        strncpy( mrl , (s_mrl + 1) , s_temp - s_mrl - 2 );
        mrl[ s_temp - s_mrl - 2 ] = '\0';
    } else
    {
        mrl = (char *)malloc( (s_temp - s_mrl + 1) * sizeof( char ) );
        strncpy( mrl , s_mrl , s_temp - s_mrl );
        mrl[ s_temp - s_mrl ] = '\0';
    }

    s_mrl = s_temp;

    /* now we can take care of the options */
    while( (*s_mrl != '\0') && (i_error == 0) )
    {
        switch( *s_mrl )
        {
            case ' ':
            {
                s_mrl++;
                break;
            }
            case ':': /* an option */
            {
                s_temp = Find_end_MRL( s_mrl );

                if( s_temp == NULL )
                {
                    i_error = 1;
                }
                else
                {
                    i_options++;
                    ppsz_options = realloc( ppsz_options , i_options *
                                            sizeof(char *) );
                    ppsz_options[ i_options - 1 ] =
                        malloc( (s_temp - s_mrl + 1) * sizeof(char) );

                    strncpy( ppsz_options[ i_options - 1 ] , s_mrl ,
                             s_temp - s_mrl );

                    /* don't forget to finish the string with a '\0' */
                    (ppsz_options[ i_options - 1 ])[ s_temp - s_mrl ] = '\0';

                    s_mrl = s_temp;
                }
                break;
            }
            default:
            {
                i_error = 1;
                break;
            }
        }
    }

    if( i_error != 0 )
    {
        free( mrl );
    }
    else
    {
        /* now create an item */
        p_item = playlist_ItemNew( p_intf, mrl, psz_name );
        for( i = 0 ; i< i_options ; i++ )
        {
            playlist_ItemAddOption( p_item, ppsz_options[i] );
        }
    }

    for( i = 0; i < i_options; i++ ) free( ppsz_options[i] );
    if( i_options ) free( ppsz_options );

    return p_item;
}
