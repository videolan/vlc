/*****************************************************************************
 * directory.c: expands a directory (directory: access plug-in)
 *****************************************************************************
 * Copyright (C) 2001, 2002 VideoLAN
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
#define MAX_DIR_SIZE 100000

#define MODE_EXPAND 0
#define MODE_COLLAPSE 1
#define MODE_NONE 2

typedef struct input_directory_s
{
    char   p_dir_buffer[MAX_DIR_SIZE];
    int    i_buf_pos;
    int    i_buf_length;
    int    i_pos;
} input_directory_t;


/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int     Open   ( vlc_object_t * );
static void    Close  ( vlc_object_t * );

static ssize_t Read   ( input_thread_t *, byte_t *, size_t );
int ReadDir( input_thread_t *p_input, char *psz_name , int i_mode );

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
    char *                      psz_name;
    input_directory_t *         p_access_data;
    char *                      psz_mode;
    int                         i_mode;
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

    /* Remove the ending '/' char */
    psz_name = strdup( p_input->psz_name );
    if( psz_name == NULL )
        return VLC_EGENERIC;

    if( (psz_name[strlen(psz_name)-1] == '/') ||
        (psz_name[strlen(psz_name)-1] == '\\') )
    {
        psz_name[strlen(psz_name)-1] = '\0';
    }

#ifdef HAVE_SYS_STAT_H
    if( ( stat( psz_name, &stat_info ) == -1 ) ||
        !S_ISDIR( stat_info.st_mode ) )
#else
    if( !p_input->psz_access || strcmp(p_input->psz_access, "dir") )
#endif
    {
        free( psz_name );
        return VLC_EGENERIC;
    }

    /* Initialize structure */
    msg_Dbg( p_input, "opening directory `%s'", psz_name );
    p_access_data = malloc( sizeof(input_directory_t) );
    p_input->p_access_data = (void *)p_access_data;
    if( p_access_data == NULL )
    {
        msg_Err( p_input, "out of memory" );
        free( psz_name );
        return VLC_ENOMEM;
    }
    p_access_data->i_pos = 0;

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

    if( ReadDir( p_input, psz_name , i_mode ) != VLC_SUCCESS )
    {
        free( p_access_data );
        free( psz_name );
        return VLC_EGENERIC;
    }

    msg_Dbg(p_input,"Directory read complete. Read %i bytes",
                    p_access_data->i_pos);

    p_access_data->p_dir_buffer[p_access_data->i_pos] = '\0';
    p_access_data->i_pos++;
    p_access_data->i_buf_length = p_access_data->i_pos;
    p_access_data->i_buf_pos = 0;

    /* Force m3u demuxer */
    p_input->psz_demux = "m3u";

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close: close the target
 *****************************************************************************/
static void Close( vlc_object_t * p_this )
{
    input_thread_t * p_input = (input_thread_t *)p_this;
    input_directory_t * p_access_data =
        (input_directory_t *)p_input->p_access_data;

    msg_Info( p_input, "closing `%s/%s://%s'",
              p_input->psz_access, p_input->psz_demux, p_input->psz_name );

    free( p_access_data );
}

/*****************************************************************************
 * Read: read directory and output to demux.
 *****************************************************************************/
static ssize_t Read( input_thread_t * p_input, byte_t * p_buffer, size_t i_len )
{
    input_directory_t * p_access_data =
        (input_directory_t *)p_input->p_access_data;
    unsigned int i_remaining = p_access_data->i_buf_length -
                               p_access_data->i_buf_pos;

    if( i_remaining > 0 )
    {
        int i_ret;

        i_ret = __MIN( i_len, i_remaining );
        memcpy( p_buffer,
                &p_access_data->p_dir_buffer[p_access_data->i_buf_pos],
                i_ret );
        p_access_data->i_buf_pos += i_ret;
        return (ssize_t) i_ret;
    }
    return 0;
}

/* Local functions */

/*****************************************************************************
 * ReadDir: read a directory and add its content to the list
 *****************************************************************************/
int ReadDir( input_thread_t *p_input, char *psz_name , int i_mode )
{
    DIR *                       p_current_dir;
    struct dirent *             p_dir_content;

    input_directory_t * p_access_data =
        (input_directory_t *)p_input->p_access_data;

    /* have to cd into this dir */
    p_current_dir = opendir( psz_name );

    if( p_current_dir == NULL )
    {
        /* something went bad, get out of here ! */
#   ifdef HAVE_ERRNO_H
        msg_Warn( p_input, "cannot open directory `%s' (%s)",
                  psz_name, strerror(errno));
#   else
        msg_Warn( p_input, "cannot open directory `%s'", psz_name );
#   endif
        return VLC_EGENERIC;
    }

    p_dir_content = readdir( p_current_dir );

    /* while we still have entries in the directory */
    while( p_dir_content != NULL && p_access_data->i_pos < MAX_DIR_SIZE )
    {
        int i_size_entry = strlen( psz_name ) +
                           strlen( p_dir_content->d_name ) + 2;
        char *psz_entry = (char *)malloc( sizeof(char)*i_size_entry);

        sprintf( psz_entry, "%s/%s",psz_name,p_dir_content->d_name);

#if 0 /* Disable this message, it makes too much output */
        msg_Dbg( p_input, "Entry %s",psz_entry );
#endif

        /* if it is "." or "..", forget it */
        if( strcmp( p_dir_content->d_name, "." ) &&
            strcmp( p_dir_content->d_name, ".." ) &&
            p_access_data->i_pos + i_size_entry < MAX_DIR_SIZE )
        {
#if defined( S_ISDIR )
            struct stat stat_data;
            stat( psz_entry, &stat_data );
            if( S_ISDIR(stat_data.st_mode) )
#elif defined( DT_DIR )
            if( p_dir_content->d_type == DT_DIR )
#else
            if( 0 )
#endif
            {
                if( i_mode == MODE_NONE )
                {
                    msg_Dbg( p_input, "Skipping subdirectory %s", psz_entry );
                    p_dir_content = readdir( p_current_dir );
                    continue;
                }
                else if(i_mode == MODE_EXPAND )
                {
                    msg_Dbg(p_input, "Reading subdirectory %s", psz_entry );
                    if( ReadDir( p_input, psz_entry , MODE_EXPAND )
                                 != VLC_SUCCESS )
                    {
                        return VLC_EGENERIC;
                    }
                }
                else
                {
                    msg_Dbg(p_input, "Adding subdirectory %s", psz_entry );
                    sprintf( &p_access_data->p_dir_buffer[p_access_data->i_pos],
                             "%s", psz_entry );
                    p_access_data->i_pos += i_size_entry -1 ;
                    p_access_data->p_dir_buffer[p_access_data->i_pos] = '\n';
                    p_access_data->i_pos++;
                }
            }
            else
            {
                sprintf( &p_access_data->p_dir_buffer[p_access_data->i_pos],
                         "%s", psz_entry );
                p_access_data->i_pos += i_size_entry - 1;
                p_access_data->p_dir_buffer[p_access_data->i_pos] = '\n';
                p_access_data->i_pos++;
            }
        }
        free( psz_entry );
        p_dir_content = readdir( p_current_dir );
    }
    closedir( p_current_dir );
    return VLC_SUCCESS;
}
