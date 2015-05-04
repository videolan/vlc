/*****************************************************************************
 * directory.c: expands a directory (directory: access_browser plug-in)
 *****************************************************************************
 * Copyright (C) 2002-2008 VLC authors and VideoLAN
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

enum
{
    ENTRY_DIR       = 0,
    ENTRY_ENOTDIR   = -1,
    ENTRY_EACCESS   = -2,
};

enum
{
    MODE_NONE,
    MODE_COLLAPSE,
    MODE_EXPAND,
};

typedef struct directory directory;
struct directory
{
    directory   *parent;
    DIR         *handle;
    char        *uri;
    char       **filev;
    int          filec, i;
#ifdef HAVE_OPENAT
    dev_t        device;
    ino_t        inode;
#else
    char         *path;
#endif
};

struct access_sys_t
{
    directory *current;
    char      *ignored_exts;
    char       mode;
    int        (*compar) (const char **a, const char **b);
};

/* Select non-hidden files only */
static int visible (const char *name)
{
    return name[0] != '.';
}

static int collate (const char **a, const char **b)
{
#ifdef HAVE_STRCOLL
    return strcoll (*a, *b);
#else
    return strcmp  (*a, *b);
#endif
}

static int version (const char **a, const char **b)
{
    return strverscmp (*a, *b);
}

/**
 * Does the provided URI/path/stuff has one of the extension provided ?
 *
 * \param psz_exts A comma separated list of extension without dot, or only
 * one ext (ex: "avi,mkv,webm")
 * \param psz_uri The uri/path to check (ex: "file:///home/foo/bar.avi"). If
 * providing an URI, it must not contain a query string.
 *
 * \return true if the uri/path has one of the provided extension
 * false otherwise.
 */
static bool has_ext (const char *psz_exts, const char *psz_uri)
{
    if (psz_exts == NULL)
        return false;

    const char *ext = strrchr (psz_uri, '.');
    if (ext == NULL)
        return false;

    size_t extlen = strlen (++ext);

    for (const char *type = psz_exts, *end; type[0]; type = end + 1)
    {
        end = strchr (type, ',');
        if (end == NULL)
            end = type + strlen (type);

        if (type + extlen == end && !strncasecmp (ext, type, extlen))
            return true;

        if (*end == '\0')
            break;
    }

    return false;
}


#ifdef HAVE_OPENAT
/* Detect directories that recurse into themselves. */
static bool has_inode_loop (const directory *dir, dev_t dev, ino_t inode)
{
    while (dir != NULL)
    {
        if ((dir->device == dev) && (dir->inode == inode))
            return true;
        dir = dir->parent;
    }
    return false;
}
#endif

/* success -> returns ENTRY_DIR and the handle parameter is set to the handle,
 * error -> return ENTRY_ENOTDIR or ENTRY_EACCESS */
static int directory_open (directory *p_dir, char *psz_entry, DIR **handle)
{
    *handle = NULL;

#ifdef HAVE_OPENAT
    int fd = vlc_openat (dirfd (p_dir->handle), psz_entry,
                         O_RDONLY | O_DIRECTORY);

    if (fd == -1)
    {
        if (errno == ENOTDIR)
            return ENTRY_ENOTDIR;
        else
            return ENTRY_EACCESS;
    }

    struct stat st;
    if (fstat (fd, &st)
        || has_inode_loop (p_dir, st.st_dev, st.st_ino)
        || (*handle = fdopendir (fd)) == NULL)
    {
        close (fd);
        return ENTRY_EACCESS;
    }
#else
    char *path;
    if (asprintf (&path, "%s/%s", p_dir->path, psz_entry) == -1)
        return ENTRY_EACCESS;

    *handle = vlc_opendir (path);

    free(path);

    if (*handle == NULL) {
        return ENTRY_ENOTDIR;
    }
#endif

    return ENTRY_DIR;
}

static bool directory_push (access_sys_t *p_sys, DIR *handle, char *psz_uri)
{
    directory *p_dir = malloc (sizeof (*p_dir));

    psz_uri = strdup (psz_uri);
    if (unlikely (p_dir == NULL || psz_uri == NULL))
        goto error;

    p_dir->parent = p_sys->current;
    p_dir->handle = handle;
    p_dir->uri = psz_uri;
    p_dir->filec = vlc_loaddir (handle, &p_dir->filev, visible, p_sys->compar);
    if (p_dir->filec < 0)
        p_dir->filev = NULL;
    p_dir->i = 0;

#ifdef HAVE_OPENAT
    struct stat st;
    if (fstat (dirfd (handle), &st))
        goto error_filev;
    p_dir->device = st.st_dev;
    p_dir->inode = st.st_ino;
#else
    p_dir->path = make_path (psz_uri);
    if (p_dir->path == NULL)
        goto error_filev;
#endif

    p_sys->current = p_dir;
    return true;

error_filev:
    for (int i = 0; i < p_dir->filec; i++)
        free (p_dir->filev[i]);
    free (p_dir->filev);

error:
    closedir (handle);
    free (p_dir);
    free (psz_uri);
    return false;
}

static bool directory_pop (access_sys_t *p_sys)
{
    directory *p_old = p_sys->current;

    if (p_old == NULL)
        return false;

    p_sys->current = p_old->parent;
    closedir (p_old->handle);
    free (p_old->uri);
    for (int i = 0; i < p_old->filec; i++)
        free (p_old->filev[i]);
    free (p_old->filev);
#ifndef HAVE_OPENAT
    free (p_old->path);
#endif
    free (p_old);

    return p_sys->current != NULL;
}


/*****************************************************************************
 * Open: open the directory
 *****************************************************************************/
int DirOpen (vlc_object_t *p_this)
{
    access_t *p_access = (access_t*)p_this;

    if (!p_access->psz_filepath)
        return VLC_EGENERIC;

    DIR *handle = vlc_opendir (p_access->psz_filepath);
    if (handle == NULL)
        return VLC_EGENERIC;

    return DirInit (p_access, handle);
}

int DirInit (access_t *p_access, DIR *handle)
{
    access_sys_t *p_sys = malloc (sizeof (*p_sys));
    if (unlikely (p_sys == NULL))
        goto error;

    char *psz_sort = var_InheritString (p_access, "directory-sort");
    if (!psz_sort)
        p_sys->compar = collate;
    else if (!strcasecmp ( psz_sort, "version"))
        p_sys->compar = version;
    else if (!strcasecmp (psz_sort, "none"))
        p_sys->compar = NULL;
    else
        p_sys->compar = collate;
    free(psz_sort);

    char *uri;
    if (!strcmp (p_access->psz_access, "fd"))
    {
        if (asprintf (&uri, "fd://%s", p_access->psz_location) == -1)
            uri = NULL;
    }
    else
        uri = vlc_path2uri (p_access->psz_filepath, "file");
    if (unlikely (uri == NULL))
    {
        closedir (handle);
        goto error;
    }

    /* "Open" the base directory */
    p_sys->current = NULL;
    if (!directory_push (p_sys, handle, uri))
    {
        free (uri);
        goto error;
    }
    free (uri);

    p_access->p_sys = p_sys;
    p_sys->ignored_exts = var_InheritString (p_access, "ignore-filetypes");

    p_access->pf_readdir = DirRead;

    return VLC_SUCCESS;

error:
    free (p_sys);
    return VLC_EGENERIC;
}

/*****************************************************************************
 * Close: close the target
 *****************************************************************************/
void DirClose( vlc_object_t * p_this )
{
    access_t *p_access = (access_t*)p_this;
    access_sys_t *p_sys = p_access->p_sys;

    while (directory_pop (p_sys))
        ;

    free (p_sys->ignored_exts);
    free (p_sys);
}

/* This function is a little bit too complex for what it seems to do, but the
 * point is to de-recursify directory recusion to avoid overruning the stack
 * in case there's a high directory depth */
int DirRead (access_t *p_access, input_item_node_t *p_current_node)
{
    access_sys_t *p_sys = p_access->p_sys;

    while (p_sys->current != NULL
           && p_sys->current->i <= p_sys->current->filec)
    {
        directory *p_current = p_sys->current;

        /* End of the current folder, let's pop directory and node */
        if (p_current->i == p_current->filec)
        {
            directory_pop (p_sys);
            p_current_node = p_current_node->p_parent;
            continue;
        }

        char *psz_entry = p_current->filev[p_current->i++];
        char *psz_full_uri, *psz_uri;
        DIR *handle;
        input_item_t *p_new = NULL;
        int i_res;

        /* Check if it is a directory or even readable */
        i_res = directory_open (p_current, psz_entry, &handle);

        if (i_res == ENTRY_EACCESS
            || (i_res == ENTRY_ENOTDIR && has_ext (p_sys->ignored_exts, psz_entry)))
            continue;


        /* Create an input item for the current entry */
        psz_uri = encode_URI_component (psz_entry);
        if (psz_uri == NULL
         || asprintf (&psz_full_uri, "%s/%s", p_current->uri, psz_uri) == -1)
            psz_full_uri = NULL;

        free (psz_uri);
        if (psz_full_uri == NULL)
        {
            closedir (handle);
            continue;
        }

        int i_type = i_res == ENTRY_DIR ? ITEM_TYPE_DIRECTORY : ITEM_TYPE_FILE;
        p_new = input_item_NewWithType (psz_full_uri, psz_entry,
                                        0, NULL, 0, 0, i_type);
        if (p_new == NULL)
        {
            free (psz_full_uri);
            closedir (handle);
            continue;
        }

        input_item_CopyOptions (p_current_node->p_item, p_new);
        input_item_node_AppendItem (p_current_node, p_new);

        free (psz_full_uri);
        input_item_Release (p_new);
    }

    return VLC_SUCCESS;
}
