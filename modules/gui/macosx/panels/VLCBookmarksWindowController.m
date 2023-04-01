/*****************************************************************************
 * VLCBookmarksWindowController.m: MacOS X Bookmarks window
 *****************************************************************************
 * Copyright (C) 2005 - 2015 VLC authors and VideoLAN
 *
 * Authors: Felix Paul Kühne <fkuehne at videolan dot org>
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


/*****************************************************************************
 * Note:
 * the code used to bind with VLC's modules is heavily based upon
 * ../wxwidgets/bookmarks.cpp, written by Gildas Bazin.
 * (he is a member of the VideoLAN team)
 *****************************************************************************/


/*****************************************************************************
 * Preamble
 *****************************************************************************/

#import "VLCBookmarksWindowController.h"

#import "bookmarks/VLCBookmark.h"
#import "bookmarks/VLCBookmarksTableViewDataSource.h"
#import "bookmarks/VLCBookmarksTableViewDelegate.h"

#import "extensions/NSString+Helpers.h"

#import "library/VLCInputItem.h"

#import "main/CompatibilityFixes.h"

#import "playlist/VLCPlaylistController.h"
#import "playlist/VLCPlayerController.h"

#import "windows/video/VLCVideoOutputProvider.h"

@interface VLCBookmarksWindowController() <NSTableViewDataSource, NSTableViewDelegate>
{
    VLCBookmarksTableViewDataSource *_tableViewDataSource;
    VLCBookmarksTableViewDelegate *_tableViewDelegate;
    VLCInputItem *_oldInputItem;
}
@end

@implementation VLCBookmarksWindowController

/*****************************************************************************
 * GUI methods
 *****************************************************************************/

- (id)init
{
    self = [super initWithWindowNibName:@"Bookmarks"];
    if (self) {
        [NSNotificationCenter.defaultCenter addObserver:self
                                               selector:@selector(updateCocoaWindowLevel:)
                                                   name:VLCWindowShouldUpdateLevel
                                                 object:nil];
    }
    return self;
}

- (void)dealloc
{
    [NSNotificationCenter.defaultCenter removeObserver:self];
}

- (void)windowDidLoad
{
    [self.window setCollectionBehavior: NSWindowCollectionBehaviorFullScreenAuxiliary];

    _tableViewDataSource = [[VLCBookmarksTableViewDataSource alloc] init];
    _tableViewDelegate = [[VLCBookmarksTableViewDelegate alloc] initWithBookmarksWindowController:self];

    _dataTable.dataSource = _tableViewDataSource;
    _dataTable.delegate = _tableViewDelegate;
    _dataTable.action = @selector(goToBookmark:);
    _dataTable.target = self;

    /* main window */
    [self.window setTitle: _NS("Bookmarks")];
    [_addButton setTitle: _NS("Add")];
    [_clearButton setTitle: _NS("Clear")];
    [_editButton setTitle: _NS("Edit")];
    [_removeButton setTitle: _NS("Remove")];
    [[[_dataTable tableColumnWithIdentifier:@"description"] headerCell]
     setStringValue: _NS("Description")];
    [[[_dataTable tableColumnWithIdentifier:@"time_offset"] headerCell]
     setStringValue: _NS("Time")];

    /* edit window */
    [_editOKButton setTitle: _NS("OK")];
    [_editCancelButton setTitle: _NS("Cancel")];
    [_editNameLabel setStringValue: _NS("Name")];
    [_editTimeLabel setStringValue: _NS("Time")];
}

- (void)updateCocoaWindowLevel:(NSNotification *)aNotification
{
    NSInteger i_level = [aNotification.userInfo[VLCWindowLevelKey] integerValue];
    if (self.isWindowLoaded && [self.window isVisible] && [self.window level] != i_level)
        [self.window setLevel: i_level];
}

- (IBAction)toggleWindow:(id)sender
{
    if ([self.window isVisible])
        [self.window orderOut:sender];
    else {
        [self.window setLevel: VLCMain.sharedInstance.voutProvider.currentStatusWindowLevel];
        [self.window makeKeyAndOrderFront:sender];
    }
}

-(void)inputChangedEvent:(NSNotification *)o_notification
{
    [_dataTable reloadData];
}

- (IBAction)add:(id)sender
{
    [_tableViewDataSource addBookmark];
    [_dataTable reloadData];
}

- (IBAction)clear:(id)sender
{
    [_tableViewDataSource clearBookmarks];
    [_dataTable reloadData];
}

- (IBAction)edit:(id)sender
{
    VLCInputItem * const currentlyPlayingInputItem = VLCMain.sharedInstance.playlistController.currentlyPlayingInputItem;
    if (currentlyPlayingInputItem == nil) {
        return;
    }

    const NSInteger selectedRow = [_dataTable selectedRow];
    if (selectedRow < 0) {
        return;
    }

    VLCBookmark * const bookmark = [_tableViewDataSource bookmarkForRow:selectedRow];

    [_editNameTextField setStringValue:bookmark.bookmarkName];
    [_editTimeTextField setStringValue:[NSString stringWithTime:bookmark.bookmarkTime / 1000]];

    [self.window beginSheet:_editBookmarksWindow completionHandler:nil];

    _oldInputItem = currentlyPlayingInputItem;
}

- (IBAction)edit_cancel:(id)sender
{
    /* close sheet */
    [NSApp endSheet:_editBookmarksWindow];
    [_editBookmarksWindow close];
}

- (IBAction)edit_ok:(id)sender
{
    VLCInputItem * const currentlyPlayingInputItem = VLCMain.sharedInstance.playlistController.currentlyPlayingInputItem;
    if (currentlyPlayingInputItem == nil) {
        NSAlert *alert = [[NSAlert alloc] init];
        [alert setAlertStyle:NSCriticalAlertStyle];
        [alert setMessageText:_NS("No input")];
        [alert setInformativeText:_NS("No input found. A stream must be playing or paused for bookmarks to work.")];
        [alert beginSheetModalForWindow:self.window
                      completionHandler:nil];
        return;
    }

    if (_oldInputItem.vlcInputItem != currentlyPlayingInputItem.vlcInputItem) {
        NSAlert *alert = [[NSAlert alloc] init];
        [alert setAlertStyle:NSCriticalAlertStyle];
        [alert setMessageText:_NS("Input has changed")];
        [alert setInformativeText:_NS("Input has changed, unable to save bookmark. Suspending playback with \"Pause\" while editing bookmarks to ensure to keep the same input.")];
        [alert beginSheetModalForWindow:self.window
                      completionHandler:nil];
        return;
    }

    const NSInteger selectedRow = [_dataTable selectedRow];
    VLCBookmark * const bookmark = [_tableViewDataSource bookmarkForRow:selectedRow];
    VLCBookmark * const originalBookmark = [bookmark copy];

    NSArray * const components = [[_editTimeTextField stringValue] componentsSeparatedByString:@":"];
    const NSUInteger componentCount = [components count];
    NSLog(@"%@", components);

    if (componentCount == 1) {
        bookmark.bookmarkTime = ([[components firstObject] longLongValue]) * 1000;
    } else if (componentCount == 2) {
        bookmark.bookmarkTime = ([[components firstObject] longLongValue] * 60 +
                                 [[components objectAtIndex:1] longLongValue]) * 1000;
    } else if (componentCount == 3) {
        bookmark.bookmarkTime = ([[components firstObject] longLongValue] * 3600 +
                                 [[components objectAtIndex:1] longLongValue] * 60 +
                                 [[components objectAtIndex:2] longLongValue]) * 1000;
    } else {
        msg_Err(getIntf(), "Invalid string format for time");
    }

    bookmark.bookmarkName = _editNameTextField.stringValue;

    [_tableViewDataSource editBookmark:bookmark originalBookmark:originalBookmark];
    [_dataTable reloadData];

    [NSApp endSheet: _editBookmarksWindow];
    [_editBookmarksWindow close];
}


- (IBAction)goToBookmark:(id)sender
{
    const NSInteger selectedRow = [_dataTable selectedRow];
    VLCBookmark * const bookmark = [_tableViewDataSource bookmarkForRow:selectedRow];
    vlc_tick_t bookmarkTime = VLC_TICK_FROM_MS(bookmark.bookmarkTime);

    VLCPlayerController * const playerController = VLCMain.sharedInstance.playlistController.playerController;
    [playerController setTimeFast:bookmarkTime];
}

- (IBAction)remove:(id)sender
{
    const NSInteger selectedRow = [_dataTable selectedRow];
    VLCBookmark * const bookmark = [_tableViewDataSource bookmarkForRow:selectedRow];
    [_tableViewDataSource removeBookmark:bookmark];
    [_dataTable reloadData];
}

/* Called when the user hits CMD + C or copy is clicked in the edit menu
 */
- (void)copy:(id)sender
{
    NSArray<VLCBookmark *> * const bookmarks = _tableViewDataSource.bookmarks;
    if (bookmarks == nil || bookmarks.count == 0) {
        return;
    }

    NSPasteboard * const pasteBoard = [NSPasteboard generalPasteboard];
    NSIndexSet * const selectionIndices = [_dataTable selectedRowIndexes];
    NSUInteger index = [selectionIndices firstIndex];

    while (index != NSNotFound) {
        /* Get values */
        if (index >= bookmarks.count) {
            break;
        }

        VLCBookmark * const bookmark = bookmarks[index];
        NSString * const name = bookmark.bookmarkName;
        NSString * const time = [NSString stringWithTime:bookmark.bookmarkTime / 1000];
        NSString * const message = [NSString stringWithFormat:@"%@ - %@", name, time];
        [pasteBoard writeObjects:@[message]];

        /* Get next index */
        index = [selectionIndices indexGreaterThanIndex:index];
    }
}

#pragma mark -
#pragma mark UI validation

/* Validate the copy menu item
 */
- (BOOL)validateUserInterfaceItem:(id <NSValidatedUserInterfaceItem>)anItem
{
    SEL theAction = [anItem action];

    if (theAction == @selector(copy:)) {
        if ([[_dataTable selectedRowIndexes] count] > 0) {
            return YES;
        }
        return NO;
    }
    /* Indicate that we handle the validation method,
     * even if we don’t implement the action
     */
    return YES;
}

- (void)toggleRowDependentButtonsEnabled:(BOOL)enabled
{
    _editButton.enabled = enabled;
    _removeButton.enabled = enabled;
}

@end
