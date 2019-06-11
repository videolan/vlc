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

#import "extensions/NSString+Helpers.h"
#import "main/CompatibilityFixes.h"
#import "windows/video/VLCVideoOutputProvider.h"

@interface VLCBookmarksWindowController() <NSTableViewDataSource, NSTableViewDelegate>
{
    //input_thread_t *p_old_input;
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
        [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(updateCocoaWindowLevel:) name:VLCWindowShouldUpdateLevel object:nil];
    }
    return self;
}

- (void)dealloc
{
    //if (p_old_input)
    //    input_Release(p_old_input);

    [[NSNotificationCenter defaultCenter] removeObserver:self];
}

- (void)windowDidLoad
{
    [self.window setCollectionBehavior: NSWindowCollectionBehaviorFullScreenAuxiliary];

    _dataTable.dataSource = self;
    _dataTable.delegate = self;
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
        [self.window setLevel: [[[VLCMain sharedInstance] voutProvider] currentStatusWindowLevel]];
        [self.window makeKeyAndOrderFront:sender];
    }
}

-(void)inputChangedEvent:(NSNotification *)o_notification
{
    [_dataTable reloadData];
}

- (IBAction)add:(id)sender
{
#if 0
    /* add item to list */
    input_thread_t * p_input = pl_CurrentInput(getIntf());

    if (!p_input)
        return;

    seekpoint_t bookmark;

    if (!input_Control(p_input, INPUT_GET_BOOKMARK, &bookmark)) {
        bookmark.psz_name = (char *)_("Untitled");
        input_Control(p_input, INPUT_ADD_BOOKMARK, &bookmark);
    }

    input_Release(p_input);

    [_dataTable reloadData];
#endif
}

- (IBAction)clear:(id)sender
{
#if 0
    /* clear table */
    input_thread_t * p_input = pl_CurrentInput(getIntf());

    if (!p_input)
        return;

    input_Control(p_input, INPUT_CLEAR_BOOKMARKS);

    input_Release(p_input);

    [_dataTable reloadData];
#endif
}

- (IBAction)edit:(id)sender
{
#if 0
    /* put values to the sheet's fields and show sheet */
    /* we take the values from the core and not the table, because we cannot
     * really trust it */
    input_thread_t * p_input = pl_CurrentInput(getIntf());
    seekpoint_t **pp_bookmarks;
    int i_bookmarks;
    int row = (int)[_dataTable selectedRow];

    if (!p_input)
        return;

    if (row < 0) {
        input_Release(p_input);
        return;
    }

    if (input_Control(p_input, INPUT_GET_BOOKMARKS, &pp_bookmarks, &i_bookmarks) != VLC_SUCCESS) {
        input_Release(p_input);
        return;
    }

    [_editNameTextField setStringValue: toNSStr(pp_bookmarks[row]->psz_name)];
    [_editTimeTextField setStringValue:[self timeStringForBookmark:pp_bookmarks[row]]];

    /* Just keep the pointer value to check if it
     * changes. Note, we don't need to keep a reference to the object.
     * so release it now. */
    p_old_input = p_input;
    input_Release(p_input);

    [self.window beginSheet:_editBookmarksWindow completionHandler:nil];

    // Clear the bookmark list
    for (int i = 0; i < i_bookmarks; i++)
        vlc_seekpoint_Delete(pp_bookmarks[i]);
    free(pp_bookmarks);
#endif
}

- (IBAction)edit_cancel:(id)sender
{
    /* close sheet */
    [NSApp endSheet:_editBookmarksWindow];
    [_editBookmarksWindow close];
}

- (IBAction)edit_ok:(id)sender
{
#if 0
    /* save field contents and close sheet */
     seekpoint_t **pp_bookmarks;
    int i_bookmarks;
    NSInteger i;
    input_thread_t * p_input = pl_CurrentInput(getIntf());

    if (!p_input) {
        NSAlert *alert = [[NSAlert alloc] init];
        [alert setAlertStyle:NSCriticalAlertStyle];
        [alert setMessageText:_NS("No input")];
        [alert setInformativeText:_NS("No input found. A stream must be playing or paused for bookmarks to work.")];
        [alert beginSheetModalForWindow:self.window
                      completionHandler:nil];
        return;
    }
    if (p_old_input != p_input) {
        NSAlert *alert = [[NSAlert alloc] init];
        [alert setAlertStyle:NSCriticalAlertStyle];
        [alert setMessageText:_NS("Input has changed")];
        [alert setInformativeText:_NS("Input has changed, unable to save bookmark. Suspending playback with \"Pause\" while editing bookmarks to ensure to keep the same input.")];
        [alert beginSheetModalForWindow:self.window
                      completionHandler:nil];
        input_Release(p_input);
        return;
    }

    if (input_Control(p_input, INPUT_GET_BOOKMARKS, &pp_bookmarks, &i_bookmarks) != VLC_SUCCESS) {
        input_Release(p_input);
        return;
    }

    i = [_dataTable selectedRow];

    free(pp_bookmarks[i]->psz_name);

    pp_bookmarks[i]->psz_name = strdup([[_editNameTextField stringValue] UTF8String]);

    NSArray * components = [[_editTimeTextField stringValue] componentsSeparatedByString:@":"];
    NSUInteger componentCount = [components count];
    if (componentCount == 1)
        pp_bookmarks[i]->i_time_offset = vlc_tick_from_sec([[components firstObject] floatValue]);
    else if (componentCount == 2)
        pp_bookmarks[i]->i_time_offset = vlc_tick_from_sec([[components firstObject] longLongValue] * 60 + [[components objectAtIndex:1] longLongValue]);
    else if (componentCount == 3)
        pp_bookmarks[i]->i_time_offset = vlc_tick_from_sec([[components firstObject] longLongValue] * 3600 + [[components objectAtIndex:1] longLongValue] * 60 + [[components objectAtIndex:2] floatValue]);
    else {
        msg_Err(getIntf(), "Invalid string format for time");
        goto clear;
    }

    if (input_Control(p_input, INPUT_CHANGE_BOOKMARK, pp_bookmarks[i], i) != VLC_SUCCESS) {
        msg_Warn(getIntf(), "Unable to change the bookmark");
        goto clear;
    }

    [_dataTable reloadData];
    input_Release(p_input);

    [NSApp endSheet: _editBookmarksWindow];
    [_editBookmarksWindow close];

clear:
    // Clear the bookmark list
    for (int i = 0; i < i_bookmarks; i++)
        vlc_seekpoint_Delete(pp_bookmarks[i]);
    free(pp_bookmarks);
#endif
}


- (IBAction)goToBookmark:(id)sender
{
#if 0
    input_thread_t * p_input = pl_CurrentInput(getIntf());

    if (!p_input)
        return;

    input_Control(p_input, INPUT_SET_BOOKMARK, [_dataTable selectedRow]);

    input_Release(p_input);
#endif
}

- (IBAction)remove:(id)sender
{
#if 0
    input_thread_t * p_input = pl_CurrentInput(getIntf());

    if (!p_input)
        return;

    int i_focused = (int)[_dataTable selectedRow];

    if (i_focused >= 0)
        input_Control(p_input, INPUT_DEL_BOOKMARK, i_focused);

    input_Release(p_input);

    [_dataTable reloadData];
#endif
}

- (NSString *)timeStringForBookmark:(seekpoint_t *)bookmark
{
    assert(bookmark != NULL);

    vlc_tick_t total = bookmark->i_time_offset;
    uint64_t hour = ( total / VLC_TICK_FROM_SEC(3600) );
    uint64_t min = ( total % VLC_TICK_FROM_SEC(3600) ) / VLC_TICK_FROM_SEC(60);
    float    sec = secf_from_vlc_tick( total % VLC_TICK_FROM_SEC(60) );

    return [NSString stringWithFormat:@"%02llu:%02llu:%06.3f", hour, min, sec];
}

/*****************************************************************************
 * data source methods
 *****************************************************************************/

- (NSInteger)numberOfRowsInTableView:(NSTableView *)theDataTable
{
#if 0
    /* return the number of bookmarks */
    input_thread_t * p_input = pl_CurrentInput(getIntf());
    seekpoint_t **pp_bookmarks;
    int i_bookmarks;

    if (!p_input)
        return 0;

    int returnValue = input_Control(p_input, INPUT_GET_BOOKMARKS, &pp_bookmarks, &i_bookmarks);
    input_Release(p_input);

    if (returnValue != VLC_SUCCESS)
        return 0;

    for (int i = 0; i < i_bookmarks; i++)
        vlc_seekpoint_Delete(pp_bookmarks[i]);
    free(pp_bookmarks);

    return i_bookmarks;
#endif
    return 0;
}

- (id)tableView:(NSTableView *)theDataTable objectValueForTableColumn: (NSTableColumn *)theTableColumn row: (NSInteger)row
{
#if 0
    /* return the corresponding data as NSString */
    input_thread_t * p_input = pl_CurrentInput(getIntf());
    seekpoint_t **pp_bookmarks;
    int i_bookmarks;
    id ret = @"";

    if (!p_input)
        return @"";
    else if (input_Control(p_input, INPUT_GET_BOOKMARKS, &pp_bookmarks, &i_bookmarks) != VLC_SUCCESS)
        ret = @"";
    else if (row >= i_bookmarks)
        ret = @"";
    else {
        NSString * identifier = [theTableColumn identifier];
        if ([identifier isEqualToString: @"description"])
            ret = toNSStr(pp_bookmarks[row]->psz_name);
		else if ([identifier isEqualToString: @"time_offset"]) {
            ret = [self timeStringForBookmark:pp_bookmarks[row]];
        }

        // Clear the bookmark list
        for (int i = 0; i < i_bookmarks; i++)
            vlc_seekpoint_Delete(pp_bookmarks[i]);
        free(pp_bookmarks);
    }
    input_Release(p_input);
    return ret;
#endif
    return @"";
}

/*****************************************************************************
 * delegate methods
 *****************************************************************************/

- (void)tableViewSelectionDidChange:(NSNotification *)aNotification
{
    /* check whether a row is selected and en-/disable the edit/remove buttons */
    if ([_dataTable selectedRow] == -1) {
        /* no row is selected */
        [_editButton setEnabled: NO];
        [_removeButton setEnabled: NO];
    } else {
        /* a row is selected */
        [_editButton setEnabled: YES];
        [_removeButton setEnabled: YES];
    }
}

/* Called when the user hits CMD + C or copy is clicked in the edit menu
 */
- (void) copy:(id)sender {
#if 0
    NSPasteboard *pasteBoard = [NSPasteboard generalPasteboard];
    NSIndexSet *selectionIndices = [_dataTable selectedRowIndexes];


    input_thread_t *p_input = pl_CurrentInput(getIntf());
    int i_bookmarks;
    seekpoint_t **pp_bookmarks;

    if (input_Control(p_input, INPUT_GET_BOOKMARKS, &pp_bookmarks, &i_bookmarks) != VLC_SUCCESS)
        return;

    [pasteBoard clearContents];
    NSUInteger index = [selectionIndices firstIndex];

    while(index != NSNotFound) {
        /* Get values */
        if (index >= i_bookmarks)
            break;
        NSString *name = toNSStr(pp_bookmarks[index]->psz_name);
        NSString *time = [self timeStringForBookmark:pp_bookmarks[index]];

        NSString *message = [NSString stringWithFormat:@"%@ - %@", name, time];
        [pasteBoard writeObjects:@[message]];

        /* Get next index */
        index = [selectionIndices indexGreaterThanIndex:index];
    }

    // Clear the bookmark list
    for (int i = 0; i < i_bookmarks; i++)
        vlc_seekpoint_Delete(pp_bookmarks[i]);
    free(pp_bookmarks);
#endif
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

@end
