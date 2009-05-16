/*****************************************************************************
 * encoder.c: stats encoder plugin for vlc.
 *****************************************************************************
 * Copyright (C) 2002-2008 the VideoLAN team
 *
 * Authors: Gildas Bazin <gbazin@netcourrier.com>
 *          Pierre d'Herbemont <pdherbemont@videolan.org>
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

#include "stats.h"

/*****************************************************************************
 * encoder_sys_t
 *****************************************************************************/
struct encoder_sys_t
{
    int i;
};

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static block_t *EncodeVideo( encoder_t *p_enc, picture_t *p_pict );
static block_t *EncodeAudio( encoder_t *p_enc, aout_buffer_t *p_abuff );

/*****************************************************************************
 * OpenDecoder: open the dummy encoder.
 *****************************************************************************/
int OpenEncoder ( vlc_object_t *p_this )
{
    encoder_t *p_enc = (encoder_t *)p_this;

    p_enc->p_sys = malloc(sizeof(encoder_sys_t));

    if( !p_enc->p_sys ) return VLC_ENOMEM;

    p_enc->p_sys->i = 0;

    msg_Dbg( p_this, "opening stats encoder" );

    p_enc->pf_encode_video = EncodeVideo;
    p_enc->pf_encode_audio = EncodeAudio;


    return VLC_SUCCESS;
}

/****************************************************************************
 * EncodeVideo: the whole thing
 ****************************************************************************/
static block_t *EncodeVideo( encoder_t *p_enc, picture_t *p_pict )
{
    (void)p_pict;
    block_t * p_block = block_New( p_enc, kBufferSize );

    *(mtime_t*)p_block->p_buffer = mdate();
    p_block->i_buffer = kBufferSize;
    p_block->i_length = kBufferSize;
    p_block->i_dts = p_pict->date;

    msg_Dbg( p_enc, "putting %"PRIu64"ms",
             *(mtime_t*)p_block->p_buffer / 1000 );
    return p_block;
}

/****************************************************************************
 * EncodeVideo: the whole thing
 ****************************************************************************/
static block_t *EncodeAudio( encoder_t *p_enc, aout_buffer_t *p_abuff )
{
    (void)p_abuff;
    (void)p_enc;
    return NULL;
}


/*****************************************************************************
 * CloseDecoder: decoder destruction
 *****************************************************************************/
void CloseEncoder ( vlc_object_t *p_this )
{
    encoder_t *p_enc = (encoder_t *)p_this;

    msg_Dbg( p_this, "closing stats encoder" );
    free( p_enc->p_sys );
}
