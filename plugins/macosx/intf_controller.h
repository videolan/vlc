/*****************************************************************************
 * intf_controller.h: MacOS X plugin for vlc
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: intf_controller.h,v 1.8.2.2 2002/06/02 12:17:23 massiot Exp $
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

#include "intf_vlc_wrapper.h"
#include "vout_vlc_wrapper.h"

@interface Intf_PlaylistDS : NSObject
{
    NSMutableArray       *o_playlist;

    IBOutlet NSTableView *o_table;
}

- (void)readPlaylist;

- (int)numberOfRowsInTableView:(NSTableView *)o_table;
- (id)tableView:(NSTableView *)o_table objectValueForTableColumn:(NSTableColumn *)o_column row:(int)i_row;
- (void)tableView:(NSTableView *)o_table setObjectValue:o_value forTableColumn:(NSTableColumn *)o_column row:(int)i_index;

@end

@interface Intf_Controller : NSObject
{
    Intf_VLCWrapper *o_intf;
    Vout_VLCWrapper *o_vout;

    NSLock *o_slider_lock;
    float f_slider, f_slider_old;

    IBOutlet NSWindow       *o_window;
    IBOutlet NSButton       *o_play;
    IBOutlet NSButton       *o_pause;
    IBOutlet NSButton       *o_stop;
    IBOutlet NSButton       *o_stepr;
    IBOutlet NSButton       *o_stepf;
    IBOutlet NSSlider       *o_timeslider;
    IBOutlet NSTextField    *o_time;

    IBOutlet Intf_PlaylistDS *o_playlistds;
}

/* Initialization & Event-Management */
- (void)awakeFromNib;
- (void)manage;
- (void)terminate;

/* NSApplication messages */
- (void)applicationWillFinishLaunching:(NSNotification *)o_notification;
- (BOOL)application:(NSApplication *)o_app openFile:(NSString *)o_filename;

/* Functions attached to user interface */
- (IBAction)pause:(id)sender;
- (IBAction)play:(id)sender;
- (IBAction)stop:(id)sender;
- (IBAction)faster:(id)sender;
- (IBAction)slower:(id)sender;
- (IBAction)prev:(id)sender;
- (IBAction)next:(id)sender;
- (IBAction)prevChannel:(id)sender;
- (IBAction)nextChannel:(id)sender;
- (IBAction)loop:(id)sender;
- (IBAction)mute:(id)sender;
- (IBAction)fullscreen:(id)fullscreen;
- (IBAction)eject:(id)sender;
- (IBAction)maxvolume:(id)sender;
- (IBAction)timesliderUpdate:(id)slider;
- (IBAction)quit:(id)sender;

- (BOOL)validateMenuItem:(id)sender;

@end
