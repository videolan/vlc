/*****************************************************************************
 * file.c
 *****************************************************************************
 * Copyright (C) 2001, 2002 VideoLAN (Centrale Réseaux) and its contributors
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>

#include <vlc/vlc.h>
#include <vlc/sout.h>

#ifdef HAVE_UNISTD_H
#   include <unistd.h>
#elif defined( WIN32 ) && !defined( UNDER_CE )
#   include <io.h>
#endif

/* For those platforms that don't use these */
#ifndef S_IRGRP
#   define S_IRGRP 0
#endif
#ifndef S_IROTH
#   define S_IROTH 0
#endif
#ifndef STDOUT_FILENO
#   define STDOUT_FILENO 1
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

vlc_module_begin();
    set_description( _("File stream output") );
    set_shortname( N_("File" ));
    set_capability( "sout access", 50 );
    set_category( CAT_SOUT );
    set_subcategory( SUBCAT_SOUT_ACO );
    add_shortcut( "file" );
    add_shortcut( "stream" );
    add_bool( SOUT_CFG_PREFIX "append", 0, NULL, APPEND_TEXT,APPEND_LONGTEXT,
              VLC_TRUE );
    set_callbacks( Open, Close );
vlc_module_end();


/*****************************************************************************
 * Exported prototypes
 *****************************************************************************/
static const char *ppsz_sout_options[] = {
    "append", NULL
};

static int Write( sout_access_out_t *, block_t * );
static int Seek ( sout_access_out_t *, off_t  );
static int Read ( sout_access_out_t *, block_t * );

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
    int                 i_flags;
    vlc_value_t         val;

    sout_CfgParse( p_access, SOUT_CFG_PREFIX, ppsz_sout_options, p_access->p_cfg );

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
    i_flags = O_RDWR|O_CREAT|O_LARGEFILE;

    var_Get( p_access, SOUT_CFG_PREFIX "append", &val );
    if( val.b_bool )
    {
        i_flags |= O_APPEND;
    }
    else
    {
        i_flags |= O_TRUNC;
    }
    if( !strcmp( p_access->psz_name, "-" ) )
    {
        p_access->p_sys->i_handle = STDOUT_FILENO;
        msg_Dbg( p_access, "using stdout" );
    }
    else if( ( p_access->p_sys->i_handle =
               open( p_access->psz_name, i_flags,
                     S_IWRITE | S_IREAD | S_IRGRP | S_IROTH ) ) == -1 )
    {
        msg_Err( p_access, "cannot open `%s'", p_access->psz_name );
        free( p_access->p_sys );
        return( VLC_EGENERIC );
    }

    p_access->pf_write = Write;
    p_access->pf_read  = Read;
    p_access->pf_seek  = Seek;

    msg_Dbg( p_access, "file access output opened (`%s')", p_access->psz_name );

    /* Update pace control flag */
    if( p_access->psz_access && !strcmp( p_access->psz_access, "stream" ) )
        p_access->p_sout->i_out_pace_nocontrol++;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close: close the target
 *****************************************************************************/
static void Close( vlc_object_t * p_this )
{
    sout_access_out_t *p_access = (sout_access_out_t*)p_this;

    if( strcmp( p_access->psz_name, "-" ) )
    {
        if( p_access->p_sys->i_handle )
        {
            close( p_access->p_sys->i_handle );
        }
    }
    free( p_access->p_sys );

    /* Update pace control flag */
    if( p_access->psz_access && !strcmp( p_access->psz_access, "stream" ) )
        p_access->p_sout->i_out_pace_nocontrol--;

    msg_Dbg( p_access, "file access output closed" );
}

/*****************************************************************************
 * Read: standard read on a file descriptor.
 *****************************************************************************/
static int Read( sout_access_out_t *p_access, block_t *p_buffer )
{
    if( strcmp( p_access->psz_name, "-" ) )
    {
        return read( p_access->p_sys->i_handle, p_buffer->p_buffer,
                     p_buffer->i_buffer );
    }

    msg_Err( p_access, "cannot read while using stdout" );
    return VLC_EGENERIC;
}

/*****************************************************************************
 * Write: standard write on a file descriptor.
 *****************************************************************************/
static int Write( sout_access_out_t *p_access, block_t *p_buffer )
{
    size_t i_write = 0;

    while( p_buffer )
    {
        block_t *p_next = p_buffer->p_next;;

        i_write += write( p_access->p_sys->i_handle,
                          p_buffer->p_buffer, p_buffer->i_buffer );
        block_Release( p_buffer );

        p_buffer = p_next;
    }

    return i_write;
}

/*****************************************************************************
 * Seek: seek to a specific location in a file
 *****************************************************************************/
static int Seek( sout_access_out_t *p_access, off_t i_pos )
{
    if( strcmp( p_access->psz_name, "-" ) )
    {
#if defined( WIN32 ) && !defined( UNDER_CE )
        return( _lseeki64( p_access->p_sys->i_handle, i_pos, SEEK_SET ) );
#else
        return( lseek( p_access->p_sys->i_handle, i_pos, SEEK_SET ) );
#endif
    }
    else
    {
        msg_Err( p_access, "cannot seek while using stdout" );
        return VLC_EGENERIC;
    }
}
