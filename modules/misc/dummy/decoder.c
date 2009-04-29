/*****************************************************************************
 * decoder.c: dummy decoder plugin for vlc.
 *****************************************************************************
 * Copyright (C) 2002 the VideoLAN team
 * $Id$
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
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

#include <vlc_common.h>
#include <vlc_codec.h>

#ifdef HAVE_UNISTD_H
#   include <unistd.h> /* write(), close() */
#elif defined( WIN32 ) && !defined( UNDER_CE )
#   include <io.h>
#endif
#ifdef HAVE_SYS_TYPES_H
#   include <sys/types.h> /* open() */
#endif
#ifdef HAVE_SYS_STAT_H
#   include <sys/stat.h>
#endif
#ifdef HAVE_FCNTL_H
#   include <fcntl.h>
#endif

#include <limits.h> /* PATH_MAX */


#include "dummy.h"

/*****************************************************************************
 * decoder_sys_t : theora decoder descriptor
 *****************************************************************************/
struct decoder_sys_t
{
    int i_fd;
};

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static void *DecodeBlock( decoder_t *p_dec, block_t **pp_block );

/*****************************************************************************
 * OpenDecoder: Open the decoder
 *****************************************************************************/
static int OpenDecoderCommon( vlc_object_t *p_this, bool b_force_dump )
{
    decoder_t *p_dec = (decoder_t*)p_this;
    decoder_sys_t *p_sys;
    char psz_file[ PATH_MAX ];
    vlc_value_t val;

    /* Allocate the memory needed to store the decoder's structure */
    if( ( p_dec->p_sys = p_sys =
          (decoder_sys_t *)malloc(sizeof(decoder_sys_t)) ) == NULL )
    {
        return VLC_ENOMEM;
    }

    snprintf( psz_file, sizeof( psz_file), "stream.%p", p_dec );

#ifndef UNDER_CE
    if( !b_force_dump )
    {
        var_Create( p_dec, "dummy-save-es", VLC_VAR_BOOL | VLC_VAR_DOINHERIT );
        var_Get( p_dec, "dummy-save-es", &val );
        b_force_dump = val.b_bool;
    }
    if( b_force_dump )
    {
        p_sys->i_fd = open( psz_file, O_WRONLY | O_CREAT | O_TRUNC, 00644 );

        if( p_sys->i_fd == -1 )
        {
            msg_Err( p_dec, "cannot create `%s'", psz_file );
            free( p_sys );
            return VLC_EGENERIC;
        }

        msg_Dbg( p_dec, "dumping stream to file `%s'", psz_file );
    }
    else
#endif
    {
        p_sys->i_fd = -1;
    }

    /* Set callbacks */
    p_dec->pf_decode_video = (picture_t *(*)(decoder_t *, block_t **))
        DecodeBlock;
    p_dec->pf_decode_audio = (aout_buffer_t *(*)(decoder_t *, block_t **))
        DecodeBlock;
    p_dec->pf_decode_sub = (subpicture_t *(*)(decoder_t *, block_t **))
        DecodeBlock;

    es_format_Copy( &p_dec->fmt_out, &p_dec->fmt_in );

    return VLC_SUCCESS;
}

int OpenDecoder( vlc_object_t *p_this )
{
    return OpenDecoderCommon( p_this, false );
}
int  OpenDecoderDump( vlc_object_t *p_this )
{
    return OpenDecoderCommon( p_this, true );
}

/****************************************************************************
 * RunDecoder: the whole thing
 ****************************************************************************
 * This function must be fed with ogg packets.
 ****************************************************************************/
static void *DecodeBlock( decoder_t *p_dec, block_t **pp_block )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    block_t *p_block;

    if( !pp_block || !*pp_block ) return NULL;
    p_block = *pp_block;

    if( p_sys->i_fd >= 0 &&
        p_block->i_buffer > 0 &&
        (p_block->i_flags & (BLOCK_FLAG_DISCONTINUITY|BLOCK_FLAG_CORRUPTED) ) == 0 )
    {
#ifndef UNDER_CE
        write( p_sys->i_fd, p_block->p_buffer, p_block->i_buffer );
#endif

        msg_Dbg( p_dec, "dumped %zu bytes", p_block->i_buffer );
    }

    block_Release( p_block );
    return NULL;
}

/*****************************************************************************
 * CloseDecoder: decoder destruction
 *****************************************************************************/
void CloseDecoder ( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t *)p_this;
    decoder_sys_t *p_sys = p_dec->p_sys;

#ifndef UNDER_CE
    if( p_sys->i_fd >= 0 )
        close( p_sys->i_fd );
#endif

    free( p_sys );
}

