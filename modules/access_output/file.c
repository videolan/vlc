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
#include <sys/stat.h>
#include <time.h>
#include <fcntl.h>
#include <errno.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_sout.h>
#include <vlc_block.h>
#include <vlc_charset.h>
#include "vlc_strings.h"

#if defined( WIN32 ) && !defined( UNDER_CE )
#   include <io.h>
#   define lseek _lseeki64
#else
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
#define APPEND_TEXT N_("Append to file")
#define APPEND_LONGTEXT N_( "Append to file if it exists instead " \
                            "of replacing it.")

vlc_module_begin ()
    set_description( N_("File stream output") )
    set_shortname( N_("File" ))
    set_capability( "sout access", 50 )
    set_category( CAT_SOUT )
    set_subcategory( SUBCAT_SOUT_ACO )
    add_shortcut( "file" )
    add_shortcut( "stream" )
    add_bool( SOUT_CFG_PREFIX "append", 0, NULL, APPEND_TEXT,APPEND_LONGTEXT,
              true )
    set_callbacks( Open, Close )
vlc_module_end ()


/*****************************************************************************
 * Exported prototypes
 *****************************************************************************/
static const char *const ppsz_sout_options[] = {
    "append", NULL
};

static ssize_t Write( sout_access_out_t *, block_t * );
static int Seek ( sout_access_out_t *, off_t  );
static ssize_t Read ( sout_access_out_t *, block_t * );
static int Control( sout_access_out_t *, int, va_list );

struct sout_access_out_sys_t
{
    int i_handle;
};

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

    bool append = var_GetBool( p_access, SOUT_CFG_PREFIX "append" );

    if( !strcmp( p_access->psz_path, "-" ) )
    {
#ifndef UNDER_CE
#ifdef WIN32
        setmode (fileno (stdout), O_BINARY);
#endif
        fd = dup (fileno (stdout));
        msg_Dbg( p_access, "using stdout" );
#else
#warning stdout is not supported on Windows Mobile, but may be used on Windows CE
        fd = -1;
#endif
    }
    else
    {
        char *psz_tmp = str_format( p_access, p_access->psz_path );
        path_sanitize( psz_tmp );

        fd = utf8_open( psz_tmp, O_RDWR | O_CREAT | O_LARGEFILE |
                        (append ? 0 : O_TRUNC), 0666 );
        free( psz_tmp );
    }

    if (fd == -1)
    {
        msg_Err( p_access, "cannot open `%s' (%m)", p_access->psz_path );
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
        if (val == -1)
        {
            if (errno == EINTR)
                continue;
            block_ChainRelease (p_buffer);
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
