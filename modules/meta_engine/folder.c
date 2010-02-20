/*****************************************************************************
 * folder.c
 *****************************************************************************
 * Copyright (C) 2006 the VideoLAN team
 * $Id$
 *
 * Authors: Antoine Cellerier <dionoea -at- videolan -dot- org>
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
#include <vlc_plugin.h>
#include <vlc_playlist.h>
#include <vlc_art_finder.h>
#include <vlc_fs.h>
#include <vlc_url.h>

#ifdef HAVE_SYS_STAT_H
#   include <sys/stat.h>
#endif

#ifndef MAX_PATH
#   define MAX_PATH 250
#endif

static const char* cover_files[] = {
    "Folder.jpg",           /* Windows */
    "AlbumArtSmall.jpg",    /* Windows */
    ".folder.png",          /* KDE?    */
    "cover.jpg",            /* rockbox */
};

static const int i_covers = (sizeof(cover_files)/sizeof(cover_files[0]));

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int FindMeta( vlc_object_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

vlc_module_begin ()
    set_shortname( N_( "Folder" ) )
    set_description( N_("Folder meta data") )
    add_file( "album-art-filename", NULL, NULL,
        N_("Album art filename"), N_("Filename to look for album art in current directory"), false );
    set_capability( "art finder", 90 )
    set_callbacks( FindMeta, NULL )
vlc_module_end ()

/*****************************************************************************
 *****************************************************************************/
static int FindMeta( vlc_object_t *p_this )
{
    art_finder_t *p_finder = (art_finder_t *)p_this;
    input_item_t *p_item = p_finder->p_item;
    bool b_have_art = false;

    int i;
    struct stat a;
    char psz_filename[MAX_PATH];

    if( !p_item )
        return VLC_EGENERIC;

    char *psz_dir = input_item_GetURI( p_item );
    if( !psz_dir )
        return VLC_EGENERIC;

    char *psz_path = make_path( psz_dir );
    free( psz_dir );
    if( psz_path == NULL )
        return VLC_EGENERIC;

    char *psz_buf = strrchr( psz_path, DIR_SEP_CHAR );
    if( psz_buf )
        *++psz_buf = '\0';
    else
        *psz_path = '\0'; /* relative path */

    for( i = -1; !b_have_art && i < i_covers; i++ )
    {
        if( i == -1 ) /* higher priority : configured filename */
        {
            char *psz_userfile = var_InheritString( p_this, "album-art-filename" );
            if( !psz_userfile )
                continue;
            snprintf( psz_filename, MAX_PATH, "%s%s", psz_path, psz_userfile );
            free( psz_userfile );
        }
        else
            snprintf( psz_filename, MAX_PATH, "%s%s", psz_path, cover_files[i] );

        if( vlc_stat( psz_filename, &a ) != -1 )
        {
            char *psz_uri = make_URI( psz_filename );
            if( psz_uri )
            {
                input_item_SetArtURL( p_item, psz_uri );
                free( psz_uri );
                b_have_art = true;
            }
        }
    }

    return b_have_art ? VLC_SUCCESS : VLC_EGENERIC;
}
