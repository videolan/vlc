/*****************************************************************************
 * playlist.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2002-2004 VideoLAN
 * $Id$
 *
 * Authors: Jon Lech Johansen <jon-vl@nanocrew.net>
 *          Derk-Jan Hartman <hartman at videolan dot org>
 *          Benjamin Pracht <bigben at videolab dot org>
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

/* TODO
 * connect delegates, actions and outlets in IB
 * implement delete by backspace
 * implement playlist item rightclick menu
 * implement sorting
 * add 'icons' for different types of nodes? (http://www.cocoadev.com/index.pl?IconAndTextInTableCell)
 * create a new 'playlist toggle' that hides the playlist and in effect give you the old controller
 * create a new search field build with pictures from the 'regular' search field, so it can be emulated on 10.2
 * create toggle buttons for the shuffle, repeat one, repeat all functions.
 * implement drag and drop and item reordering.
 * reimplement enable/disable item
 * create a new 'tool' button (see the gear button in the Finder window) for 'actions'
   (adding service discovery, other views, new node/playlist, save node/playlist) stuff like that
 */



/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <stdlib.h>                                      /* malloc(), free() */
#include <sys/param.h>                                    /* for MAXPATHLEN */
#include <string.h>
#include <math.h>
#include <sys/mount.h>
#include <vlc_keys.h>

#include "intf.h"
#include "playlist.h"
#include "controls.h"
#include <OSD.h>

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
    int i, c, i_row;
    NSMutableArray *o_to_delete;
    NSNumber *o_number;

    playlist_t * p_playlist;
    intf_thread_t * p_intf = VLCIntf;
msg_Dbg( p_intf, "KEYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYY");
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
        case NSDeleteCharacter:
        case NSDeleteFunctionKey:
        case NSDeleteCharFunctionKey:
        case NSBackspaceCharacter:
            o_to_delete = [NSMutableArray arrayWithArray:[[self selectedRowEnumerator] allObjects]];
            c = [o_to_delete count];

            for( i = 0; i < c; i++ ) {
                o_number = [o_to_delete lastObject];
                i_row = [o_number intValue];

                if( p_playlist->status.p_item == [[self itemAtRow: i_row] pointerValue] && p_playlist->status.i_status )
                {
                    playlist_Stop( p_playlist );
                }
                [o_to_delete removeObject: o_number];
                [self deselectRow: i_row];
                playlist_ItemDelete( [[self itemAtRow: i_row] pointerValue] );
            }
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
        //i_moveRow = -1;
    }
    return self;
}

- (void)awakeFromNib
{
    [o_outline_view setTarget: self];
    [o_outline_view setDelegate: self];
    [o_outline_view setDataSource: self];

    [o_outline_view setDoubleAction: @selector(playItem:)];

    [o_outline_view registerForDraggedTypes: 
        [NSArray arrayWithObjects: NSFilenamesPboardType, nil]];
    [o_outline_view setIntercellSpacing: NSMakeSize (0.0, 1.0)];

/* We need to check whether _defaultTableHeaderSortImage exists, since it 
belongs to an Apple hidden private API, and then can "disapear" at any time*/

    if( [[NSOutlineView class] respondsToSelector:@selector(_defaultTableHeaderSortImage)] )
    {
        o_ascendingSortingImage = [[NSOutlineView class] _defaultTableHeaderSortImage];
    }
    else
    {
        o_ascendingSortingImage = nil;
    }

    if( [[NSOutlineView class] respondsToSelector:@selector(_defaultTableHeaderReverseSortImage)] )
    {
        o_descendingSortingImage = [[NSOutlineView class] _defaultTableHeaderReverseSortImage];
    }
    else
    {
        o_descendingSortingImage = nil;
    }

    [self initStrings];
    //[self playlistUpdated];
}

- (void)initStrings
{
    [o_mi_save_playlist setTitle: _NS("Save Playlist...")];
#if 0
    [o_mi_play setTitle: _NS("Play")];
    [o_mi_delete setTitle: _NS("Delete")];
    [o_mi_selectall setTitle: _NS("Select All")];
    [o_mi_toggleItemsEnabled setTitle: _NS("Item Enabled")];
    [o_mi_enableGroup setTitle: _NS("Enable all group items")];
    [o_mi_disableGroup setTitle: _NS("Disable all group items")];
    [o_mi_info setTitle: _NS("Properties")];
#endif
    [[o_tc_name headerCell] setStringValue:_NS("Name")];
    [[o_tc_author headerCell] setStringValue:_NS("Author")];
    [[o_tc_duration headerCell] setStringValue:_NS("Duration")];
    [o_random_ckb setTitle: _NS("Random")];
#if 0
    [o_search_button setTitle: _NS("Search")];
#endif
    [o_btn_playlist setToolTip: _NS("Playlist")];
    [[o_loop_popup itemAtIndex:0] setTitle: _NS("Standard Play")];
    [[o_loop_popup itemAtIndex:1] setTitle: _NS("Repeat One")];
    [[o_loop_popup itemAtIndex:2] setTitle: _NS("Repeat All")];
}

- (void)playlistUpdated
{
    [o_outline_view reloadData];
}

- (IBAction)playItem:(id)sender
{
    intf_thread_t * p_intf = VLCIntf;
    playlist_t * p_playlist = vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                                       FIND_ANYWHERE );

    if( p_playlist != NULL )
    {
        playlist_item_t *p_item;
        playlist_view_t *p_view;
        p_view = playlist_ViewFind( p_playlist, VIEW_SIMPLE );
        p_item = [[o_outline_view itemAtRow:[o_outline_view selectedRow]] pointerValue];
        
        if( p_item )
            playlist_Control( p_playlist, PLAYLIST_VIEWPLAY, VIEW_SIMPLE, p_view ? p_view->p_root : NULL, p_item );
        vlc_object_release( p_playlist );
    }
}

- (void)appendArray:(NSArray*)o_array atPos:(int)i_position enqueue:(BOOL)b_enqueue
{
    int i_item;
    intf_thread_t * p_intf = VLCIntf;
    playlist_t * p_playlist = vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                                       FIND_ANYWHERE );

    if( p_playlist == NULL )
    {
        return;
    }

    for ( i_item = 0; i_item < (int)[o_array count]; i_item++ )
    {
        /* One item */
        NSDictionary *o_one_item;
        int j, i_total_options = 0, i_new_id = -1;
        int i_mode = PLAYLIST_INSERT;
        BOOL b_rem = FALSE, b_dir = FALSE;
        NSString *o_uri, *o_name;
        NSArray *o_options;
        NSURL *o_true_file;
        char **ppsz_options = NULL;

        /* Get the item */
        o_one_item = [o_array objectAtIndex: i_item];
        o_uri = (NSString *)[o_one_item objectForKey: @"ITEM_URL"];
        o_name = (NSString *)[o_one_item objectForKey: @"ITEM_NAME"];
        o_options = (NSArray *)[o_one_item objectForKey: @"ITEM_OPTIONS"];

        /* If no name, then make a guess */
        if( !o_name) o_name = [[NSFileManager defaultManager] displayNameAtPath: o_uri];

        if( [[NSFileManager defaultManager] fileExistsAtPath:o_uri isDirectory:&b_dir] && b_dir &&
            [[NSWorkspace sharedWorkspace] getFileSystemInfoForPath: o_uri isRemovable: &b_rem
                    isWritable:NULL isUnmountable:NULL description:NULL type:NULL] && b_rem   )
        {
            /* All of this is to make sure CD's play when you D&D them on VLC */
            /* Converts mountpoint to a /dev file */
            struct statfs *buf;
            char *psz_dev;
            buf = (struct statfs *) malloc (sizeof(struct statfs));
            statfs( [o_uri fileSystemRepresentation], buf );
            psz_dev = strdup(buf->f_mntfromname);
            o_uri = [NSString stringWithCString: psz_dev ];
        }

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

        /* Add the item */
        i_new_id = playlist_AddExt( p_playlist, [o_uri fileSystemRepresentation],
                      [o_name UTF8String], i_mode,
                      i_position == -1 ? PLAYLIST_END : i_position + i_item,
                      0, (ppsz_options != NULL ) ? (const char **)ppsz_options : 0, i_total_options );

        /* clean up
        for( j = 0; j < i_total_options; j++ )
            free( ppsz_options[j] );
        if( ppsz_options ) free( ppsz_options ); */

        /* Recent documents menu */
        o_true_file = [NSURL fileURLWithPath: o_uri];
        if( o_true_file != nil )
        {
            [[NSDocumentController sharedDocumentController]
                noteNewRecentDocumentURL: o_true_file];
        }

        if( i_item == 0 && !b_enqueue )
        {
            playlist_Goto( p_playlist, playlist_GetPositionById( p_playlist, i_new_id ) );
            playlist_Play( p_playlist );
        }
    }

    vlc_object_release( p_playlist );
}

- (IBAction)handlePopUp:(id)sender

{
             intf_thread_t * p_intf = VLCIntf;
             vlc_value_t val1,val2;
             playlist_t * p_playlist = vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                                        FIND_ANYWHERE );
             if( p_playlist == NULL )
             {
                 return;
             }

    switch ([o_loop_popup indexOfSelectedItem])
    {
        case 1:

             val1.b_bool = 0;
             var_Set( p_playlist, "loop", val1 );
             val1.b_bool = 1;
             var_Set( p_playlist, "repeat", val1 );
             vout_OSDMessage( p_intf, DEFAULT_CHAN, _( "Repeat One" ) );
        break;

        case 2:
             val1.b_bool = 0;
             var_Set( p_playlist, "repeat", val1 );
             val1.b_bool = 1;
             var_Set( p_playlist, "loop", val1 );
             vout_OSDMessage( p_intf, DEFAULT_CHAN, _( "Repeat All" ) );
        break;

        default:
             var_Get( p_playlist, "repeat", &val1 );
             var_Get( p_playlist, "loop", &val2 );
             if (val1.b_bool || val2.b_bool)
             {
                  val1.b_bool = 0;
                  var_Set( p_playlist, "repeat", val1 );
                  var_Set( p_playlist, "loop", val1 );
                  vout_OSDMessage( p_intf, DEFAULT_CHAN, _( "Repeat Off" ) );
             }
         break;
     }
     vlc_object_release( p_playlist );
     [self playlistUpdated];
}

@end

@implementation VLCPlaylist (NSOutlineViewDataSource)

/* return the number of children for Obj-C pointer item */ /* DONE */
- (int)outlineView:(NSOutlineView *)outlineView numberOfChildrenOfItem:(id)item
{
    int i_return = 0;
    playlist_t * p_playlist = vlc_object_find( VLCIntf, VLC_OBJECT_PLAYLIST,
                                                       FIND_ANYWHERE );
    if( p_playlist == NULL )
        return 0;

    if( item == nil )
    {
        /* root object */
        playlist_view_t *p_view;
        p_view = playlist_ViewFind( p_playlist, VIEW_SIMPLE );
        if( p_view && p_view->p_root )
            i_return = p_view->p_root->i_children;
    }
    else
    {
        playlist_item_t *p_item = (playlist_item_t *)[item pointerValue];
        if( p_item )
            i_return = p_item->i_children;
    }
    vlc_object_release( p_playlist );
    if( i_return == -1 ) i_return = 0;
    msg_Dbg( p_playlist, "I have %d children", i_return );
    [o_status_field setStringValue: [NSString stringWithFormat:
                                _NS("%i items in playlist"), i_return]];
    return i_return;
}

/* return the child at index for the Obj-C pointer item */ /* DONE */
- (id)outlineView:(NSOutlineView *)outlineView child:(int)index ofItem:(id)item
{
    playlist_item_t *p_return = NULL;
    playlist_t * p_playlist = vlc_object_find( VLCIntf, VLC_OBJECT_PLAYLIST,
                                                       FIND_ANYWHERE );
    if( p_playlist == NULL )
        return nil;

    if( item == nil )
    {
        /* root object */
        playlist_view_t *p_view;
        p_view = playlist_ViewFind( p_playlist, VIEW_SIMPLE );
        if( p_view && index < p_view->p_root->i_children )
            p_return = p_view->p_root->pp_children[index];
    }
    else
    {
        playlist_item_t *p_item = (playlist_item_t *)[item pointerValue];
        if( p_item && index < p_item->i_children )
        {
            p_return = p_item->pp_children[index];
        }
    }

    vlc_object_release( p_playlist );
    msg_Dbg( p_playlist, "childitem with index %d", index );
    return [[NSValue valueWithPointer: p_return] retain];
}

/* is the item expandable */
- (BOOL)outlineView:(NSOutlineView *)outlineView isItemExpandable:(id)item
{
    int i_return = 0;
    playlist_t * p_playlist = vlc_object_find( VLCIntf, VLC_OBJECT_PLAYLIST,
                                                       FIND_ANYWHERE );
    if( p_playlist == NULL )
        return NO;

    if( item == nil )
    {
        /* root object */
        playlist_view_t *p_view;
        p_view = playlist_ViewFind( p_playlist, VIEW_SIMPLE );
        if( p_view && p_view->p_root )
            i_return = p_view->p_root->i_children;
    }
    else
    {
        playlist_item_t *p_item = (playlist_item_t *)[item pointerValue];
        if( p_item )
            i_return = p_item->i_children;
    }
    vlc_object_release( p_playlist );

    if( i_return == -1 || i_return == 0 )
        return NO;
    else
        return YES;
}

/* retrieve the string values for the cells */
- (id)outlineView:(NSOutlineView *)outlineView objectValueForTableColumn:(NSTableColumn *)o_tc byItem:(id)item
{
    id o_value = nil;
    intf_thread_t * p_intf = VLCIntf;
    playlist_t * p_playlist = vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                               FIND_ANYWHERE );
    playlist_item_t *p_item = (playlist_item_t *)[item pointerValue];

    if( p_playlist == NULL || p_item == NULL )
    {
        return( @"error" );
    }

    if( [[o_tc identifier] isEqualToString:@"1"] )
    {
        o_value = [NSString stringWithUTF8String:
            p_item->input.psz_name];
        if( o_value == NULL )
            o_value = [NSString stringWithCString:
                p_item->input.psz_name];
    }
    else if( [[o_tc identifier] isEqualToString:@"2"] )
    {
        char *psz_temp;
        psz_temp = playlist_ItemGetInfo( p_item ,_("Meta-information"),_("Artist") );

        if( psz_temp == NULL )
            o_value = @"";
        else
        {
            o_value = [NSString stringWithUTF8String: psz_temp];
            if( o_value == NULL )
            {
                o_value = [NSString stringWithCString: psz_temp];
            }
            free( psz_temp );
        }
    }
    else if( [[o_tc identifier] isEqualToString:@"3"] )
    {
        char psz_duration[MSTRTIME_MAX_SIZE];
        mtime_t dur = p_item->input.i_duration;
        if( dur != -1 )
        {
            secstotimestr( psz_duration, dur/1000000 );
            o_value = [NSString stringWithUTF8String: psz_duration];
        }
        else
        {
            o_value = @"-:--:--";
        }
    }

    vlc_object_release( p_playlist );

    return( o_value );
}

/* Required for drag & drop and reordering */
- (BOOL)outlineView:(NSOutlineView *)outlineView writeItems:(NSArray *)items toPasteboard:(NSPasteboard *)pboard
{
    return NO;
}

- (NSDragOperation)outlineView:(NSOutlineView *)outlineView validateDrop:(id <NSDraggingInfo>)info proposedItem:(id)item proposedChildIndex:(int)index
{
    return NSDragOperationNone;
}

/* Delegate method of NSWindow */
- (void)windowWillClose:(NSNotification *)aNotification
{
    [o_btn_playlist setState: NSOffState];
}

@end


