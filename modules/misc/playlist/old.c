/*****************************************************************************
 * old.c : Old playlist format import/export
 *****************************************************************************
 * Copyright (C) 2004 the VideoLAN team
 * $Id$
 *
 * Authors: Cl�ent Stenac <zorglub@videolan.org>
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
#include <stdlib.h>                                      /* malloc(), free() */

#include <vlc/vlc.h>
#include <vlc/intf.h>
#include <charset.h>

#include <errno.h>                                                 /* ENOMEM */

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

    msg_Dbg(p_playlist, "Saving using old format");

    /* Write header */
    fprintf( p_export->p_file , PLAYLIST_FILE_HEADER "\n" );

    for ( i = 0 ; i < p_playlist->i_size ; i++ )
    {
        char *psz_uri;

        psz_uri = ToLocale( p_playlist->pp_items[i]->input.psz_uri );
        fprintf( p_export->p_file , "%s\n" , psz_uri );
        LocaleFree( psz_uri );
    }
    return VLC_SUCCESS;
}
