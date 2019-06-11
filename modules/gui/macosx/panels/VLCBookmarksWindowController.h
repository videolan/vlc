/*****************************************************************************
 * VLCBookmarksWindowController.h: MacOS X Bookmarks window
 *****************************************************************************
 * Copyright (C) 2005, 2007, 2015 VLC authors and VideoLAN
 *
 * Authors: Felix Kühne <fkuehne at videolan dot org>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#import <Cocoa/Cocoa.h>
#import "main/VLCMain.h"
#import <vlc_common.h>

@interface VLCBookmarksWindowController : NSWindowController

/* main window */
@property (readwrite, weak) IBOutlet NSButton *addButton;
@property (readwrite, weak) IBOutlet NSButton *clearButton;
@property (readwrite, weak) IBOutlet NSButton *editButton;
@property (readwrite, weak) IBOutlet NSButton *removeButton;
@property (readwrite, weak) IBOutlet NSTableView *dataTable;

/* edit window */
@property (readwrite) IBOutlet NSWindow *editBookmarksWindow;
@property (readwrite, weak) IBOutlet NSButton *editOKButton;
@property (readwrite, weak) IBOutlet NSButton *editCancelButton;
@property (readwrite, weak) IBOutlet NSTextField *editNameLabel;
@property (readwrite, weak) IBOutlet NSTextField *editTimeLabel;
@property (readwrite, weak) IBOutlet NSTextField *editNameTextField;
@property (readwrite, weak) IBOutlet NSTextField *editTimeTextField;

- (IBAction)toggleWindow:(id)sender;

- (IBAction)add:(id)sender;
- (IBAction)clear:(id)sender;
- (IBAction)edit:(id)sender;
- (IBAction)remove:(id)sender;
- (IBAction)goToBookmark:(id)sender;

- (IBAction)edit_cancel:(id)sender;
- (IBAction)edit_ok:(id)sender;

@end
