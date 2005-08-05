/*****************************************************************************
 * directory.c: expands a directory (directory: access plug-in)
 *****************************************************************************
 * Copyright (C) 2002-2004 the VideoLAN team
 * $Id$
 *
 * Authors: Derk-Jan Hartman <hartman at videolan dot org>
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

#include <vlc/vlc.h>
#include <vlc/input.h>
#include <vlc_playlist.h>

#include <stdlib.h>
#include <string.h>
#ifdef HAVE_SYS_TYPES_H
#   include <sys/types.h>
#endif
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
#elif defined( UNDER_CE )
#   define strcoll strcmp
#endif

#ifdef HAVE_DIRENT_H
#   include <dirent.h>
#endif

#include "charset.h"

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

static int  DemuxOpen ( vlc_object_t * );

#define RECURSIVE_TEXT N_("Subdirectory behavior")
#define RECURSIVE_LONGTEXT N_( \
        "Select whether subdirectories must be expanded.\n" \
        "none: subdirectories do not appear in the playlist.\n" \
        "collapse: subdirectories appear but are expanded on first play.\n" \
        "expand: all subdirectories are expanded.\n" )

static char *psz_recursive_list[] = { "none", "collapse", "expand" };
static char *psz_recursive_list_text[] = { N_("none"), N_("collapse"),
                                           N_("expand") };

#define IGNORE_TEXT N_("Ignore files with these extensions")
#define IGNORE_LONGTEXT N_( \
        "Specify a comma seperated list of file extensions. " \
        "Files with these extensions will not be added to playlist when opening a directory. " \
        "This is useful if you add directories that contain mp3 albums for instance." )

vlc_module_begin();
    set_category( CAT_INPUT );
    set_shortname( _("Directory" ) );
    set_subcategory( SUBCAT_INPUT_ACCESS );
    set_description( _("Standard filesystem directory input") );
    set_capability( "access2", 55 );
    add_shortcut( "directory" );
    add_shortcut( "dir" );
    add_string( "recursive", "expand" , NULL, RECURSIVE_TEXT,
                RECURSIVE_LONGTEXT, VLC_FALSE );
      change_string_list( psz_recursive_list, psz_recursive_list_text, 0 );
#ifdef HAVE_STRSEP
    add_string( "ignore-filetypes", "m3u,db,nfo,jpg,gif,sfv,txt,sub,idx,srt,cue",
                NULL, IGNORE_TEXT, IGNORE_LONGTEXT, VLC_FALSE );
#endif
    set_callbacks( Open, Close );

    add_submodule();
        set_description( "Directory EOF");
        set_capability( "demux2", 0 );
        add_shortcut( "directory" );
        set_callbacks( DemuxOpen, NULL );
vlc_module_end();


/*****************************************************************************
 * Local prototypes, constants, structures
 *****************************************************************************/

#define MODE_EXPAND 0
#define MODE_COLLAPSE 1
#define MODE_NONE 2

static int Read( access_t *, uint8_t *, int );
static int ReadNull( access_t *, uint8_t *, int );
static int Control( access_t *, int, va_list );

static int Demux( demux_t *p_demux );
static int DemuxControl( demux_t *p_demux, int i_query, va_list args );


static int ReadDir( playlist_t *, char *psz_name, int i_mode, int *pi_pos,
                    playlist_item_t * );

/*****************************************************************************
 * Open: open the directory
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    access_t *p_access = (access_t*)p_this;

#ifdef HAVE_SYS_STAT_H
    struct stat stat_info;

    if( ( stat( p_access->psz_path, &stat_info ) == -1 ) ||
        !S_ISDIR( stat_info.st_mode ) )

#elif defined(WIN32)
    int i_ret;

#   ifdef UNICODE
    wchar_t psz_path[MAX_PATH];
    mbstowcs( psz_path, p_access->psz_path, MAX_PATH );
    psz_path[MAX_PATH-1] = 0;
#   else
    char *psz_path = p_access->psz_path;
#   endif /* UNICODE */

    i_ret = GetFileAttributes( psz_path );
    if( i_ret == -1 || !(i_ret & FILE_ATTRIBUTE_DIRECTORY) )

#else
    if( strcmp( p_access->psz_access, "dir") &&
        strcmp( p_access->psz_access, "directory") )
#endif
    {
        return VLC_EGENERIC;
    }

    p_access->pf_read  = Read;
    p_access->pf_block = NULL;
    p_access->pf_seek  = NULL;
    p_access->pf_control= Control;

    /* Force a demux */
    p_access->psz_demux = strdup( "directory" );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close: close the target
 *****************************************************************************/
static void Close( vlc_object_t * p_this )
{
}

/*****************************************************************************
 * ReadNull: read the directory
 *****************************************************************************/
static int ReadNull( access_t *p_access, uint8_t *p_buffer, int i_len)
{
    /* Return fake data */
    memset( p_buffer, 0, i_len );
    return i_len;
}

/*****************************************************************************
 * Read: read the directory
 *****************************************************************************/
static int Read( access_t *p_access, uint8_t *p_buffer, int i_len)
{
    char *psz_name = NULL;
    char *psz;
    int  i_mode, i_pos;

    playlist_item_t *p_item;
    vlc_bool_t b_play = VLC_FALSE;

    playlist_t *p_playlist =
        (playlist_t *) vlc_object_find( p_access,
                                        VLC_OBJECT_PLAYLIST, FIND_ANYWHERE );

    if( !p_playlist )
    {
        msg_Err( p_access, "can't find playlist" );
        goto end;
    }

    /* Remove the ending '/' char */
    psz_name = strdup( p_access->psz_path );
    if( psz_name == NULL )
        goto end;

    if( (psz_name[strlen(psz_name)-1] == '/') ||
        (psz_name[strlen(psz_name)-1] == '\\') )
    {
        psz_name[strlen(psz_name)-1] = '\0';
    }

    /* Initialize structure */
    psz = var_CreateGetString( p_access, "recursive" );
    if( *psz == '\0' || !strncmp( psz, "none" , 4 )  )
    {
        i_mode = MODE_NONE;
    }
    else if( !strncmp( psz, "collapse", 8 )  )
    {
        i_mode = MODE_COLLAPSE;
    }
    else
    {
        i_mode = MODE_EXPAND;
    }
    free( psz );

    /* Make sure we are deleted when we are done */
    /* The playlist position we will use for the add */
    i_pos = p_playlist->i_index + 1;

    msg_Dbg( p_access, "opening directory `%s'", psz_name );

    if( &p_playlist->status.p_item->input ==
        ((input_thread_t *)p_access->p_parent)->input.p_item )
    {
        p_item = p_playlist->status.p_item;
        b_play = VLC_TRUE;
        msg_Dbg( p_access, "starting directory playback");
    }
    else
    {
        input_item_t *p_current = ( (input_thread_t*)p_access->p_parent)->
                                                        input.p_item;
        p_item = playlist_LockItemGetByInput( p_playlist, p_current );
        msg_Dbg( p_access, "not starting directory playback");
        if( !p_item )
        {
            msg_Dbg( p_playlist, "unable to find item in playlist");
            return -1;
        }
        b_play = VLC_FALSE;
    }

    p_item->input.i_type = ITEM_TYPE_DIRECTORY;
    if( ReadDir( p_playlist, psz_name , i_mode, &i_pos,
                 p_item ) != VLC_SUCCESS )
    {
    }
end:

    /* Begin to read the directory */
    if( b_play )
    {
        playlist_Control( p_playlist, PLAYLIST_VIEWPLAY,
                          p_playlist->status.i_view,
                          p_playlist->status.p_item, NULL );
    }
    if( psz_name ) free( psz_name );
    vlc_object_release( p_playlist );

    /* Return fake data forever */
    p_access->pf_read = ReadNull;
    return ReadNull( p_access, p_buffer, i_len );
}

/*****************************************************************************
 * DemuxOpen:
 *****************************************************************************/
static int Control( access_t *p_access, int i_query, va_list args )
{
    vlc_bool_t   *pb_bool;
    int          *pi_int;
    int64_t      *pi_64;

    switch( i_query )
    {
        /* */
        case ACCESS_CAN_SEEK:
        case ACCESS_CAN_FASTSEEK:
        case ACCESS_CAN_PAUSE:
        case ACCESS_CAN_CONTROL_PACE:
            pb_bool = (vlc_bool_t*)va_arg( args, vlc_bool_t* );
            *pb_bool = VLC_FALSE;    /* FIXME */
            break;

        /* */
        case ACCESS_GET_MTU:
            pi_int = (int*)va_arg( args, int * );
            *pi_int = 0;
            break;

        case ACCESS_GET_PTS_DELAY:
            pi_64 = (int64_t*)va_arg( args, int64_t * );
            *pi_64 = DEFAULT_PTS_DELAY * 1000;
            break;

        /* */
        case ACCESS_SET_PAUSE_STATE:
        case ACCESS_GET_TITLE_INFO:
        case ACCESS_SET_TITLE:
        case ACCESS_SET_SEEKPOINT:
        case ACCESS_SET_PRIVATE_ID_STATE:
            return VLC_EGENERIC;

        default:
            msg_Warn( p_access, "unimplemented query in control" );
            return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}

/*****************************************************************************
 * DemuxOpen:
 *****************************************************************************/
static int DemuxOpen ( vlc_object_t *p_this )
{
    demux_t *p_demux = (demux_t*)p_this;

    if( strcmp( p_demux->psz_demux, "directory" ) )
        return VLC_EGENERIC;

    p_demux->pf_demux   = Demux;
    p_demux->pf_control = DemuxControl;
    return VLC_SUCCESS;
}

/*****************************************************************************
 * Demux: EOF
 *****************************************************************************/
static int Demux( demux_t *p_demux )
{
    return 0;
}
/*****************************************************************************
 * DemuxControl:
 *****************************************************************************/
static int DemuxControl( demux_t *p_demux, int i_query, va_list args )
{
    return demux2_vaControlHelper( p_demux->s, 0, 0, 0, 1, i_query, args );
}

static int Filter( const struct dirent *foo )
{
    return VLC_TRUE;
}
/*****************************************************************************
 * ReadDir: read a directory and add its content to the list
 *****************************************************************************/
static int ReadDir( playlist_t *p_playlist,
                    char *psz_name, int i_mode, int *pi_position,
                    playlist_item_t *p_parent )
{
    struct dirent   **pp_dir_content;
    int             i_dir_content, i;
    playlist_item_t *p_node;

    /* Build array with ignores */
#ifdef HAVE_STRSEP
    char **ppsz_extensions = 0;
    int i_extensions = 0;
    char *psz_ignore = var_CreateGetString( p_playlist, "ignore-filetypes" );
    if( psz_ignore && *psz_ignore )
    {
        char *psz_backup;
        char *psz_parser = psz_backup = strdup( psz_ignore );
        int a = 0;

        while( strsep( &psz_parser, "," ) ) i_extensions++;
        free( psz_backup );

        ppsz_extensions = (char **)malloc( sizeof( char * ) * i_extensions );

        psz_parser = psz_ignore;
        while( a < i_extensions &&
               ( ppsz_extensions[a++] = strsep( &psz_parser, "," ) ) );
    }
#endif /* HAVE_STRSEP */

    /* Change the item to a node */
    if( p_parent->i_children == -1 )
    {
        playlist_LockItemToNode( p_playlist,p_parent );
    }

    /* get the first directory entry */
    i_dir_content = scandir( psz_name, &pp_dir_content, Filter, alphasort );
    if( i_dir_content == -1 )
    {
        msg_Warn( p_playlist, "Failed to read directory" );
        return VLC_EGENERIC;
    }
    else if( i_dir_content <= 0 )
    {
        /* directory is empty */
        return VLC_SUCCESS;
    }

    /* While we still have entries in the directory */
    for( i = 0; i < i_dir_content; i++ )
    {
        struct dirent *p_dir_content = pp_dir_content[i];
        int i_size_entry = strlen( psz_name ) +
                           strlen( p_dir_content->d_name ) + 2;
        char *psz_uri = (char *)malloc( sizeof(char) * i_size_entry );

        sprintf( psz_uri, "%s/%s", psz_name, p_dir_content->d_name );

        /* if it starts with '.' then forget it */
        if( p_dir_content->d_name[0] != '.' )
        {
#if defined( S_ISDIR )
            struct stat stat_data;
            stat( psz_uri, &stat_data );
            if( S_ISDIR(stat_data.st_mode) && i_mode != MODE_COLLAPSE )
#elif defined( DT_DIR )
            if( ( p_dir_content->d_type & DT_DIR ) && i_mode != MODE_COLLAPSE )
#else
            if( 0 )
#endif
            {
                if( i_mode == MODE_NONE )
                {
                    msg_Dbg( p_playlist, "Skipping subdirectory %s", psz_uri );
                    free( psz_uri );
                    continue;
                }
                else if( i_mode == MODE_EXPAND )
                {
                    char *psz_newname;
                    msg_Dbg(p_playlist, "Reading subdirectory %s", psz_uri );

                    if( !strncmp( psz_uri, psz_name, strlen( psz_name ) ) )
                    {
                        char *psz_subdir = psz_uri;
                        /* Skip the parent path + the separator */
                        psz_subdir += strlen( psz_name ) + 1;
                        psz_newname = vlc_fix_readdir_charset(
                                                p_playlist, psz_subdir );
                    }
                    else
                    {
                        psz_newname = vlc_fix_readdir_charset(
                                                p_playlist, psz_name );
                    }
                    p_node = playlist_NodeCreate( p_playlist,
                                       p_parent->pp_parents[0]->i_view,
                                       psz_newname, p_parent );

                    playlist_CopyParents(  p_parent, p_node );

                    p_node->input.i_type = ITEM_TYPE_DIRECTORY;

                    if( ReadDir( p_playlist, psz_uri , MODE_EXPAND,
                                 pi_position, p_node ) != VLC_SUCCESS )
                    {
                        return VLC_EGENERIC;
                    }

                    free( psz_newname );
                }
            }
            else
            {
                playlist_item_t *p_item;
                char *psz_tmp1, *psz_tmp2;

#ifdef HAVE_STRSEP
                if( i_extensions > 0 )
                {
                    char *psz_dot = strrchr( p_dir_content->d_name, '.' );
                    if( psz_dot++ && *psz_dot )
                    {
                        int a;
                        for( a = 0; a < i_extensions; a++ )
                        {
                            if( !strcmp( psz_dot, ppsz_extensions[a] ) )
                                break;
                        }
                        if( a < i_extensions )
                        {
                            msg_Dbg( p_playlist, "Ignoring file %s", psz_uri );
                            free( psz_uri );
                            continue;
                        }
                    }
                }
#endif /* HAVE_STRSEP */

                psz_tmp1 = vlc_fix_readdir_charset( VLC_OBJECT(p_playlist),
                                                    psz_uri );
                psz_tmp2 = vlc_fix_readdir_charset( VLC_OBJECT(p_playlist),
                                                    p_dir_content->d_name );

                p_item = playlist_ItemNewWithType( VLC_OBJECT(p_playlist),
                        psz_tmp1, psz_tmp2, ITEM_TYPE_VFILE );
                playlist_NodeAddItem( p_playlist,p_item,
                                      p_parent->pp_parents[0]->i_view,
                                      p_parent,
                                      PLAYLIST_APPEND, PLAYLIST_END );

                playlist_CopyParents( p_parent, p_item );
            }
        }
        free( psz_uri );
    }

#ifdef HAVE_STRSEP
    if( ppsz_extensions ) free( ppsz_extensions );
    if( psz_ignore ) free( psz_ignore );
#endif /* HAVE_STRSEP */

    free( pp_dir_content );
    return VLC_SUCCESS;
}
