/*****************************************************************************
 * playlist.m: MacOS X interface plugin
 *****************************************************************************
 * Copyright (C) 2002-2003 VideoLAN
 * $Id: playlist.m,v 1.15 2003/03/17 17:10:21 hartman Exp $
 *
 * Authors: Jon Lech Johansen <jon-vl@nanocrew.net>
 *          Derk-Jan Hartman <thedj@users.sourceforge.net>
 * Thanks:  Andrew Stone for documenting the row reordering methods on the net
 *              http://www.omnigroup.com/mailman/archive/macosx-dev/
 *              2001-January/008195.html
 *          Apple Computer for documenting the Alternating row colors
 *		http://developer.apple.com/samplecode/Sample_Code/Cocoa/
 *              MP3_Player/MyTableView.m.htm
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
 * Preamble
 *****************************************************************************/
#include <stdlib.h>                                      /* malloc(), free() */
#include <sys/param.h>                                    /* for MAXPATHLEN */
#include <string.h>

#include "intf.h"
#include "playlist.h"

// RGB values for stripe color (light blue)
#define STRIPE_RED   (237.0 / 255.0)
#define STRIPE_GREEN (243.0 / 255.0)
#define STRIPE_BLUE  (254.0 / 255.0)
static NSColor *sStripeColor = nil;

/*****************************************************************************
 * VLCPlaylistView implementation 
 *****************************************************************************/
@implementation VLCPlaylistView

- (NSMenu *)menuForEvent:(NSEvent *)o_event
{
    return( [[self delegate] menuForEvent: o_event] );
}

- (void)keyDown:(NSEvent *)o_event
{
    unichar key = 0;
    int i_row;
    playlist_t * p_playlist;
    intf_thread_t * p_intf = [NSApp getIntf];

    if( [[o_event characters] length] )
    {
        key = [[o_event characters] characterAtIndex: 0];
    }

    p_playlist = vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                          FIND_ANYWHERE );
    
    if ( p_playlist == NULL )
    {
        return;
    }
    
    switch( key )
    {
        case ' ':
            vlc_mutex_lock( &p_playlist->object_lock );
            if( p_playlist->p_input != NULL )
            {
                input_SetStatus( p_playlist->p_input, INPUT_STATUS_PAUSE );
            }
            vlc_mutex_unlock( &p_playlist->object_lock );
            break;

        case NSDeleteCharacter:
        case NSDeleteFunctionKey:
        case NSDeleteCharFunctionKey:
        case NSBackspaceCharacter:
            while( ( i_row = [self selectedRow] ) != -1 )
            {
                if( p_playlist->i_index == i_row && p_playlist->i_status )
                {
                    playlist_Stop( p_playlist );
                }
        
                playlist_Delete( p_playlist, i_row ); 
        
                [self deselectRow: i_row];
            }
            [self reloadData];
            break;
            
        default:
            [super keyDown: o_event];
            break;
    }

    if( p_playlist != NULL )
    {
        vlc_object_release( p_playlist );
    }
}


/* This is called after the table background is filled in, but before the cell contents are drawn.
 * We override it so we can do our own light-blue row stripes a la iTunes.
 */
- (void) highlightSelectionInClipRect:(NSRect)rect {
    [self drawStripesInRect:rect];
    [super highlightSelectionInClipRect:rect];
}

/* This routine does the actual blue stripe drawing, filling in every other row of the table
 * with a blue background so you can follow the rows easier with your eyes.
 */
- (void) drawStripesInRect:(NSRect)clipRect {
    NSRect stripeRect;
    float fullRowHeight = [self rowHeight] + [self intercellSpacing].height;
    float clipBottom = NSMaxY(clipRect);
    int firstStripe = clipRect.origin.y / fullRowHeight;
    if (firstStripe % 2 == 0)
        firstStripe++;   // we're only interested in drawing the stripes
                         // set up first rect
    stripeRect.origin.x = clipRect.origin.x;
    stripeRect.origin.y = firstStripe * fullRowHeight;
    stripeRect.size.width = clipRect.size.width;
    stripeRect.size.height = fullRowHeight;
    // set the color
    if (sStripeColor == nil)
        sStripeColor = [[NSColor colorWithCalibratedRed:STRIPE_RED green:STRIPE_GREEN blue:STRIPE_BLUE alpha:1.0] retain];
    [sStripeColor set];
    // and draw the stripes
    while (stripeRect.origin.y < clipBottom) {
        NSRectFill(stripeRect);
        stripeRect.origin.y += fullRowHeight * 2.0;
    }
}

@end

/*****************************************************************************
 * VLCPlaylist implementation 
 *****************************************************************************/
@implementation VLCPlaylist

- (void)awakeFromNib
{
    [o_table_view setTarget: self];
    [o_table_view setDelegate: self];
    [o_table_view setDataSource: self];

    [o_table_view setDoubleAction: @selector(playItem:)];

    [o_table_view registerForDraggedTypes: 
        [NSArray arrayWithObjects: NSFilenamesPboardType, nil]];

    [o_mi_play setTitle: _NS("Play")];
    [o_mi_delete setTitle: _NS("Delete")];
    [o_mi_selectall setTitle: _NS("Select All")];
    
    [o_btn_add setToolTip: _NS("Add")];
    [o_btn_remove setToolTip: _NS("Delete")];
}

- (BOOL)tableView:(NSTableView *)o_tv 
                  shouldEditTableColumn:(NSTableColumn *)o_tc
                  row:(int)i_row
{
    return( NO );
}

- (NSMenu *)menuForEvent:(NSEvent *)o_event
{
    NSPoint pt;
    vlc_bool_t b_rows;
    vlc_bool_t b_item_sel;

    pt = [o_table_view convertPoint: [o_event locationInWindow] 
                                                 fromView: nil];
    b_item_sel = ( [o_table_view rowAtPoint: pt] != -1 &&
                   [o_table_view selectedRow] != -1 );
    b_rows = [o_table_view numberOfRows] != 0;

    [o_mi_play setEnabled: b_item_sel];
    [o_mi_delete setEnabled: b_item_sel];
    [o_mi_selectall setEnabled: b_rows];

    return( o_ctx_menu );
}

- (IBAction)playItem:(id)sender
{
    intf_thread_t * p_intf = [NSApp getIntf];
    playlist_t * p_playlist = vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                                       FIND_ANYWHERE );

    if( p_playlist == NULL )
    {
        return;
    }

    playlist_Goto( p_playlist, [o_table_view selectedRow] );

    vlc_object_release( p_playlist );
}

- (IBAction)deleteItems:(id)sender
{
    int i_row;

    intf_thread_t * p_intf = [NSApp getIntf];
    playlist_t * p_playlist = vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                                       FIND_ANYWHERE );

    if( p_playlist == NULL )
    {
        return;
    }

    while( ( i_row = [o_table_view selectedRow] ) != -1 )
    {
        if( p_playlist->i_index == i_row && p_playlist->i_status )
        {
            playlist_Stop( p_playlist );
        }

        playlist_Delete( p_playlist, i_row ); 

        [o_table_view deselectRow: i_row];
    }

    vlc_object_release( p_playlist );

    /* this is actually duplicity, because the intf.m manage also updates the view
     * when the playlist changes. we do this on purpose, because else there is a 
     * delay of .5 sec or so when we delete an item */
    [self playlistUpdated];
}

- (IBAction)selectAll:(id)sender
{
    [o_table_view selectAll: nil];
}

- (void)appendArray:(NSArray*)o_array atPos:(int)i_pos enqueue:(BOOL)b_enqueue
{
    int i_items;
    NSString * o_value;
    NSEnumerator * o_enum;
    intf_thread_t * p_intf = [NSApp getIntf];
    playlist_t * p_playlist = vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                                       FIND_ANYWHERE );

    if( p_playlist == NULL )
    {
        return;
    }

    i_items = 0;
    o_enum = [o_array objectEnumerator];
    while( ( o_value = [o_enum nextObject] ) )
    {
        NSURL * o_url;

        int i_mode = PLAYLIST_INSERT;
        
        if (i_items == 0 && !b_enqueue)
            i_mode |= PLAYLIST_GO;

        playlist_Add( p_playlist, [o_value fileSystemRepresentation],
            i_mode, i_pos == -1 ? PLAYLIST_END : i_pos + i_items );

        o_url = [NSURL fileURLWithPath: o_value];
        if( o_url != nil )
        { 
            [[NSDocumentController sharedDocumentController]
                noteNewRecentDocumentURL: o_url]; 
        }

        i_items++;
    }

    vlc_object_release( p_playlist );
}

- (void)playlistUpdated
{
    [o_table_view reloadData];
}

- (void)updateRowSelection
{
    int i_row;

    intf_thread_t * p_intf = [NSApp getIntf];
    playlist_t * p_playlist = vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                                       FIND_ANYWHERE );

    if( p_playlist == NULL )
    {
        return;
    }

    vlc_mutex_lock( &p_playlist->object_lock );    
    i_row = p_playlist->i_index;
    vlc_mutex_unlock( &p_playlist->object_lock );
    vlc_object_release( p_playlist );

    [o_table_view selectRow: i_row byExtendingSelection: NO];
    [o_table_view scrollRowToVisible: i_row];
}

@end

@implementation VLCPlaylist (NSTableDataSource)

- (int)numberOfRowsInTableView:(NSTableView *)o_tv
{
    int i_count = 0;
    intf_thread_t * p_intf = [NSApp getIntf];
    playlist_t * p_playlist = vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                                       FIND_ANYWHERE );

    if( p_playlist != NULL )
    {
        vlc_mutex_lock( &p_playlist->object_lock );
        i_count = p_playlist->i_size;
        vlc_mutex_unlock( &p_playlist->object_lock );
        vlc_object_release( p_playlist );
    }

    return( i_count );
}

- (id)tableView:(NSTableView *)o_tv 
                objectValueForTableColumn:(NSTableColumn *)o_tc 
                row:(int)i_row
{
    id o_value = nil;
    intf_thread_t * p_intf = [NSApp getIntf];
    playlist_t * p_playlist = vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                               FIND_ANYWHERE );

    if( p_playlist == NULL )
    {
        return( nil );
    }

    vlc_mutex_lock( &p_playlist->object_lock );
    o_value = [[NSString stringWithUTF8String: 
        p_playlist->pp_items[i_row]->psz_name] lastPathComponent]; 
    vlc_mutex_unlock( &p_playlist->object_lock ); 

    vlc_object_release( p_playlist );

    return( o_value );
}

// NEW API for Dragging in TableView:
// typedef enum { NSTableViewDropOn, NSTableViewDropAbove } NSTableViewDropOperation;
// In drag and drop, used to specify a dropOperation. For example, given a table with N rows (numbered with row 0 at the top visually), a row of N-1 and operation of NSTableViewDropOn would specify a drop on the last row. To specify a drop below the last row, one would use a row of N and NSTableViewDropAbove for the operation.

static int _moveRow = -1;

- (BOOL)tableView:(NSTableView *)tv
                    writeRows:(NSArray*)rows
                    toPasteboard:(NSPasteboard*)pboard 
// This method is called after it has been determined that a drag should begin, but before the drag has been started. To refuse the drag, return NO. To start a drag, return YES and place the drag data onto the pasteboard (data, owner, etc...). The drag image and other drag related information will be set up and provided by the table view once this call returns with YES. The rows array is the list of row numbers that will be participating in the drag.
{
    int rowCount = [rows count];
    NSArray *o_filenames = [NSArray array];
    
    // we should allow group selection and copy between windows: PENDING
    [pboard declareTypes:[NSArray arrayWithObject:NSFilenamesPboardType] owner:self];
    [pboard setPropertyList:o_filenames forType:NSFilenamesPboardType];
    if (rowCount == 1)
    {
        _moveRow = [[rows objectAtIndex:0]intValue];
        return YES;
    }
    return NO;
}

- (NSDragOperation)tableView:(NSTableView*)tv
                    validateDrop:(id <NSDraggingInfo>)info
                    proposedRow:(int)row
                    proposedDropOperation:(NSTableViewDropOperation)op 
// This method is used by NSTableView to determine a valid drop target. Based on the mouse position, the table view will suggest a proposed drop location. This method must return a value that indicates which dragging operation the data source will perform. The data source may "re-target" a drop if desired by calling setDropRow:dropOperation: and returning something other than NSDragOperationNone. One may choose to re-target for various reasons (eg. for better visual feedback when inserting into a sorted position).
{
    if ( op == NSTableViewDropAbove )
    {
        if ( row != _moveRow && _moveRow >= 0 )
        {
            return NSDragOperationMove;
        }
        return NSDragOperationLink;
    }
    return NSDragOperationNone;
}

- (BOOL)tableView:(NSTableView*)tv
                    acceptDrop:(id <NSDraggingInfo>)info
                    row:(int)i_row
                    dropOperation:(NSTableViewDropOperation)op 
// This method is called when the mouse is released over an outline view that previously decided to allow a drop via the validateDrop method. The data source should incorporate the data from the dragging pasteboard at this time. 
{
    if (  _moveRow >= 0 )
    {
        BOOL result = [self tableView:tv didDepositRow:_moveRow at:(int)i_row];
        [self playlistUpdated];
        _moveRow = -1;
        return result;
    }
    else
    {
        NSArray * o_values;
        NSPasteboard * o_pasteboard;
        
        o_pasteboard = [info draggingPasteboard];
        
        if( [[o_pasteboard types] containsObject: NSFilenamesPboardType] )
        {
            o_values = [o_pasteboard propertyListForType: NSFilenamesPboardType];
        
            [self appendArray: o_values atPos: i_row enqueue:YES];
        
            return( YES );
        }
        
        return( NO );
    }
}

-  (BOOL)tableView:(NSTableView *)tv didDepositRow:(int)i_row at:(int)i_newrow
{
    if (i_row != -1 && i_newrow != -1)
    {
        intf_thread_t * p_intf = [NSApp getIntf];
        playlist_t * p_playlist = vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                                        FIND_ANYWHERE );
    
        if( p_playlist == NULL )
        {
            return NO;
        }

        playlist_Move( p_playlist, i_row, i_newrow ); 
    
        vlc_object_release( p_playlist );
        return YES;
    }
    return NO;
}

@end

