/*****************************************************************************
 * dummy.c
 *****************************************************************************
 * Copyright (C) 2001, 2002 VideoLAN
 * $Id: dummy.c,v 1.2 2003/02/16 14:10:44 fenrir Exp $
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

#include <vlc/vlc.h>
#include <vlc/input.h>
#include <vlc/sout.h>

#ifdef HAVE_UNISTD_H
#   include <unistd.h>
#elif defined( _MSC_VER ) && defined( _WIN32 ) && !defined( UNDER_CE )
#   include <io.h>
#endif

/*****************************************************************************
 * Exported prototypes
 *****************************************************************************/
static int     Open   ( vlc_object_t * );
static void    Close  ( vlc_object_t * );

static int     Write( sout_access_out_t *, sout_buffer_t * );
static int     Seek ( sout_access_out_t *, off_t  );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_description( _("Dummy stream ouput") );
    set_capability( "sout access", 0 );
    add_shortcut( "dummy" );
    set_callbacks( Open, Close );
vlc_module_end();


/*****************************************************************************
 * Open: open the file
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    sout_access_out_t   *p_access = (sout_access_out_t*)p_this;

    p_access->p_sys    = NULL;
    p_access->pf_write = Write;
    p_access->pf_seek  = Seek;

    msg_Info( p_access, "dummy stream output access launched" );
    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close: close the target
 *****************************************************************************/
static void Close( vlc_object_t * p_this )
{
    sout_access_out_t   *p_access = (sout_access_out_t*)p_this;
    msg_Info( p_access, "Close" );
}

/*****************************************************************************
 * Read: standard read on a file descriptor.
 *****************************************************************************/
static int Write( sout_access_out_t *p_access, sout_buffer_t *p_buffer )
{
    size_t i_write = 0;

    do
    {
        sout_buffer_t *p_next;
        i_write += p_buffer->i_size;
        p_next = p_buffer->p_next;
        sout_BufferDelete( p_access->p_sout, p_buffer );
        p_buffer = p_next;
    } while( p_buffer );

    msg_Dbg( p_access, "Dummy Skipped: len:"I64Fd, (int64_t)i_write );

    return( i_write );
}

/*****************************************************************************
 * Seek: seek to a specific location in a file
 *****************************************************************************/
static int Seek( sout_access_out_t *p_access, off_t i_pos )
{
    msg_Dbg( p_access, "Seek: pos:"I64Fd, (int64_t)i_pos );
    return( 0 );
}


