/*****************************************************************************
 * vout_vlc_wrapper.c: MacOS X plugin for vlc
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: vout_vlc_wrapper.m,v 1.2 2002/05/18 18:48:24 massiot Exp $
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
#include "vout_vlc_wrapper.h"

/*****************************************************************************
 * Vout_VLCWrapper implementation 
 *****************************************************************************/
@implementation Vout_VLCWrapper

static Vout_VLCWrapper *o_vout = nil;

+ (Vout_VLCWrapper *)instance
{
    if( o_vout == nil )
    {
        o_vout = [[[Vout_VLCWrapper alloc] init] autorelease];

        [[Vout_VLCWrapper sendPort] setDelegate: o_vout];
        [[NSRunLoop currentRunLoop]
            addPort: [Vout_VLCWrapper sendPort]
            forMode: NSDefaultRunLoopMode];
    }

    return( o_vout );
}

- (void)dealloc
{
    o_vout = nil;
    [super dealloc];
}

+ (NSPort *)sendPort
{
    return( p_main->p_intf->p_sys->o_port );
}

- (void)mouseEvent:(unsigned int)ui_status forVout:(void *)_p_vout
{
    struct vout_thread_s *p_vout =
        (struct vout_thread_s *)_p_vout;

    if( ui_status & MOUSE_MOVED ) 
        p_vout->p_sys->b_mouse_moved = 1;
    if( ui_status & MOUSE_NOT_MOVED ) 
        p_vout->p_sys->b_mouse_moved = 0;
    if( ui_status & MOUSE_LAST_MOVED ) 
        p_vout->p_sys->i_time_mouse_last_moved = mdate();
    if( ui_status & MOUSE_NOT_LAST_MOVED )
        p_vout->p_sys->i_time_mouse_last_moved = 0;
    if( ui_status & MOUSE_DOWN )
    {
        if( p_vout->p_sys->b_mouse_pointer_visible )
        {
            CGDisplayHideCursor( kCGDirectMainDisplay );
        }
        else
        {
            CGDisplayShowCursor( kCGDirectMainDisplay );
        }
        p_vout->p_sys->b_mouse_pointer_visible = !p_vout->p_sys->b_mouse_pointer_visible;
    }
}

- (BOOL)keyDown:(NSEvent *)o_event forVout:(void *)_p_vout
{
    unichar key = 0;

    struct vout_thread_s *p_vout =
        (struct vout_thread_s *)_p_vout;

    if( [[o_event characters] length] )
    {
        key = [[o_event characters] characterAtIndex: 0];
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
            return( NO );
            break;
    }

    return( YES );
}

- (void)voutDidResize:(void *)_p_vout
{
    vout_thread_t * p_vout = (vout_thread_t *)_p_vout;

    p_vout->i_changes |= VOUT_SIZE_CHANGE;
}

@end

@implementation Vout_VLCWrapper (Internal)

- (void)handlePortMessage:(NSPortMessage *)o_msg
{
    NSData *o_req;
    struct vout_req_s *p_req;

    o_req = [[o_msg components] lastObject];
    p_req = *((struct vout_req_s **)[o_req bytes]);

    [p_req->o_lock lock];

    if( p_req->i_type == VOUT_REQ_CREATE_WINDOW )
    {
        VLCView *o_view;

        p_req->p_vout->p_sys->o_window = [VLCWindow alloc];
        [p_req->p_vout->p_sys->o_window 
            setWrapper: self forVout: (void *)p_req->p_vout];
        [p_req->p_vout->p_sys->o_window setReleasedWhenClosed: YES];

        if( p_req->p_vout->b_fullscreen )
        {
            [p_req->p_vout->p_sys->o_window 
                initWithContentRect: [[NSScreen mainScreen] frame] 
                styleMask: NSBorderlessWindowMask 
                backing: NSBackingStoreBuffered
                defer: NO screen: [NSScreen mainScreen]];

            [p_req->p_vout->p_sys->o_window 
                setLevel: CGShieldingWindowLevel()];
        }
        else
        {
            unsigned int i_stylemask = NSTitledWindowMask |
                                       NSMiniaturizableWindowMask |
                                       NSResizableWindowMask;

            [p_req->p_vout->p_sys->o_window 
                initWithContentRect: p_req->p_vout->p_sys->s_rect 
                styleMask: i_stylemask
                backing: NSBackingStoreBuffered
                defer: NO screen: [NSScreen mainScreen]];

            if( !p_req->p_vout->p_sys->b_pos_saved )
            {
                [p_req->p_vout->p_sys->o_window center];
            }
        }

        o_view = [[VLCView alloc] 
            initWithWrapper: self forVout: (void *)p_req->p_vout];
        [p_req->p_vout->p_sys->o_window setContentView: o_view];
        [o_view autorelease];

        [o_view lockFocus];
        p_req->p_vout->p_sys->p_qdport = [o_view qdPort];
        [o_view unlockFocus];

        [p_req->p_vout->p_sys->o_window setTitle: [NSString 
            stringWithCString: VOUT_TITLE]];
        [p_req->p_vout->p_sys->o_window setAcceptsMouseMovedEvents: YES];
        [p_req->p_vout->p_sys->o_window makeKeyAndOrderFront: nil];

        p_req->i_result = 1;
    }
    else if( p_req->i_type == VOUT_REQ_DESTROY_WINDOW )
    {
        if( !p_req->p_vout->b_fullscreen )
        {
            NSRect s_rect;

            s_rect = [[p_req->p_vout->p_sys->o_window contentView] frame];
            p_req->p_vout->p_sys->s_rect.size = s_rect.size;

            s_rect = [p_req->p_vout->p_sys->o_window frame];
            p_req->p_vout->p_sys->s_rect.origin = s_rect.origin;

            p_req->p_vout->p_sys->b_pos_saved = 1;
        }

        p_req->p_vout->p_sys->p_qdport = nil;
        [p_req->p_vout->p_sys->o_window close];
        p_req->p_vout->p_sys->o_window = nil;

        p_req->i_result = 1;
    }

    [p_req->o_lock unlockWithCondition: 1];
}

@end
