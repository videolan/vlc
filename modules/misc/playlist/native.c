/*****************************************************************************
 * native.c :  Native playlist export module
 *****************************************************************************
 * Copyright (C) 2004 VideoLAN
 * $Id: native.c,v 1.2 2004/02/22 15:52:33 zorglub Exp $
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
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
#include <stdlib.h>                                      /* malloc(), free() */

#include <vlc/vlc.h>
#include <vlc/intf.h>

#include <errno.h>                                                 /* ENOMEM */

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
int Export_Native ( vlc_object_t * );

/*****************************************************************************
 * Native: main export function
 *****************************************************************************/
int Export_Native( vlc_object_t *p_this )
{
    playlist_t *p_playlist = (playlist_t*)p_this;

    msg_Dbg(p_playlist, "Saving using native format");
    return VLC_SUCCESS; 
}
