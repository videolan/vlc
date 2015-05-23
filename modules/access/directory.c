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

#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

#include <vlc_fs.h>
#include <vlc_url.h>
#include <vlc_strings.h>
#include <vlc_charset.h>

struct access_sys_t
{
    char *psz_base_uri;
    DIR *p_dir;
};

/*****************************************************************************
 * Open: open the directory
 *****************************************************************************/
int DirOpen (vlc_object_t *p_this)
{
    access_t *p_access = (access_t*)p_this;
    DIR *p_dir;
    char *psz_base_uri;

    if (!p_access->psz_filepath)
        return VLC_EGENERIC;

    p_dir = vlc_opendir (p_access->psz_filepath);
    if (p_dir == NULL)
        return VLC_EGENERIC;

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

    return VLC_SUCCESS;
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

static bool is_looping(access_t *p_access, const char *psz_uri)
{
#ifdef S_ISLNK
    struct stat st;
    bool b_looping = false;

    if (vlc_lstat (psz_uri, &st) != 0)
        return false;
    if (S_ISLNK (st.st_mode))
    {
        char *psz_link = malloc(st.st_size + 1);
        ssize_t i_ret;

        if (psz_link)
        {
            i_ret = readlink(psz_uri, psz_link, st.st_size + 1);
            if (i_ret > 0 && i_ret <= st.st_size)
            {
                psz_link[i_ret] = '\0';
                if (strstr(p_access->psz_filepath, psz_link))
                    b_looping = true;
            }
            free (psz_link);
        }
    }
    return b_looping;
#else
    VLC_UNUSED(p_access);
    VLC_UNUSED(psz_uri);
    return false;
#endif
}

input_item_t* DirRead (access_t *p_access)
{
    access_sys_t *p_sys = p_access->p_sys;
    DIR *p_dir = p_sys->p_dir;
    input_item_t *p_item = NULL;
    const char *psz_entry;

    while (!p_item && (psz_entry = vlc_readdir (p_dir)))
    {
        char *psz_uri, *psz_encoded_entry;
        struct stat st;
        int i_type;

        /* Check if it is a directory or even readable */
        if (asprintf (&psz_uri, "%s/%s",
                      p_access->psz_filepath, psz_entry) == -1)
            return NULL;
        if (vlc_stat (psz_uri, &st) != 0)
        {
            free (psz_uri);
            continue;
        }
        i_type = S_ISDIR (st.st_mode) ? ITEM_TYPE_DIRECTORY : ITEM_TYPE_FILE;
        if (i_type == ITEM_TYPE_DIRECTORY && is_looping(p_access, psz_uri))
        {
            free (psz_uri);
            continue;
        }
        free (psz_uri);

        /* Create an input item for the current entry */
        psz_encoded_entry = encode_URI_component (psz_entry);
        if (psz_encoded_entry == NULL)
            continue;
        if (asprintf (&psz_uri, "%s/%s",
                      p_sys->psz_base_uri, psz_encoded_entry) == -1)
            return NULL;
        free (psz_encoded_entry);

        p_item = input_item_NewWithType (psz_uri, psz_entry,
                                         0, NULL, 0, 0, i_type);
        free (psz_uri);
        if (!p_item)
            return NULL;
    }
    return p_item;
}
