/*****************************************************************************
 * directory.c: expands a directory (directory: access plug-in)
 *****************************************************************************
 * Copyright (C) 2002-2004 VideoLAN
 * $Id$
 *
 * Authors: Derk-Jan Hartman <thedj@users.sourceforge.net>
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
#endif

#if (!defined( WIN32 ) || defined(__MINGW32__))
/* Mingw has its own version of dirent */
#   include <dirent.h>
#endif

/*****************************************************************************
 * Constants and structures
 *****************************************************************************/
#define MODE_EXPAND 0
#define MODE_COLLAPSE 1
#define MODE_NONE 2

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int     Open   ( vlc_object_t * );
static void    Close  ( vlc_object_t * );

static ssize_t Read   ( input_thread_t *, byte_t *, size_t );
int ReadDir( playlist_t *p_playlist, char *psz_name , int i_mode, int *pi_pos );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define RECURSIVE_TEXT N_("Subdirectory behaviour")
#define RECURSIVE_LONGTEXT N_( \
        "Select whether subdirectories must be expanded.\n" \
        "none: subdirectories do not appear in the playlist.\n" \
        "collapse: subdirectories appear but are expanded on first play.\n" \
        "expand: all subdirectories are expanded.\n" )

static char *psz_recursive_list[] = { "none", "collapse", "expand" };
static char *psz_recursive_list_text[] = { N_("none"), N_("collapse"),
                                           N_("expand") };

vlc_module_begin();
    set_description( _("Standard filesystem directory input") );
    set_capability( "access", 55 );
    add_shortcut( "directory" );
    add_shortcut( "dir" );
    add_string( "recursive", "expand" , NULL, RECURSIVE_TEXT,
                RECURSIVE_LONGTEXT, VLC_FALSE );
      change_string_list( psz_recursive_list, psz_recursive_list_text, 0 );
    set_callbacks( Open, Close );
vlc_module_end();


/*****************************************************************************
 * Open: open the directory
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    input_thread_t *            p_input = (input_thread_t *)p_this;
#ifdef HAVE_SYS_STAT_H
    struct stat                 stat_info;
#endif

    /* Initialize access plug-in structures. */
    if( p_input->i_mtu == 0 )
    {
        /* Improve speed. */
        p_input->i_bufsize = INPUT_DEFAULT_BUFSIZE;
    }

    p_input->pf_read = Read;
    p_input->pf_set_program = NULL;
    p_input->pf_set_area = NULL;
    p_input->pf_seek = NULL;

#ifdef HAVE_SYS_STAT_H
    if( ( stat( p_input->psz_name, &stat_info ) == -1 ) ||
        !S_ISDIR( stat_info.st_mode ) )
#else
    if( !p_input->psz_access || strcmp(p_input->psz_access, "dir") )
#endif
    {
        return VLC_EGENERIC;
    }

    /* Force a demux */
    p_input->psz_demux = "dummy";

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close: close the target
 *****************************************************************************/
static void Close( vlc_object_t * p_this )
{
    input_thread_t * p_input = (input_thread_t *)p_this;

    msg_Info( p_input, "closing `%s/%s://%s'",
              p_input->psz_access, p_input->psz_demux, p_input->psz_name );
}

/*****************************************************************************
 * Read: read the directory
 *****************************************************************************/
static ssize_t Read( input_thread_t * p_input, byte_t * p_buffer, size_t i_len )
{
    char *                      psz_name;
    char *                      psz_mode;
    int                         i_mode, i_pos;

    playlist_t * p_playlist = (playlist_t *) vlc_object_find(
                        p_input, VLC_OBJECT_PLAYLIST, FIND_ANYWHERE );

    if( !p_playlist )
    {
        msg_Err( p_input, "can't find playlist" );
        goto end;
    }
    
    /* Remove the ending '/' char */
    psz_name = strdup( p_input->psz_name );
    if( psz_name == NULL )
        goto end;

    if( (psz_name[strlen(psz_name)-1] == '/') ||
        (psz_name[strlen(psz_name)-1] == '\\') )
    {
        psz_name[strlen(psz_name)-1] = '\0';
    }
    
    /* Initialize structure */
    psz_mode = config_GetPsz( p_input , "recursive" );
    if( !psz_mode || !strncmp( psz_mode, "none" , 4 )  )
    {
        i_mode = MODE_NONE;
    }
    else if( !strncmp( psz_mode, "collapse", 8 )  )
    {
        i_mode = MODE_COLLAPSE;
    }
    else
    {
        i_mode = MODE_EXPAND;
    }
    
    /* Make sure we are deleted when we are done */
    p_playlist->pp_items[p_playlist->i_index]->b_autodeletion = VLC_TRUE;
    /* The playlist position we will use for the add */
    i_pos = p_playlist->i_index + 1;

    msg_Dbg( p_input, "opening directory `%s'", psz_name );
    if( ReadDir( p_playlist, psz_name , i_mode, &i_pos ) != VLC_SUCCESS )
    {
        free( psz_name );
        goto end;
    }

end:
    vlc_object_release( p_playlist );
    p_input->b_eof = 1;
    return 0;
}

/* Local functions */

/*****************************************************************************
 * ReadDir: read a directory and add its content to the list
 *****************************************************************************/
int ReadDir( playlist_t *p_playlist, char *psz_name , int i_mode, int *pi_position )
{
    DIR *                       p_current_dir;
    struct dirent *             p_dir_content;

    /* Open the dir */
    p_current_dir = opendir( psz_name );

    if( p_current_dir == NULL )
    {
        /* something went bad, get out of here ! */
#   ifdef HAVE_ERRNO_H
        msg_Warn( p_playlist, "cannot open directory `%s' (%s)",
                  psz_name, strerror(errno));
#   else
        msg_Warn( p_playlist, "cannot open directory `%s'", psz_name );
#   endif
        return VLC_EGENERIC;
    }

    /* get the first directory entry */
    p_dir_content = readdir( p_current_dir );

    /* while we still have entries in the directory */
    while( p_dir_content != NULL )
    {
        int i_size_entry = strlen( psz_name ) +
                           p_dir_content->d_namlen + 2;
        char *psz_uri = (char *)malloc( sizeof(char)*i_size_entry);

        sprintf( psz_uri, "%s/%s", psz_name, p_dir_content->d_name );

        /* if it starts with '.' then forget it */
        if( p_dir_content->d_name[0] != '.' )
        {
#if defined( S_ISDIR )
            struct stat stat_data;
            stat( psz_uri, &stat_data );
            if( S_ISDIR(stat_data.st_mode) && i_mode != MODE_COLLAPSE )
#elif defined( DT_DIR )
            if( p_dir_content->d_type == DT_DIR && i_mode != MODE_COLLAPSE )
#else
            if( 0 )
#endif
            {
                if( i_mode == MODE_NONE )
                {
                    msg_Dbg( p_playlist, "Skipping subdirectory %s", psz_uri );
                    p_dir_content = readdir( p_current_dir );
                    continue;
                }
                else if(i_mode == MODE_EXPAND )
                {
                    msg_Dbg(p_playlist, "Reading subdirectory %s", psz_uri );
                    if( ReadDir( p_playlist, psz_uri , MODE_EXPAND, pi_position )
                                 != VLC_SUCCESS )
                    {
                        return VLC_EGENERIC;
                    }
                }
            }
            else
            {
                playlist_Add( p_playlist, psz_uri, p_dir_content->d_name,
                          PLAYLIST_INSERT, *pi_position );
                (*pi_position)++;
            }
        }
        free( psz_uri );
        p_dir_content = readdir( p_current_dir );
    }
    closedir( p_current_dir );
    return VLC_SUCCESS;
}
