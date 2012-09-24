/*****************************************************************************
 * folder.c
 *****************************************************************************
 * Copyright (C) 2006 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Antoine Cellerier <dionoea -at- videolan -dot- org>
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <sys/stat.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_art_finder.h>
#include <vlc_fs.h>
#include <vlc_url.h>
#include <vlc_input_item.h>

static const char* cover_files[] = {
    "Folder.jpg",           /* Windows */
    "AlbumArtSmall.jpg",    /* Windows */
    "AlbumArt.jpg",         /* Windows */
    "Album.jpg",
    ".folder.png",          /* KDE?    */
    "cover.jpg",            /* rockbox */
    "thumb.jpg",
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
    add_loadfile( "album-art-filename", NULL,
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

    for( int i = -1; !b_have_art && i < i_covers; i++ )
    {
        const char *filename;
        char *filebuf, *filepath;

        if( i == -1 ) /* higher priority : configured filename */
        {
            filebuf = var_InheritString( p_this, "album-art-filename" );
            if( filebuf == NULL )
                continue;
            filename = filebuf;
        }
        else
        {
            filename = cover_files[i];
            filebuf = NULL;
        }

        if( asprintf( &filepath, "%s%s", psz_path, filename ) == -1 )
            filepath = NULL;
        free( filebuf );
        if( unlikely(filepath == NULL) )
            continue;

        struct stat dummy;
        if( vlc_stat( filepath, &dummy ) == 0 )
        {
            char *psz_uri = vlc_path2uri( filepath, "file" );
            if( psz_uri )
            {
                input_item_SetArtURL( p_item, psz_uri );
                free( psz_uri );
                b_have_art = true;
            }
        }
        free( filepath );
    }
    free( psz_path );

    return b_have_art ? VLC_SUCCESS : VLC_EGENERIC;
}
