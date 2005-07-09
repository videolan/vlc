/*****************************************************************************
 * applescript.m: MacOS X AppleScript support
 *****************************************************************************
 * Copyright (C) 2002-2003 the VideoLAN team
 * $Id$
 *
 * Authors: Derk-Jan Hartman <thedj@users.sourceforge.net>
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
#include "intf.h"
#include "applescript.h"
#include "controls.h"
#include "open.h"


/*****************************************************************************
 * VLGetURLScriptCommand implementation 
 *****************************************************************************/
@implementation VLGetURLScriptCommand

- (id)performDefaultImplementation {
    NSString *o_command = [[self commandDescription] commandName];
    NSString *o_urlString = [self directParameter];

    if ( [o_command isEqualToString:@"GetURL"] || [o_command isEqualToString:@"OpenURL"] )
    {
        intf_thread_t * p_intf = VLCIntf;
        playlist_t * p_playlist = vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                                        FIND_ANYWHERE );
        if( p_playlist == NULL )
        {
            return nil;
        }

        if ( o_urlString )
        {
            NSURL * o_url;
    
            playlist_Add( p_playlist, [o_urlString fileSystemRepresentation],
                          [[[NSFileManager defaultManager] displayNameAtPath: o_urlString] UTF8String],
                          PLAYLIST_INSERT, PLAYLIST_END );

            o_url = [NSURL fileURLWithPath: o_urlString];
            if( o_url != nil )
            { 
                [[NSDocumentController sharedDocumentController]
                    noteNewRecentDocumentURL: o_url]; 
            }
        }
        vlc_object_release( p_playlist );
    }
    return nil;
}

@end


/*****************************************************************************
 * VLControlScriptCommand implementation 
 *****************************************************************************/
/*
 * This entire control command needs a better design. more object oriented.
 * Applescript developers would be very welcome (hartman)
 */
@implementation VLControlScriptCommand

- (id)performDefaultImplementation {
    NSString *o_command = [[self commandDescription] commandName];

    intf_thread_t * p_intf = VLCIntf;
    playlist_t * p_playlist = vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                                    FIND_ANYWHERE );
    if( p_playlist == NULL )
    {
        return nil;
    }
    
    VLCControls * o_controls = (VLCControls *)[[NSApp delegate] getControls];
    
    if ( o_controls )
    {
        if ( [o_command isEqualToString:@"play"] )
        {
            [o_controls play:self];
            return nil;
        }
        else if ( [o_command isEqualToString:@"stop"] )
        {
            [o_controls stop:self];
            return nil;
        }
        else if ( [o_command isEqualToString:@"previous"] )
        {
            [o_controls prev:self];
            return nil;
        }
        else if ( [o_command isEqualToString:@"next"] )
        {
            [o_controls next:self];
            return nil;
        }
        else if ( [o_command isEqualToString:@"fullscreen"] )
        {
            NSMenuItem *o_mi = [[NSMenuItem alloc] initWithTitle: _NS("Fullscreen") action: nil keyEquivalent:@""];
            [o_controls windowAction:[o_mi autorelease]];
            return nil;
        }
        else if ( [o_command isEqualToString:@"mute"] )
        {
            [o_controls mute:self];
            return nil;
        }
        else if ( [o_command isEqualToString:@"volumeUp"] )
        {
            [o_controls volumeUp:self];
            return nil;
        }
        else if ( [o_command isEqualToString:@"volumeDown"] )
        {
            [o_controls volumeDown:self];
            return nil;
        }
    }
    vlc_object_release( p_playlist );
    return nil;
}

@end
