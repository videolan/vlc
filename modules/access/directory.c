/*****************************************************************************
 * directory.c: expands a directory (directory: access_browser plug-in)
 *****************************************************************************
 * Copyright (C) 2002-2015 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Derk-Jan Hartman <hartman at videolan dot org>
 *          Rémi Denis-Courmont
 *          Julien 'Lta' BALLET <contact # lta.io>
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

#include <vlc_common.h>
#include "fs.h"
#include <vlc_access.h>
#include <vlc_input_item.h>

#include <vlc_fs.h>
#include <vlc_url.h>

struct access_sys_t
{
    char *psz_base_uri;
    DIR *p_dir;
};

/*****************************************************************************
 * DirInit: Init the directory access with a directory stream
 *****************************************************************************/
int DirInit (access_t *p_access, DIR *p_dir)
{
    char *psz_base_uri;

    if (!strcmp (p_access->psz_access, "fd"))
    {
        if (asprintf (&psz_base_uri, "fd://%s", p_access->psz_location) == -1)
            psz_base_uri = NULL;
    }
    else
        psz_base_uri = vlc_path2uri (p_access->psz_filepath, "file");
    if (unlikely (psz_base_uri == NULL))
    {
        closedir (p_dir);
        return VLC_ENOMEM;
    }

    p_access->p_sys = calloc (1, sizeof(access_sys_t));
    if (!p_access->p_sys)
    {
        closedir(p_dir);
        free( psz_base_uri );
        return VLC_ENOMEM;
    }
    p_access->p_sys->p_dir = p_dir;
    p_access->p_sys->psz_base_uri = psz_base_uri;
    p_access->pf_readdir = DirRead;
    p_access->pf_control = access_vaDirectoryControlHelper;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * DirOpen: Open the directory access
 *****************************************************************************/
int DirOpen (vlc_object_t *p_this)
{
    access_t *access = (access_t *)p_this;

    if (access->psz_filepath == NULL)
        return VLC_EGENERIC;

    DIR *dir = vlc_opendir (access->psz_filepath);
    if (dir == NULL)
        return VLC_EGENERIC;

    return DirInit (access, dir);
}

/*****************************************************************************
 * Close: close the target
 *****************************************************************************/
void DirClose( vlc_object_t * p_this )
{
    access_t *p_access = (access_t*)p_this;
    access_sys_t *p_sys = p_access->p_sys;

    free (p_sys->psz_base_uri);
    closedir (p_sys->p_dir);

    free (p_sys);
}

input_item_t* DirRead (access_t *p_access)
{
    access_sys_t *p_sys = p_access->p_sys;
    const char *entry;

    while ((entry = vlc_readdir (p_sys->p_dir)) != NULL)
    {
        /* Create an input item for the current entry */
        char *encoded_entry = encode_URI_component (entry);
        if (unlikely(entry == NULL))
            return NULL;

        char *uri;
        if (unlikely(asprintf (&uri, "%s/%s", p_sys->psz_base_uri,
                               encoded_entry) == -1))
            uri = NULL;
        free (encoded_entry);
        if (unlikely(uri == NULL))
            return NULL;

        input_item_t *item = input_item_NewWithType (uri, entry, 0, NULL, 0, 0,
                                                     ITEM_TYPE_FILE);
        free (uri);
        if (likely(item != NULL))
            return item;
    }
    return NULL;
}
