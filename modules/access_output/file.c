/*****************************************************************************
 * file.c
 *****************************************************************************
 * Copyright (C) 2001, 2002 VideoLAN
 * $Id: file.c,v 1.1 2002/12/14 21:32:41 fenrir Exp $
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

static int     Write( sout_instance_t *, sout_buffer_t * );
static int     Seek( sout_instance_t *, off_t  );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_description( _("File stream ouput") );
    set_capability( "sout access", 50 );
    add_shortcut( "file" );
    set_callbacks( Open, Close );
vlc_module_end();

typedef struct sout_access_data_s
{
    FILE *p_file;

} sout_access_data_t;

/*****************************************************************************
 * Open: open the file
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    sout_instance_t     *p_sout = (sout_instance_t*)p_this;
    sout_access_data_t  *p_access;
    char *              psz_name = p_sout->psz_name;

    p_access = malloc( sizeof( sout_access_data_t ) );

    if( !( p_access->p_file = fopen( psz_name, "wb" ) ) )
    {
        msg_Err( p_sout, "cannot open `%s'", psz_name );
        free( p_access );
        return( -1 );
    }

    p_sout->i_method        = SOUT_METHOD_FILE;
    p_sout->p_access_data   = p_access;
    p_sout->pf_write        = Write;
    p_sout->pf_seek         = Seek;

    msg_Info( p_sout, "Open: name:`%s'", psz_name );
    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close: close the target
 *****************************************************************************/
static void Close( vlc_object_t * p_this )
{
    sout_instance_t     *p_sout = (sout_instance_t*)p_this;
    sout_access_data_t  *p_access = (sout_access_data_t*)p_sout->p_access_data;

    if( p_access->p_file )
    {
        fclose( p_access->p_file );
    }

    msg_Info( p_sout, "Close" );
}

/*****************************************************************************
 * Read: standard read on a file descriptor.
 *****************************************************************************/
static int Write( sout_instance_t *p_sout, sout_buffer_t *p_buffer )
{
    sout_access_data_t  *p_access = (sout_access_data_t*)p_sout->p_access_data;
    size_t i_write = 0;

    do
    {
        sout_buffer_t *p_next;

        i_write += fwrite( p_buffer->p_buffer, 1, p_buffer->i_size,
                           p_access->p_file );
        p_next = p_buffer->p_next;
        sout_BufferDelete( p_sout, p_buffer );
        p_buffer = p_next;

    } while( p_buffer );

    msg_Dbg( p_sout, "Write: len:%d", (uint32_t)i_write );

    return( i_write );
}

/*****************************************************************************
 * Seek: seek to a specific location in a file
 *****************************************************************************/
static int Seek( sout_instance_t *p_sout, off_t i_pos )
{

    sout_access_data_t  *p_access = (sout_access_data_t*)p_sout->p_access_data;

    msg_Dbg( p_sout, "Seek: pos:%lld", (int64_t)i_pos );
    return( fseek( p_access->p_file, i_pos, SEEK_SET ) );
}



