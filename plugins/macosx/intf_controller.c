/*****************************************************************************
 * intf_controller.c: MacOS X plugin for vlc
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: intf_controller.c,v 1.4 2002/03/19 03:33:52 jlj Exp $
 *
 * Authors: Florian G. Pflug <fgp@phlo.org>
 *          Jon Lech Johansen <jon-vl@nanocrew.net>
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

#import <ApplicationServices/ApplicationServices.h>

#include <videolan/vlc.h>

#include "interface.h"
#include "intf_playlist.h"

#include "macosx.h"
#include "intf_controller.h"

@implementation Intf_Controller

/* Initialization & Event-Management */

- (void)awakeFromNib
{
    NSString *pTitle = [NSString
        stringWithCString: VOUT_TITLE " (Cocoa)"];

    [o_window setTitle: pTitle];
}

- (void)applicationDidFinishLaunching:(NSNotification *)o_notification
{
    o_intf = [[Intf_VLCWrapper instance] retain];
    o_vout = [[Vout_VLCWrapper instance] retain];

    f_slider = f_slider_old = 0.0;
    o_slider_lock = [[NSLock alloc] init];

    [NSThread detachNewThreadSelector: @selector(manage)
        toTarget: self withObject: nil];
}

- (void)manage
{
    NSDate *sleepDate;
    NSAutoreleasePool *o_pool;

    o_pool = [[NSAutoreleasePool alloc] init];

    while( ![o_intf manage] )
    {
        if( [o_intf playlistPlaying] )
        { 
            [o_currenttime setStringValue: [o_intf getTimeAsString]];

            if( f_slider == f_slider_old )
            {
                float f_updated = [o_intf getTimeAsFloat];

                if( f_updated != f_slider )
                {
                    if( [o_slider_lock tryLock] )
                    {
                        [o_timeslider setFloatValue: f_updated];
                        [o_slider_lock unlock];
                    }
                }
            }
            else
            {
                [o_intf setTimeAsFloat: f_slider];
                f_slider_old = f_slider;
            }

            UpdateSystemActivity( UsrActivity );
        }

        sleepDate = [NSDate dateWithTimeIntervalSinceNow: 0.1];
        [NSThread sleepUntilDate: sleepDate];
    }

    [self terminate];

    [o_pool release];
}

- (void)terminate
{
    NSEvent *pEvent;

    [NSApp stop: nil];

    [o_vout release];
    [o_intf release];

    /* send a dummy event to break out of the event loop */
    pEvent = [NSEvent mouseEventWithType: NSLeftMouseDown
                location: NSMakePoint( 1, 1 ) modifierFlags: 0
                timestamp: 1 windowNumber: [[NSApp mainWindow] windowNumber]
                context: [NSGraphicsContext currentContext] eventNumber: 1
                clickCount: 1 pressure: 0.0];
    [NSApp postEvent: pEvent atStart: YES];
}

/* Functions attached to user interface */
 
- (IBAction)openFile:(id)sender
{
    NSOpenPanel *o_panel = [NSOpenPanel openPanel];
    
    [o_panel setAllowsMultipleSelection: YES];

    if( [o_panel runModalForDirectory: NSHomeDirectory() 
            file: nil types: nil] == NSOKButton )
    {
        NSString *o_file;
        NSEnumerator *o_files = [[o_panel filenames] objectEnumerator];

        while( ( o_file = (NSString *)[o_files nextObject] ) )
        {
            [o_intf playlistAdd: o_file];
        }
        
        [o_intf playlistPlayCurrent];
    }
}
    
- (IBAction)pause:(id)sender
{
    [o_intf playlistPause];
}

- (IBAction)play:(id)sender
{
    [o_intf playlistPlayCurrent];
}

- (IBAction)stop:(id)sender
{
    [o_intf playlistStop];
}

- (IBAction)timeslider_update:(id)slider
{
    switch( [[NSApp currentEvent] type] )
    {
        case NSLeftMouseDown:
            [o_slider_lock lock];
            break;

        case NSLeftMouseUp:
            f_slider = [o_timeslider floatValue];
            [o_slider_lock unlock];
            break;

        default:
            break;
    }
}

- (IBAction)speedslider_update:(id)slider
{
    [o_intf setSpeed: (intf_speed_t)[slider intValue]];
}
  
- (IBAction)fullscreen_toggle:(id)sender
{

}

- (IBAction)quit:(id)sender
{
    [o_intf quit];
}

@end

@implementation Intf_PlaylistDS

- (id)init
{
    if( [super init] == nil )
        return( nil );

    o_playlist = nil;

    return( self );
}

- (void)readPlaylist
{
    o_playlist = [[[Intf_VLCWrapper instance] playlistAsArray] retain];
}

- (int)numberOfRowsInTableView:(NSTableView*)o_table
{
    [self readPlaylist];
    return( [o_playlist count] );
}
    
- (id)tableView:(NSTableView *)o_table objectValueForTableColumn:(NSTableColumn*)o_column row:(int)i_row
{
    return( [o_playlist objectAtIndex: i_row] );
}
    
- (void)tableView:(NSTableView *)o_table setObjectValue:o_value forTableColumn:(NSTableColumn *)o_column row:(int)i_index
{
}

@end
