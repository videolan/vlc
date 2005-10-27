/*****************************************************************************
 * bookmarks.m: MacOS X Bookmarks window
 *****************************************************************************
 * Copyright (C) 2005 the VideoLAN team
 * $Id$
 *
 * Authors: Felix KŸhne <fkuehne@users.sf.net>
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
 * Note: 
 * the code used to bind with VLC's modules is heavily based upon 
 * ../wxwidgets/bookmarks.cpp, written by Gildas Bazin. 
 * (he is a member of the VideoLAN team) 
 *****************************************************************************/


/*****************************************************************************
 * Preamble
 *****************************************************************************/

#import "bookmarks.h"
#import "intf.h"
#import "wizard.h"
#import <vlc/intf.h>

/*****************************************************************************
 * VLCExtended implementation
 *
 * implements the GUI functions for the window, the data source and the
 * delegate for o_tbl_dataTable
 *****************************************************************************/

@implementation VLCBookmarks

static VLCBookmarks *_o_sharedInstance = nil;

+ (VLCBookmarks *)sharedInstance
{
    return _o_sharedInstance ? _o_sharedInstance : [[self alloc] init];
}

- (id)init
{
    if (_o_sharedInstance) {
        [self dealloc];
    } else {
        _o_sharedInstance = [super init];
    }

    return _o_sharedInstance;
}

/*****************************************************************************
 * GUI methods
 *****************************************************************************/

- (void)awakeFromNib
{
    [self initStrings];
}

- (void)dealloc
{
    if(p_old_input)
    {
        free(p_old_input);
    }
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
    [[[o_tbl_dataTable tableColumnWithIdentifier:@"description"] headerCell] \
        setStringValue: _NS("Description")];
    [[[o_tbl_dataTable tableColumnWithIdentifier:@"size_offset"] headerCell] \
        setStringValue: _NS("Size offset")];
    [[[o_tbl_dataTable tableColumnWithIdentifier:@"time_offset"] headerCell] \
        setStringValue: _NS("Time offset")];
        
    /* edit window */
    [o_edit_btn_ok setTitle: _NS("OK")];
    [o_edit_btn_cancel setTitle: _NS("Cancel")];
    [o_edit_lbl_name setStringValue: _NS("Name")];
    [o_edit_lbl_time setStringValue: _NS("Time")];
    [o_edit_lbl_bytes setStringValue: _NS("Bytes")];
}

- (void)showBookmarks
{
    /* show the window, called from intf.m */
    [o_bookmarks_window displayIfNeeded];
    [o_bookmarks_window makeKeyAndOrderFront:nil];
}

- (IBAction)add:(id)sender
{
    /* add item to list */
    intf_thread_t * p_intf = VLCIntf;
    input_thread_t * p_input = (input_thread_t *)vlc_object_find( p_intf, \
        VLC_OBJECT_INPUT, FIND_ANYWHERE );
    if( !p_input )
        return;
    
    seekpoint_t bookmark;
    vlc_value_t pos;
    bookmark.psz_name = NULL;
    bookmark.i_byte_offset = 0;
    bookmark.i_time_offset = 0;
    
    var_Get(p_intf, "position", &pos);
    bookmark.psz_name = _("Untitled");
    input_Control( p_input, INPUT_GET_BYTE_POSITION, &bookmark.i_byte_offset );
    var_Get( p_input, "time", &pos );
    bookmark.i_time_offset = pos.i_time;
    input_Control( p_input, INPUT_ADD_BOOKMARK, &bookmark );
    
    vlc_object_release( p_input );
    
    [o_tbl_dataTable reloadData];
}

- (IBAction)clear:(id)sender
{
    /* clear table */
    intf_thread_t * p_intf = VLCIntf;
    input_thread_t *p_input = (input_thread_t *)vlc_object_find( p_intf, \
        VLC_OBJECT_INPUT, FIND_ANYWHERE );
    
    if( !p_input )
        return;

    input_Control( p_input, INPUT_CLEAR_BOOKMARKS );

    vlc_object_release( p_input );
    
    [o_tbl_dataTable reloadData];
}

- (IBAction)edit:(id)sender
{
    /* put values to the sheet's fields and show sheet */
    /* we take the values from the core and not the table, because we cannot
     * really trust it */
    intf_thread_t * p_intf = VLCIntf;
    input_thread_t * p_input = (input_thread_t *)vlc_object_find( p_intf, \
        VLC_OBJECT_INPUT, FIND_ANYWHERE );
    seekpoint_t **pp_bookmarks;
    int i_bookmarks;
    char * toBeReturned;
    toBeReturned = "";
    int i_toBeReturned;
    i_toBeReturned = 0;
    int row;
    row = [o_tbl_dataTable selectedRow];
    
    if( !p_input )
    {
        return;
    } 
    else if( input_Control( p_input, INPUT_GET_BOOKMARKS, &pp_bookmarks, \
        &i_bookmarks ) != VLC_SUCCESS )
    {
        vlc_object_release( p_input );
        return;
    } 
    else if(row < 0)
    {
        vlc_object_release( p_input );
        return;
    } else {
        [o_edit_fld_name setStringValue: [NSString stringWithUTF8String: \
            pp_bookmarks[row]->psz_name]];
        [o_edit_fld_time setStringValue: [[NSNumber numberWithInt: \
            (pp_bookmarks[row]->i_time_offset / 1000000)] stringValue]];
        [o_edit_fld_bytes setStringValue: [[NSNumber numberWithInt: \
            pp_bookmarks[row]->i_byte_offset] stringValue]];
    }
    
    p_old_input = p_input;
    vlc_object_release( p_input );

    [NSApp beginSheet: o_edit_window
        modalForWindow: o_bookmarks_window
        modalDelegate: o_edit_window
        didEndSelector: nil
        contextInfo: nil];
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
    
    intf_thread_t * p_intf = VLCIntf;
    seekpoint_t **pp_bookmarks;
    int i_bookmarks, i;
    input_thread_t *p_input = (input_thread_t *)vlc_object_find( p_intf, \
        VLC_OBJECT_INPUT, FIND_ANYWHERE );
    
    if( !p_input )
    {
        NSBeginCriticalAlertSheet(_NS("No input"), _NS("OK"), \
                @"", @"", o_bookmarks_window, nil, nil, nil, nil, _NS("No " \
                "input found. The stream must be playing or paused for " \
                "bookmarks to work."));
        return;
    }
    if( p_old_input != p_input )
    {
        NSBeginCriticalAlertSheet(_NS("Input has changed"), _NS("OK"), \
            @"", @"", o_bookmarks_window, nil, nil, nil, nil, _NS("Input " \
            "has changed, unable to save bookmark. Use \"Pause\" while " \
            "editing bookmarks to keep the same input."));
        vlc_object_release( p_input );
        return;
    }
    
    if( input_Control( p_input, INPUT_GET_BOOKMARKS, &pp_bookmarks, \
        &i_bookmarks ) != VLC_SUCCESS )
    {
        vlc_object_release( p_input );
        return;
    } 

    i = [o_tbl_dataTable selectedRow];
    
    if( pp_bookmarks[i]->psz_name ) 
    {
        free( pp_bookmarks[i]->psz_name );
    }

    pp_bookmarks[i]->psz_name = strdup([[o_edit_fld_name stringValue] UTF8String]); 
    pp_bookmarks[i]->i_byte_offset = [[o_edit_fld_bytes stringValue] intValue];
    pp_bookmarks[i]->i_time_offset = ([[o_edit_fld_time stringValue] intValue]  * 1000000);
    
    if( input_Control( p_input, INPUT_CHANGE_BOOKMARK, pp_bookmarks[i], i ) \
        != VLC_SUCCESS )
    {
        msg_Warn( p_intf, "VLCBookmarks: changing bookmark failed");
        vlc_object_release( p_input );
        return;
    }
    
    [o_tbl_dataTable reloadData];
    vlc_object_release( p_input );
     
    
    [NSApp endSheet: o_edit_window];
    [o_edit_window close];
}

- (IBAction)extract:(id)sender
{
    /* extract */
    
    intf_thread_t * p_intf = VLCIntf;
    
    if( [o_tbl_dataTable numberOfSelectedRows] < 2 )
    {
        NSBeginAlertSheet(_NS("Invalid selection"), _NS("OK"), \
            @"", @"", o_bookmarks_window, nil, nil, nil, nil, _NS("" \
            "You have to select two bookmarks."));
        return;
    }
    input_thread_t *p_input =
        (input_thread_t *)vlc_object_find( p_intf, VLC_OBJECT_INPUT,
                                           FIND_ANYWHERE );
    if( !p_input )
    {
        NSBeginCriticalAlertSheet(_NS("No input found"), _NS("OK"), \
            @"", @"", o_bookmarks_window, nil, nil, nil, nil, _NS("" \
            "The stream must be playing or paused for bookmarks to work."));
        return;
    }
    
    seekpoint_t **pp_bookmarks;
    int i_bookmarks ;
    int i_first = -1;
    int i_second = -1;
    int x = 0;
    int c = 0;
    while (c != 2)
    {
        if([o_tbl_dataTable isRowSelected:x])
        {
            if (i_first == -1)
            {
                i_first = x;
                c = 1;
            } 
            else if (i_second == -1)
            {
                i_second = x;
                c = 2;
            }
        }
        x = (x + 1);
    }
    
    msg_Dbg(p_intf, "got the bookmark-indexes");
    
    if( input_Control( p_input, INPUT_GET_BOOKMARKS, &pp_bookmarks, \
        &i_bookmarks ) != VLC_SUCCESS )
    {
        vlc_object_release( p_input );
        msg_Err(p_intf, "bookmarks couldn't be retrieved from core");
        return;
    }
    msg_Dbg(p_intf, "calling wizard");

    [[[VLCMain sharedInstance] getWizard] initWithExtractValuesFrom: \
            [[NSNumber numberWithInt: \
            (pp_bookmarks[i_first]->i_time_offset/1000000)] stringValue] \
            to: [[NSNumber numberWithInt: \
            (pp_bookmarks[i_second]->i_time_offset/1000000)] stringValue] \
            ofItem: [NSString stringWithUTF8String: \
            p_input->input.p_item->psz_uri]];
    vlc_object_release( p_input );
    msg_Dbg(p_intf, "released input");
}

- (IBAction)goToBookmark:(id)sender
{
    intf_thread_t * p_intf = VLCIntf;
    input_thread_t *p_input =
    (input_thread_t *)vlc_object_find( p_intf, VLC_OBJECT_INPUT, FIND_ANYWHERE );
    
    if( !p_input ) 
    {
        return;
    }

    input_Control( p_input, INPUT_SET_BOOKMARK, [o_tbl_dataTable selectedRow] );

    vlc_object_release( p_input );
}

- (IBAction)remove:(id)sender
{
    /* remove selected item */
    intf_thread_t * p_intf = VLCIntf;
    input_thread_t *p_input =
    (input_thread_t *)vlc_object_find( p_intf, VLC_OBJECT_INPUT, FIND_ANYWHERE );
    
    if( !p_input ) return;

    int i_focused = [o_tbl_dataTable selectedRow];
    if( i_focused >= 0 )
    {
        input_Control( p_input, INPUT_DEL_BOOKMARK, i_focused );
    }

    vlc_object_release( p_input );
    
    [o_tbl_dataTable reloadData];
}

/*****************************************************************************
 * callback stuff
 *****************************************************************************/

-(id)getDataTable
{
    return o_tbl_dataTable;
}

/*****************************************************************************
 * data source methods
 *****************************************************************************/

- (int)numberOfRowsInTableView:(NSTableView *)theDataTable
{
    /* return the number of bookmarks */
    intf_thread_t * p_intf = VLCIntf;
    input_thread_t * p_input = (input_thread_t *)vlc_object_find( p_intf, \
        VLC_OBJECT_INPUT, FIND_ANYWHERE );
    seekpoint_t **pp_bookmarks;
    int i_bookmarks;
    
    if( !p_input )
    {
        return 0;
    }
    else if( input_Control( p_input, INPUT_GET_BOOKMARKS, &pp_bookmarks, \
                       &i_bookmarks ) != VLC_SUCCESS )
    {
        vlc_object_release( p_input );
        return 0;
    }
    else {
        vlc_object_release( p_input );
        return i_bookmarks;
    }
}

- (id)tableView:(NSTableView *)theDataTable objectValueForTableColumn: \
    (NSTableColumn *)theTableColumn row: (int)row
{
    /* return the corresponding data as NSString */
    intf_thread_t * p_intf = VLCIntf;
    input_thread_t * p_input = (input_thread_t *)vlc_object_find( p_intf, \
        VLC_OBJECT_INPUT, FIND_ANYWHERE );
    seekpoint_t **pp_bookmarks;
    int i_bookmarks;
    char * toBeReturned;
    toBeReturned = "";
    int i_toBeReturned;
    i_toBeReturned = 0;
    
    if( !p_input )
    {
        return @"";
    } 
    else if( input_Control( p_input, INPUT_GET_BOOKMARKS, &pp_bookmarks, \
                       &i_bookmarks ) != VLC_SUCCESS )
    {
        vlc_object_release( p_input );
        return @"";
    }
    else
    {
        if ([[theTableColumn identifier] isEqualToString: @"description"])
        {
            toBeReturned = pp_bookmarks[row]->psz_name;
            vlc_object_release( p_input );
            return [NSString stringWithUTF8String: toBeReturned];
        } 
        else if ([[theTableColumn identifier] isEqualToString: @"size_offset"])
        {
            i_toBeReturned = pp_bookmarks[row]->i_byte_offset;
            vlc_object_release( p_input );
            return [[NSNumber numberWithInt: i_toBeReturned] stringValue];
        }
        else if ([[theTableColumn identifier] isEqualToString: @"time_offset"])
        {
            i_toBeReturned = pp_bookmarks[row]->i_time_offset;
            vlc_object_release( p_input );
            return [[NSNumber numberWithInt: (i_toBeReturned / 1000000)] \
                stringValue];
        }
        else
        {
            /* may not happen, but just in case */
            vlc_object_release( p_input );
            msg_Err(p_intf, "VLCBookmarks: unknown table column identifier " \
                "(%s) while updating table", [[theTableColumn identifier] \
                UTF8String] );
            return @"unknown identifier";
        }
    }

}

/*****************************************************************************
 * delegate methods
 *****************************************************************************/

- (void)tableViewSelectionDidChange:(NSNotification *)aNotification
{
    /* check whether a row is selected and en-/disable the edit/remove buttons */
    if ([o_tbl_dataTable selectedRow] == -1)
    {
        /* no row is selected */
        [o_btn_edit setEnabled: NO];
        [o_btn_rm setEnabled: NO];
        [o_btn_extract setEnabled: NO];
    }
    else
    {
        /* a row is selected */
        [o_btn_edit setEnabled: YES];
        [o_btn_rm setEnabled: YES];
        if ([o_tbl_dataTable numberOfSelectedRows] == 2)
        {
            [o_btn_extract setEnabled: YES];
        }
    }
}

@end
