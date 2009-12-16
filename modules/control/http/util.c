/*****************************************************************************
 * util.c : Utility functions for HTTP interface
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include "http.h"
#include <vlc_strings.h>

/****************************************************************************
 * File and directory functions
 ****************************************************************************/

/* ToUrl: create a good name for an url from filename */
char *FileToUrl( char *name, bool *pb_index )
{
    char *url, *p;

    url = p = malloc( strlen( name ) + 1 );

    *pb_index = false;
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
            *name = '/';
        name++;
    }
#endif

    /* index.* -> / */
    if( ( p = strrchr( url, '/' ) ) != NULL )
    {
        if( !strncmp( p, "/index.", 7 ) )
        {
            p[1] = '\0';
            *pb_index = true;
        }
    }
    return url;
}

/* Load a file */
int FileLoad( FILE *f, char **pp_data, int *pi_data )
{
    int i_read;

    /* just load the file */
    *pi_data = 0;
    *pp_data = xmalloc( 1025 );  /* +1 for \0 */

    while( ( i_read = fread( &(*pp_data)[*pi_data], 1, 1024, f ) ) == 1024 )
    {
        *pi_data += 1024;
        *pp_data = xrealloc( *pp_data, *pi_data  + 1025 );
    }
    if( i_read > 0 )
    {
        *pi_data += i_read;
    }
    (*pp_data)[*pi_data] = '\0';

    return VLC_SUCCESS;
}

/* Parse a directory and recursively add files */
int ParseDirectory( intf_thread_t *p_intf, char *psz_root,
                        char *psz_dir )
{
    intf_sys_t     *p_sys = p_intf->p_sys;
    char           dir[MAX_DIR_SIZE];
    DIR           *p_dir;
    vlc_acl_t     *p_acl;
    FILE          *file;

    char          *user = NULL;
    char          *password = NULL;

    int           i_dirlen;

    if( ( p_dir = utf8_opendir( psz_dir ) ) == NULL )
    {
        if( errno != ENOENT && errno != ENOTDIR )
            msg_Err( p_intf, "cannot open directory (%s)", psz_dir );
        return VLC_EGENERIC;
    }

    i_dirlen = strlen( psz_dir );
    if( i_dirlen + 10 > MAX_DIR_SIZE )
    {
        msg_Warn( p_intf, "skipping too deep directory (%s)", psz_dir );
        closedir( p_dir );
        return 0;
    }

    msg_Dbg( p_intf, "dir=%s", psz_dir );

    snprintf( dir, sizeof( dir ), "%s"DIR_SEP".access", psz_dir );
    if( ( file = utf8_fopen( dir, "r" ) ) != NULL )
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
        msg_Dbg( p_intf, "using user=%s (read=%d)", user, i_size );

        fclose( file );
    }

    snprintf( dir, sizeof( dir ), "%s"DIR_SEP".hosts", psz_dir );
    p_acl = ACL_Create( p_intf, false );
    if( ACL_LoadFile( p_acl, dir ) )
    {
        ACL_Destroy( p_acl );

        struct stat st;
        if( utf8_stat( dir, &st ) == 0 )
        {
            free( user );
            free( password );
            closedir( p_dir );
            return VLC_EGENERIC;
        }
        p_acl = NULL;
    }

    for( ;; )
    {
        char *psz_filename;
        /* parse psz_src dir */
        if( ( psz_filename = utf8_readdir( p_dir ) ) == NULL )
        {
            break;
        }

        if( ( psz_filename[0] == '.' )
         || ( i_dirlen + strlen( psz_filename ) > MAX_DIR_SIZE ) )
        {
            free( psz_filename );
            continue;
        }

        snprintf( dir, sizeof( dir ), "%s"DIR_SEP"%s", psz_dir, psz_filename );
        free( psz_filename );

        if( ParseDirectory( p_intf, psz_root, dir ) )
        {
            httpd_file_sys_t *f = NULL;
            httpd_handler_sys_t *h = NULL;
            bool b_index;
            char *psz_name, *psz_ext;

            psz_name = FileToUrl( &dir[strlen( psz_root )], &b_index );
            psz_ext = strrchr( dir, '.' );
            if( psz_ext != NULL )
            {
                int i;
                psz_ext++;
                for( i = 0; i < p_sys->i_handlers; i++ )
                    if( !strcmp( p_sys->pp_handlers[i]->psz_ext, psz_ext ) )
                        break;
                if( i < p_sys->i_handlers )
                {
                    f = malloc( sizeof( httpd_handler_sys_t ) );
                    h = (httpd_handler_sys_t *)f;
                    f->b_handler = true;
                    h->p_association = p_sys->pp_handlers[i];
                }
            }
            if( f == NULL )
            {
                f = xmalloc( sizeof( httpd_file_sys_t ) );
                f->b_handler = false;
            }

            f->p_intf  = p_intf;
            f->p_file = NULL;
            f->p_redir = NULL;
            f->p_redir2 = NULL;
            f->file = strdup (dir);
            f->name = psz_name;
            f->b_html = strstr( &dir[strlen( psz_root )], ".htm" ) || strstr( &dir[strlen( psz_root )], ".xml" ) ? true : false;

            if( !f->name )
            {
                msg_Err( p_intf , "unable to parse directory" );
                closedir( p_dir );
                free( f );
                return( VLC_ENOMEM );
            }
            msg_Dbg( p_intf, "file=%s (url=%s)",
                     f->file, f->name );

            if( !f->b_handler )
            {
                char *psz_type = strdup( "text/html; charset=UTF-8" );
                if( strstr( &dir[strlen( psz_root )], ".xml" ) )
                {
                    char *psz = strstr( psz_type, "html;" );
                    if( psz )
                    {
                        psz[0] = 'x';
                        psz[1] = 'm';
                        psz[2] = 'l';
                        psz[3] = ';';
                        psz[4] = ' ';
                    }
                }
                f->p_file = httpd_FileNew( p_sys->p_httpd_host,
                                           f->name,
                                           f->b_html ? psz_type : NULL,
                                           user, password, p_acl,
                                           HttpCallback, f );
                free( psz_type );
                if( f->p_file != NULL )
                {
                    TAB_APPEND( p_sys->i_files, p_sys->pp_files, f );
                }
            }
            else
            {
                h->p_handler = httpd_HandlerNew( p_sys->p_httpd_host,
                                                 f->name,
                                                 user, password, p_acl,
                                                 HandlerCallback, h );
                if( h->p_handler != NULL )
                {
                    TAB_APPEND( p_sys->i_files, p_sys->pp_files,
                                (httpd_file_sys_t *)h );
                }
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
                    if( asprintf( &psz_redir, "%s%s", f->name, p ) != -1 )
                    {
                        msg_Dbg( p_intf, "redir=%s -> %s", psz_redir, f->name );
                        f->p_redir2 = httpd_RedirectNew( p_sys->p_httpd_host,
                                                         f->name, psz_redir );

                        free( psz_redir );
                    }
                }
            }
        }
    }

    free( user );
    free( password );

    ACL_Destroy( p_acl );
    closedir( p_dir );

    return VLC_SUCCESS;
}


/*************************************************************************
 * Playlist stuff
 *************************************************************************/
void PlaylistListNode( intf_thread_t *p_intf, playlist_t *p_pl,
                           playlist_item_t *p_node, char *name, mvar_t *s,
                           int i_depth )
{
    if( !p_node || !p_node->p_input )
        return;

    if( p_node->i_children == -1 )
    {
        char value[512];
        char *psz;
        playlist_item_t * p_item = playlist_CurrentPlayingItem( p_pl );
        if( !p_item || !p_item->p_input )
            return;

        mvar_t *itm = mvar_New( name, "set" );
        if( p_item->p_input == p_node->p_input )
            mvar_AppendNewVar( itm, "current", "1" );
        else
            mvar_AppendNewVar( itm, "current", "0" );

        sprintf( value, "%d", p_node->i_id );
        mvar_AppendNewVar( itm, "index", value );

        psz = input_item_GetName( p_node->p_input );
        mvar_AppendNewVar( itm, "name", psz );
        free( psz );

        psz = input_item_GetURI( p_node->p_input );
        mvar_AppendNewVar( itm, "uri", psz );
        free( psz );

        mvar_AppendNewVar( itm, "type", "Item" );

        sprintf( value, "%d", i_depth );
        mvar_AppendNewVar( itm, "depth", value );

        if( p_node->i_flags & PLAYLIST_RO_FLAG )
            mvar_AppendNewVar( itm, "ro", "ro" );
        else
            mvar_AppendNewVar( itm, "ro", "rw" );

        sprintf( value, "%"PRId64, input_item_GetDuration( p_node->p_input ) );
        mvar_AppendNewVar( itm, "duration", value );

        //Adding extra meta-information to each playlist item

        psz = input_item_GetTitle( p_node->p_input );
        mvar_AppendNewVar( itm, "title", psz );
        free( psz );

        psz = input_item_GetArtist( p_node->p_input );
        mvar_AppendNewVar( itm, "artist", psz );
        free( psz );

        psz = input_item_GetGenre( p_node->p_input );
        mvar_AppendNewVar( itm, "genre", psz );
        free( psz );

        psz = input_item_GetCopyright( p_node->p_input );
        mvar_AppendNewVar( itm, "copyright", psz );
        free( psz );

        psz = input_item_GetAlbum( p_node->p_input );
        mvar_AppendNewVar( itm, "album", psz );
        free( psz );

        psz = input_item_GetTrackNum( p_node->p_input );
        mvar_AppendNewVar( itm, "track", psz );
        free( psz );

        psz = input_item_GetDescription( p_node->p_input );
        mvar_AppendNewVar( itm, "description", psz );
        free( psz );

        psz = input_item_GetRating( p_node->p_input );
        mvar_AppendNewVar( itm, "rating", psz );
        free( psz );

        psz = input_item_GetDate( p_node->p_input );
        mvar_AppendNewVar( itm, "date", psz );
        free( psz );

        psz = input_item_GetURL( p_node->p_input );
        mvar_AppendNewVar( itm, "url", psz );
        free( psz );

        psz = input_item_GetLanguage( p_node->p_input );
        mvar_AppendNewVar( itm, "language", psz );
        free( psz );

        psz = input_item_GetNowPlaying( p_node->p_input );
        mvar_AppendNewVar( itm, "now_playing", psz );
        free( psz );

        psz = input_item_GetPublisher( p_node->p_input );
        mvar_AppendNewVar( itm, "publisher", psz );
        free( psz );

        psz = input_item_GetEncodedBy( p_node->p_input );
        mvar_AppendNewVar( itm, "encoded_by", psz );
        free( psz );

        psz = input_item_GetArtURL( p_node->p_input );
        mvar_AppendNewVar( itm, "art_url", psz );
        free( psz );

        psz = input_item_GetTrackID( p_node->p_input );
        mvar_AppendNewVar( itm, "track_id", psz );
        free( psz );

        mvar_AppendVar( s, itm );
    }
    else
    {
        char value[512];
        int i_child;
        mvar_t *itm = mvar_New( name, "set" );

        mvar_AppendNewVar( itm, "name", p_node->p_input->psz_name );
        mvar_AppendNewVar( itm, "uri", p_node->p_input->psz_name );

        mvar_AppendNewVar( itm, "type", "Node" );

        sprintf( value, "%d", p_node->i_id );
        mvar_AppendNewVar( itm, "index", value );

        sprintf( value, "%d", p_node->i_children);
        mvar_AppendNewVar( itm, "i_children", value );

        sprintf( value, "%d", i_depth );
        mvar_AppendNewVar( itm, "depth", value );

        if( p_node->i_flags & PLAYLIST_RO_FLAG )
            mvar_AppendNewVar( itm, "ro", "ro" );
        else
            mvar_AppendNewVar( itm, "ro", "rw" );

        mvar_AppendVar( s, itm );

        for( i_child = 0 ; i_child < p_node->i_children ; i_child++ )
             PlaylistListNode( p_intf, p_pl, p_node->pp_children[i_child],
                               name, s, i_depth + 1);
    }
}

/****************************************************************************
 * Seek command parsing handling
 ****************************************************************************/
void HandleSeek( intf_thread_t *p_intf, char *p_value )
{
    intf_sys_t     *p_sys = p_intf->p_sys;
    vlc_value_t val;
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
        var_Get( p_sys->p_input, "length", &val );
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
                    /* other characters which are not numbers are not
                     * important */
                    while( ((p_value[0] < '0') || (p_value[0] > '9'))
                           && (p_value[0] != '\0') )
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
                    while( ((p_value[0] < '0') || (p_value[0] > '9'))
                           && (p_value[0] != '\0') )
                    {
                        p_value++;
                    }
                    break;
                }
                case 's': case 'S': case '"':  /* seconds */
                {
                    i_value += i_stock;
                    i_stock = 0;
                    while( ((p_value[0] < '0') || (p_value[0] > '9'))
                           && (p_value[0] != '\0') )
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

        /* if there is no known symbol, I consider it as seconds.
         * Otherwise, i_stock = 0 */
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
                val.f_float = __MIN( __MAX( ((float) i_value ) / 100.0 ,
                                            0.0 ), 100.0 );
                var_Set( p_sys->p_input, "position", val );
                msg_Dbg( p_intf, "requested seek percent: %d%%", i_value );
                break;
            }
            case POSITION_REL_FOR:
            {
                var_Get( p_sys->p_input, "position", &val );
                val.f_float += __MIN( __MAX( ((float) i_value ) / 100.0,
                                             0.0 ) , 100.0 );
                var_Set( p_sys->p_input, "position", val );
                msg_Dbg( p_intf, "requested seek percent forward: %d%%",
                         i_value );
                break;
            }
            case POSITION_REL_BACK:
            {
                var_Get( p_sys->p_input, "position", &val );
                val.f_float -= __MIN( __MAX( ((float) i_value ) / 100.0,
                                             0.0 ) , 100.0 );
                var_Set( p_sys->p_input, "position", val );
                msg_Dbg( p_intf, "requested seek percent backward: %d%%",
                         i_value );
                break;
            }
            default:
            {
                msg_Dbg( p_intf, "invalid seek request" );
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
}


/****************************************************************************
 * URI Parsing functions
 ****************************************************************************/
int TestURIParam( char *psz_uri, const char *psz_name )
{
    char *p = psz_uri;

    while( (p = strstr( p, psz_name )) )
    {
        /* Verify that we are dealing with a post/get argument */
        if( (p == psz_uri || *(p - 1) == '&' || *(p - 1) == '\n')
              && p[strlen(psz_name)] == '=' )
        {
            return true;
        }
        p++;
    }

    return false;
}

static const char *FindURIValue( const char *psz_uri, const char *restrict psz_name,
                           size_t *restrict p_len )
{
    const char *p = psz_uri, *end;
    size_t len;

    while( (p = strstr( p, psz_name )) )
    {
        /* Verify that we are dealing with a post/get argument */
        if( (p == psz_uri || *(p - 1) == '&' || *(p - 1) == '\n')
              && p[strlen(psz_name)] == '=' )
            break;
        p++;
    }

    if( p == NULL )
    {
        *p_len = 0;
        return NULL;
    }

    p += strlen( psz_name );
    if( *p == '=' ) p++;

    if( ( end = strchr( p, '\n' ) ) != NULL )
    {
        /* POST method */
        if( ( end > p ) && ( end[-1] == '\r' ) )
            end--;

        len = end - p;
    }
    else
    {
        /* GET method */
        if( ( end = strchr( p, '&' ) ) != NULL )
            len = end - p;
        else
            len = strlen( p );
    }

    *p_len = len;
    return p;
}

const char *ExtractURIValue( const char *restrict psz_uri,
                           const char *restrict psz_name,
                           char *restrict psz_buf, size_t bufsize )
{
    size_t len;
    const char *psz_value = FindURIValue( psz_uri, psz_name, &len );
    const char *psz_next;

    if( psz_value == NULL )
    {
        if( bufsize > 0 )
            *psz_buf = '\0';
        return NULL;
    }

    psz_next = psz_value + len;

    if( len >= bufsize )
        len = bufsize - 1;

    if( len > 0 )
        strncpy( psz_buf, psz_value, len );
    if( bufsize > 0 )
        psz_buf[len] = '\0';

    return psz_next;
}

char *ExtractURIString( const char *restrict psz_uri,
                            const char *restrict psz_name )
{
    size_t len;
    const char *psz_value = FindURIValue( psz_uri, psz_name, &len );

    if( psz_value == NULL )
        return NULL;

    char *res = malloc( len + 1 );
    if( res == NULL )
        return NULL;

    memcpy( res, psz_value, len );
    res[len] = '\0';

    return res;
}

/* Since the resulting string is smaller we can work in place, so it is
 * permitted to have psz == new. new points to the first word of the
 * string, the function returns the remaining string. */
char *FirstWord( char *psz, char *new )
{
    bool b_end;

    while( *psz == ' ' )
        psz++;

    while( *psz != '\0' && *psz != ' ' )
    {
        if( *psz == '\'' )
        {
            char c = *psz++;
            while( *psz != '\0' && *psz != c )
            {
                if( *psz == '\\' && psz[1] != '\0' )
                    psz++;
                *new++ = *psz++;
            }
            if( *psz == c )
                psz++;
        }
        else
        {
            if( *psz == '\\' && psz[1] != '\0' )
                psz++;
            *new++ = *psz++;
        }
    }
    b_end = !*psz;

    *new++ = '\0';
    if( !b_end )
        return psz + 1;
    else
        return NULL;
}

/**********************************************************************
 * MRLParse: parse the MRL, find the MRL string and the options,
 * create an item with all information in it, and return the item.
 * return NULL if there is an error.
 **********************************************************************/

/* Function analog to FirstWord except that it relies on colon instead
 * of space to delimit option boundaries. */
static char *FirstOption( char *psz, char *new )
{
    bool b_end, b_start = true;

    while( *psz == ' ' )
        psz++;

    while( *psz != '\0' && (*psz != ' ' || psz[1] != ':') )
    {
        if( *psz == '\'' )
        {
            char c = *psz++;
            while( *psz != '\0' && *psz != c )
            {
                if( *psz == '\\' && psz[1] != '\0' )
                    psz++;
                *new++ = *psz++;
                b_start = false;
            }
            if( *psz == c )
                psz++;
        }
        else
        {
            if( *psz == '\\' && psz[1] != '\0' )
                psz++;
            *new++ = *psz++;
            b_start = false;
        }
    }
    b_end = !*psz;

    if ( !b_start )
        while (new[-1] == ' ')
            new--;

    *new++ = '\0';
    if( !b_end )
        return psz + 1;
    else
        return NULL;
}

input_item_t *MRLParse( intf_thread_t *p_intf, const char *mrl,
                                   char *psz_name )
{
    char *psz = strdup( mrl ), *s_mrl = psz, *s_temp;
    if( psz == NULL )
        return NULL;
    /* extract the mrl */
    s_temp = FirstOption( s_mrl, s_mrl );
    if( s_temp == NULL )
    {
        s_temp = s_mrl + strlen( s_mrl );
    }

    input_item_t *p_input = input_item_New( p_intf, s_mrl, psz_name );
    if( p_input == NULL )
        return NULL;
    s_mrl = s_temp;

    /* now we can take care of the options */
    while ( *s_mrl != '\0' )
    {
        s_temp = FirstOption( s_mrl, s_mrl );
        if( s_mrl == '\0' )
            break;
        if( s_temp == NULL )
        {
            s_temp = s_mrl + strlen( s_mrl );
        }
        input_item_AddOption( p_input, s_mrl, VLC_INPUT_OPTION_TRUSTED );
        s_mrl = s_temp;
    }

    return p_input;
}

/**********************************************************************
 * RealPath: parse ../, ~ and path stuff
 **********************************************************************/
char *RealPath( const char *psz_src )
{
    char *psz_dir;
    char *p;
    int i_len = strlen(psz_src);

    psz_dir = xmalloc( i_len + 2 );
    strcpy( psz_dir, psz_src );

    /* Add a trailing sep to ease the .. step */
    psz_dir[i_len] = DIR_SEP_CHAR;
    psz_dir[i_len + 1] = '\0';

#if (DIR_SEP_CHAR != '/')
    /* Convert all / to native separator */
    p = psz_dir;
    while( (p = strchr( p, '/' )) != NULL )
    {
        *p = DIR_SEP_CHAR;
    }
#endif

    /* FIXME: this could be O(N) rather than O(N²)... */
    /* Remove multiple separators and /./ */
    p = psz_dir;
    while( (p = strchr( p, DIR_SEP_CHAR )) != NULL )
    {
        if( p[1] == DIR_SEP_CHAR )
            memmove( &p[1], &p[2], strlen(&p[2]) + 1 );
        else if( p[1] == '.' && p[2] == DIR_SEP_CHAR )
            memmove( &p[1], &p[3], strlen(&p[3]) + 1 );
        else
            p++;
    }

    if( psz_dir[0] == '~' )
    {
        char *home = config_GetUserDir( VLC_HOME_DIR ), *dir;
        if( asprintf( &dir, "%s%s", home, psz_dir + 1 ) != -1 )
        {
            free( psz_dir );
            psz_dir = dir;
        }
        free( home );
    }

    if( strlen(psz_dir) > 2 )
    {
        /* Fix all .. dir */
        p = psz_dir + 3;
        while( (p = strchr( p, DIR_SEP_CHAR )) != NULL )
        {
            if( p[-1] == '.' && p[-2] == '.' && p[-3] == DIR_SEP_CHAR )
            {
                char *q;
                p[-3] = '\0';
                if( (q = strrchr( psz_dir, DIR_SEP_CHAR )) != NULL )
                {
                    memmove( q + 1, p + 1, strlen(p + 1) + 1 );
                    p = q + 1;
                }
                else
                {
                    memmove( psz_dir, p, strlen(p) + 1 );
                    p = psz_dir + 3;
                }
            }
            else
                p++;
        }
    }

    /* Remove trailing sep if there are at least 2 sep in the string
     * (handles the C:\ stuff) */
    p = strrchr( psz_dir, DIR_SEP_CHAR );
    if( p != NULL && p[1] == '\0' && p != strchr( psz_dir, DIR_SEP_CHAR ) )
        *p = '\0';

    return psz_dir;
}
