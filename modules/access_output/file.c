/*****************************************************************************
 * file.c
 *****************************************************************************
 * Copyright (C) 2001, 2002 VideoLAN
 * $Id: file.c,v 1.5 2003/04/29 15:40:31 fenrir Exp $
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
#elif defined( WIN32 ) && !defined( UNDER_CE )
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
    set_description( _("File stream ouput") );
    set_capability( "sout access", 50 );
    add_shortcut( "file" );
    set_callbacks( Open, Close );
vlc_module_end();

struct sout_access_out_sys_t
{
    FILE *p_file;

};

/*****************************************************************************
 * Open: open the file
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    sout_access_out_t   *p_access = (sout_access_out_t*)p_this;

    if( !( p_access->p_sys = malloc( sizeof( sout_access_out_sys_t ) ) ) )
    {
        msg_Err( p_access, "out of memory" );
        return( VLC_EGENERIC );
    }

    if( !p_access->psz_name )
    {
        msg_Err( p_access, "no file name specified" );
        return VLC_EGENERIC;
    }
    if( !strcmp( p_access->psz_name, "-" ) )
    {
        p_access->p_sys->p_file = stdout;
        msg_Dbg( p_access, "using stdout" );
    }
    else if( !( p_access->p_sys->p_file = fopen( p_access->psz_name, "wb" ) ) )
    {
        msg_Err( p_access, "cannot open `%s'", p_access->psz_name );
        free( p_access->p_sys );
        return( VLC_EGENERIC );
    }

    p_access->pf_write        = Write;
    p_access->pf_seek         = Seek;

    msg_Info( p_access, "Open: name:`%s'", p_access->psz_name );
    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close: close the target
 *****************************************************************************/
static void Close( vlc_object_t * p_this )
{
    sout_access_out_t   *p_access = (sout_access_out_t*)p_this;

    if( strcmp( p_access->psz_name, "-" ) )
    {
        if( p_access->p_sys->p_file )
        {
            fclose( p_access->p_sys->p_file );
        }
    }
    free( p_access->p_sys );

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

        i_write += fwrite( p_buffer->p_buffer, 1, p_buffer->i_size,
                           p_access->p_sys->p_file );
        p_next = p_buffer->p_next;
        sout_BufferDelete( p_access->p_sout, p_buffer );
        p_buffer = p_next;

    } while( p_buffer );

    return( i_write );
}

/*****************************************************************************
 * Seek: seek to a specific location in a file
 *****************************************************************************/
static int Seek( sout_access_out_t *p_access, off_t i_pos )
{

    msg_Dbg( p_access, "Seek: pos:"I64Fd, (int64_t)i_pos );
    if( strcmp( p_access->psz_name, "-" ) )
    {
        return( fseek( p_access->p_sys->p_file, i_pos, SEEK_SET ) );
    }
    else
    {
        msg_Err( p_access, "cannot seek while using stdout" );
        return VLC_EGENERIC;
    }
}



