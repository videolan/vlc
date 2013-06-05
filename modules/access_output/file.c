/*****************************************************************************
 * file.c
 *****************************************************************************
 * Copyright (C) 2001, 2002 the VideoLAN team
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Eric Petit <titer@videolan.org>
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <sys/types.h>
#include <time.h>
#include <fcntl.h>
#include <errno.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_sout.h>
#include <vlc_block.h>
#include <vlc_fs.h>
#include <vlc_strings.h>
#include <vlc_dialog.h>

#if defined( _WIN32 ) || defined( __OS2__ )
#   include <io.h>
#endif

#ifndef _WIN32
#   include <unistd.h>
#endif

#ifndef O_LARGEFILE
#   define O_LARGEFILE 0
#endif

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

#define SOUT_CFG_PREFIX "sout-file-"
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


/*****************************************************************************
 * Exported prototypes
 *****************************************************************************/
static const char *const ppsz_sout_options[] = {
    "append",
    "format",
    "overwrite",
#ifdef O_SYNC
    "sync",
#endif
    NULL
};

static ssize_t Write( sout_access_out_t *, block_t * );
static int Seek ( sout_access_out_t *, off_t  );
static ssize_t Read ( sout_access_out_t *, block_t * );
static int Control( sout_access_out_t *, int, va_list );

/*****************************************************************************
 * Open: open the file
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    sout_access_out_t   *p_access = (sout_access_out_t*)p_this;
    int                 fd;

    config_ChainParse( p_access, SOUT_CFG_PREFIX, ppsz_sout_options, p_access->p_cfg );

    if( !p_access->psz_path )
    {
        msg_Err( p_access, "no file name specified" );
        return VLC_EGENERIC;
    }

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
            msg_Err (p_access, "cannot use file descriptor: %m");
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
            msg_Err (p_access, "cannot use standard output: %m");
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
            buf = str_format_time (path);
            path_sanitize (buf);
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
                msg_Err (p_access, "cannot create %s: %m", path);
            if (overwrite || errno != EEXIST)
                break;
            flags &= ~O_EXCL;
        }
        while (dialog_Question (p_access, path,
                                _("The output file already exists. "
                                "If recording continues, the file will be "
                                "overridden and its content will be lost."),
                                _("Keep existing file"),
                                _("Overwrite"), NULL) == 2);
        free (buf);
        if (fd == -1)
            return VLC_EGENERIC;
    }

    p_access->pf_write = Write;
    p_access->pf_read  = Read;
    p_access->pf_seek  = Seek;
    p_access->pf_control = Control;
    p_access->p_sys    = (void *)(intptr_t)fd;

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

    close( (intptr_t)p_access->p_sys );

    msg_Dbg( p_access, "file access output closed" );
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

        default:
            return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}

/*****************************************************************************
 * Read: standard read on a file descriptor.
 *****************************************************************************/
static ssize_t Read( sout_access_out_t *p_access, block_t *p_buffer )
{
    ssize_t val;

    do
        val = read( (intptr_t)p_access->p_sys, p_buffer->p_buffer,
                    p_buffer->i_buffer );
    while (val == -1 && errno == EINTR);
    return val;
}

/*****************************************************************************
 * Write: standard write on a file descriptor.
 *****************************************************************************/
static ssize_t Write( sout_access_out_t *p_access, block_t *p_buffer )
{
    size_t i_write = 0;

    while( p_buffer )
    {
        ssize_t val = write ((intptr_t)p_access->p_sys,
                             p_buffer->p_buffer, p_buffer->i_buffer);
        if (val <= 0)
        {
            if (errno == EINTR)
                continue;
            block_ChainRelease (p_buffer);
            msg_Err( p_access, "cannot write: %m" );
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

/*****************************************************************************
 * Seek: seek to a specific location in a file
 *****************************************************************************/
static int Seek( sout_access_out_t *p_access, off_t i_pos )
{
    return lseek( (intptr_t)p_access->p_sys, i_pos, SEEK_SET );
}
