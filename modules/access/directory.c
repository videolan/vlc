/*****************************************************************************
 * directory.c: expands a directory (directory: access plug-in)
 *****************************************************************************
 * Copyright (C) 2002-2008 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Derk-Jan Hartman <hartman at videolan dot org>
 *          Rémi Denis-Courmont
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

#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#ifdef HAVE_UNISTD_H
#   include <unistd.h>
#   include <fcntl.h>
#elif defined( _WIN32 )
#   include <io.h>
#endif

#include <vlc_fs.h>
#include <vlc_url.h>
#include <vlc_strings.h>
#include <vlc_charset.h>

enum
{
    MODE_NONE,
    MODE_COLLAPSE,
    MODE_EXPAND,
};

typedef struct directory_t directory_t;
struct directory_t
{
    directory_t *parent;
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
    directory_t *current;
    char *ignored_exts;
    char mode;
    bool header;
    int i_item_count;
    char *xspf_ext;
    int (*compar)(const char **a, const char **b);
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

/*****************************************************************************
 * Open: open the directory
 *****************************************************************************/
int DirOpen( vlc_object_t *p_this )
{
    access_t *p_access = (access_t*)p_this;

    if( !p_access->psz_filepath )
        return VLC_EGENERIC;

    DIR *handle = vlc_opendir (p_access->psz_filepath);
    if (handle == NULL)
        return VLC_EGENERIC;

    return DirInit (p_access, handle);
}

int DirInit (access_t *p_access, DIR *handle)
{
    access_sys_t *p_sys = malloc (sizeof (*p_sys));
    if (unlikely(p_sys == NULL))
        goto error;

    char *uri;
    if (!strcmp (p_access->psz_access, "fd"))
    {
        if (asprintf (&uri, "fd://%s", p_access->psz_location) == -1)
            uri = NULL;
    }
    else
        uri = vlc_path2uri (p_access->psz_filepath, "file");
    if (unlikely(uri == NULL))
        goto error;

    /* "Open" the base directory */
    directory_t *root = malloc (sizeof (*root));
    if (unlikely(root == NULL))
    {
        free (uri);
        goto error;
    }

    char *psz_sort = var_InheritString (p_access, "directory-sort");
    if (!psz_sort)
        p_sys->compar = collate;
    else if (!strcasecmp (psz_sort, "version"))
        p_sys->compar = version;
    else if (!strcasecmp (psz_sort, "none"))
        p_sys->compar = NULL;
    else
        p_sys->compar = collate;
    free(psz_sort);

    root->parent = NULL;
    root->handle = handle;
    root->uri = uri;
    root->filec = vlc_loaddir (handle, &root->filev, visible, p_sys->compar);
    if (root->filec < 0)
        root->filev = NULL;
    root->i = 0;
#ifdef HAVE_OPENAT
    struct stat st;
    if (fstat (dirfd (handle), &st))
    {
        free (root);
        free (uri);
        goto error;
    }
    root->device = st.st_dev;
    root->inode = st.st_ino;
#else
    root->path = strdup (p_access->psz_filepath);
#endif

    p_access->p_sys = p_sys;
    p_sys->current = root;
    p_sys->ignored_exts = var_InheritString (p_access, "ignore-filetypes");
    p_sys->header = true;
    p_sys->i_item_count = 0;
    p_sys->xspf_ext = strdup ("");

    /* Handle mode */
    char *psz = var_InheritString (p_access, "recursive");
    if (psz == NULL || !strcasecmp (psz, "none"))
        p_sys->mode = MODE_NONE;
    else if( !strcasecmp( psz, "collapse" )  )
        p_sys->mode = MODE_COLLAPSE;
    else
        p_sys->mode = MODE_EXPAND;
    free( psz );

    access_InitFields(p_access);
    p_access->pf_read  = NULL;
    p_access->pf_block = DirBlock;
    p_access->pf_seek  = NULL;
    p_access->pf_control= DirControl;
    free (p_access->psz_demux);
    p_access->psz_demux = strdup ("xspf-open");

    return VLC_SUCCESS;

error:
    closedir (handle);
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

    while (p_sys->current)
    {
        directory_t *current = p_sys->current;

        p_sys->current = current->parent;
        closedir (current->handle);
        free (current->uri);
        while (current->i < current->filec)
            free (current->filev[current->i++]);
        free (current->filev);
#ifndef HAVE_OPENAT
        free (current->path);
#endif
        free (current);
    }

    free (p_sys->xspf_ext);
    free (p_sys->ignored_exts);
    free (p_sys);
}

#ifdef HAVE_OPENAT
/* Detect directories that recurse into themselves. */
static bool has_inode_loop (const directory_t *dir, dev_t dev, ino_t inode)
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

block_t *DirBlock (access_t *p_access)
{
    access_sys_t *p_sys = p_access->p_sys;
    directory_t *current = p_sys->current;

    if (p_access->info.b_eof)
        return NULL;

    if (p_sys->header)
    {   /* Startup: send the XSPF header */
        static const char header[] =
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
            "<playlist version=\"1\" xmlns=\"http://xspf.org/ns/0/\" xmlns:vlc=\"http://www.videolan.org/vlc/playlist/ns/0/\">\n"
            " <trackList>\n";
        block_t *block = block_Alloc (sizeof (header) - 1);
        if (!block)
            goto fatal;
        memcpy (block->p_buffer, header, sizeof (header) - 1);
        p_sys->header = false;
        return block;
    }

    if (current->i >= current->filec)
    {   /* End of directory, go back to parent */
        closedir (current->handle);
        p_sys->current = current->parent;
        free (current->uri);
        free (current->filev);
#ifndef HAVE_OPENAT
        free (current->path);
#endif
        free (current);

        if (p_sys->current == NULL)
        {   /* End of XSPF playlist */
            char *footer;
            int len = asprintf (&footer, " </trackList>\n"
                " <extension application=\"http://www.videolan.org/"
                                             "vlc/playlist/0\">\n"
                "%s"
                " </extension>\n"
                "</playlist>\n", p_sys->xspf_ext ? p_sys->xspf_ext : "");
            if (unlikely(len == -1))
                goto fatal;

            block_t *block = block_heap_Alloc (footer, len);
            p_access->info.b_eof = true;
            return block;
        }
        else
        {
            /* This was the end of a "subnode" */
            /* Write the ID to the extension */
            char *old_xspf_ext = p_sys->xspf_ext;
            if (old_xspf_ext != NULL
             && asprintf (&p_sys->xspf_ext, "%s  </vlc:node>\n",
                          old_xspf_ext ? old_xspf_ext : "") == -1)
                p_sys->xspf_ext = NULL;
            free (old_xspf_ext);
        }
        return NULL;
    }

    char *entry = current->filev[current->i++];

    /* Handle recursion */
    if (p_sys->mode != MODE_COLLAPSE)
    {
        DIR *handle;
#ifdef HAVE_OPENAT
        int fd = vlc_openat (dirfd (current->handle), entry,
                             O_RDONLY | O_DIRECTORY);
        if (fd == -1)
        {
            if (errno == ENOTDIR)
                goto notdir;
            goto skip; /* File cannot be opened... forget it */
        }

        struct stat st;
        if (fstat (fd, &st)
         || p_sys->mode == MODE_NONE
         || has_inode_loop (current, st.st_dev, st.st_ino)
         || (handle = fdopendir (fd)) == NULL)
        {
            close (fd);
            goto skip;
        }
#else
        char *path;
        if (asprintf (&path, "%s/%s", current->path, entry) == -1)
            goto skip;
        if ((handle = vlc_opendir (path)) == NULL)
            goto notdir;
        if (p_sys->mode == MODE_NONE)
            goto skip;
#endif
        directory_t *sub = malloc (sizeof (*sub));
        if (unlikely(sub == NULL))
        {
            closedir (handle);
#ifndef HAVE_OPENAT
            free (path);
#endif
            goto skip;
        }
        sub->parent = current;
        sub->handle = handle;
        sub->filec = vlc_loaddir (handle, &sub->filev, visible, p_sys->compar);
        if (sub->filec < 0)
            sub->filev = NULL;
        sub->i = 0;
#ifdef HAVE_OPENAT
        sub->device = st.st_dev;
        sub->inode = st.st_ino;
#else
        sub->path = path;
#endif
        p_sys->current = sub;

        char *encoded = encode_URI_component (entry);
        if (encoded == NULL
         || (asprintf (&sub->uri, "%s/%s", current->uri, encoded) == -1))
             sub->uri = NULL;
        free (encoded);
        if (unlikely(sub->uri == NULL))
        {
            free (entry);
            goto fatal;
        }

        /* Add node to XSPF extension */
        char *old_xspf_ext = p_sys->xspf_ext;
        EnsureUTF8 (entry);
        char *title = convert_xml_special_chars (entry);
        if (old_xspf_ext != NULL
         && asprintf (&p_sys->xspf_ext, "%s  <vlc:node title=\"%s\">\n",
                      old_xspf_ext, title ? title : "?") == -1)
            p_sys->xspf_ext = NULL;
        free (old_xspf_ext);
        free (title);
        goto skip;
    }

notdir:
    /* Skip files with ignored extensions */
    if (p_sys->ignored_exts != NULL)
    {
        const char *ext = strrchr (entry, '.');
        if (ext != NULL)
        {
            size_t extlen = strlen (++ext);
            for (const char *type = p_sys->ignored_exts, *end;
                 type[0]; type = end + 1)
            {
                end = strchr (type, ',');
                if (end == NULL)
                    end = type + strlen (type);

                if (type + extlen == end
                 && !strncasecmp (ext, type, extlen))
                {
                    free (entry);
                    return NULL;
                }

                if (*end == '\0')
                    break;
            }
        }
    }

    char *encoded = encode_URI_component (entry);
    free (entry);
    if (encoded == NULL)
        goto fatal;
    int len = asprintf (&entry,
                        "  <track><location>%s/%s</location>\n" \
                        "   <extension application=\"http://www.videolan.org/vlc/playlist/0\">\n" \
                        "    <vlc:id>%d</vlc:id>\n" \
                        "   </extension>\n" \
                        "  </track>\n",
                        current->uri, encoded, p_sys->i_item_count++);
    free (encoded);
    if (len == -1)
        goto fatal;

    /* Write the ID to the extension */
    char *old_xspf_ext = p_sys->xspf_ext;
    if (old_xspf_ext != NULL
     && asprintf (&p_sys->xspf_ext, "%s   <vlc:item tid=\"%i\" />\n",
                  old_xspf_ext, p_sys->i_item_count - 1) == -1)
        p_sys->xspf_ext = NULL;
    free (old_xspf_ext);

    block_t *block = block_heap_Alloc (entry, len);
    if (unlikely(block == NULL))
        goto fatal;
    return block;

fatal:
    p_access->info.b_eof = true;
    return NULL;

skip:
    free (entry);
    return NULL;
}

/*****************************************************************************
 * Control:
 *****************************************************************************/
int DirControl( access_t *p_access, int i_query, va_list args )
{
    switch( i_query )
    {
        /* */
        case ACCESS_CAN_SEEK:
        case ACCESS_CAN_FASTSEEK:
            *va_arg( args, bool* ) = false;
            break;

        case ACCESS_CAN_PAUSE:
        case ACCESS_CAN_CONTROL_PACE:
            *va_arg( args, bool* ) = true;
            break;

        /* */
        case ACCESS_GET_PTS_DELAY:
            *va_arg( args, int64_t * ) = DEFAULT_PTS_DELAY * 1000;
            break;

        /* */
        case ACCESS_SET_PAUSE_STATE:
        case ACCESS_GET_TITLE_INFO:
        case ACCESS_SET_TITLE:
        case ACCESS_SET_SEEKPOINT:
        case ACCESS_SET_PRIVATE_ID_STATE:
        case ACCESS_GET_CONTENT_TYPE:
        case ACCESS_GET_META:
            return VLC_EGENERIC;

        default:
            msg_Warn( p_access, "unimplemented query in control" );
            return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}
