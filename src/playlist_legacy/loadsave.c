/*****************************************************************************
 * loadsave.c : Playlist loading / saving functions
 *****************************************************************************
 * Copyright (C) 1999-2004 VLC authors and VideoLAN
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
    (void) p_playlist; (void) psz_filename; (void) psz_type;
    return VLC_EGENERIC;
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

    vlc_object_delete(dummy);
    return ret;
}
