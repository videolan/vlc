/*****************************************************************************
 * intf_controller.c : MacOS X plugin for vlc
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $$
 *
 * Authors: Florian G. Pflug <fgp@phlo.org>
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

/* Remark:
    I need to subclass NSQuickDrawView, and post a notification when its display
    method is called. This is necessary because GetPortBound and similar functions
    return the actual on-screen size of the QDPort, which isn't updated immidiately
    after calling e.g. setFrame
*/

#include <QuickTime/QuickTime.h>
#include <ApplicationServices/ApplicationServices.h>
#include <CoreServices/CoreServices.h>
#import "intf_controller.h"
#import "intf_vlc_wrapper.h"

@implementation Intf_Controller
//Initialization & Event-Management
    - (void) awakeFromNib {
        o_vlc = [Intf_VlcWrapper instance] ;
        b_window_is_fullscreen = FALSE ;
        [NSTimer scheduledTimerWithTimeInterval: 0.5
            target: self
            selector: @selector(manage:)
            userInfo: nil
            repeats:TRUE
        ] ;
        [o_vlc initWithDelegate:self] ;
    }

    - (void) manage:(NSTimer *)timer {
        if ([o_vlc manage])
            [NSApp terminate: self] ;
        
        [o_currenttime setStringValue: [o_vlc getTimeAsString]] ;
        [o_timeslider setFloatValue: [o_vlc getTimeAsFloat]] ;
        if ([o_vlc playlistPlaying])
            UpdateSystemActivity(UsrActivity) ;
   }
    
    - (void)applicationDidBecomeActive:(NSNotification*)aNotification {
        if (b_window_is_fullscreen) {
            [o_window orderFront:self] ;
            [o_vlc playlistPlayCurrent] ;
        }
    }
    
    - (void)applicationDidResignActive:(NSNotification*)aNotification {
        if (b_window_is_fullscreen) {
            [o_vlc playlistPause] ;
            [o_window orderOut:self] ;
        }
    }
        
        
        
        
//Functions attached to user interface     
    - (IBAction) openFile:(id)sender {
        NSOpenPanel *o_panel = [NSOpenPanel openPanel] ;
        
        [o_panel setAllowsMultipleSelection:YES] ;
        if ([o_panel runModalForDirectory:NSHomeDirectory() file:nil types:nil] == NSOKButton) {
            NSEnumerator* o_files = [[o_panel filenames] objectEnumerator] ;
            NSString* o_file ;
            
            while ((o_file = (NSString*)[o_files nextObject])) {
                [o_vlc playlistAdd:o_file] ;
            }
        }
        [o_vlc playlistPlayCurrent] ;
    }
    
    - (IBAction) pause:(id)sender {
        [o_vlc playlistPause] ;
    }
    
    - (IBAction) play:(id)sender {
        [o_vlc playlistPlayCurrent] ;
    }
    
    - (IBAction) stop:(id)sender {
        [o_vlc playlistStop] ;
    }
    
    - (IBAction) prevPlaylist:(id)sender {
        [o_vlc playlistPlayPrev] ;
    }
    
    - (IBAction) nextPlaylist:(id)sender {
        [o_vlc playlistPlayNext] ;
    }
    
    - (IBAction) timeslider_update:(id)slider {
        [o_vlc setTimeAsFloat: [o_timeslider floatValue]] ;
    }
    
    - (IBAction) speedslider_update:(id)slider {
        [o_vlc setSpeed: (intf_speed_t) [slider intValue]] ;
    }
  
    - (IBAction) fullscreen_toggle:(id)sender {
        [self requestQDPortFullscreen:!b_window_is_fullscreen] ;
    }



                               
//Callbacks - we are the delegate for the VlcWrapper
    - (void) requestQDPortFullscreen:(bool)b_fullscreen {
        NSRect s_rect ;
        VlcQuickDrawView *o_qdview ;
        
        s_rect.origin.x = s_rect.origin.y = 0 ;
        
        [self releaseQDPort] ;
        o_window = [NSWindow alloc] ;
        if (b_fullscreen) {
            [o_window
                initWithContentRect: [[NSScreen mainScreen] frame]
                styleMask: NSBorderlessWindowMask
                backing: NSBackingStoreBuffered
                defer:NO screen:[NSScreen mainScreen]
            ] ;
            [o_window setLevel:CGShieldingWindowLevel()] ;
            CGDisplayHideCursor(kCGDirectMainDisplay) ;
            [o_menu_fullscreen setState:NSOffState] ;
            b_window_is_fullscreen = TRUE ;
        }
        else {
            s_rect.size = [o_vlc videoSize] ;
            [o_window
                initWithContentRect: s_rect
                styleMask: (NSTitledWindowMask | NSMiniaturizableWindowMask | NSResizableWindowMask)
                backing: NSBackingStoreBuffered
                defer:NO screen:[NSScreen mainScreen]
            ] ;
            [o_window setAspectRatio:[o_vlc videoSize]] ;            
            [o_window center] ;
            [o_window setDelegate:self] ;
            b_window_is_fullscreen = FALSE ;
        }
        o_qdview = [[VlcQuickDrawView alloc] init] ;
        [o_qdview setPostsFrameChangedNotifications:YES] ;
        [[NSNotificationCenter defaultCenter]
            addObserver: o_vlc
            selector: @selector(sizeChangeQDPort)
            name: VlcQuickDrawViewDidResize
            object: o_qdview
        ] ;
        [o_window setContentView:o_qdview] ;
        [o_window orderFront:self] ;
        [o_vlc setQDPort:[o_qdview qdPort]] ;
    }
    
    - (void) releaseQDPort {
        [[NSNotificationCenter defaultCenter]
            removeObserver: nil
            name: nil
            object: [o_window contentView]
        ] ;
        [o_vlc setQDPort:nil] ;
        if (o_window) {
            [o_window close] ;
            o_window = nil ;
        }
        if (b_window_is_fullscreen)
        {
            [o_menu_fullscreen setState:NSOnState] ;
            CGDisplayShowCursor(kCGDirectMainDisplay) ;
        }
    }
    
    - (void) resizeQDPortFullscreen:(bool)b_fullscreen {
        if (b_window_is_fullscreen != b_fullscreen) {
            [self requestQDPortFullscreen:b_fullscreen] ;
        }
        else if (!b_window_is_fullscreen && !b_fullscreen) {
            [o_window setAspectRatio:[o_vlc videoSize]] ;
        }
    }
@end

@implementation Intf_PlaylistDS
    - (void ) awakeFromNib {
        o_vlc = [Intf_VlcWrapper instance] ;
        o_playlist = nil ;
    }
    
    - (void) readPlaylist {
        o_playlist = [[o_vlc playlistAsArray] retain] ;
    }

    - (int) numberOfRowsInTableView:(NSTableView*)o_table {
        [self readPlaylist] ;
        return [o_playlist count] ;
    }
    
    - (id) tableView:(NSTableView*)o_table objectValueForTableColumn:(NSTableColumn*)o_column row:(int)i_row {
        return [o_playlist objectAtIndex:i_row] ;
    }
    
    - (void)tableView:(NSTableView *)aTableView setObjectValue:anObject forTableColumn:(NSTableColumn *)aTableColumn row:(int)rowIndex {
    }
@end
