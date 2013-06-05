/*****************************************************************************
 * file.c: file input (file: access plug-in)
 *****************************************************************************
 * Copyright (C) 2001-2006 VLC authors and VideoLAN
 * Copyright © 2006-2007 Rémi Denis-Courmont
 * $Id$
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
 *          Rémi Denis-Courmont <rem # videolan # org>
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
#include <vlc_input.h>
#include <vlc_access.h>
#include <vlc_dialog.h>
#ifdef _WIN32
# include <vlc_charset.h>
#endif
#include <vlc_fs.h>
#include <vlc_url.h>

struct access_sys_t
{
    int fd;

    /* */
    bool b_pace_control;
};

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

static ssize_t FileRead (access_t *, uint8_t *, size_t);
static int FileSeek (access_t *, uint64_t);
static ssize_t StreamRead (access_t *, uint8_t *, size_t);
static int NoSeek (access_t *, uint64_t);
static int FileControl (access_t *, int, va_list);

/*****************************************************************************
 * FileOpen: open the file
 *****************************************************************************/
int FileOpen( vlc_object_t *p_this )
{
    access_t     *p_access = (access_t*)p_this;

    /* Open file */
    int fd = -1;

    if (!strcasecmp (p_access->psz_access, "fd"))
    {
        char *end;
        int oldfd = strtol (p_access->psz_location, &end, 10);

        if (*end == '\0')
            fd = vlc_dup (oldfd);
        else if (*end == '/' && end > p_access->psz_location)
        {
            char *name = decode_URI_duplicate (end - 1);
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
        const char *path = p_access->psz_filepath;

        if (unlikely(path == NULL))
            return VLC_EGENERIC;
        msg_Dbg (p_access, "opening file `%s'", path);
        fd = vlc_open (path, O_RDONLY | O_NONBLOCK);
        if (fd == -1)
        {
            msg_Err (p_access, "cannot open file %s (%m)", path);
            dialog_Fatal (p_access, _("File reading failed"),
                          _("VLC could not open the file \"%s\" (%m)."), path);
        }
    }
    if (fd == -1)
        return VLC_EGENERIC;

    struct stat st;
    if (fstat (fd, &st))
    {
        msg_Err (p_access, "failed to read (%m)");
        goto error;
    }

#if O_NONBLOCK
    int flags = fcntl (fd, F_GETFL);
    if (S_ISFIFO (st.st_mode) || S_ISSOCK (st.st_mode))
        /* Force non-blocking mode where applicable (fd://) */
        flags |= O_NONBLOCK;
    else
        /* Force blocking mode when not useful or not specified */
        flags &= ~O_NONBLOCK;
    fcntl (fd, F_SETFL, flags);
#endif

    /* Directories can be opened and read from, but only readdir() knows
     * how to parse the data. The directory plugin will do it. */
    if (S_ISDIR (st.st_mode))
    {
#ifdef HAVE_FDOPENDIR
        DIR *handle = fdopendir (fd);
        if (handle == NULL)
            goto error; /* Uh? */
        return DirInit (p_access, handle);
#else
        msg_Dbg (p_access, "ignoring directory");
        goto error;
#endif
    }

    access_sys_t *p_sys = malloc (sizeof (*p_sys));
    if (unlikely(p_sys == NULL))
        goto error;
    access_InitFields (p_access);
    p_access->pf_block = NULL;
    p_access->pf_control = FileControl;
    p_access->p_sys = p_sys;
    p_sys->fd = fd;

    if (S_ISREG (st.st_mode) || S_ISBLK (st.st_mode))
    {
        p_access->pf_read = FileRead;
        p_access->pf_seek = FileSeek;
        p_access->info.i_size = st.st_size;
        p_sys->b_pace_control = true;

        /* Demuxers will need the beginning of the file for probing. */
        posix_fadvise (fd, 0, 4096, POSIX_FADV_WILLNEED);
        /* In most cases, we only read the file once. */
        posix_fadvise (fd, 0, 0, POSIX_FADV_NOREUSE);
#ifdef F_RDAHEAD
        fcntl (fd, F_RDAHEAD, 1);
#endif
#ifdef F_NOCACHE
        fcntl (fd, F_NOCACHE, 0);
#endif
    }
    else
    {
        p_access->pf_read = StreamRead;
        p_access->pf_seek = NoSeek;
        p_sys->b_pace_control = strcasecmp (p_access->psz_access, "stream");
    }

    return VLC_SUCCESS;

error:
    close (fd);
    return VLC_EGENERIC;
}

/*****************************************************************************
 * FileClose: close the target
 *****************************************************************************/
void FileClose (vlc_object_t * p_this)
{
    access_t     *p_access = (access_t*)p_this;

    if (p_access->pf_read == NULL)
    {
        DirClose (p_this);
        return;
    }

    access_sys_t *p_sys = p_access->p_sys;

    close (p_sys->fd);
    free (p_sys);
}


#include <vlc_network.h>

/**
 * Reads from a regular file.
 */
static ssize_t FileRead (access_t *p_access, uint8_t *p_buffer, size_t i_len)
{
    access_sys_t *p_sys = p_access->p_sys;
    int fd = p_sys->fd;
    ssize_t val = read (fd, p_buffer, i_len);

    if (val < 0)
    {
        switch (errno)
        {
            case EINTR:
            case EAGAIN:
                return -1;
        }

        msg_Err (p_access, "read error: %m");
        dialog_Fatal (p_access, _("File reading failed"),
                      _("VLC could not read the file (%m)."));
        val = 0;
    }

    p_access->info.i_pos += val;
    p_access->info.b_eof = !val;
    if (p_access->info.i_pos >= p_access->info.i_size)
    {
        struct stat st;

        if (fstat (fd, &st) == 0)
            p_access->info.i_size = st.st_size;
    }
    return val;
}


/*****************************************************************************
 * Seek: seek to a specific location in a file
 *****************************************************************************/
static int FileSeek (access_t *p_access, uint64_t i_pos)
{
    p_access->info.i_pos = i_pos;
    p_access->info.b_eof = false;

    lseek (p_access->p_sys->fd, i_pos, SEEK_SET);
    return VLC_SUCCESS;
}

/**
 * Reads from a non-seekable file.
 */
static ssize_t StreamRead (access_t *p_access, uint8_t *p_buffer, size_t i_len)
{
    access_sys_t *p_sys = p_access->p_sys;
    int fd = p_sys->fd;

#if !defined (_WIN32) && !defined (__OS2__)
    ssize_t val = net_Read (p_access, fd, NULL, p_buffer, i_len, false);
#else
    ssize_t val = read (fd, p_buffer, i_len);
#endif

    if (val < 0)
    {
        switch (errno)
        {
            case EINTR:
            case EAGAIN:
                return -1;
        }
        msg_Err (p_access, "read error: %m");
        val = 0;
    }

    p_access->info.i_pos += val;
    p_access->info.b_eof = !val;
    return val;
}

static int NoSeek (access_t *p_access, uint64_t i_pos)
{
    /* assert(0); ?? */
    (void) p_access; (void) i_pos;
    return VLC_EGENERIC;
}

/*****************************************************************************
 * Control:
 *****************************************************************************/
static int FileControl( access_t *p_access, int i_query, va_list args )
{
    access_sys_t *p_sys = p_access->p_sys;
    bool    *pb_bool;
    int64_t *pi_64;

    switch( i_query )
    {
        /* */
        case ACCESS_CAN_SEEK:
        case ACCESS_CAN_FASTSEEK:
            pb_bool = (bool*)va_arg( args, bool* );
            *pb_bool = (p_access->pf_seek != NoSeek);
            break;

        case ACCESS_CAN_PAUSE:
        case ACCESS_CAN_CONTROL_PACE:
            pb_bool = (bool*)va_arg( args, bool* );
            *pb_bool = p_sys->b_pace_control;
            break;

        /* */
        case ACCESS_GET_PTS_DELAY:
            pi_64 = (int64_t*)va_arg( args, int64_t * );
            if (IsRemote (p_sys->fd, p_access->psz_filepath))
                *pi_64 = var_InheritInteger (p_access, "network-caching");
            else
                *pi_64 = var_InheritInteger (p_access, "file-caching");
            *pi_64 *= 1000;
            break;

        /* */
        case ACCESS_SET_PAUSE_STATE:
            /* Nothing to do */
            break;

        case ACCESS_GET_TITLE_INFO:
        case ACCESS_SET_TITLE:
        case ACCESS_SET_SEEKPOINT:
        case ACCESS_SET_PRIVATE_ID_STATE:
        case ACCESS_GET_META:
        case ACCESS_GET_PRIVATE_ID_STATE:
        case ACCESS_GET_CONTENT_TYPE:
            return VLC_EGENERIC;

        default:
            msg_Warn( p_access, "unimplemented query %d in control", i_query );
            return VLC_EGENERIC;

    }
    return VLC_SUCCESS;
}
