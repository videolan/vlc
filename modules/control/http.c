/*****************************************************************************
 * http.c :  http mini-server ;)
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: http.c,v 1.21 2003/08/01 17:30:33 fenrir Exp $
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
/*
 * TODO:
 *
 * - clean up ?
 * - doc ! (mouarf ;)
 *
 */

#include <stdlib.h>
#include <vlc/vlc.h>
#include <vlc/intf.h>

#include "httpd.h"

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

#if (!defined( WIN32 ) || defined(__MINGW32__))
/* Mingw has its own version of dirent */
#   include <dirent.h>
#endif

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Activate     ( vlc_object_t * );
static void Close        ( vlc_object_t * );

#define HOST_TEXT N_( "Host address" )
#define HOST_LONGTEXT N_( \
    "You can set the address and port on which the http interface will bind" )
#define SRC_TEXT N_( "Source directory" )
#define SRC_LONGTEXT N_( "Source directory" )

vlc_module_begin();
    set_description( _("HTTP remote control interface") );
    add_category_hint( N_("HTTP remote control"), NULL, VLC_TRUE );
        add_string ( "http-host", NULL, NULL, HOST_TEXT, HOST_LONGTEXT, VLC_TRUE );
        add_string ( "http-src",  NULL, NULL, SRC_TEXT,  SRC_LONGTEXT,  VLC_TRUE );
    set_capability( "interface", 0 );
    set_callbacks( Activate, Close );
vlc_module_end();


/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static void Run          ( intf_thread_t *p_intf );

static int ParseDirectory( intf_thread_t *p_intf, char *psz_root,
                           char *psz_dir );

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


static int  http_get( httpd_file_callback_args_t *p_args,
                      uint8_t *p_request, int i_request,
                      uint8_t **pp_data, int *pi_data );

static void uri_extract_value( char *psz_uri, char *psz_name,
                               char *psz_value, int i_value_max );
static void uri_decode_url_encoded( char *psz );

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

struct httpd_file_callback_args_t
{
    intf_thread_t *p_intf;
    httpd_file_t  *p_file;

    char          *file;
    char          *name;
    char          *mime;

    /* inited for each access */
    rpn_stack_t       stack;
    mvar_t        *vars;
};

struct intf_sys_t
{
    httpd_t             *p_httpd;
    httpd_host_t        *p_httpd_host;

    int                         i_files;
    httpd_file_callback_args_t  **pp_files;

    playlist_t          *p_playlist;
    input_thread_t      *p_input;
};



/*****************************************************************************
 * Activate: initialize and create stuff
 *****************************************************************************/
static int Activate( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t*)p_this;
    intf_sys_t    *p_sys;
    char          *psz_host;
    char          *psz_address = "";
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
    if( i_port <= 0 )
    {
        i_port= 8080;
    }

    msg_Dbg( p_intf, "base %s:%d", psz_address, i_port );
    p_intf->p_sys = p_sys = malloc( sizeof( intf_sys_t ) );
    p_sys->p_playlist = NULL;
    p_sys->p_input    = NULL;

    if( ( p_sys->p_httpd = httpd_Find( VLC_OBJECT(p_intf), VLC_TRUE ) ) == NULL )
    {
        msg_Err( p_intf, "cannot create/find httpd" );
        free( p_sys );
        return VLC_EGENERIC;
    }

    if( ( p_sys->p_httpd_host =
                p_sys->p_httpd->pf_register_host( p_sys->p_httpd,
                                                  psz_address, i_port ) ) == NULL )
    {
        msg_Err( p_intf, "cannot listen on %s:%d", psz_address, i_port );
        httpd_Release( p_sys->p_httpd );
        free( p_sys );
        return VLC_EGENERIC;
    }

    if( psz_host )
    {
        free( psz_host );
    }

    p_sys->i_files = 0;
    p_sys->pp_files = malloc( sizeof( httpd_file_callback_args_t *) );

#if defined(SYS_DARWIN) || defined(SYS_BEOS) || \
        ( defined(WIN32) && !defined(UNDER_CE ) )
    if ( ( psz_src = config_GetPsz( p_intf, "http-src" )) == NULL )
    {
        char * psz_vlcpath = p_intf->p_libvlc->psz_vlcpath;
        psz_src = malloc( strlen(psz_vlcpath) + strlen("/share/http" ) + 1 );
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

    return VLC_SUCCESS;

failed:
    free( p_sys->pp_files );
    p_sys->p_httpd->pf_unregister_host( p_sys->p_httpd,
                                        p_sys->p_httpd_host );
    httpd_Release( p_sys->p_httpd );
    free( p_sys );
    return VLC_EGENERIC;
}

/*****************************************************************************
 * CloseIntf: destroy interface
 *****************************************************************************/
void Close ( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_this;
    intf_sys_t    *p_sys = p_intf->p_sys;

    int i;

    for( i = 0; i < p_sys->i_files; i++ )
    {
       p_sys->p_httpd->pf_unregister_file( p_sys->p_httpd,
                                           p_sys->pp_files[i]->p_file );
       /* do not free mime */
       free( p_sys->pp_files[i]->file );
       free( p_sys->pp_files[i]->name );
       free( p_sys->pp_files[i] );
    }
    free( p_sys->pp_files );
    p_sys->p_httpd->pf_unregister_host( p_sys->p_httpd,
                                        p_sys->p_httpd_host );
    httpd_Release( p_sys->p_httpd );
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
static char *FileToUrl( char *name )
{
    char *url, *p;

    url = p = malloc( strlen( name ) + 1 );

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
        }
    }
    return url;
}

/****************************************************************************
 * FileToMime: XXX duplicated with modules/access_out/http.c
 ****************************************************************************/
static struct
{
    char *psz_ext;
    char *psz_mime;
} http_mime[] =
{
    { ".htm",   "text/html" },
    { ".html",  "text/html" },

    { ".css",   "text/css" },

    /* media mime */
    { ".avi",   "video/avi" },
    { ".asf",   "video/x-ms-asf" },
    { ".m1a",   "audio/mpeg" },
    { ".m2a",   "audio/mpeg" },
    { ".m1v",   "video/mpeg" },
    { ".m2v",   "video/mpeg" },
    { ".mp2",   "audio/mpeg" },
    { ".mp3",   "audio/mpeg" },
    { ".mpa",   "audio/mpeg" },
    { ".mpg",   "video/mpeg" },
    { ".mpeg",  "video/mpeg" },
    { ".mpe",   "video/mpeg" },
    { ".mov",   "video/quicktime" },
    { ".moov",  "video/quicktime" },
    { ".ogg",   "application/ogg" },
    { ".ogm",   "application/ogg" },
    { ".wav",   "audio/wav" },

    /* end */
    { NULL,     NULL }
};

static char *FileToMime( char *psz_name )
{
    char *psz_ext;

    psz_ext = strrchr( psz_name, '.' );
    if( psz_ext )
    {
        int i;

        for( i = 0; http_mime[i].psz_ext != NULL ; i++ )
        {
            if( !strcmp( http_mime[i].psz_ext, psz_ext ) )
            {
                return( http_mime[i].psz_mime );
            }
        }
    }
    return( "application/octet-stream" );
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
    FILE          *file;

    char          *user = NULL;
    char          *password = NULL;

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

    for( ;; )
    {
        /* parse psz_src dir */
        if( ( p_dir_content = readdir( p_dir ) ) == NULL )
        {
            break;
        }

        if( p_dir_content->d_name[0] == '.' )
        {
            continue;
        }
        sprintf( dir, "%s/%s", psz_dir, p_dir_content->d_name );
        if( ParseDirectory( p_intf, psz_root, dir ) )
        {
#define f p_sys->pp_files[p_sys->i_files]
            f = malloc( sizeof( httpd_file_callback_args_t ) );
            f->p_intf  = p_intf;
            f->file = strdup( dir );
            f->name = FileToUrl( &dir[strlen( psz_root )] );
            f->mime = FileToMime( &dir[strlen( psz_root )] );

            msg_Dbg( p_intf, "file=%s (url=%s mime=%s)",
                     f->file, f->name, f->mime );

            f->p_file =
                p_sys->p_httpd->pf_register_file( p_sys->p_httpd,
                                                  f->name, f->mime,
                                                  user, password,
                                                  http_get, http_get,
                                                  f );
            if( f->p_file )
            {
                p_sys->i_files++;
                p_sys->pp_files = realloc( p_sys->pp_files,
                  (p_sys->i_files+1) * sizeof( httpd_file_callback_args_t ) );
            }
#define fold p_sys->pp_files[p_sys->i_files-1]

            /* FIXME for rep/ add rep (it would be better to do a redirection) */
            if( strlen(fold->name) > 1 &&
                fold->name[strlen(fold->name) - 1] == '/' )
            {
                f = malloc( sizeof( httpd_file_callback_args_t ) );
                f->p_intf  = p_intf;
                f->file = strdup( fold->file );
                f->name = strdup( fold->name );
                f->mime = fold->mime;

                f->name[strlen(f->name) - 1] = '\0';
                msg_Dbg( p_intf, "file=%s (url=%s mime=%s)", f->file, f->name,
                         f->mime );
                f->p_file =
                    p_sys->p_httpd->pf_register_file( p_sys->p_httpd,
                                                      f->name, f->mime,
                                                      user, password,
                                                      http_get, http_get,
                                                      f );
                if( f->p_file )
                {
                    p_sys->i_files++;
                    p_sys->pp_files =
                        realloc( p_sys->pp_files, (p_sys->i_files+1) *
                                 sizeof( httpd_file_callback_args_t ) );
                }
            }
#undef fold
#undef f
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
    return VLC_SUCCESS;
}

/****************************************************************************
 * var and set handling
 ****************************************************************************/

static mvar_t *mvar_New( char *name, char *value )
{
    mvar_t *v = malloc( sizeof( mvar_t ) );

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

static mvar_t *mvar_Duplicate( mvar_t *v )
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

static mvar_t *mvar_GetVar( mvar_t *s, char *name )
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

static void mvar_PushNewVar( mvar_t *vars, char *name, char *value )
{
    mvar_t *f = mvar_New( name, value );
    mvar_PushVar( vars, f );
}

static void mvar_AppendNewVar( mvar_t *vars, char *name, char *value )
{
    mvar_t *f = mvar_New( name, value );
    mvar_AppendVar( vars, f );
}


/* arg= start[:stop[:step]],.. */
static mvar_t *mvar_IntegerSetNew( char *name, char *arg )
{
    char *dup = strdup( arg );
    char *str = dup;
    mvar_t *s = mvar_New( name, "set" );

    fprintf( stderr," mvar_IntegerSetNew: name=`%s' arg=`%s'\n", name, str );

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
        fprintf( stderr," mvar_IntegerSetNew: m=%d start=%d stop=%d step=%d\n", i_match, i_start, i_stop, i_step );

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

            if( ( i_start < i_stop && i_step > 0 ) ||
                ( i_start > i_stop && i_step < 0 ) )
            {
                for( i = i_start; ; i += i_step )
                {
                    char   value[512];

                    if( ( i_step > 0 && i > i_stop ) ||
                        ( i_step < 0 && i < i_stop ) )
                    {
                        break;
                    }

                    fprintf( stderr," mvar_IntegerSetNew: adding %d\n", i );
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

static mvar_t *mvar_PlaylistSetNew( char *name, playlist_t *p_pl )
{
    mvar_t *s = mvar_New( name, "set" );
    int    i;

    fprintf( stderr," mvar_PlaylistSetNew: name=`%s'\n", name );

    vlc_mutex_lock( &p_pl->object_lock );
    for( i = 0; i < p_pl->i_size; i++ )
    {
        mvar_t *itm = mvar_New( name, "set" );
        char   value[512];

        sprintf( value, "%d", i == p_pl->i_index ? 1 : 0 );
        mvar_AppendNewVar( itm, "current", value );

        sprintf( value, "%d", i );
        mvar_AppendNewVar( itm, "index", value );

        mvar_AppendNewVar( itm, "name", p_pl->pp_items[i]->psz_name );

        mvar_AppendVar( s, itm );
    }
    vlc_mutex_unlock( &p_pl->object_lock );

    return s;
}

static mvar_t *mvar_InfoSetNew( char *name, input_thread_t *p_input )
{
    mvar_t *s = mvar_New( name, "set" );

    input_info_category_t * p_category;
    input_info_t * p_info;

    fprintf( stderr," mvar_InfoSetNew: name=`%s'\n", name );
    if( p_input == NULL )
    {
        return s;
    }

    vlc_mutex_lock( &p_input->stream.stream_lock );
    p_category = p_input->stream.p_info;
    while ( p_category )
    {
        mvar_t *cat  = mvar_New( name, "set" );
        mvar_t *iset = mvar_New( "info", "set" );

        mvar_AppendNewVar( cat, "name", p_category->psz_name );
        mvar_AppendVar( cat, iset );

        p_info = p_category->p_info;
        while ( p_info )
        {
            mvar_t *info = mvar_New( "info", "" );

            msg_Dbg( p_input, "adding info name=%s value=%s", p_info->psz_name, p_info->psz_value );
            mvar_AppendNewVar( info, "name",  p_info->psz_name );
            mvar_AppendNewVar( info, "value", p_info->psz_value );
            mvar_AppendVar( iset, info );
            p_info = p_info->p_next;
        }
        mvar_AppendVar( s, cat );
        p_category = p_category->p_next;
    }
    vlc_mutex_unlock( &p_input->stream.stream_lock );

    return s;
}

static mvar_t *mvar_HttpdInfoSetNew( char *name, httpd_t *p_httpd, int i_type )
{
    mvar_t       *s = mvar_New( name, "set" );
    httpd_info_t info;
    int          i;

    fprintf( stderr," mvar_HttpdInfoSetNew: name=`%s'\n", name );
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

    return s;
}
static mvar_t *mvar_FileSetNew( char *name, char *psz_dir )
{
    mvar_t *s = mvar_New( name, "set" );
    char           tmp[MAX_DIR_SIZE], *p, *src;
#ifdef HAVE_SYS_STAT_H
    struct stat   stat_info;
#endif
    DIR           *p_dir;
    struct dirent *p_dir_content;
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

    fprintf( stderr," mvar_FileSetNew: name=`%s' dir=`%s'\n", name, psz_dir );

#ifdef HAVE_SYS_STAT_H
    if( stat( psz_dir, &stat_info ) == -1 || !S_ISDIR( stat_info.st_mode ) )
    {
        return s;
    }
#endif

    if( ( p_dir = opendir( psz_dir ) ) == NULL )
    {
        fprintf( stderr, "cannot open dir (%s)", psz_dir );
        return s;
    }

    /* remove traling / or \ */
    for( p = &psz_dir[strlen( psz_dir) - 1]; p >= psz_dir && ( *p =='/' || *p =='\\' ); p-- )
    {
        *p = '\0';
    }

    for( ;; )
    {
        mvar_t *f;

        /* parse psz_src dir */
        if( ( p_dir_content = readdir( p_dir ) ) == NULL )
        {
            break;
        }
        if( !strcmp( p_dir_content->d_name, "." ) )
        {
            continue;
        }

#if defined( WIN32 )
        sprintf( tmp, "%s\\%s", psz_dir, p_dir_content->d_name );
#else
        sprintf( tmp, "%s/%s", psz_dir, p_dir_content->d_name );
#endif

#ifdef HAVE_SYS_STAT_H
        if( stat( tmp, &stat_info ) == -1 )
        {
            continue;
        }
#endif
        f = mvar_New( name, "set" );
        mvar_AppendNewVar( f, "name", tmp );
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

        sprintf( tmp, "%lld", stat_info.st_size );
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

static void SSInit( rpn_stack_t * );
static void SSClean( rpn_stack_t * );
static void EvaluateRPN( mvar_t  *, rpn_stack_t *, char * );

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

static int FileLoad( FILE *f, uint8_t **pp_data, int *pi_data )
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

static int MacroParse( macro_t *m, uint8_t *psz_src )
{
    uint8_t *dup = strdup( psz_src );
    uint8_t *src = dup;
    uint8_t *p;
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

        MVLC_CLOSE,
        MVLC_SHUTDOWN,
    MVLC_FOREACH,
    MVLC_IF,
    MVLC_RPN,
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

        /* playlist management */
        { "add",            MVLC_ADD },
        { "del",            MVLC_DEL },
        { "empty",          MVLC_EMPTY },

        /* admin control */
        { "close",          MVLC_CLOSE },
        { "shutdown",       MVLC_SHUTDOWN },

    { "rpn",        MVLC_RPN },

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

static void MacroDo( httpd_file_callback_args_t *p_args,
                     macro_t *m,
                     uint8_t *p_request, int i_request,
                     uint8_t **pp_data,  int *pi_data,
                     uint8_t **pp_dst )
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
    *pp_dst += sprintf( *pp_dst, str, s );

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
                    playlist_Command( p_sys->p_playlist, PLAYLIST_GOTO, i_item );
                    msg_Dbg( p_intf, "requested playlist item: %i", i_item );
                    break;
                }
                case MVLC_STOP:
                    playlist_Command( p_sys->p_playlist, PLAYLIST_STOP, 0 );
                    msg_Dbg( p_intf, "requested playlist stop" );
                    break;
                case MVLC_PAUSE:
                    playlist_Command( p_sys->p_playlist, PLAYLIST_PAUSE, 0 );
                    msg_Dbg( p_intf, "requested playlist pause" );
                    break;
                case MVLC_NEXT:
                    playlist_Command( p_sys->p_playlist, PLAYLIST_GOTO,
                                      p_sys->p_playlist->i_index + 1 );
                    msg_Dbg( p_intf, "requested playlist next" );
                    break;
                case MVLC_PREVIOUS:
                    playlist_Command( p_sys->p_playlist, PLAYLIST_GOTO,
                                      p_sys->p_playlist->i_index - 1 );
                    msg_Dbg( p_intf, "requested playlist next" );
                    break;

                /* playlist management */
                case MVLC_ADD:
                {
                    char mrl[512];
                    uri_extract_value( p_request, "mrl", mrl, 512 );
                    uri_decode_url_encoded( mrl );
                    playlist_Add( p_sys->p_playlist, mrl, NULL, 0,
                                  PLAYLIST_APPEND, PLAYLIST_END );
                    msg_Dbg( p_intf, "requested playlist add: %s", mrl );
                    break;
                }
                case MVLC_DEL:
                {
                    int i_item;
                    char item[512];

                    uri_extract_value( p_request, "item", item, 512 );
                    i_item = atoi( item );

                    playlist_Delete( p_sys->p_playlist, i_item );
                    msg_Dbg( p_intf, "requested playlist del: %d", i_item );
                    break;
                }
                case MVLC_EMPTY:
                {
                    while( p_sys->p_playlist->i_size > 0 )
                    {
                        playlist_Delete( p_sys->p_playlist, 0 );
                    }
                    msg_Dbg( p_intf, "requested playlist empty" );
                    break;
                }

                /* admin function */
                case MVLC_CLOSE:
                {
                    char id[512];
                    uri_extract_value( p_request, "id", id, 512 );
                    msg_Dbg( p_intf, "requested close id=%s", id );
                    if( p_sys->p_httpd->pf_control( p_sys->p_httpd, HTTPD_SET_CLOSE, id, NULL ) )
                    {
                        msg_Warn( p_intf, "close failed for id=%s", id );
                    }
                    break;
                }
                case MVLC_SHUTDOWN:
                {
                    msg_Dbg( p_intf, "requested shutdown" );
                    p_intf->p_vlc->b_die = VLC_TRUE;
                    break;
                }
                default:
                    PRINTS( "<!-- control param(%s) unsuported -->", control );
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
                    sprintf( value, "%i", i );
                    break;
                case MVLC_FLOAT:
                    f = config_GetFloat( p_intf, m->param1 );
                    sprintf( value, "%f", f );
                    break;
                case MVLC_STRING:
                    psz = config_GetPsz( p_intf, m->param1 );
                    sprintf( value, "%s", psz ? psz : "" );
                    break;
                default:
                    sprintf( value, "invalid type(%s) in set", m->param2 );
                    break;
            }
            msg_Dbg( p_intf, "get name=%s value=%s type=%s", m->param1, value, m->param2 );
            PRINTS( "%s", value );
            break;
        }
        case MVLC_VALUE:
        {
            char *s, *v;

            if( m->param1 )
            {
                EvaluateRPN( p_args->vars, &p_args->stack, m->param1 );
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
            EvaluateRPN( p_args->vars, &p_args->stack, m->param1 );
            break;
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

static uint8_t *MacroSearch( uint8_t *src, uint8_t *end, int i_mvlc, vlc_bool_t b_after )
{
    int     i_id;
    int     i_level = 0;

    while( src < end )
    {
        if( src + 4 < end  && !strncmp( src, "<vlc", 4 ) )
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

static void Execute( httpd_file_callback_args_t *p_args,
                     uint8_t *p_request, int i_request,
                     uint8_t **pp_data, int *pi_data,
                     uint8_t **pp_dst,
                     uint8_t *_src, uint8_t *_end )
{
    intf_thread_t  *p_intf = p_args->p_intf;

    uint8_t *src, *dup, *end;
    uint8_t *dst = *pp_dst;

    src = dup = malloc( _end - _src + 1 );
    end = src +( _end - _src );

    memcpy( src, _src, _end - _src );
    *end = '\0';

    /* we parse searching <vlc */
    while( src < end )
    {
        uint8_t *p;
        int i_copy;

        p = strstr( src, "<vlc" );
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
                    uint8_t    *endif;

                    EvaluateRPN( p_args->vars, &p_args->stack, m.param1 );
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
                        uint8_t *start = MacroSearch( src, endif, MVLC_ELSE, VLC_TRUE );

                        if( start )
                        {
                            uint8_t *stop  = MacroSearch( start, endif, MVLC_END, VLC_FALSE );
                            if( stop )
                            {
                                Execute( p_args, p_request, i_request, pp_data, pi_data, &dst, start, stop );
                            }
                        }
                    }
                    else if( i_test == 1 )
                    {
                        uint8_t *stop;
                        if( ( stop = MacroSearch( src, endif, MVLC_ELSE, VLC_FALSE ) ) == NULL )
                        {
                            stop = MacroSearch( src, endif, MVLC_END, VLC_FALSE );
                        }
                        if( stop )
                        {
                            Execute( p_args, p_request, i_request, pp_data, pi_data, &dst, src, stop );
                        }
                    }

                    src = endif;
                    break;
                }
                case MVLC_FOREACH:
                {
                    uint8_t *endfor = MacroSearch( src, end, MVLC_END, VLC_TRUE );
                    uint8_t *start = src;
                    uint8_t *stop = MacroSearch( src, end, MVLC_END, VLC_FALSE );

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
                            index = mvar_FileSetNew( m.param1, arg );
                            free( arg );
                        }
                        else if( !strcmp( m.param2, "playlist" ) )
                        {
                            index = mvar_PlaylistSetNew( m.param1, p_intf->p_sys->p_playlist );
                        }
                        else if( !strcmp( m.param2, "informations" ) )
                        {
                            index = mvar_InfoSetNew( m.param1, p_intf->p_sys->p_input );
                        }
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
                            Execute( p_args, p_request, i_request, pp_data, pi_data, &dst, start, stop );
                            mvar_RemoveVar( p_args->vars, f );

                            mvar_Delete( f );
                        }
                        mvar_Delete( index );

                        src = endfor;
                    }
                    break;
                }
                default:
                    MacroDo( p_args, &m, p_request, i_request, pp_data, pi_data, &dst );
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
 * http_get:
 ****************************************************************************
 * a file with mime == text/html is parsed and all "macro" replaced
 * <vlc id="macro name" [param1="" [param2=""]] />
 * valid id are
 *
 ****************************************************************************/
static int  http_get( httpd_file_callback_args_t *p_args,
                      uint8_t *p_request, int i_request,
                      uint8_t **pp_data, int *pi_data )
{
    char *p;
    FILE *f;

    if( ( f = fopen( p_args->file, "r" ) ) == NULL )
    {
        p = *pp_data = malloc( 10240 );

        p += sprintf( p, "<html>\n" );
        p += sprintf( p, "<head>\n" );
        p += sprintf( p, "<title>Error loading %s</title>\n", p_args->file );
        p += sprintf( p, "</head>\n" );
        p += sprintf( p, "<body>\n" );
        p += sprintf( p, "<h1><center>Error loading %s for %s</center></h1>\n", p_args->file, p_args->name );
        p += sprintf( p, "<hr />\n" );
        p += sprintf( p, "<a href=\"http://www.videolan.org\">VideoLAN</a>\n" );
        p += sprintf( p, "</body>\n" );
        p += sprintf( p, "</html>\n" );

        *pi_data = strlen( *pp_data ) + 1;

        return VLC_SUCCESS;
    }

    if( strcmp( p_args->mime, "text/html" ) )
    {
        FileLoad( f, pp_data, pi_data );
    }
    else
    {
        int  i_buffer;
        uint8_t *p_buffer;
        uint8_t *dst;

        p_args->vars = mvar_New( "variables", "" );
        mvar_AppendNewVar( p_args->vars, "url_param", i_request > 0 ? "1" : "0" );
        mvar_AppendNewVar( p_args->vars, "url_value", p_request );
        mvar_AppendNewVar( p_args->vars, "version",   VERSION_MESSAGE );
        mvar_AppendNewVar( p_args->vars, "copyright", COPYRIGHT_MESSAGE );

        SSInit( &p_args->stack );

        /* first we load in a temporary buffer */
        FileLoad( f, &p_buffer, &i_buffer );

        /* allocate output */
        *pi_data = i_buffer + 1000;
        dst = *pp_data = malloc( *pi_data );

        /* we parse executing all  <vlc /> macros */
        Execute( p_args, p_request, i_request, pp_data, pi_data, &dst, &p_buffer[0], &p_buffer[i_buffer] );

        *dst++ = '\0';
        *pi_data = dst - *pp_data;

        SSClean( &p_args->stack );
        mvar_Delete( p_args->vars );
    }

    fclose( f );

    return VLC_SUCCESS;
}

/****************************************************************************
 * uri parser
 ****************************************************************************/
static void uri_extract_value( char *psz_uri, char *psz_name,
                               char *psz_value, int i_value_max )
{
    char *p;

    p = strstr( psz_uri, psz_name );
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
    }
    else
    {
        strncpy( psz_value, "", i_value_max );
    }
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
    *psz++  ='\0';
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

static void  EvaluateRPN( mvar_t  *vars, rpn_stack_t *st, char *exp )
{
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
            int i = SSPopN( st, vars );
            int j = SSPopN( st, vars );
            SSPushN( st, i - j );
        }
        else if( !strcmp( s, "*" ) )
        {
            SSPushN( st, SSPopN( st, vars ) * SSPopN( st, vars ) );
        }
        else if( !strcmp( s, "/" ) )
        {
            int i, j;

            i = SSPopN( st, vars );
            j = SSPopN( st, vars );

            SSPushN( st, j != 0 ? i / j : 0 );
        }
        else if( !strcmp( s, "%" ) )
        {
            int i, j;

            i = SSPopN( st, vars );
            j = SSPopN( st, vars );

            SSPushN( st, j != 0 ? i % j : 0 );
        }
        /* 2. integer tests */
        else if( !strcmp( s, "=" ) )
        {
            SSPushN( st, SSPopN( st, vars ) == SSPopN( st, vars ) ? -1 : 0 );
        }
        else if( !strcmp( s, "<" ) )
        {
            int i = SSPopN( st, vars );
            int j = SSPopN( st, vars );

            SSPushN( st, i < j ? -1 : 0 );
        }
        else if( !strcmp( s, ">" ) )
        {
            int i = SSPopN( st, vars );
            int j = SSPopN( st, vars );

            SSPushN( st, i > j ? -1 : 0 );
        }
        else if( !strcmp( s, "<=" ) )
        {
            int i = SSPopN( st, vars );
            int j = SSPopN( st, vars );

            SSPushN( st, i <= j ? -1 : 0 );
        }
        else if( !strcmp( s, ">=" ) )
        {
            int i = SSPopN( st, vars );
            int j = SSPopN( st, vars );

            SSPushN( st, i >= j ? -1 : 0 );
        }
        /* 3. string functions */
        else if( !strcmp( s, "strcat" ) )
        {
            char *s1 = SSPop( st );
            char *s2 = SSPop( st );
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
            char *s1 = SSPop( st );
            char *s2 = SSPop( st );

            SSPushN( st, strcmp( s1, s2 ) );
            free( s1 );
            free( s2 );
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
            char *name  = SSPop( st );
            char *value = SSPop( st );

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

            uri_extract_value( url, name, value, 512 );
            uri_decode_url_encoded( value );
            SSPush( st, value );
        }
        else
        {
            SSPush( st, s );
        }
    }
}



