/*****************************************************************************
 * vout_window.c: MacOS X plugin for vlc
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: vout_window.c,v 1.1 2002/02/18 01:34:44 jlj Exp $
 *
 * Authors: Jon Lech Johansen <jon-vl@nanocrew.net> 
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
#include <errno.h>                                                 /* ENOMEM */
#include <stdlib.h>                                                /* free() */
#include <string.h>                                            /* strerror() */

#include <videolan/vlc.h>

#include "video.h"
#include "video_output.h"

#include "interface.h"

#include "macosx.h"

/*****************************************************************************
 * VLCWindow implementation 
 *****************************************************************************/
@implementation VLCWindow

- (void)setVout:(struct vout_thread_s *)_p_vout
{
    p_vout = _p_vout;
}

- (BOOL)canBecomeKeyWindow
{
    return( YES );
}

- (void)becomeKeyWindow
{
    [super becomeKeyWindow];
    p_vout->p_sys->b_mouse_moved = 0;
    p_vout->p_sys->i_time_mouse_last_moved = mdate();
}

- (void)resignKeyWindow
{
    [super resignKeyWindow];
    p_vout->p_sys->b_mouse_moved = 1;
    p_vout->p_sys->i_time_mouse_last_moved = 0;
}

- (void)keyDown:(NSEvent *)theEvent
{
    unichar key = 0;

    if( [[theEvent characters] length] )
    {
        key = [[theEvent characters] characterAtIndex: 0]; 
    }

    switch( key )
    {
        case 'f': case 'F':
            p_vout->i_changes |= VOUT_FULLSCREEN_CHANGE;
            break;

        case 'q': case 'Q':
            p_main->p_intf->b_die = 1;
            break;

        default:
            [super keyDown: theEvent];
            break;
    }
}

- (void)mouseMoved:(NSEvent *)theEvent
{
    p_vout->p_sys->i_time_mouse_last_moved = mdate(); 
}

@end
