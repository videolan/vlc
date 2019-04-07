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
#include <vlc_meta_fetcher.h>
#include <vlc_fs.h>
#include <vlc_url.h>
#include <vlc_input_item.h>

static const char* cover_files[] = {
    "Folder.jpg",           /* Windows */
    "Folder.png",
    "AlbumArtSmall.jpg",    /* Windows */
    "AlbumArt.jpg",         /* Windows */
    "Album.jpg",
    ".folder.png",          /* KDE?    */
    "cover.jpg",            /* rockbox */
    "cover.png",
    "cover.gif",
    "front.jpg",
    "front.png",
    "front.gif",
    "front.bmp",
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
    set_category( CAT_PLAYLIST )
    set_subcategory( SUBCAT_PLAYLIST_GENERAL )
    add_loadfile( "album-art-filename", NULL,
        N_("Album art filename"), N_("Filename to look for album art in current directory"), false );
    set_capability( "art finder", 90 )
    set_callbacks( FindMeta, NULL )
vlc_module_end ()

/*****************************************************************************
 *****************************************************************************/
static int FindMeta( vlc_object_t *p_this )
{
    meta_fetcher_t *p_finder = (meta_fetcher_t *)p_this;
    input_item_t *p_item = p_finder->p_item;
    bool b_have_art = false;
    struct stat statinfo;
    char *psz_path = NULL;

    if( !p_item )
        return VLC_EGENERIC;

    char *psz_uri = input_item_GetURI( p_item );
    if( !psz_uri )
        return VLC_EGENERIC;

    if ( *psz_uri && psz_uri[strlen( psz_uri ) - 1] != DIR_SEP_CHAR )
    {
        if ( asprintf( &psz_path, "%s"DIR_SEP, psz_uri ) == -1 )
        {
            free( psz_uri );
            return VLC_EGENERIC;
        }
        char *psz_basedir = vlc_uri2path( psz_path );
        FREENULL( psz_path );
        if( psz_basedir == NULL )
        {
            free( psz_uri );
            return VLC_EGENERIC;
        }
        if( vlc_stat( psz_basedir, &statinfo ) == 0 && S_ISDIR(statinfo.st_mode) )
            psz_path = psz_basedir;
        else
            free( psz_basedir );
    }

    if ( psz_path == NULL )
    {
        char *psz_basedir = vlc_uri2path( psz_uri );
        if( psz_basedir == NULL )
        {
            free( psz_uri );
            return VLC_EGENERIC;
        }

        char *psz_buf = strrchr( psz_basedir, DIR_SEP_CHAR );
        if( psz_buf )
            *++psz_buf = '\0';
        else
            *psz_basedir = '\0'; /* relative path */
        psz_path = psz_basedir;
    }

    free( psz_uri );

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

        if( vlc_stat( filepath, &statinfo ) == 0 && S_ISREG(statinfo.st_mode) )
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
