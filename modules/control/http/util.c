/*****************************************************************************
 * util.c : Utility functions for HTTP interface
 *****************************************************************************
 * Copyright (C) 2001-2005 the VideoLAN team
 * $Id: http.c 12225 2005-08-18 10:01:30Z massiot $
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

/****************************************************************************
 * File and directory functions
 ****************************************************************************/

/* ToUrl: create a good name for an url from filename */
char *E_(FileToUrl)( char *name, vlc_bool_t *pb_index )
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

/* Load a file */
int E_(FileLoad)( FILE *f, char **pp_data, int *pi_data )
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

/* Parse a directory and recursively add files */
int E_(ParseDirectory)( intf_thread_t *p_intf, char *psz_root,
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
        if( E_(ParseDirectory)( p_intf, psz_root, dir ) )
        {
            httpd_file_sys_t *f = malloc( sizeof( httpd_file_sys_t ) );
            vlc_bool_t b_index;
            char *psz_tmp;

            f->p_intf  = p_intf;
            f->p_file = NULL;
            f->p_redir = NULL;
            f->p_redir2 = NULL;
            psz_tmp = vlc_fix_readdir_charset( p_intf, dir );
            f->file = E_(FromUTF8)( p_intf, psz_tmp );
            free( psz_tmp );
            psz_tmp = vlc_fix_readdir_charset( p_intf,
                                               &dir[strlen( psz_root )] );
            f->name = E_(FileToUrl)( psz_tmp, &b_index );
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
                                       E_(HttpCallback), f );

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


/**************************************************************************
 * Locale functions
 **************************************************************************/
char *E_(FromUTF8)( intf_thread_t *p_intf, char *psz_utf8 )
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

char *E_(ToUTF8)( intf_thread_t *p_intf, char *psz_local )
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

/*************************************************************************
 * Playlist stuff
 *************************************************************************/
void E_(PlaylistListNode)( intf_thread_t *p_intf, playlist_t *p_pl,
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

            psz = E_(FromUTF8)( p_intf, p_node->input.psz_name );
            mvar_AppendNewVar( itm, "name", psz );
            free( psz );

            psz = E_(FromUTF8)( p_intf, p_node->input.psz_uri );
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

            psz = E_(FromUTF8)( p_intf, p_node->input.psz_name );
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
                E_(PlaylistListNode)( p_intf, p_pl,
                                      p_node->pp_children[i_child],
                                      name, s, i_depth + 1);

        }
    }
}

/****************************************************************************
 * Seek command parsing handling
 ****************************************************************************/

void E_(Seek)( intf_thread_t *p_intf, char *p_value )
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
}


/****************************************************************************
 * URI Parsing functions
 ****************************************************************************/
int E_(uri_test_param)( char *psz_uri, const char *psz_name )
{
    char *p = psz_uri;

    while( (p = strstr( p, psz_name )) )
    {
        /* Verify that we are dealing with a post/get argument */
        if( (p == psz_uri || *(p - 1) == '&' || *(p - 1) == '\n')
              && p[strlen(psz_name)] == '=' )
        {
            return VLC_TRUE;
        }
        p++;
    }

    return VLC_FALSE;
}
char *E_(uri_extract_value)( char *psz_uri, const char *psz_name,
                             char *psz_value, int i_value_max )
{
    char *p = psz_uri;

    while( (p = strstr( p, psz_name )) )
    {
        /* Verify that we are dealing with a post/get argument */
        if( (p == psz_uri || *(p - 1) == '&' || *(p - 1) == '\n')
              && p[strlen(psz_name)] == '=' )
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

void E_(uri_decode_url_encoded)( char *psz )
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

/* Since the resulting string is smaller we can work in place, so it is
 * permitted to have psz == new. new points to the first word of the
 * string, the function returns the remaining string. */
char *E_(FirstWord)( char *psz, char *new )
{
    vlc_bool_t b_end;

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
 * parse_MRL: parse the MRL, find the mrl string and the options,
 * create an item with all information in it, and return the item.
 * return NULL if there is an error.
 **********************************************************************/
playlist_item_t *E_(parse_MRL)( intf_thread_t *p_intf, char *_psz,
                                   char *psz_name )
{
    char *psz = strdup( _psz );
    char *s_mrl = psz;
    char *s_temp;
    playlist_item_t * p_item = NULL;

    /* extract the mrl */
    s_temp = E_(FirstWord)( s_mrl, s_mrl );
    if( s_temp == NULL )
    {
        s_temp = s_mrl + strlen( s_mrl );
    }

    p_item = playlist_ItemNew( p_intf, s_mrl, psz_name );
    s_mrl = s_temp;

    /* now we can take care of the options */
    while( *s_mrl != '\0' )
    {
        s_temp = E_(FirstWord)( s_mrl, s_mrl );
        if( s_mrl == '\0' )
            break;
        if( s_temp == NULL )
        {
            s_temp = s_mrl + strlen( s_mrl );
        }
        if( *s_mrl != ':' )
            msg_Warn( p_intf, "invalid MRL option: %s", s_mrl );
        else
            playlist_ItemAddOption( p_item, s_mrl );
        s_mrl = s_temp;
    }

    free( psz );
    return p_item;
}
