/*****************************************************************************
 * playlist.m: MacOS X interface plugin
 *****************************************************************************
 * Copyright (C) 2002-2003 VideoLAN
 * $Id: playlist.m,v 1.29 2003/07/27 23:05:41 hartman Exp $
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
#include <math.h>

#include "intf.h"
#include "playlist.h"

int MacVersion102 = -1;

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
    int i, c, i_row;
    NSMutableArray *o_to_delete;
    NSNumber *o_number;
    
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
            o_to_delete = [NSMutableArray arrayWithArray:[[self selectedRowEnumerator] allObjects]];
            c = [o_to_delete count];
            
            for( i = 0; i < c; i++ ) {
                o_number = [o_to_delete lastObject];
                i_row = [o_number intValue];
                
                if( p_playlist->i_index == i_row && p_playlist->i_status )
                {
                    playlist_Stop( p_playlist );
                }
                [o_to_delete removeObject: o_number];
                [self deselectRow: i_row];
                playlist_Delete( p_playlist, i_row );
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
    
    if ( i_row % 2 == 0 )
    {
        i_row++;
    }
    
    o_new_rect.size.width = o_rect.size.width;
    o_new_rect.size.height = f_height;
    o_new_rect.origin.x = o_rect.origin.x;
    o_new_rect.origin.y = i_row * f_height;
   
    if( ( MacVersion102 < 0 ) && ( floor( NSAppKitVersionNumber ) > NSAppKitVersionNumber10_1 ) )
    {
        MacVersion102 = 102;
    }
 
    if ( MacVersion102 == 102 && o_striped_row_color == nil )
    {
        o_striped_row_color = [[[NSColor alternateSelectedControlColor]
                                highlightWithLevel: 0.90] retain];
        
    }
    else if ( o_striped_row_color == nil )
    {
        /* OSX 10.1 and before ain't that smart ;) */
        o_striped_row_color = [[NSColor whiteColor] retain];
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
    int i, c, i_row;
    NSMutableArray *o_to_delete;
    NSNumber *o_number;

    intf_thread_t * p_intf = [NSApp getIntf];
    playlist_t * p_playlist = vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                                       FIND_ANYWHERE );

    if( p_playlist == NULL )
    {
        return;
    }
    
    o_to_delete = [NSMutableArray arrayWithArray:[[o_table_view selectedRowEnumerator] allObjects]];
    c = [o_to_delete count];
    
    for( i = 0; i < c; i++ ) {
        o_number = [o_to_delete lastObject];
        i_row = [o_number intValue];
        
        if( p_playlist->i_index == i_row && p_playlist->i_status )
        {
            playlist_Stop( p_playlist );
        }
        [o_to_delete removeObject: o_number];
        [o_table_view deselectRow: i_row];
        playlist_Delete( p_playlist, i_row );
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

- (void)appendArray:(NSArray*)o_array atPos:(int)i_position enqueue:(BOOL)b_enqueue
{
    int i_item;
    intf_thread_t * p_intf = [NSApp getIntf];
    playlist_t * p_playlist = vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                                       FIND_ANYWHERE );

    if( p_playlist == NULL )
    {
        return;
    }

    for ( i_item = 0; i_item < [o_array count]; i_item++ )
    {
        /* One item */
        NSDictionary *o_one_item;
        NSString *o_url;
        NSString *o_name;
        NSArray *o_options;
        int j, i_total_options = 0;
        char **ppsz_options = NULL;
        int i_mode = PLAYLIST_INSERT;
        
        /* Get the item */
        o_one_item = [o_array objectAtIndex: i_item];
        o_url = (NSString *)[o_one_item objectForKey: @"ITEM_URL"];
        o_name = (NSString *)[o_one_item objectForKey: @"ITEM_NAME"];
        o_options = (NSArray *)[o_one_item objectForKey: @"ITEM_OPTIONS"];
        
        if( !o_name) o_name = o_url;
        
        if (i_item == 0 && !b_enqueue)
            i_mode |= PLAYLIST_GO;

        if( o_options && [o_options count] > 0 )
        {
            /* Count the input options */
            i_total_options = [o_options count];
    
            /* Allocate ppsz_options */
            for( j = 0; j < i_total_options; j++ )
            {
                if( !ppsz_options )
                    ppsz_options = (char **)malloc( sizeof(char *) * i_total_options );
    
                ppsz_options[j] = strdup([[o_options objectAtIndex:j] UTF8String]);
            }
        }
        
        playlist_AddName( p_playlist, [o_url fileSystemRepresentation], [o_name UTF8String],
            (ppsz_options != NULL ) ? (const char **)ppsz_options : 0, i_total_options,
            i_mode, i_position == -1 ? PLAYLIST_END : i_position + i_item );

        /* clean up */
        for( j = 0; j < i_total_options; j++ )
            free( ppsz_options[j] );
        if( ppsz_options ) free( ppsz_options );

        NSURL *o_true_url = [NSURL fileURLWithPath: o_url];
        if( o_true_url != nil )
        { 
            [[NSDocumentController sharedDocumentController]
                noteNewRecentDocumentURL: o_true_url]; 
        }
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
        NSPasteboard * o_pasteboard;
        
        intf_thread_t * p_intf = [NSApp getIntf];
        o_pasteboard = [o_info draggingPasteboard];
        
        if( [[o_pasteboard types] containsObject: NSFilenamesPboardType] )
        {
            int i;
            NSArray *o_array = [NSArray array];
            NSArray *o_values = [[o_pasteboard propertyListForType: NSFilenamesPboardType]
                        sortedArrayUsingSelector:@selector(caseInsensitiveCompare:)];

            for( i = 0; i < [o_values count]; i++)
            {
                NSDictionary *o_dic;
                o_dic = [NSDictionary dictionaryWithObject:[o_values objectAtIndex:i] forKey:@"ITEM_URL"];
                o_array = [o_array arrayByAddingObject: o_dic];
            }
            [self appendArray: o_array atPos: i_proposed_row enqueue:YES];

            return( YES );
        }
        
        return( NO );
    }
    [self updateRowSelection];
}

@end

