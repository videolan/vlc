/*****************************************************************************
 * folder.c
 *****************************************************************************
 * Copyright (C) 2006 VLC authors and VideoLAN
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

static const char cover_files[][20] = {
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
    add_loadfile("album-art-filename", NULL, N_("Album art filename"),
                 N_("Filename to look for album art in current directory"))
    set_capability( "art finder", 90 )
    set_callback( FindMeta )
vlc_module_end ()

static bool ProbeArtFile(input_item_t *item,
                         const char *dirpath, const char *filename)
{
    char *filepath;
    struct stat st;
    bool found = false;

    if (asprintf(&filepath, "%s"DIR_SEP"%s", dirpath, filename) == -1)
        return false;

    if (vlc_stat(filepath, &st) == 0 && S_ISREG(st.st_mode))
    {
        char *url = vlc_path2uri(filepath, "file");
        if (likely(url != NULL))
        {
            input_item_SetArtURL(item, url);
            free(url);
            found = true;
        }
    }

    free(filepath);
    return found;
}

static int FindMeta( vlc_object_t *p_this )
{
    meta_fetcher_t *p_finder = (meta_fetcher_t *)p_this;
    input_item_t *p_item = p_finder->p_item;

    if( !p_item )
        return VLC_EGENERIC;

    char *psz_uri = input_item_GetURI( p_item );
    if( !psz_uri )
        return VLC_EGENERIC;

    char *psz_basedir = vlc_uri2path(psz_uri);
    free(psz_uri);
    if (psz_basedir == NULL)
        return VLC_EGENERIC;

    /* If the item is an accessible directory, look for art inside it.
     * Otherwise, look for art in the same directory. */
    struct stat st;
    if (vlc_stat(psz_basedir, &st) == 0 && !S_ISDIR(st.st_mode))
    {
        char *psz_buf = strrchr( psz_basedir, DIR_SEP_CHAR );
        if (psz_buf != NULL)
            *psz_buf = '\0';
    }

    int ret = VLC_EGENERIC;

    char *filename = var_InheritString(p_this, "album-art-filename");
    if (filename != NULL && ProbeArtFile(p_item, psz_basedir, filename))
        ret = VLC_SUCCESS;
    else
    {
        for (size_t i = 0; i < ARRAY_SIZE(cover_files); i++)
            if (ProbeArtFile(p_item, psz_basedir, cover_files[i]))
            {
                ret = VLC_SUCCESS;
                break;
            }
    }

    free(psz_basedir);
    return ret;
}
