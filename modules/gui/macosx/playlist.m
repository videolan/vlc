/*****************************************************************************
 * playlist.m: MacOS X interface plugin
 *****************************************************************************
 * Copyright (C) 2002-2003 VideoLAN
 * $Id: playlist.m,v 1.23 2003/05/22 13:40:13 hartman Exp $
 *
 * Authors: Jon Lech Johansen <jon-vl@nanocrew.net>
 *          Derk-Jan Hartman <thedj@users.sourceforge.net>
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

/*****************************************************************************
 * VLCPlaylistView implementation 
 *****************************************************************************/
@implementation VLCPlaylistView

- (void)dealloc
{
    if( o_striped_row_color != nil )
    {
        [o_striped_row_color release];
    }
    [super dealloc];
}

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

- (void)highlightSelectionInClipRect:(NSRect)o_rect {
    NSRect o_new_rect;
    float f_height = [self rowHeight] + [self intercellSpacing].height;
    float f_origin_y = NSMaxY( o_rect );
    int i_row = o_rect.origin.y / f_height;
    
    if( [NSColor respondsTo:alternateSelectedControlColor] )
    {
        [super highlightSelectionInClipRect:o_rect];
        break;
    }
    
    if ( i_row % 2 == 0 )
    {
        i_row++;
    }
    
    o_new_rect.size.width = o_rect.size.width;
    o_new_rect.size.height = f_height;
    o_new_rect.origin.x = o_rect.origin.x;
    o_new_rect.origin.y = i_row * f_height;
    
    if ( o_striped_row_color == nil )
    {
        o_striped_row_color = [[[NSColor alternateSelectedControlColor]
                                highlightWithLevel: 0.90] retain];
    }
    [o_striped_row_color set];
    
    while ( o_new_rect.origin.y < f_origin_y ) {
        NSRectFill( o_new_rect );
        o_new_rect.origin.y += f_height * 2.0;
    }
    [super highlightSelectionInClipRect:o_rect];
}

@end

/*****************************************************************************
 * VLCPlaylist implementation 
 *****************************************************************************/
@implementation VLCPlaylist

- (id)init
{
    self = [super init];
    if ( self !=nil )
    {
        i_moveRow = -1;
    }
    return self;
}

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
    [self updateRowSelection];
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

- (BOOL)tableView:(NSTableView *)o_tv
                    writeRows:(NSArray*)o_rows
                    toPasteboard:(NSPasteboard*)o_pasteboard 
{
    int i_rows = [o_rows count];
    NSArray *o_filenames = [NSArray array];
    
    [o_pasteboard declareTypes:[NSArray arrayWithObject:NSFilenamesPboardType] owner:self];
    [o_pasteboard setPropertyList:o_filenames forType:NSFilenamesPboardType];
    if ( i_rows == 1 )
    {
        i_moveRow = [[o_rows objectAtIndex:0]intValue];
        return YES;
    }
    return NO;
}

- (NSDragOperation)tableView:(NSTableView*)o_tv
                    validateDrop:(id <NSDraggingInfo>)o_info
                    proposedRow:(int)i_row
                    proposedDropOperation:(NSTableViewDropOperation)o_operation 
{
    if ( o_operation == NSTableViewDropAbove )
    {
        if ( i_moveRow >= 0 )
        {
            if ( i_row != i_moveRow )
            {
                return NSDragOperationMove;
            }
            /* what if in the previous run, the row wasn't actually moved? 
               then we can't drop new files on this location */
            return NSDragOperationNone;
        }
        return NSDragOperationGeneric;
    }
    return NSDragOperationNone;
}

- (BOOL)tableView:(NSTableView*)o_tv
                    acceptDrop:(id <NSDraggingInfo>)o_info
                    row:(int)i_proposed_row
                    dropOperation:(NSTableViewDropOperation)o_operation 
{
    if (  i_moveRow >= 0 )
    {
        if (i_moveRow != -1 && i_proposed_row != -1)
        {
            intf_thread_t * p_intf = [NSApp getIntf];
            playlist_t * p_playlist = vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                                            FIND_ANYWHERE );
        
            if( p_playlist == NULL )
            {
                i_moveRow = -1;
                return NO;
            }
    
            playlist_Move( p_playlist, i_moveRow, i_proposed_row ); 
        
            vlc_object_release( p_playlist );
        }
        [self playlistUpdated];
        i_moveRow = -1;
        return YES;
    }
    else
    {
        NSArray * o_values;
        NSPasteboard * o_pasteboard;
        
        intf_thread_t * p_intf = [NSApp getIntf];
        o_pasteboard = [o_info draggingPasteboard];
        
        if( [[o_pasteboard types] containsObject: NSFilenamesPboardType] )
        {
            o_values = [[o_pasteboard propertyListForType: NSFilenamesPboardType]
                        sortedArrayUsingSelector:@selector(caseInsensitiveCompare:)];

            config_PutPsz( p_intf, "sub-file", "" );
            config_PutInt( p_intf, "sub-delay", 0 );
            config_PutFloat( p_intf, "sub-fps", 0.0 );
            config_PutPsz( p_intf, "sout", "" );

            [self appendArray: o_values atPos: i_proposed_row enqueue:YES];

            return( YES );
        }
        
        return( NO );
    }
    [self updateRowSelection];
}

@end

