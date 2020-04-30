/*****************************************************************************
 * file.c
 *****************************************************************************
 * Copyright (C) 2001, 2002 VLC authors and VideoLAN
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Eric Petit <titer@videolan.org>
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

#include <assert.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>
#ifdef __OS2__
#   include <io.h>      /* setmode() */
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_sout.h>
#include <vlc_block.h>
#include <vlc_fs.h>
#include <vlc_network.h>
#include <vlc_strings.h>
#include <vlc_dialog.h>

#ifndef O_LARGEFILE
#   define O_LARGEFILE 0
#endif
#ifndef _POSIX_REALTIME_SIGNALS
# define _POSIX_REALTIME_SIGNALS (-1)
#endif

#define SOUT_CFG_PREFIX "sout-file-"

/*****************************************************************************
 * Read: standard read on a file descriptor.
 *****************************************************************************/
static ssize_t Read( sout_access_out_t *p_access, block_t *p_buffer )
{
    int *fdp = p_access->p_sys, fd = *fdp;
    ssize_t val;

    do
        val = read(fd, p_buffer->p_buffer, p_buffer->i_buffer);
    while (val == -1 && errno == EINTR);
    return val;
}

/*****************************************************************************
 * Write: standard write on a file descriptor.
 *****************************************************************************/
static ssize_t Write( sout_access_out_t *p_access, block_t *p_buffer )
{
    int *fdp = p_access->p_sys, fd = *fdp;
    size_t i_write = 0;

    while( p_buffer )
    {
        ssize_t val = write(fd, p_buffer->p_buffer, p_buffer->i_buffer);
        if (val <= 0)
        {
            if (errno == EINTR)
                continue;
            block_ChainRelease (p_buffer);
            msg_Err( p_access, "cannot write: %s", vlc_strerror_c(errno) );
            return -1;
        }

        if ((size_t)val >= p_buffer->i_buffer)
        {
            block_t *p_next = p_buffer->p_next;
            block_Release (p_buffer);
            p_buffer = p_next;
        }
        else
        {
            p_buffer->p_buffer += val;
            p_buffer->i_buffer -= val;
        }
        i_write += val;
    }
    return i_write;
}

static ssize_t WritePipe(sout_access_out_t *access, block_t *block)
{
    int *fdp = access->p_sys, fd = *fdp;
    ssize_t total = 0;

    while (block != NULL)
    {
        if (block->i_buffer == 0)
        {
            block_t *next = block->p_next;
            block_Release(block);
            block = next;
            continue;
        }

        /* TODO: vectorized I/O with writev() */
        ssize_t val = vlc_write(fd, block->p_buffer, block->i_buffer);
        if (val < 0)
        {
            if (errno == EINTR)
                continue;

            block_ChainRelease(block);
            msg_Err(access, "cannot write: %s", vlc_strerror_c(errno));
            total = -1;
            break;
        }

        total += val;

        assert((size_t)val <= block->i_buffer);
        block->p_buffer += val;
        block->i_buffer -= val;
    }

    return total;
}

#ifdef S_ISSOCK
static ssize_t Send(sout_access_out_t *access, block_t *block)
{
    int *fdp = access->p_sys, fd = *fdp;
    size_t total = 0;

    while (block != NULL)
    {
        if (block->i_buffer == 0)
        {
            block_t *next = block->p_next;
            block_Release(block);
            block = next;
            continue;
        }

        /* TODO: vectorized I/O with sendmsg() */
        ssize_t val = vlc_send(fd, block->p_buffer, block->i_buffer, 0);
        if (val <= 0)
        {   /* FIXME: errno is meaningless if val is zero */
            if (errno == EINTR)
                continue;
            block_ChainRelease(block);
            msg_Err(access, "cannot write: %s", vlc_strerror_c(errno));
            return -1;
        }

        total += val;

        assert((size_t)val <= block->i_buffer);
        block->p_buffer += val;
        block->i_buffer -= val;
    }
    return total;
}
#endif

/*****************************************************************************
 * Seek: seek to a specific location in a file
 *****************************************************************************/
static int Seek( sout_access_out_t *p_access, off_t i_pos )
{
    int *fdp = p_access->p_sys, fd = *fdp;

    return lseek(fd, i_pos, SEEK_SET);
}

static int Control( sout_access_out_t *p_access, int i_query, va_list args )
{
    switch( i_query )
    {
        case ACCESS_OUT_CONTROLS_PACE:
        {
            bool *pb = va_arg( args, bool * );
            *pb = strcmp( p_access->psz_access, "stream" );
            break;
        }

        case ACCESS_OUT_CAN_SEEK:
        {
            bool *pb = va_arg( args, bool * );
            *pb = p_access->pf_seek != NULL;
            break;
        }

        default:
            return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}

static const char *const ppsz_sout_options[] = {
    "append",
    "format",
    "overwrite",
#ifdef O_SYNC
    "sync",
#endif
    NULL
};

/*****************************************************************************
 * Open: open the file
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    sout_access_out_t   *p_access = (sout_access_out_t*)p_this;
    int fd;
    int *fdp = vlc_obj_malloc(p_this, sizeof (*fdp));

    if (unlikely(fdp == NULL))
        return VLC_ENOMEM;

    config_ChainParse( p_access, SOUT_CFG_PREFIX, ppsz_sout_options, p_access->p_cfg );

    bool overwrite = var_GetBool (p_access, SOUT_CFG_PREFIX"overwrite");
    bool append = var_GetBool( p_access, SOUT_CFG_PREFIX "append" );

    if (!strcmp (p_access->psz_access, "fd"))
    {
        char *end;

        fd = strtol (p_access->psz_path, &end, 0);
        if (!*p_access->psz_path || *end)
        {
            msg_Err (p_access, "invalid file descriptor: %s",
                     p_access->psz_path);
            return VLC_EGENERIC;
        }
        fd = vlc_dup (fd);
        if (fd == -1)
        {
            msg_Err (p_access, "cannot use file descriptor: %s",
                     vlc_strerror_c(errno));
            return VLC_EGENERIC;
        }
    }
    else
    if( !strcmp( p_access->psz_path, "-" ) )
    {
#if defined( _WIN32 ) || defined( __OS2__ )
        setmode (STDOUT_FILENO, O_BINARY);
#endif
        fd = vlc_dup (STDOUT_FILENO);
        if (fd == -1)
        {
            msg_Err (p_access, "cannot use standard output: %s",
                     vlc_strerror_c(errno));
            return VLC_EGENERIC;
        }
        msg_Dbg( p_access, "using stdout" );
    }
    else
    {
        const char *path = p_access->psz_path;
        char *buf = NULL;

        if (var_InheritBool (p_access, SOUT_CFG_PREFIX"format"))
        {
            buf = vlc_strftime (path);
            path = buf;
        }

        int flags = O_RDWR | O_CREAT | O_LARGEFILE;
        if (!overwrite)
            flags |= O_EXCL;
        if (!append)
            flags |= O_TRUNC;
#ifdef O_SYNC
        if (var_GetBool (p_access, SOUT_CFG_PREFIX"sync"))
            flags |= O_SYNC;
#endif
        do
        {
            fd = vlc_open (path, flags, 0666);
            if (fd != -1)
                break;
            if (fd == -1)
                msg_Err (p_access, "cannot create %s: %s", path,
                         vlc_strerror_c(errno));
            if (overwrite || errno != EEXIST)
                break;
            flags &= ~O_EXCL;
        }
        while (vlc_dialog_wait_question (p_access, VLC_DIALOG_QUESTION_NORMAL,
                                         _("Keep existing file"),
                                         _("Overwrite"), NULL, path,
                                         _("The output file already exists. "
                                         "If recording continues, the file will be "
                                         "overridden and its content will be lost.")) == 1);
        free (buf);
        if (fd == -1)
            return VLC_EGENERIC;
    }

    *fdp = fd;
    p_access->p_sys = fdp;

    struct stat st;

    if (fstat (fd, &st))
    {
        msg_Err (p_access, "write error: %s", vlc_strerror_c(errno));
        vlc_close (fd);
        return VLC_EGENERIC;
    }

    p_access->pf_read  = Read;

    if (S_ISREG(st.st_mode) || S_ISBLK(st.st_mode))
    {
        p_access->pf_write = Write;
        p_access->pf_seek  = Seek;
    }
#ifdef S_ISSOCK
    else if (S_ISSOCK(st.st_mode))
    {
        p_access->pf_write = Send;
        p_access->pf_seek = NULL;
    }
#endif
    else
    {
        p_access->pf_write = WritePipe;
        p_access->pf_seek = NULL;
    }
    p_access->pf_control = Control;

    msg_Dbg( p_access, "file access output opened (%s)", p_access->psz_path );
    if (append)
        lseek (fd, 0, SEEK_END);

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close: close the target
 *****************************************************************************/
static void Close( vlc_object_t * p_this )
{
    sout_access_out_t *p_access = (sout_access_out_t*)p_this;
    int *fdp = p_access->p_sys, fd = *fdp;

    vlc_close(fd);
    msg_Dbg( p_access, "file access output closed" );
}

#define OVERWRITE_TEXT N_("Overwrite existing file")
#define OVERWRITE_LONGTEXT N_( \
    "If the file already exists, it will be overwritten.")
#define APPEND_TEXT N_("Append to file")
#define APPEND_LONGTEXT N_( "Append to file if it exists instead " \
                            "of replacing it.")
#define FORMAT_TEXT N_("Format time and date")
#define FORMAT_LONGTEXT N_("Perform ISO C time and date formatting " \
    "on the file path")
#define SYNC_TEXT N_("Synchronous writing")
#define SYNC_LONGTEXT N_( "Open the file with synchronous writing.")

vlc_module_begin ()
    set_description( N_("File stream output") )
    set_shortname( N_("File" ))
    set_capability( "sout access", 50 )
    set_category( CAT_SOUT )
    set_subcategory( SUBCAT_SOUT_ACO )
    add_shortcut( "file", "stream", "fd" )
    add_bool( SOUT_CFG_PREFIX "overwrite", true, OVERWRITE_TEXT,
              OVERWRITE_LONGTEXT, true )
    add_bool( SOUT_CFG_PREFIX "append", false, APPEND_TEXT,APPEND_LONGTEXT,
              true )
    add_bool( SOUT_CFG_PREFIX "format", false, FORMAT_TEXT, FORMAT_LONGTEXT,
              true )
#ifdef O_SYNC
    add_bool( SOUT_CFG_PREFIX "sync", false, SYNC_TEXT,SYNC_LONGTEXT,
              false )
#endif
    set_callbacks( Open, Close )
vlc_module_end ()
