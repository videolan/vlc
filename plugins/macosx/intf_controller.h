/*****************************************************************************
 * intf_controller.h : MacOS X plugin for vlc
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

#import <Cocoa/Cocoa.h>
#import "intf_vlc_wrapper.h"
#import "intf_qdview.h"

@interface Intf_PlaylistDS : NSObject {
    Intf_VlcWrapper* o_vlc ;
    NSMutableArray* o_playlist ;

    IBOutlet NSTableView *o_table ;
}

- (void ) awakeFromNib ;
- (void) readPlaylist ;

- (int) numberOfRowsInTableView:(NSTableView*)o_table ;
- (id) tableView:(NSTableView*)o_table objectValueForTableColumn:(NSTableColumn*)o_column row:(int)i_row ;
- (void)tableView:(NSTableView *)aTableView setObjectValue:anObject forTableColumn:(NSTableColumn *)aTableColumn row:(int)rowIndex ;
@end

@interface Intf_Controller : NSObject <VlcWrapper_Delegate> {
    Intf_VlcWrapper* o_vlc ;

    NSWindow *o_window ;
    bool b_window_is_fullscreen ;
    
    IBOutlet NSButton *o_play ;
    IBOutlet NSButton *o_pause ;
    IBOutlet NSButton *o_stop ;
    IBOutlet NSButton *o_stepr ;
    IBOutlet NSButton *o_stepf ;
    IBOutlet NSSlider *o_timeslider ;
    IBOutlet NSTextField *o_currenttime ;
    IBOutlet NSMenuItem *o_menu_fullscreen ;
    
    IBOutlet Intf_PlaylistDS *o_playlistds ;
}

//Initialization & Event-Management
- (void) awakeFromNib ;
- (void) manage:(NSTimer *)timer ;
- (void)applicationDidBecomeActive:(NSNotification*)aNotification ;
- (void)applicationDidResignActive:(NSNotification*)aNotification ;

//Functions atteched to user interface 
- (IBAction) pause:(id)sender ;
- (IBAction) play:(id)sender ;
- (IBAction) timeslider_update:(id)slider ;
- (IBAction) speedslider_update:(id)slider ;
- (IBAction) fullscreen_toggle:(id)sender ;
@end
