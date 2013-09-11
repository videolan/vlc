/*****************************************************************************
 * bookmarks.m: MacOS X Bookmarks window
 *****************************************************************************
 * Copyright (C) 2005 - 2012 VLC authors and VideoLAN
 * $Id$
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

#import "bookmarks.h"
#import "wizard.h"
#import <vlc_interface.h>
#import "CompatibilityFixes.h"

@interface VLCBookmarks (Internal)
- (void)initStrings;
@end

@implementation VLCBookmarks

static VLCBookmarks *_o_sharedInstance = nil;

+ (VLCBookmarks *)sharedInstance
{
    return _o_sharedInstance ? _o_sharedInstance : [[self alloc] init];
}

- (id)init
{
    if (_o_sharedInstance)
        [self dealloc];
    else
        _o_sharedInstance = [super init];

    return _o_sharedInstance;
}

/*****************************************************************************
 * GUI methods
 *****************************************************************************/

- (void)awakeFromNib
{
    if (!OSX_SNOW_LEOPARD)
        [o_bookmarks_window setCollectionBehavior: NSWindowCollectionBehaviorFullScreenAuxiliary];

    [self initStrings];
}

- (void)dealloc
{
    if (p_old_input)
        vlc_object_release(p_old_input);

    [super dealloc];
}

- (void)initStrings
{
    /* localise the items */

    /* main window */
    [o_bookmarks_window setTitle: _NS("Bookmarks")];
    [o_btn_add setTitle: _NS("Add")];
    [o_btn_clear setTitle: _NS("Clear")];
    [o_btn_edit setTitle: _NS("Edit")];
    [o_btn_extract setTitle: _NS("Extract")];
    [o_btn_rm setTitle: _NS("Remove")];
    [[[o_tbl_dataTable tableColumnWithIdentifier:@"description"] headerCell]
        setStringValue: _NS("Description")];
    [[[o_tbl_dataTable tableColumnWithIdentifier:@"size_offset"] headerCell]
        setStringValue: _NS("Position")];
    [[[o_tbl_dataTable tableColumnWithIdentifier:@"time_offset"] headerCell]
        setStringValue: _NS("Time")];

    /* edit window */
    [o_edit_btn_ok setTitle: _NS("OK")];
    [o_edit_btn_cancel setTitle: _NS("Cancel")];
    [o_edit_lbl_name setStringValue: _NS("Name")];
    [o_edit_lbl_time setStringValue: _NS("Time")];
    [o_edit_lbl_bytes setStringValue: _NS("Position")];
}

- (void)updateCocoaWindowLevel:(NSInteger)i_level
{
    if (o_bookmarks_window && [o_bookmarks_window isVisible] && [o_bookmarks_window level] != i_level)
        [o_bookmarks_window setLevel: i_level];
}

- (void)showBookmarks
{
    /* show the window, called from intf.m */
    [o_bookmarks_window displayIfNeeded];
    [o_bookmarks_window setLevel: [[[VLCMain sharedInstance] voutController] currentWindowLevel]];
    [o_bookmarks_window makeKeyAndOrderFront:nil];
}

- (IBAction)add:(id)sender
{
    /* add item to list */
    input_thread_t * p_input = pl_CurrentInput(VLCIntf);

    if (!p_input)
        return;

    seekpoint_t bookmark;

    if (!input_Control(p_input, INPUT_GET_BOOKMARK, &bookmark)) {
        bookmark.psz_name = _("Untitled");
        input_Control(p_input, INPUT_ADD_BOOKMARK, &bookmark);
    }

    vlc_object_release(p_input);

    [o_tbl_dataTable reloadData];
}

- (IBAction)clear:(id)sender
{
    /* clear table */
    input_thread_t * p_input = pl_CurrentInput(VLCIntf);

    if (!p_input)
        return;

    input_Control(p_input, INPUT_CLEAR_BOOKMARKS);

    vlc_object_release(p_input);

    [o_tbl_dataTable reloadData];
}

- (IBAction)edit:(id)sender
{
    /* put values to the sheet's fields and show sheet */
    /* we take the values from the core and not the table, because we cannot
     * really trust it */
    input_thread_t * p_input = pl_CurrentInput(VLCIntf);
    seekpoint_t **pp_bookmarks;
    int i_bookmarks;
    int row;
    row = [o_tbl_dataTable selectedRow];

    if (!p_input)
        return;

    if (row < 0) {
        vlc_object_release(p_input);
        return;
    }

    if (input_Control(p_input, INPUT_GET_BOOKMARKS, &pp_bookmarks, &i_bookmarks) != VLC_SUCCESS) {
        vlc_object_release(p_input);
        return;
    }

    [o_edit_fld_name setStringValue: toNSStr(pp_bookmarks[row]->psz_name)];
    int total = pp_bookmarks[row]->i_time_offset/ 1000000;
    int hour = total / (60*60);
    int min = (total - hour*60*60) / 60;
    int sec = total - hour*60*60 - min*60;
    [o_edit_fld_time setStringValue: [NSString stringWithFormat:@"%02d:%02d:%02d", hour, min, sec]];
    [o_edit_fld_bytes setStringValue: [NSString stringWithFormat:@"%lli", pp_bookmarks[row]->i_byte_offset]];

    /* Just keep the pointer value to check if it
     * changes. Note, we don't need to keep a reference to the object.
     * so release it now. */
    p_old_input = p_input;
    vlc_object_release(p_input);

    [NSApp beginSheet: o_edit_window modalForWindow: o_bookmarks_window modalDelegate: o_edit_window didEndSelector: nil contextInfo: nil];

    // Clear the bookmark list
    for (int i = 0; i < i_bookmarks; i++)
        vlc_seekpoint_Delete(pp_bookmarks[i]);
    free(pp_bookmarks);
}

- (IBAction)edit_cancel:(id)sender
{
    /* close sheet */
    [NSApp endSheet:o_edit_window];
    [o_edit_window close];
}

- (IBAction)edit_ok:(id)sender
{
    /* save field contents and close sheet */
     seekpoint_t **pp_bookmarks;
    int i_bookmarks, i;
    input_thread_t * p_input = pl_CurrentInput(VLCIntf);

    if (!p_input) {
        NSBeginCriticalAlertSheet(_NS("No input"), _NS("OK"), @"", @"", o_bookmarks_window, nil, nil, nil, nil, @"%@",_NS("No input found. A stream must be playing or paused for bookmarks to work."));
        return;
    }
    if (p_old_input != p_input) {
        NSBeginCriticalAlertSheet(_NS("Input has changed"), _NS("OK"), @"", @"", o_bookmarks_window, nil, nil, nil, nil, @"%@",_NS("Input has changed, unable to save bookmark. Suspending playback with \"Pause\" while editing bookmarks to ensure to keep the same input."));
        vlc_object_release(p_input);
        return;
    }

    if (input_Control(p_input, INPUT_GET_BOOKMARKS, &pp_bookmarks, &i_bookmarks) != VLC_SUCCESS) {
        vlc_object_release(p_input);
        return;
    }

    i = [o_tbl_dataTable selectedRow];

    free(pp_bookmarks[i]->psz_name);

    pp_bookmarks[i]->psz_name = strdup([[o_edit_fld_name stringValue] UTF8String]);
    pp_bookmarks[i]->i_byte_offset = [[o_edit_fld_bytes stringValue] intValue];

    NSArray * components = [[o_edit_fld_time stringValue] componentsSeparatedByString:@":"];
    NSUInteger componentCount = [components count];
    if (componentCount == 1)
        pp_bookmarks[i]->i_time_offset = 1000000 * ([[components objectAtIndex:0] intValue]);
    else if (componentCount == 2)
        pp_bookmarks[i]->i_time_offset = 1000000 * ([[components objectAtIndex:0] intValue] * 60 + [[components objectAtIndex:1] intValue]);
    else if (componentCount == 3)
        pp_bookmarks[i]->i_time_offset = 1000000 * ([[components objectAtIndex:0] intValue] * 3600 + [[components objectAtIndex:1] intValue] * 60 + [[components objectAtIndex:2] intValue]);
    else {
        msg_Err(VLCIntf, "Invalid string format for time");
        goto clear;
    }

    if (input_Control(p_input, INPUT_CHANGE_BOOKMARK, pp_bookmarks[i], i) != VLC_SUCCESS) {
        msg_Warn(VLCIntf, "Unable to change the bookmark");
        goto clear;
    }

    [o_tbl_dataTable reloadData];
    vlc_object_release(p_input);

    [NSApp endSheet: o_edit_window];
    [o_edit_window close];

clear:
    // Clear the bookmark list
    for (int i = 0; i < i_bookmarks; i++)
        vlc_seekpoint_Delete(pp_bookmarks[i]);
    free(pp_bookmarks);
}

- (IBAction)extract:(id)sender
{
    if ([o_tbl_dataTable numberOfSelectedRows] < 2) {
        NSBeginAlertSheet(_NS("Invalid selection"), _NS("OK"), @"", @"", o_bookmarks_window, nil, nil, nil, nil, @"%@",_NS("Two bookmarks have to be selected."));
        return;
    }
    input_thread_t * p_input = pl_CurrentInput(VLCIntf);
    if (!p_input) {
        NSBeginCriticalAlertSheet(_NS("No input found"), _NS("OK"), @"", @"", o_bookmarks_window, nil, nil, nil, nil, @"%@",_NS("The stream must be playing or paused for bookmarks to work."));
        return;
    }

    seekpoint_t **pp_bookmarks;
    int i_bookmarks ;
    int i_first = -1;
    int i_second = -1;
    int c = 0;
    for (NSUInteger x = 0; c != 2; x++) {
        if ([o_tbl_dataTable isRowSelected:x]) {
            if (i_first == -1) {
                i_first = x;
                c = 1;
            } else if (i_second == -1) {
                i_second = x;
                c = 2;
            }
        }
    }

    if (input_Control(p_input, INPUT_GET_BOOKMARKS, &pp_bookmarks, &i_bookmarks) != VLC_SUCCESS) {
        vlc_object_release(p_input);
        msg_Err(VLCIntf, "already defined bookmarks couldn't be retrieved");
        return;
    }

    char *psz_uri = input_item_GetURI(input_GetItem(p_input));
    [[[VLCMain sharedInstance] wizard] initWithExtractValuesFrom: [NSString stringWithFormat:@"%lli", pp_bookmarks[i_first]->i_time_offset/1000000] to: [NSString stringWithFormat:@"%lli", pp_bookmarks[i_second]->i_time_offset/1000000] ofItem: toNSStr(psz_uri)];
    free(psz_uri);
    vlc_object_release(p_input);

    // Clear the bookmark list
    for (int i = 0; i < i_bookmarks; i++)
        vlc_seekpoint_Delete(pp_bookmarks[i]);
    free(pp_bookmarks);
}

- (IBAction)goToBookmark:(id)sender
{
    input_thread_t * p_input = pl_CurrentInput(VLCIntf);

    if (!p_input)
        return;

    input_Control(p_input, INPUT_SET_BOOKMARK, [o_tbl_dataTable selectedRow]);

    vlc_object_release(p_input);
}

- (IBAction)remove:(id)sender
{
    input_thread_t * p_input = pl_CurrentInput(VLCIntf);

    if (!p_input)
        return;

    int i_focused = [o_tbl_dataTable selectedRow];

    if (i_focused >= 0)
        input_Control(p_input, INPUT_DEL_BOOKMARK, i_focused);

    vlc_object_release(p_input);

    [o_tbl_dataTable reloadData];
}

/*****************************************************************************
 * callback stuff
 *****************************************************************************/

-(id)dataTable
{
    return o_tbl_dataTable;
}

/*****************************************************************************
 * data source methods
 *****************************************************************************/

- (NSInteger)numberOfRowsInTableView:(NSTableView *)theDataTable
{
    /* return the number of bookmarks */
    input_thread_t * p_input = pl_CurrentInput(VLCIntf);
    seekpoint_t **pp_bookmarks;
    int i_bookmarks;

    if (!p_input)
        return 0;

    int returnValue = input_Control(p_input, INPUT_GET_BOOKMARKS, &pp_bookmarks, &i_bookmarks);
    vlc_object_release(p_input);

    if (returnValue != VLC_SUCCESS)
        return 0;

    for (int i = 0; i < i_bookmarks; i++)
        vlc_seekpoint_Delete(pp_bookmarks[i]);
    free(pp_bookmarks);

    return i_bookmarks;
}

- (id)tableView:(NSTableView *)theDataTable objectValueForTableColumn: (NSTableColumn *)theTableColumn row: (NSInteger)row
{
    /* return the corresponding data as NSString */
    input_thread_t * p_input = pl_CurrentInput(VLCIntf);
    seekpoint_t **pp_bookmarks;
    int i_bookmarks;
    id ret;

    if (!p_input)
        return @"";
    else if (input_Control(p_input, INPUT_GET_BOOKMARKS, &pp_bookmarks, &i_bookmarks) != VLC_SUCCESS)
        ret = @"";
    else {
        NSString * identifier = [theTableColumn identifier];
        if ([identifier isEqualToString: @"description"])
            ret = toNSStr(pp_bookmarks[row]->psz_name);
        else if ([identifier isEqualToString: @"size_offset"])
            ret = [NSString stringWithFormat:@"%lli", pp_bookmarks[row]->i_byte_offset];
        else if ([identifier isEqualToString: @"time_offset"]) {
            int total = pp_bookmarks[row]->i_time_offset/ 1000000;
            int hour = total / (60*60);
            int min = (total - hour*60*60) / 60;
            int sec = total - hour*60*60 - min*60;
            ret = [NSString stringWithFormat:@"%02d:%02d:%02d", hour, min, sec];
        }

        // Clear the bookmark list
        for (int i = 0; i < i_bookmarks; i++)
            vlc_seekpoint_Delete(pp_bookmarks[i]);
        free(pp_bookmarks);
    }
    vlc_object_release(p_input);
    return ret;
}

/*****************************************************************************
 * delegate methods
 *****************************************************************************/

- (void)tableViewSelectionDidChange:(NSNotification *)aNotification
{
    /* check whether a row is selected and en-/disable the edit/remove buttons */
    if ([o_tbl_dataTable selectedRow] == -1) {
        /* no row is selected */
        [o_btn_edit setEnabled: NO];
        [o_btn_rm setEnabled: NO];
        [o_btn_extract setEnabled: NO];
    } else {
        /* a row is selected */
        [o_btn_edit setEnabled: YES];
        [o_btn_rm setEnabled: YES];
        if ([o_tbl_dataTable numberOfSelectedRows] == 2)
            [o_btn_extract setEnabled: YES];
    }
}

@end
