/*****************************************************************************
 * vout_window.c: MacOS X plugin for vlc
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: vout_window.m,v 1.1 2002/05/12 20:56:34 massiot Exp $
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
#import <Cocoa/Cocoa.h>

#import "vout_window.h"

/*****************************************************************************
 * VLCWindow implementation 
 *****************************************************************************/
@implementation VLCWindow

- (void)setWrapper:(Vout_VLCWrapper *)_o_wrapper forVout:(void *)_p_vout
{
    p_vout = _p_vout;
    o_wrapper = _o_wrapper;
}

- (BOOL)canBecomeKeyWindow
{
    return( YES );
}

- (void)becomeKeyWindow
{
    [super becomeKeyWindow];
#if 0
    [o_wrapper 
        mouseEvent: (MOUSE_NOT_MOVED | MOUSE_LAST_MOVED)
        forVout: p_vout];
#endif
}

- (void)resignKeyWindow
{
    [super resignKeyWindow];

    [o_wrapper
        mouseEvent: (MOUSE_MOVED | MOUSE_NOT_LAST_MOVED)
        forVout: p_vout];
}

- (void)keyDown:(NSEvent *)o_event
{
    if( [o_wrapper keyDown: o_event forVout: p_vout] == NO )
    {
        [super keyDown: o_event];
    }
}

- (void)mouseMoved:(NSEvent *)o_event
{
    [o_wrapper
        mouseEvent: MOUSE_LAST_MOVED
        forVout: p_vout];
}

- (void)mouseDown:(NSEvent *)o_event
{
    [o_wrapper
        mouseEvent: MOUSE_DOWN
        forVout: p_vout];
}

@end
