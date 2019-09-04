/*****************************************************************************
 * file.c: file input (file: access plug-in)
 *****************************************************************************
 * Copyright (C) 2001-2006 VLC authors and VideoLAN
 * Copyright © 2006-2007 Rémi Denis-Courmont
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <assert.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#ifdef HAVE_FSTATVFS
#   include <sys/statvfs.h>
#   if defined (HAVE_SYS_MOUNT_H)
#      include <sys/param.h>
#      include <sys/mount.h>
#   endif
#endif
#ifdef HAVE_LINUX_MAGIC_H
#   include <sys/vfs.h>
#   include <linux/magic.h>
#endif

#if defined( _WIN32 )
#   include <io.h>
#   include <ctype.h>
#   include <shlwapi.h>
#else
#   include <unistd.h>
#endif
#include <dirent.h>

#include <vlc_common.h>
#include "fs.h"
#include <vlc_access.h>
#ifdef _WIN32
# include <vlc_charset.h>
#endif
#include <vlc_fs.h>
#include <vlc_url.h>
#include <vlc_interrupt.h>

typedef struct
{
    int fd;

    bool b_pace_control;
} access_sys_t;

#if !defined (_WIN32) && !defined (__OS2__)
static bool IsRemote (int fd)
{
#if defined (HAVE_FSTATVFS) && defined (MNT_LOCAL)
    struct statvfs stf;

    if (fstatvfs (fd, &stf))
        return false;
    /* fstatvfs() is in POSIX, but MNT_LOCAL is not */
    return !(stf.f_flag & MNT_LOCAL);

#elif defined (HAVE_LINUX_MAGIC_H)
    struct statfs stf;

    if (fstatfs (fd, &stf))
        return false;

    switch ((unsigned long)stf.f_type)
    {
        case AFS_SUPER_MAGIC:
        case CODA_SUPER_MAGIC:
        case NCP_SUPER_MAGIC:
        case NFS_SUPER_MAGIC:
        case SMB_SUPER_MAGIC:
        case 0xFF534D42 /*CIFS_MAGIC_NUMBER*/:
            return true;
    }
    return false;

#else
    (void)fd;
    return false;

#endif
}
# define IsRemote(fd,path) IsRemote(fd)

#else /* _WIN32 || __OS2__ */
static bool IsRemote (const char *path)
{
# if !defined(__OS2__) && !VLC_WINSTORE_APP
    wchar_t *wpath = ToWide (path);
    bool is_remote = (wpath != NULL && PathIsNetworkPathW (wpath));
    free (wpath);
    return is_remote;
# else
    return (! strncmp(path, "\\\\", 2));
# endif
}
# define IsRemote(fd,path) IsRemote(path)
#endif

#ifndef HAVE_POSIX_FADVISE
# define posix_fadvise(fd, off, len, adv)
#endif

static ssize_t Read (stream_t *, void *, size_t);
static int FileSeek (stream_t *, uint64_t);
static int FileControl (stream_t *, int, va_list);

/*****************************************************************************
 * FileOpen: open the file
 *****************************************************************************/
int FileOpen( vlc_object_t *p_this )
{
    stream_t *p_access = (stream_t*)p_this;

    /* Open file */
    int fd = -1;

    if (!strcasecmp (p_access->psz_name, "fd"))
    {
        char *end;
        int oldfd = strtol (p_access->psz_location, &end, 10);

        if (*end == '\0')
            fd = vlc_dup (oldfd);
        else if (*end == '/' && end > p_access->psz_location)
        {
            char *name = vlc_uri_decode_duplicate (end - 1);
            if (name != NULL)
            {
                name[0] = '.';
                fd = vlc_openat (oldfd, name, O_RDONLY | O_NONBLOCK);
                free (name);
            }
        }
    }
    else
    {
        if (unlikely(p_access->psz_filepath == NULL))
            return VLC_EGENERIC;
        fd = vlc_open (p_access->psz_filepath, O_RDONLY | O_NONBLOCK);
    }

    if (fd == -1)
    {
        msg_Err (p_access, "cannot open file %s (%s)",
                 p_access->psz_filepath ? p_access->psz_filepath
                                        : p_access->psz_location,
                 vlc_strerror_c(errno));
        return VLC_EGENERIC;
    }

    struct stat st;
    if (fstat (fd, &st))
    {
        msg_Err (p_access, "read error: %s", vlc_strerror_c(errno));
        goto error;
    }

#if O_NONBLOCK
    /* Force blocking mode back */
    fcntl (fd, F_SETFL, fcntl (fd, F_GETFL) & ~O_NONBLOCK);
#endif

    /* Directories can be opened and read from, but only readdir() knows
     * how to parse the data. The directory plugin will do it. */
    if (S_ISDIR (st.st_mode))
    {
#ifdef HAVE_FDOPENDIR
        DIR *p_dir = fdopendir(fd);
        if (!p_dir) {
            msg_Err (p_access, "fdopendir error: %s", vlc_strerror_c(errno));
            goto error;
        }
        return DirInit (p_access, p_dir);
#else
        msg_Dbg (p_access, "ignoring directory");
        goto error;
#endif
    }

    access_sys_t *p_sys = vlc_obj_malloc(p_this, sizeof (*p_sys));
    if (unlikely(p_sys == NULL))
        goto error;
    p_access->pf_read = Read;
    p_access->pf_block = NULL;
    p_access->pf_control = FileControl;
    p_access->p_sys = p_sys;
    p_sys->fd = fd;

    if (S_ISREG (st.st_mode) || S_ISBLK (st.st_mode))
    {
        p_access->pf_seek = FileSeek;
        p_sys->b_pace_control = true;

        /* Demuxers will need the beginning of the file for probing. */
        posix_fadvise (fd, 0, 4096, POSIX_FADV_WILLNEED);
        /* In most cases, we only read the file once. */
        posix_fadvise (fd, 0, 0, POSIX_FADV_NOREUSE);
#ifdef F_NOCACHE
        fcntl (fd, F_NOCACHE, 0);
#endif
#ifdef F_RDAHEAD
        if (IsRemote(fd, p_access->psz_filepath))
            fcntl (fd, F_RDAHEAD, 0);
        else
            fcntl (fd, F_RDAHEAD, 1);
#endif
    }
    else
    {
        p_access->pf_seek = NULL;
        p_sys->b_pace_control = strcasecmp (p_access->psz_name, "stream");
    }

    return VLC_SUCCESS;

error:
    vlc_close (fd);
    return VLC_EGENERIC;
}

/*****************************************************************************
 * FileClose: close the target
 *****************************************************************************/
void FileClose (vlc_object_t * p_this)
{
    stream_t     *p_access = (stream_t*)p_this;

    if (p_access->pf_read == NULL)
    {
        DirClose (p_this);
        return;
    }

    access_sys_t *p_sys = p_access->p_sys;

    vlc_close (p_sys->fd);
}


static ssize_t Read (stream_t *p_access, void *p_buffer, size_t i_len)
{
    access_sys_t *p_sys = p_access->p_sys;
    int fd = p_sys->fd;

    ssize_t val = vlc_read_i11e (fd, p_buffer, i_len);
    if (val < 0)
    {
        switch (errno)
        {
            case EINTR:
            case EAGAIN:
                return -1;
        }

        msg_Err (p_access, "read error: %s", vlc_strerror_c(errno));
        val = 0;
    }

    return val;
}

/*****************************************************************************
 * Seek: seek to a specific location in a file
 *****************************************************************************/
static int FileSeek (stream_t *p_access, uint64_t i_pos)
{
    access_sys_t *sys = p_access->p_sys;

    if (lseek(sys->fd, i_pos, SEEK_SET) == (off_t)-1)
        return VLC_EGENERIC;
    return VLC_SUCCESS;
}

/*****************************************************************************
 * Control:
 *****************************************************************************/
static int FileControl( stream_t *p_access, int i_query, va_list args )
{
    access_sys_t *p_sys = p_access->p_sys;
    bool    *pb_bool;
    vlc_tick_t *pi_64;

    switch( i_query )
    {
        case STREAM_CAN_SEEK:
        case STREAM_CAN_FASTSEEK:
            pb_bool = va_arg( args, bool * );
            *pb_bool = (p_access->pf_seek != NULL);
            break;

        case STREAM_CAN_PAUSE:
        case STREAM_CAN_CONTROL_PACE:
            pb_bool = va_arg( args, bool * );
            *pb_bool = p_sys->b_pace_control;
            break;

        case STREAM_GET_SIZE:
        {
            struct stat st;

            if (fstat (p_sys->fd, &st) || !S_ISREG(st.st_mode))
                return VLC_EGENERIC;
            *va_arg( args, uint64_t * ) = st.st_size;
            break;
        }

        case STREAM_GET_PTS_DELAY:
            pi_64 = va_arg( args, vlc_tick_t * );
            if (IsRemote (p_sys->fd, p_access->psz_filepath))
                *pi_64 = VLC_TICK_FROM_MS(
                        var_InheritInteger (p_access, "network-caching") );
            else
                *pi_64 = VLC_TICK_FROM_MS(
                        var_InheritInteger (p_access, "file-caching") );
            break;

        case STREAM_SET_PAUSE_STATE:
            /* Nothing to do */
            break;

        default:
            return VLC_EGENERIC;

    }
    return VLC_SUCCESS;
}
