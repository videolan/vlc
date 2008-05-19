/*****************************************************************************
 * decoder.c: stats decoder plugin for vlc.
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

#include <vlc/vlc.h>
#include <vlc_codec.h>

#include "stats.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static picture_t *DecodeBlock( decoder_t *p_dec, block_t **pp_block );

/*****************************************************************************
 * OpenDecoder: Open the decoder
 *****************************************************************************/
int OpenDecoder ( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t*)p_this;

    msg_Dbg( p_this, "opening stats decoder" );

    /* Set callbacks */
    p_dec->pf_decode_video = DecodeBlock;
    p_dec->pf_decode_audio = NULL;
    p_dec->pf_decode_sub = NULL;

    return VLC_SUCCESS;
}

/****************************************************************************
 * RunDecoder: the whole thing
 ****************************************************************************/
static picture_t *DecodeBlock( decoder_t *p_dec, block_t **pp_block )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    block_t *p_block;

    if( !pp_block || !*pp_block ) return NULL;
    p_block = *pp_block;

    if( p_block->i_buffer == kBufferSize )
    {
        msg_Dbg( p_dec, "got %d ms offset", (mdate() - *(mtime_t *)p_block->p_buffer) / 1000 );
    }
    else
        msg_Dbg( p_dec, "got offset of size %d", p_block->i_buffer );

    block_Release( p_block );
    return NULL;
}

/*****************************************************************************
 * CloseDecoder: decoder destruction
 *****************************************************************************/
void CloseDecoder ( vlc_object_t *p_this )
{
    msg_Dbg( p_this, "closing stats decoder" );
}
