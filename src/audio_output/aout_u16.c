/*****************************************************************************
 * aout_u16.c: 16 bit unsigned audio output functions
 *****************************************************************************
 * Copyright (C) 1999-2001 VideoLAN
 * $Id: aout_u16.c,v 1.5 2001/12/30 07:09:56 sam Exp $
 *
 * Authors: Michel Kaempf <maxx@via.ecp.fr>
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
#include <stdio.h>                                           /* "intf_msg.h" */
#include <stdlib.h>                            /* calloc(), malloc(), free() */
#include <string.h>

#include <videolan/vlc.h>

#include "audio_output.h"
#include "aout_common.h"

/*****************************************************************************
 * Functions
 *****************************************************************************/
void aout_U16MonoThread( aout_thread_t * p_aout )
{
    intf_ErrMsg( "aout error: 16 bit unsigned mono thread unsupported" );
}

void aout_U16StereoThread( aout_thread_t * p_aout )
{
    intf_ErrMsg( "aout error: 16 bit unsigned stereo thread unsupported" );
}

