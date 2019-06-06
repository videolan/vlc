/*****************************************************************************
 * edummy.c: dummy encoder plugin for vlc.
 *****************************************************************************
 * Copyright (C) 2002 VLC authors and VideoLAN
 *
 * Authors: Gildas Bazin <gbazin@netcourrier.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_codec.h>

static int OpenEncoder( vlc_object_t * );
static void CloseEncoder( vlc_object_t * );

vlc_module_begin ()
    set_shortname( N_("Dummy") )
    set_description( N_("Dummy encoder") )
    set_capability( "encoder", 0 )
    set_callbacks( OpenEncoder, CloseEncoder )
    add_shortcut( "dummy" )
vlc_module_end ()


/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static block_t *EncodeVideo( encoder_t *p_enc, picture_t *p_pict );
static block_t *EncodeAudio( encoder_t *p_enc, block_t *p_buf );

/*****************************************************************************
 * OpenDecoder: open the dummy encoder.
 *****************************************************************************/
static int OpenEncoder( vlc_object_t *p_this )
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
    VLC_UNUSED(p_enc); VLC_UNUSED(p_pict);
    return NULL;
}

/****************************************************************************
 * EncodeAudio: the whole thing
 ****************************************************************************/
static block_t *EncodeAudio( encoder_t *p_enc, block_t *p_buf )
{
    VLC_UNUSED(p_enc); VLC_UNUSED(p_buf);
    return NULL;
}

/*****************************************************************************
 * CloseDecoder: decoder destruction
 *****************************************************************************/
static void CloseEncoder( vlc_object_t *p_this )
{
    VLC_UNUSED(p_this);
}
