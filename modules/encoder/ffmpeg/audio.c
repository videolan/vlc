/*****************************************************************************
 * audio.c : audio encoder using ffmpeg library
 *****************************************************************************
 * Copyright (C) 2002 VideoLAN
 * $Id: audio.c,v 1.1 2003/01/22 10:41:57 fenrir Exp $
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

#include <avcodec.h>

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
int  E_( OpenEncoderAudio ) ( vlc_object_t * );
void E_( CloseEncoderAudio )( vlc_object_t * );

struct encoder_sys_t
{
    void *audio_handle;
};


int  E_( OpenEncoderAudio ) ( vlc_object_t *p_this )
{
    return VLC_EGENERIC;
}

void E_( CloseEncoderAudio )( vlc_object_t *p_this )
{
    ;
}

