/*****************************************************************************
 * encoder.c: dummy encoder plugin for vlc.
 *****************************************************************************
 * Copyright (C) 2002 the VideoLAN team
 * $Id$
 *
 * Authors: Gildas Bazin <gbazin@netcourrier.com>
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
#include <vlc/vlc.h>
#include <vlc/decoder.h>

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static block_t *EncodeVideo( encoder_t *p_enc, picture_t *p_pict );
static block_t *EncodeAudio( encoder_t *p_enc, aout_buffer_t *p_buf );

/*****************************************************************************
 * OpenDecoder: open the dummy encoder.
 *****************************************************************************/
int E_(OpenEncoder) ( vlc_object_t *p_this )
{
    encoder_t *p_enc = (encoder_t *)p_this;

    p_enc->pf_encode_video = EncodeVideo;
    p_enc->pf_encode_audio = EncodeAudio;

    return VLC_SUCCESS;
}

/****************************************************************************
 * EncodeVideo: the whole thing
 ****************************************************************************/
static block_t *EncodeVideo( encoder_t *p_enc, picture_t *p_pict )
{
    return NULL;
}

/****************************************************************************
 * EncodeAudio: the whole thing
 ****************************************************************************/
static block_t *EncodeAudio( encoder_t *p_enc, aout_buffer_t *p_buf )
{
    return NULL;
}

/*****************************************************************************
 * CloseDecoder: decoder destruction
 *****************************************************************************/
void E_(CloseEncoder) ( vlc_object_t *p_this )
{
}
