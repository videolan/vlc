/*****************************************************************************
 * encoder.c : audio/video encoder using ffmpeg library
 *****************************************************************************
 * Copyright (C) 2002 VideoLAN
 * $Id: encoder.c,v 1.4 2003/04/27 23:16:35 gbazin Exp $
 *
 * Authors: Laurent Aimar
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
#include <vlc/vlc.h>
#include <vlc/vout.h>
#include <vlc/input.h>
#include <vlc/decoder.h>

#include <stdlib.h>

#include "codecs.h"
#include "encoder.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
int  E_( OpenEncoderVideo  )( vlc_object_t * );
void E_( CloseEncoderVideo )( vlc_object_t * );

int  E_( OpenEncoderAudio ) ( vlc_object_t * );
void E_( CloseEncoderAudio )( vlc_object_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

vlc_module_begin();
    set_description( _("ffmpeg video encoder") );
    add_shortcut( "ffmpeg" );
    set_capability( "video encoder", 100 );
    set_callbacks( E_( OpenEncoderVideo ), E_( CloseEncoderVideo ) );
    add_category_hint( "video setting", NULL, VLC_TRUE );
        add_integer( "encoder-ffmpeg-video-bitrate", 1000, NULL, "bitrate (kb/s)", "bitrate (kb/s)", VLC_TRUE );
        add_integer( "encoder-ffmpeg-video-max-key-interval", 10, NULL, "max key interval", "maximum   value  of   frames  between   two  keyframes", VLC_TRUE );
        add_integer( "encoder-ffmpeg-video-min-quant", 2, NULL, "min quantizer", "range 1-31", VLC_TRUE );
        add_integer( "encoder-ffmpeg-video-max-quant", 31, NULL, "max quantizer", "range 1-31", VLC_TRUE );

    add_submodule();
        set_description( _("ffmpeg audio encoder") );
        set_capability( "audio encoder", 50 );
        set_callbacks( E_( OpenEncoderAudio ), E_( CloseEncoderAudio ) );
        add_category_hint( "audio setting", NULL, VLC_TRUE );
            add_integer( "encoder-ffmpeg-audio-bitrate", 64, NULL, "bitrate (kb/s)", "bitrate (kb/s)", VLC_TRUE );

vlc_module_end();
