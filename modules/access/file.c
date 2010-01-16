/*****************************************************************************
 * file.c: file input (file: access plug-in)
 *****************************************************************************
 * Copyright (C) 2001-2006 the VideoLAN team
 * Copyright © 2006-2007 Rémi Denis-Courmont
 * $Id$
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
 *          Rémi Denis-Courmont <rem # videolan # org>
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include "fs.h"
#include <vlc_input.h>
#include <vlc_access.h>
#include <vlc_dialog.h>

#include <assert.h>
#include <errno.h>
#ifdef HAVE_SYS_TYPES_H
#   include <sys/types.h>
#endif
#ifdef HAVE_SYS_STAT_H
#   include <sys/stat.h>
#endif
#ifdef HAVE_FCNTL_H
#   include <fcntl.h>
#endif
#if defined (__linux__)
#   include <sys/vfs.h>
#ifdef HAVE_LINUX_MAGIC_H
#   include <linux/magic.h>
#endif
#elif defined (HAVE_SYS_MOUNT_H)
#   include <sys/param.h>
#   include <sys/mount.h>
#endif

#if defined( WIN32 )
#   include <io.h>
#   include <ctype.h>
#   include <shlwapi.h>
#else
#   include <unistd.h>
#   include <poll.h>
#endif

#if defined( WIN32 ) && !defined( UNDER_CE )
#   ifdef lseek
#      undef lseek
#   endif
#   define lseek _lseeki64
#elif defined( UNDER_CE )
/* FIXME the commandline on wince is a mess */
# define dup(a) -1
# define PathIsNetworkPathW(wpath) (! wcsncmp(wpath, L"\\\\", 2))
#endif

#include <vlc_charset.h>

struct access_sys_t
{
    unsigned int i_nb_reads;

    int fd;

    /* */
    unsigned caching;
    bool b_pace_control;
};

#ifndef WIN32
static bool IsRemote (int fd)
{
#ifdef HAVE_FSTATFS
    struct statfs stf;

    if (fstatfs (fd, &stf))
        return false;

#if defined(MNT_LOCAL)
    return !(stf.f_flags & MNT_LOCAL);

#else
#   ifdef HAVE_LINUX_MAGIC_H
    switch (stf.f_type)
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
#   endif
#endif
#else /* !HAVE_FSTATFS */
    (void)fd;
    return false;

#endif
}
#endif

#ifndef HAVE_POSIX_FADVISE
# define posix_fadvise(fd, off, len, adv)
#endif

/*****************************************************************************
 * Open: open the file
 *****************************************************************************/
int Open( vlc_object_t *p_this )
{
    access_t     *p_access = (access_t*)p_this;
    access_sys_t *p_sys = malloc (sizeof (*p_sys));
    const char   *path = p_access->psz_path;
#ifdef WIN32
    bool is_remote = false;
#endif

    if (unlikely(p_sys == NULL))
        return VLC_ENOMEM;

    access_InitFields (p_access);
    p_access->pf_read = FileRead;
    p_access->pf_block = NULL;
    p_access->pf_control = FileControl;
    p_access->pf_seek = FileSeek;
    p_access->p_sys = p_sys;
    p_sys->i_nb_reads = 0;
    p_sys->b_pace_control = true;

    /* Open file */
    int fd = -1;

    if (!strcasecmp (p_access->psz_access, "fd"))
        fd = dup (atoi (path));
    else if (!strcmp (path, "-"))
        fd = dup (0);
    else
    {
        msg_Dbg (p_access, "opening file `%s'", path);
        fd = utf8_open (path, O_RDONLY | O_NONBLOCK);
        if (fd == -1)
        {
            msg_Err (p_access, "cannot open file %s (%m)", path);
            dialog_Fatal (p_access, _("File reading failed"),
                          _("VLC could not open the file \"%s\"."), path);
        }

#ifdef WIN32
        wchar_t wpath[MAX_PATH+1];
        if (MultiByteToWideChar (CP_UTF8, 0, path, -1,
                                 wpath, MAX_PATH)
         && PathIsNetworkPathW (wpath))
            is_remote = true;
# define IsRemote( fd ) ((void)fd, is_remote)
#endif
    }
    if (fd == -1)
        goto error;

#ifdef HAVE_SYS_STAT_H
    struct stat st;

    if (fstat (fd, &st))
    {
        msg_Err (p_access, "failed to read (%m)");
        goto error;
    }
    /* Directories can be opened and read from, but only readdir() knows
     * how to parse the data. The directory plugin will do it. */
    if (S_ISDIR (st.st_mode))
    {
        msg_Dbg (p_access, "ignoring directory");
        goto error;
    }
    if (S_ISREG (st.st_mode))
        p_access->info.i_size = st.st_size;
    else if (!S_ISBLK (st.st_mode))
    {
        p_access->pf_seek = NoSeek;
        p_sys->b_pace_control = strcasecmp (p_access->psz_access, "stream");
    }
#else
# warning File size not known!
#endif

    p_sys->caching = var_CreateGetInteger (p_access, "file-caching");
    if (IsRemote(fd))
        p_sys->caching += var_CreateGetInteger (p_access, "network-caching");

    p_sys->fd = fd;

    if (p_access->pf_seek != NoSeek)
    {
        /* Demuxers will need the beginning of the file for probing. */
        posix_fadvise (fd, 0, 4096, POSIX_FADV_WILLNEED);
        /* In most cases, we only read the file once. */
        posix_fadvise (fd, 0, 0, POSIX_FADV_NOREUSE);
#if defined(HAVE_FCNTL)
        /* We'd rather use any available memory for reading ahead
         * than for caching what we've already seen/heard */
# if defined(F_RDAHEAD)
        fcntl (fd, F_RDAHEAD, 1);
# endif
# if defined(F_NOCACHE)
        fcntl (fd, F_NOCACHE, 1);
# endif
#endif
    }
    return VLC_SUCCESS;

error:
    if (fd != -1)
        close (fd);
    free (p_sys);
    return VLC_EGENERIC;
}

/*****************************************************************************
 * Close: close the target
 *****************************************************************************/
void Close (vlc_object_t * p_this)
{
    access_t     *p_access = (access_t*)p_this;
    access_sys_t *p_sys = p_access->p_sys;

    close (p_sys->fd);
    free (p_sys);
}


#include <vlc_network.h>

/*****************************************************************************
 * Read: standard read on a file descriptor.
 *****************************************************************************/
ssize_t FileRead( access_t *p_access, uint8_t *p_buffer, size_t i_len )
{
    access_sys_t *p_sys = p_access->p_sys;
    int fd = p_sys->fd;
    ssize_t i_ret;

#ifndef WIN32
    if (p_access->pf_seek == NoSeek)
        i_ret = net_Read (p_access, fd, NULL, p_buffer, i_len, false);
    else
#endif
        i_ret = read (fd, p_buffer, i_len);

    if( i_ret < 0 )
    {
        switch (errno)
        {
            case EINTR:
            case EAGAIN:
                break;

            default:
                msg_Err (p_access, "failed to read (%m)");
                dialog_Fatal (p_access, _("File reading failed"), "%s",
                              _("VLC could not read the file."));
                p_access->info.b_eof = true;
                return 0;
        }
    }
    else if( i_ret > 0 )
        p_access->info.i_pos += i_ret;
    else
        p_access->info.b_eof = true;

    p_sys->i_nb_reads++;

    if ((p_access->info.i_size && !(p_sys->i_nb_reads % INPUT_FSTAT_NB_READS))
     || (p_access->info.i_pos > p_access->info.i_size))
    {
#ifdef HAVE_SYS_STAT_H
        struct stat st;

        if ((fstat (fd, &st) == 0)
         && (p_access->info.i_size != st.st_size))
        {
            p_access->info.i_size = st.st_size;
            p_access->info.i_update |= INPUT_UPDATE_SIZE;
        }
#endif
    }
    return i_ret;
}


/*****************************************************************************
 * Seek: seek to a specific location in a file
 *****************************************************************************/
int FileSeek (access_t *p_access, int64_t i_pos)
{
    p_access->info.i_pos = i_pos;
    p_access->info.b_eof = false;

    lseek (p_access->p_sys->fd, i_pos, SEEK_SET);
    return VLC_SUCCESS;
}

int NoSeek (access_t *p_access, int64_t i_pos)
{
    /* assert(0); ?? */
    (void) p_access; (void) i_pos;
    return VLC_EGENERIC;
}

/*****************************************************************************
 * Control:
 *****************************************************************************/
int FileControl( access_t *p_access, int i_query, va_list args )
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
            *pi_64 = p_sys->caching * INT64_C(1000);
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
