/*****************************************************************************
 * playlist.m: MacOS X interface plugin
 *****************************************************************************
 * Copyright (C) 2002 VideoLAN
 * $Id: playlist.m,v 1.2 2002/09/23 23:05:58 massiot Exp $
 *
 * Authors: Jon Lech Johansen <jon-vl@nanocrew.net>
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

#include <vlc/vlc.h>
#include <vlc/intf.h>

#include <Cocoa/Cocoa.h> 

#include "intf.h"
#include "playlist.h"

/*****************************************************************************
 * VLCPlaylistView implementation 
 *****************************************************************************/
@implementation VLCPlaylistView

- (NSMenu *)menuForEvent:(NSEvent *)o_event
{
    /* TODO */

    return( nil );
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

    [o_table_view setDoubleAction: @selector(doubleClick:)];

    [o_table_view registerForDraggedTypes: 
        [NSArray arrayWithObjects: NSFilenamesPboardType, nil]];

    [o_panel setTitle: _NS("Playlist")];
    [o_btn_close setTitle: _NS("Close")];
}

- (BOOL)tableView:(NSTableView *)o_tv 
                  shouldEditTableColumn:(NSTableColumn *)o_tc
                  row:(int)i_row
{
    return( NO );
}

- (NSDragOperation)tableView:(NSTableView*)o_tv 
                   validateDrop:(id <NSDraggingInfo>)info 
                   proposedRow:(int)i_row 
                   proposedDropOperation:(NSTableViewDropOperation)operation
{
    return( NSDragOperationPrivate );
}

- (BOOL)tableView:(NSTableView*)o_tv 
                  acceptDrop:(id <NSDraggingInfo>)info 
                  row:(int)i_row 
                  dropOperation:(NSTableViewDropOperation)operation
{
    NSArray * o_values;
    NSPasteboard * o_pasteboard;

    o_pasteboard = [info draggingPasteboard];

    if( [[o_pasteboard types] containsObject: NSFilenamesPboardType] )
    {
        o_values = [o_pasteboard propertyListForType: NSFilenamesPboardType];

        [self appendArray: o_values atPos: i_row];

        if( i_row != -1 )
        {
            [o_table_view reloadData];
        }
        
        return( YES );
    }

    return( NO ); 
}

- (void)tableView:(NSTableView *)o_tv willDisplayCell:(id)o_cell
                  forTableColumn:(NSTableColumn *)o_tc row:(int)i_row
{
    [o_cell setDrawsBackground: YES];

    if( i_row % 2 )
    {
        [o_cell setBackgroundColor: 
            [NSColor colorWithDeviceRed: 0.937255 
                                  green: 0.968627
                                   blue: 1.0
                                  alpha: 1.0]];
    }
    else
    {
        [o_cell setBackgroundColor: [NSColor whiteColor]];
    }
}

- (IBAction)doubleClick:(id)sender
{
    NSTableView * o_tv = sender;
    intf_thread_t * p_intf = [NSApp getIntf];
    playlist_t * p_playlist = vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                                       FIND_ANYWHERE );

    if( p_playlist == NULL )
    {
        return;
    }

    playlist_Goto( p_playlist, [o_tv clickedRow] );

    vlc_object_release( p_playlist );
}

- (void)appendArray:(NSArray*)o_array atPos:(int)i_pos
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

    if( p_intf->p_sys->b_loop )
    {
        playlist_Delete( p_playlist, p_playlist->i_size - 1 );
    }

    i_items = 0;
    o_enum = [o_array objectEnumerator];
    while( ( o_value = [o_enum nextObject] ) )
    {
        NSURL * o_url;

        int i_mode = i_items == 0 ? PLAYLIST_INSERT | PLAYLIST_GO :
                                                   PLAYLIST_INSERT;

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

    if( p_intf->p_sys->b_loop )
    {
        playlist_Add( p_playlist, "vlc:loop",
                      PLAYLIST_APPEND, PLAYLIST_END );
    }

    vlc_object_release( p_playlist );
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
        i_count = p_playlist->i_size;
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
    o_value = [NSString stringWithUTF8String: 
        p_playlist->pp_items[i_row]->psz_name]; 
    vlc_mutex_unlock( &p_playlist->object_lock ); 

    vlc_object_release( p_playlist );

    return( o_value );
}

@end

