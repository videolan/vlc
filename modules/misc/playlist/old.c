/*****************************************************************************
 * old.c : Old playlist format import/export
 *****************************************************************************
 * Copyright (C) 2004-2009 the VideoLAN team
 * $Id$
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_playlist.h>
#include <vlc_input.h>
#include <vlc_charset.h>


#define PLAYLIST_FILE_HEADER "# vlc playlist file version 0.5"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
int Export_Old ( vlc_object_t * );

/*****************************************************************************
 * Export_Old : main export function
 *****************************************************************************/
int Export_Old( vlc_object_t *p_this )
{
    playlist_t *p_playlist = (playlist_t*)p_this;
    playlist_export_t *p_export = (playlist_export_t *)p_playlist->p_private;
    int i;

    msg_Dbg(p_playlist, "saving using old format");

    /* Write header */
    fprintf( p_export->p_file , PLAYLIST_FILE_HEADER "\n" );

    for ( i = 0 ; i < p_export->p_root->i_children ; i++ )
        utf8_fprintf( p_export->p_file , "%s\n" ,
                      p_export->p_root->pp_children[i]->p_input->psz_name );

    return VLC_SUCCESS;
}
