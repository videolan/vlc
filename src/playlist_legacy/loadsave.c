/*****************************************************************************
 * loadsave.c : Playlist loading / saving functions
 *****************************************************************************
 * Copyright (C) 1999-2004 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <vlc_common.h>
#include <vlc_playlist_legacy.h>
#include <vlc_events.h>
#include "playlist_internal.h"
#include "config/configuration.h"
#include <vlc_fs.h>
#include <vlc_url.h>
#include <vlc_modules.h>

int playlist_Export( playlist_t * p_playlist, const char *psz_filename,
                     const char *psz_type )
{
    playlist_export_t *p_export =
        vlc_custom_create( p_playlist, sizeof( *p_export ), "playlist export" );
    if( unlikely(p_export == NULL) )
        return VLC_ENOMEM;

    msg_Dbg( p_export, "saving playlist to file %s", psz_filename );

    int ret = VLC_EGENERIC;

    /* Prepare the playlist_export_t structure */
    p_export->base_url = vlc_path2uri( psz_filename, NULL );
    p_export->p_file = vlc_fopen( psz_filename, "wt" );
    if( p_export->p_file == NULL )
    {
        msg_Err( p_export, "could not create playlist file %s: %s",
                 psz_filename, vlc_strerror_c(errno) );
        goto out;
    }

    module_t *p_module;

    /* And call the module ! All work is done now */
    playlist_Lock( p_playlist );
    p_export->p_root = p_playlist->p_playing;

    p_module = module_need( p_export, "playlist export", psz_type, true );
    playlist_Unlock( p_playlist );

    if( p_module != NULL )
    {
        module_unneed( p_export, p_module );
        if( !ferror( p_export->p_file ) )
            ret = VLC_SUCCESS;
        else
            msg_Err( p_playlist, "could not write playlist file: %s",
                     vlc_strerror_c(errno) );
    }
    else
        msg_Err( p_playlist, "could not export playlist" );
   fclose( p_export->p_file );
out:
   free( p_export->base_url );
   vlc_object_release( p_export );
   return ret;
}

int playlist_Import( playlist_t *p_playlist, const char *psz_file )
{
    input_item_t *p_input;
    char *psz_uri = vlc_path2uri( psz_file, NULL );

    if( psz_uri == NULL )
        return VLC_EGENERIC;

    p_input = input_item_New( psz_uri, psz_file );
    free( psz_uri );

    playlist_AddInput( p_playlist, p_input, false );

    vlc_object_t *dummy = vlc_object_create( p_playlist, sizeof (*dummy) );
    var_Create( dummy, "meta-file", VLC_VAR_VOID );

    int ret = input_Read( dummy, p_input, NULL, NULL );

    vlc_object_release( dummy );
    return ret;
}
