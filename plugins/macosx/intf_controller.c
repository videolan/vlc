/*****************************************************************************
 * intf_controller.c: MacOS X plugin for vlc
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: intf_controller.c,v 1.3 2002/02/18 01:34:44 jlj Exp $
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

@interface Intf_Controller (Internal)
- (void)handlePortMessage:(NSPortMessage *)o_msg;
@end

@implementation Intf_Controller

/* Initialization & Event-Management */

- (void)awakeFromNib
{
    NSString *pTitle = [NSString
        stringWithCString: VOUT_TITLE " (Cocoa)"];

    o_vlc = [Intf_VlcWrapper instance];
    [o_vlc initWithDelegate: self];

    [o_window setTitle: pTitle];
}

- (void)applicationDidFinishLaunching:(NSNotification *)o_notification
{
    [[o_vlc sendPort] setDelegate: self];

    [[NSRunLoop currentRunLoop] 
        addPort: [o_vlc sendPort] 
        forMode: NSDefaultRunLoopMode];
    
    [NSThread detachNewThreadSelector: @selector(manage)
        toTarget: self withObject: nil];
}

- (void)manage
{
    NSDate *sleepDate;
    NSAutoreleasePool *o_pool;

    o_pool = [[NSAutoreleasePool alloc] init];

    while( ![o_vlc manage] )
    {
        [o_currenttime setStringValue: [o_vlc getTimeAsString]];
        [o_timeslider setFloatValue: [o_vlc getTimeAsFloat]];

        if( [o_vlc playlistPlaying] )
        {
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
            [o_vlc playlistAdd: o_file];
        }
        
        [o_vlc playlistPlayCurrent];
    }
}
    
- (IBAction)pause:(id)sender
{
    [o_vlc playlistPause];
}

- (IBAction)play:(id)sender
{
    [o_vlc playlistPlayCurrent];
}

- (IBAction)stop:(id)sender
{
    [o_vlc playlistStop];
}

- (IBAction)timeslider_update:(id)slider
{
    [o_vlc setTimeAsFloat: [o_timeslider floatValue]];
}

- (IBAction)speedslider_update:(id)slider
{
    [o_vlc setSpeed: (intf_speed_t)[slider intValue]];
}
  
- (IBAction)fullscreen_toggle:(id)sender
{

}

- (IBAction)quit:(id)sender
{
    [o_vlc quit];
}

@end

@implementation Intf_Controller (Internal)

- (void)handlePortMessage:(NSPortMessage *)o_msg
{
    [o_vlc handlePortMessage: o_msg];
}

@end

@implementation Intf_PlaylistDS

- (void)awakeFromNib
{
    o_vlc = [Intf_VlcWrapper instance];
    o_playlist = nil;
}
    
- (void)readPlaylist
{
    o_playlist = [[o_vlc playlistAsArray] retain];
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

