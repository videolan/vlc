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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#include <vlc/vlc.h>
#include <vlc_playlist.h>
#include <vlc_input.h>
#include <vlc_access.h>
#include <vlc_demux.h>

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

#include <vlc_charset.h>

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

static const char *psz_recursive_list[] = { "none", "collapse", "expand" };
static const char *psz_recursive_list_text[] = { N_("none"), N_("collapse"),
                                                 N_("expand") };

#define IGNORE_TEXT N_("Ignored extensions")
#define IGNORE_LONGTEXT N_( \
        "Files with these extensions will not be added to playlist when " \
        "opening a directory.\n" \
        "This is useful if you add directories that contain playlist files " \
        "for instance. Use a comma-separated list of extensions." )

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
    add_string( "ignore-filetypes", "m3u,db,nfo,jpg,gif,sfv,txt,sub,idx,srt,cue",
                NULL, IGNORE_TEXT, IGNORE_LONGTEXT, VLC_FALSE );
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


static int ReadDir( playlist_t *, const char *psz_name, int i_mode,
                    playlist_item_t *, playlist_item_t *, input_item_t * );

/*****************************************************************************
 * Open: open the directory
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    access_t *p_access = (access_t*)p_this;

    struct stat stat_info;

#ifdef S_ISDIR
    if (utf8_stat (p_access->psz_path, &stat_info)
     || !S_ISDIR (stat_info.st_mode))
#else
    if( strcmp( p_access->psz_access, "dir") &&
        strcmp( p_access->psz_access, "directory") )
#endif
        return VLC_EGENERIC;

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
    char               *psz;
    int                 i_mode, i_activity;
    playlist_t         *p_playlist = pl_Yield( p_access );
    playlist_item_t    *p_item_in_category;
    input_item_t       *p_current_input = input_GetItem(
                                    (input_thread_t*)p_access->p_parent);
    playlist_item_t    *p_current = playlist_ItemGetByInput( p_playlist,
                                                             p_current_input,
                                                             VLC_FALSE );
    char               *psz_name = strdup (p_access->psz_path);

    if( psz_name == NULL )
        return VLC_ENOMEM;

    if( p_current == NULL ) {
        msg_Err( p_access, "unable to find item in playlist" );
        return VLC_ENOOBJ;
    }

    /* Remove the ending '/' char */
    if (psz_name[0])
    {
        char *ptr = psz_name + strlen (psz_name);
        switch (*--ptr)
        {
            case '/':
            case '\\':
                *ptr = '\0';
        }
    }

    /* Handle mode */
    psz = var_CreateGetString( p_access, "recursive" );
    if( *psz == '\0' || !strncmp( psz, "none" , 4 )  )
        i_mode = MODE_NONE;
    else if( !strncmp( psz, "collapse", 8 )  )
        i_mode = MODE_COLLAPSE;
    else
        i_mode = MODE_EXPAND;
    free( psz );

    msg_Dbg( p_access, "opening directory `%s'", p_access->psz_path );

    p_current->p_input->i_type = ITEM_TYPE_DIRECTORY;
    p_item_in_category = playlist_ItemToNode( p_playlist, p_current,
                                              VLC_FALSE );

    i_activity = var_GetInteger( p_playlist, "activity" );
    var_SetInteger( p_playlist, "activity", i_activity +
                    DIRECTORY_ACTIVITY );

    ReadDir( p_playlist, psz_name, i_mode, p_current, p_item_in_category,
             p_current_input );

    i_activity = var_GetInteger( p_playlist, "activity" );
    var_SetInteger( p_playlist, "activity", i_activity -
                    DIRECTORY_ACTIVITY );

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


static int Sort (const char **a, const char **b)
{
    return strcoll (*a, *b);
}

/*****************************************************************************
 * ReadDir: read a directory and add its content to the list
 *****************************************************************************/
static int ReadDir( playlist_t *p_playlist, const char *psz_name,
                    int i_mode, playlist_item_t *p_parent,
                    playlist_item_t *p_parent_category,
                    input_item_t *p_current_input )
{
    char **pp_dir_content = NULL;
    int             i_dir_content, i, i_return = VLC_SUCCESS;
    playlist_item_t *p_node;

    char **ppsz_extensions = NULL;
    int i_extensions = 0;
    char *psz_ignore;

    /* Get the first directory entry */
    i_dir_content = utf8_scandir (psz_name, &pp_dir_content, NULL, Sort);
    if( i_dir_content == -1 )
    {
        msg_Warn( p_playlist, "failed to read directory" );
        return VLC_EGENERIC;
    }
    else if( i_dir_content <= 0 )
    {
        /* directory is empty */
        if( pp_dir_content ) free( pp_dir_content );
        return VLC_SUCCESS;
    }

    /* Build array with ignores */
    psz_ignore = var_CreateGetString( p_playlist, "ignore-filetypes" );
    if( psz_ignore && *psz_ignore )
    {
        char *psz_parser = psz_ignore;
        int a;

        for( a = 0; psz_parser[a] != '\0'; a++ )
        {
            if( psz_parser[a] == ',' ) i_extensions++;
        }

        ppsz_extensions = (char **)calloc (i_extensions, sizeof (char *));

        for( a = 0; a < i_extensions; a++ )
        {
            char *tmp, *ptr;

            while( psz_parser[0] != '\0' && psz_parser[0] == ' ' ) psz_parser++;
            ptr = strchr( psz_parser, ',');
            tmp = ( ptr == NULL )
                 ? strdup( psz_parser )
                 : strndup( psz_parser, ptr - psz_parser );

            ppsz_extensions[a] = tmp;
            psz_parser = ptr + 1;
        }
    }
    if( psz_ignore ) free( psz_ignore );

    /* While we still have entries in the directory */
    for( i = 0; i < i_dir_content; i++ )
    {
        const char *entry = pp_dir_content[i];
        int i_size_entry = strlen( psz_name ) +
                           strlen( entry ) + 2;
        char psz_uri[i_size_entry];
        char psz_mrl[i_size_entry + 7]; /* "file://psz_uri" */

        sprintf( psz_uri, "%s/%s", psz_name, entry);

        /* if it starts with '.' then forget it */
        if (entry[0] != '.')
        {
#if defined( S_ISDIR )
            struct stat stat_data;

            if (!utf8_stat (psz_uri, &stat_data)
             && S_ISDIR(stat_data.st_mode) && i_mode != MODE_COLLAPSE )
#else
            if( 0 )
#endif
            {
#if defined( S_ISLNK )
/*
 * FIXME: there is a ToCToU race condition here; but it is rather tricky
 * impossible to fix while keeping some kind of portable code, and maybe even
 * in a non-portable way.
 */
                if (utf8_lstat (psz_uri, &stat_data)
                 || S_ISLNK(stat_data.st_mode) )
                {
                    msg_Dbg( p_playlist, "skipping directory symlink %s",
                             psz_uri );
                    continue;
                }
#endif
                if( i_mode == MODE_NONE )
                {
                    msg_Dbg( p_playlist, "skipping subdirectory %s", psz_uri );
                    continue;
                }
                else if( i_mode == MODE_EXPAND )
                {
                    msg_Dbg(p_playlist, "creading subdirectory %s", psz_uri );

                    p_node = playlist_NodeCreate (p_playlist, entry,
                                                  p_parent_category);

                    /* If we had the parent in category, the it is now node.
                     * Else, we still don't have  */
                    if( ReadDir( p_playlist, psz_uri , MODE_EXPAND,
                                 p_node, p_parent_category ? p_node : NULL,
                                 p_current_input )
                          != VLC_SUCCESS )
                    {
                        i_return = VLC_EGENERIC;
                        break;
                    }
                }
            }
            else
            {
                input_item_t *p_input;

                if( i_extensions > 0 )
                {
                    const char *psz_dot = strrchr (entry, '.' );
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
                            msg_Dbg( p_playlist, "ignoring file %s", psz_uri );
                            continue;
                        }
                    }
                }

                sprintf( &psz_mrl, "file://%s", psz_uri );
                p_input = input_ItemNewWithType( VLC_OBJECT(p_playlist),
                                                 psz_mrl, entry, 0, NULL,
                                                 -1, ITEM_TYPE_VFILE );
                if (p_input != NULL)
                {
                    if( p_current_input )
                        input_ItemCopyOptions( p_current_input, p_input );
                    playlist_BothAddInput( p_playlist, p_input,
                                           p_parent_category,
                                           PLAYLIST_APPEND|PLAYLIST_PREPARSE,
                                           PLAYLIST_END, NULL, NULL );
                }
            }
        }
    }

    for( i = 0; i < i_extensions; i++ )
        if( ppsz_extensions[i] ) free( ppsz_extensions[i] );
    if( ppsz_extensions ) free( ppsz_extensions );

    for( i = 0; i < i_dir_content; i++ )
        if( pp_dir_content[i] ) free( pp_dir_content[i] );
    if( pp_dir_content ) free( pp_dir_content );

    return i_return;
}
