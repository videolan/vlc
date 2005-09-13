/*****************************************************************************
 * gnomevfs.c: GnomeVFS input
 *****************************************************************************
 * Copyright (C) 2005 the VideoLAN team
 * $Id$
 *
 * Authors: Benjamin Pracht <bigben -AT- videolan -DOT- org>
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
#include <libgnomevfs/gnome-vfs.h>

#if 0
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#ifdef HAVE_SYS_TYPES_H
#   include <sys/types.h>
#endif
#ifdef HAVE_SYS_TIME_H
#   include <sys/time.h>
#endif
#ifdef HAVE_SYS_STAT_H
#   include <sys/stat.h>
#endif
#ifdef HAVE_FCNTL_H
#   include <fcntl.h>
#endif

#ifdef HAVE_UNISTD_H
#   include <unistd.h>
#endif

#endif

#include "charset.h"

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

#define CACHING_TEXT N_("Caching value in ms")
#define CACHING_LONGTEXT N_( \
    "Allows you to modify the default caching value for GnomeVFS streams."\
    "This value should be set in millisecond units." )

vlc_module_begin();
    set_description( _("GnomeVFS filesystem file input") );
    set_shortname( _("GnomeVFS") );
    set_category( CAT_INPUT );
    set_subcategory( SUBCAT_INPUT_ACCESS );
    add_integer( "gnomevfs-caching", DEFAULT_PTS_DELAY / 1000, NULL, CACHING_TEXT, CACHING_LONGTEXT, VLC_TRUE );
    set_capability( "access2", 10 );
    add_shortcut( "gnomevfs" );
    set_callbacks( Open, Close );
vlc_module_end();


/*****************************************************************************
 * Exported prototypes
 *****************************************************************************/
static int  Seek( access_t *, int64_t );
static int  Read( access_t *, uint8_t *, int );
static int  Control( access_t *, int, va_list );

struct access_sys_t
{
    unsigned int i_nb_reads;
    char *psz_name;

    GnomeVFSHandle *p_handle;
    GnomeVFSFileInfo *p_file_info;

    vlc_bool_t b_local;
    vlc_bool_t b_seekable;
    vlc_bool_t b_pace_control;
};

/*****************************************************************************
 * Open: open the file
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    access_t     *p_access = (access_t*)p_this;
    access_sys_t *p_sys;
    char *psz_name;
    char *psz, *psz_uri;
    GnomeVFSURI         *p_uri;
    GnomeVFSResult      i_ret;

    if( !(gnome_vfs_init()) )
    {
        msg_Warn( p_access, "couldn't initilize GnomeVFS" );
        return VLC_EGENERIC;
    }

    p_access->pf_read = Read;
    p_access->pf_block = NULL;
    p_access->pf_seek = Seek;
    p_access->pf_control = Control;
    p_access->info.i_update = 0;
    p_access->info.i_size = 0;
    p_access->info.i_pos = 0;
    p_access->info.b_eof = VLC_FALSE;
    p_access->info.i_title = 0;
    p_access->info.i_seekpoint = 0;
    p_access->p_sys = p_sys = malloc( sizeof( access_sys_t ) );
    p_sys->i_nb_reads = 0;
    p_sys->b_pace_control = VLC_TRUE;


    if( strcmp( "gnomevfs", p_access->psz_access ) &&
                                            *(p_access->psz_access) != '\0')
    {
        psz_name = malloc( strlen( p_access->psz_access ) +
                                            strlen( p_access->psz_path ) + 3 );
        strcpy( psz_name, p_access->psz_access );
        strcat( psz_name, "://" );
        strcat( psz_name, p_access->psz_path );
    }
    else
    {
        psz_name = strdup( p_access->psz_path );
    }

    psz = ToLocale( psz_name );
    psz_uri = gnome_vfs_make_uri_from_input_with_dirs( psz,
                                    GNOME_VFS_MAKE_URI_DIR_CURRENT);
    p_uri = gnome_vfs_uri_new( psz_uri );

    if( p_uri )
    {
        p_sys->p_file_info = gnome_vfs_file_info_new();
        i_ret = gnome_vfs_get_file_info_uri( p_uri,
                                                p_sys->p_file_info, 8 ); 

        if( i_ret )
        {
            msg_Err( p_access, "cannot get file info %s", 
                                    gnome_vfs_result_to_string( i_ret ) );
            gnome_vfs_file_info_unref( p_sys->p_file_info );
            gnome_vfs_uri_unref( p_uri);
            free( p_sys );
            free( psz_name );
            return VLC_EGENERIC;
        }
    }
    else
    {
        msg_Warn( p_access, "cannot parse MRL %s", psz_name );
        LocaleFree( psz );
        g_free( psz_uri );
        free( p_sys );
        free( psz_name );
        return VLC_EGENERIC;
    }
    LocaleFree( psz );

    msg_Dbg( p_access, "opening file `%s'", psz_name );
    i_ret = gnome_vfs_open( &(p_sys->p_handle), psz_uri, 5 );
    if( i_ret )
    {
        msg_Warn( p_access, "cannot open file %s: %s", psz_name,
                                gnome_vfs_result_to_string( i_ret ) );

        LocaleFree( psz );
        g_free( psz_uri );
        gnome_vfs_uri_unref( p_uri);
        free( p_sys );
        free( psz_name );
        return VLC_EGENERIC;
    }

    if (GNOME_VFS_FILE_INFO_LOCAL( p_sys->p_file_info ))
    {
        p_sys->b_local = VLC_TRUE;
    }

    if( p_sys->p_file_info->type == GNOME_VFS_FILE_TYPE_REGULAR || 
        p_sys->p_file_info->type == GNOME_VFS_FILE_TYPE_CHARACTER_DEVICE ||
        p_sys->p_file_info->type == GNOME_VFS_FILE_TYPE_BLOCK_DEVICE )
    {
        p_sys->b_seekable = VLC_TRUE;
        p_access->info.i_size = (int64_t)(p_sys->p_file_info->size);
    }
    else if( p_sys->p_file_info->type == GNOME_VFS_FILE_TYPE_FIFO
              || p_sys->p_file_info->type == GNOME_VFS_FILE_TYPE_SOCKET )
    {
        p_sys->b_seekable = VLC_FALSE;
    }
    else
    {
        msg_Err( p_access, "unknown file type for `%s'", psz_name );
        return VLC_EGENERIC;
    }

    if( p_sys->b_seekable && !p_access->info.i_size )
    {
        /* FIXME that's bad because all others access will be probed */
        msg_Err( p_access, "file %s is empty, aborting", psz_name );
        gnome_vfs_file_info_unref( p_sys->p_file_info );
        free( p_sys );
        free( psz_name );
        return VLC_EGENERIC;
    }

    /* Update default_pts to a suitable value for file access */
    var_Create( p_access, "gnomevfs-caching",
                                    VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );

    g_free( psz_uri );
    p_sys->psz_name = psz_name;
    gnome_vfs_uri_unref( p_uri);
    return VLC_SUCCESS;

}

/*****************************************************************************
 * Close: close the target
 *****************************************************************************/
static void Close( vlc_object_t * p_this )
{
    access_t     *p_access = (access_t*)p_this;
    access_sys_t *p_sys = p_access->p_sys;
    int i_result;

    i_result = gnome_vfs_close( p_sys->p_handle );
    if( i_result )
    {
         msg_Err( p_access, "cannot close %s: %s", p_sys->psz_name,
                                gnome_vfs_result_to_string( i_result ) );
    }

    gnome_vfs_file_info_unref( p_sys->p_file_info );

    free( p_sys->psz_name );
    free( p_sys );
}

/*****************************************************************************
 * Read: standard read on a file descriptor.
 *****************************************************************************/
static int Read( access_t *p_access, uint8_t *p_buffer, int i_len )
{
    access_sys_t *p_sys = p_access->p_sys;
    int i_ret;
    GnomeVFSFileSize i_read_len;

    i_ret = gnome_vfs_read( p_sys->p_handle, p_buffer,
                                  (GnomeVFSFileSize)i_len, &i_read_len );

    if( i_ret )
    {
        p_access->info.b_eof = VLC_TRUE;
        if( i_ret != GNOME_VFS_ERROR_EOF )
        {
            msg_Err( p_access, "read failed (%s)",
                                    gnome_vfs_result_to_string( i_ret ) );
        }
    }
    else
    {
        p_sys->i_nb_reads++;
        if( p_access->info.i_size != 0 &&
            (p_sys->i_nb_reads % INPUT_FSTAT_NB_READS) == 0 &&
            p_sys->b_local )
        {
            gnome_vfs_file_info_clear( p_sys->p_file_info );
            i_ret = gnome_vfs_get_file_info_from_handle( p_sys->p_handle,
                                                p_sys->p_file_info, 8 );
            if( i_ret )
            {
                msg_Warn( p_access, "couldn't get file properties again (%s)",
                                        gnome_vfs_result_to_string( i_ret ) );
            }
            else
            {
                p_access->info.i_size = (int64_t)(p_sys->p_file_info->size);
            }
        }
    }

    p_access->info.i_pos += (int64_t)i_read_len;

    return (int)i_read_len;
}

/*****************************************************************************
 * Seek: seek to a specific location in a file
 *****************************************************************************/
static int Seek( access_t *p_access, int64_t i_pos )
{
    access_sys_t *p_sys = p_access->p_sys;
    int i_ret;

    i_ret = gnome_vfs_seek( p_sys->p_handle, GNOME_VFS_SEEK_START,
                                            (GnomeVFSFileOffset)i_pos);

    if ( !i_ret )
    {
        p_access->info.i_pos = i_pos;
    }
    else
    {
        GnomeVFSFileSize i_offset;
        msg_Err( p_access, "cannot seek (%s)",
                                        gnome_vfs_result_to_string( i_ret ) );
        i_ret = gnome_vfs_tell( p_sys->p_handle, &i_offset );
        if( !i_ret )
        {
            msg_Err( p_access, "cannot tell the current position (%s)",
                                        gnome_vfs_result_to_string( i_ret ) );
            return VLC_EGENERIC;
        }
    }
    /* Reset eof */
    p_access->info.b_eof = VLC_FALSE;

    /* FIXME */
    return VLC_SUCCESS;
}

/*****************************************************************************
 * Control:
 *****************************************************************************/
static int Control( access_t *p_access, int i_query, va_list args )
{
    access_sys_t *p_sys = p_access->p_sys;
    vlc_bool_t   *pb_bool;
    int          *pi_int;
    int64_t      *pi_64;

    switch( i_query )
    {
        /* */
        case ACCESS_CAN_SEEK:
        case ACCESS_CAN_FASTSEEK:
            pb_bool = (vlc_bool_t*)va_arg( args, vlc_bool_t* );
            *pb_bool = p_sys->b_seekable;
            break;

        case ACCESS_CAN_PAUSE:
        case ACCESS_CAN_CONTROL_PACE:
            pb_bool = (vlc_bool_t*)va_arg( args, vlc_bool_t* );
            *pb_bool = p_sys->b_pace_control;
            break;

        /* */
        case ACCESS_GET_MTU:
            pi_int = (int*)va_arg( args, int * );
            *pi_int = 0;
            break;

        case ACCESS_GET_PTS_DELAY:
            pi_64 = (int64_t*)va_arg( args, int64_t * );
            *pi_64 = var_GetInteger( p_access,
                                        "gnomevfs-caching" ) * I64C(1000);
            break;

        /* */
        case ACCESS_SET_PAUSE_STATE:
            /* Nothing to do */
            break;

        case ACCESS_GET_TITLE_INFO:
        case ACCESS_SET_TITLE:
        case ACCESS_SET_SEEKPOINT:
        case ACCESS_SET_PRIVATE_ID_STATE:
        case ACCESS_GET_META:
            return VLC_EGENERIC;

        default:
            msg_Warn( p_access, "unimplemented query in control" );
            return VLC_EGENERIC;

    }
    return VLC_SUCCESS;
}

