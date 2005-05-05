/*****************************************************************************
 * playlist.m: MacOS X interface module
 *****************************************************************************
* Copyright (C) 2002-2005 VideoLAN
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
 * add 'icons' for different types of nodes? (http://www.cocoadev.com/index.pl?IconAndTextInTableCell)
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
#include "osd.h"
#include "misc.h"

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

    if( [[o_event characters] length] )
    {
        key = [[o_event characters] characterAtIndex: 0];
    }

    switch( key )
    {
        case NSDeleteCharacter:
        case NSDeleteFunctionKey:
        case NSDeleteCharFunctionKey:
        case NSBackspaceCharacter:
            [[self delegate] deleteItem:self];
            break;

        default:
            [super keyDown: o_event];
            break;
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
    if ( self != nil )
    {
        o_outline_dict = [[NSMutableDictionary alloc] init];
        //i_moveRow = -1;
    }
    return self;
}

- (void)awakeFromNib
{
    playlist_t * p_playlist = vlc_object_find( VLCIntf, VLC_OBJECT_PLAYLIST,
                                          FIND_ANYWHERE );
    vlc_list_t *p_list = vlc_list_find( p_playlist, VLC_OBJECT_MODULE,
                                        FIND_ANYWHERE );

    int i_index;
    i_current_view = VIEW_CATEGORY;
    playlist_ViewUpdate( p_playlist, i_current_view );

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

    o_tc_sortColumn = nil;

    for( i_index = 0; i_index < p_list->i_count; i_index++ )
    {
        NSMenuItem * o_lmi;
        module_t * p_parser = (module_t *)p_list->p_values[i_index].p_object ;

        if( !strcmp( p_parser->psz_capability, "services_discovery" ) )
        {
            /* create the menu entries used in the playlist menu */
            o_lmi = [[o_mi_services submenu] addItemWithTitle:
                     [NSString stringWithCString:
                     p_parser->psz_longname ? p_parser->psz_longname :
                     ( p_parser->psz_shortname ? p_parser->psz_shortname:
                     p_parser->psz_object_name)]
                                             action: @selector(servicesChange:)
                                             keyEquivalent: @""];
            [o_lmi setTarget: self];
            [o_lmi setRepresentedObject:
                   [NSString stringWithCString: p_parser->psz_object_name]];
            if( playlist_IsServicesDiscoveryLoaded( p_playlist,
                    p_parser->psz_object_name ) )
                [o_lmi setState: NSOnState];
                
            /* create the menu entries for the main menu */
            o_lmi = [[o_mm_mi_services submenu] addItemWithTitle:
                     [NSString stringWithCString:
                     p_parser->psz_longname ? p_parser->psz_longname :
                     ( p_parser->psz_shortname ? p_parser->psz_shortname:
                     p_parser->psz_object_name)]
                                             action: @selector(servicesChange:)
                                             keyEquivalent: @""];
            [o_lmi setTarget: self];
            [o_lmi setRepresentedObject:
                   [NSString stringWithCString: p_parser->psz_object_name]];
            if( playlist_IsServicesDiscoveryLoaded( p_playlist,
                    p_parser->psz_object_name ) )
                [o_lmi setState: NSOnState];
        }
    }
    vlc_list_release( p_list );
    vlc_object_release( p_playlist );

    [self initStrings];
    //[self playlistUpdated];
}

- (void)initStrings
{
    [o_mi_save_playlist setTitle: _NS("Save Playlist...")];
    [o_mi_play setTitle: _NS("Play")];
    [o_mi_delete setTitle: _NS("Delete")];
    [o_mi_selectall setTitle: _NS("Select All")];
    [o_mi_info setTitle: _NS("Properties")];
    [o_mi_sort_name setTitle: _NS("Sort Node by Name")];
    [o_mi_sort_author setTitle: _NS("Sort Node by Author")];
    [o_mi_services setTitle: _NS("Services discovery")];
    [[o_tc_name headerCell] setStringValue:_NS("Name")];
    [[o_tc_author headerCell] setStringValue:_NS("Author")];
    [[o_tc_duration headerCell] setStringValue:_NS("Duration")];
    [o_status_field setStringValue: [NSString stringWithFormat:
                        _NS("no items in playlist")]];

    [o_random_ckb setTitle: _NS("Random")];
#if 0
    [o_search_button setTitle: _NS("Search")];
#endif
    [[o_loop_popup itemAtIndex:0] setTitle: _NS("Standard Play")];
    [[o_loop_popup itemAtIndex:1] setTitle: _NS("Repeat One")];
    [[o_loop_popup itemAtIndex:2] setTitle: _NS("Repeat All")];
}

- (NSOutlineView *)outlineView
{
    return o_outline_view;
}

- (void)playlistUpdated
{
    unsigned int i;

    /* Clear indications of any existing column sorting*/
    for( i = 0 ; i < [[o_outline_view tableColumns] count] ; i++ )
    {
        [o_outline_view setIndicatorImage:nil inTableColumn:
                            [[o_outline_view tableColumns] objectAtIndex:i]];
    }

    [o_outline_view setHighlightedTableColumn:nil];
    o_tc_sortColumn = nil;
    // TODO Find a way to keep the dict size to a minimum
    //[o_outline_dict removeAllObjects];
    [o_outline_view reloadData];
}

- (void)playModeUpdated
{
    playlist_t *p_playlist = vlc_object_find( VLCIntf, VLC_OBJECT_PLAYLIST,
                                          FIND_ANYWHERE );
    vlc_value_t val, val2;

    if( p_playlist == NULL )
    {
        return;
    }

    var_Get( p_playlist, "loop", &val2 );
    var_Get( p_playlist, "repeat", &val );
    if( val.b_bool == VLC_TRUE )
    {
        [o_loop_popup selectItemAtIndex: 1];
   }
    else if( val2.b_bool == VLC_TRUE )
    {
        [o_loop_popup selectItemAtIndex: 2];
    }
    else
    {
        [o_loop_popup selectItemAtIndex: 0];
    }

    var_Get( p_playlist, "random", &val );
    [o_random_ckb setState: val.b_bool];

    vlc_object_release( p_playlist );
}

- (void)updateRowSelection
{
    int i,i_row;
    unsigned int j;

    playlist_t *p_playlist = vlc_object_find( VLCIntf, VLC_OBJECT_PLAYLIST,
                                          FIND_ANYWHERE );
    playlist_item_t *p_item, *p_temp_item;
    NSMutableArray *o_array = [NSMutableArray array];

    if( p_playlist == NULL )
        return;

    p_item = p_playlist->status.p_item;
    if( p_item == NULL ) return;

    p_temp_item = p_item;
    while( p_temp_item->i_parents > 0 )
    {
        [o_array insertObject: [NSValue valueWithPointer: p_temp_item] atIndex: 0];
        for (i = 0 ; i < p_temp_item->i_parents ; i++)
        {
            if( p_temp_item->pp_parents[i]->i_view == i_current_view )
            {
                p_temp_item = p_temp_item->pp_parents[i]->p_parent;
                break;
            }
        }
    }

    for (j = 0 ; j < [o_array count] - 1 ; j++)
    {
        [o_outline_view expandItem: [o_outline_dict objectForKey:
                            [NSString stringWithFormat: @"%p",
                            [[o_array objectAtIndex:j] pointerValue]]]];

    }

    i_row = [o_outline_view rowForItem:[o_outline_dict
            objectForKey:[NSString stringWithFormat: @"%p", p_item]]];

    [o_outline_view selectRow: i_row byExtendingSelection: NO];
    [o_outline_view scrollRowToVisible: i_row];

    vlc_object_release(p_playlist);
}


- (BOOL)isItem: (playlist_item_t *)p_item inNode: (playlist_item_t *)p_node
{
    playlist_t * p_playlist = vlc_object_find( VLCIntf, VLC_OBJECT_PLAYLIST,
                                          FIND_ANYWHERE );
    playlist_item_t *p_temp_item = p_item;

    if( p_playlist == NULL )
    {
        return NO;
    }

    if ( p_temp_item )
    {
        while( p_temp_item->i_parents > 0 )
        {
            int i;
            for( i = 0; i < p_temp_item->i_parents ; i++ )
            {
                if( p_temp_item->pp_parents[i]->i_view == i_current_view )
                {
                    if( p_temp_item->pp_parents[i]->p_parent == p_node )
                    {
                        vlc_object_release( p_playlist );
                        return YES;
                    }
                    else
                    {
                        p_temp_item = p_temp_item->pp_parents[i]->p_parent;
                        break;
                    }
                }
            }
        }
    }

    vlc_object_release( p_playlist );
    return NO;
}


/* When called retrieves the selected outlineview row and plays that node or item */
- (IBAction)playItem:(id)sender
{
    intf_thread_t * p_intf = VLCIntf;
    playlist_t * p_playlist = vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                                       FIND_ANYWHERE );

    if( p_playlist != NULL )
    {
        playlist_item_t *p_item;
        playlist_item_t *p_node = NULL;
        int i;

        p_item = [[o_outline_view itemAtRow:[o_outline_view selectedRow]] pointerValue];

        if( p_item )
        {
            if( p_item->i_children == -1 )
            {
                for( i = 0 ; i < p_item->i_parents ; i++ )
                {
                    if( p_item->pp_parents[i]->i_view == i_current_view )
                    {
                        p_node = p_item->pp_parents[i]->p_parent;
                    }
                }
            }
            else
            {
                p_node = p_item;
                if( p_node->i_children > 0 && p_node->pp_children[0]->i_children == -1 )
                {
                    p_item = p_node->pp_children[0];
                }
                else
                {
                    p_item = NULL;
                }
            }
            playlist_Control( p_playlist, PLAYLIST_VIEWPLAY, i_current_view, p_node, p_item );
        }
        vlc_object_release( p_playlist );
    }
}

- (IBAction)servicesChange:(id)sender
{
    NSMenuItem *o_mi = (NSMenuItem *)sender;
    NSString *o_string = [o_mi representedObject];
    playlist_t * p_playlist = vlc_object_find( VLCIntf, VLC_OBJECT_PLAYLIST,
                                          FIND_ANYWHERE );
    if( !playlist_IsServicesDiscoveryLoaded( p_playlist, [o_string cString] ) )
        playlist_ServicesDiscoveryAdd( p_playlist, [o_string cString] );
    else
        playlist_ServicesDiscoveryRemove( p_playlist, [o_string cString] );

    [o_mi setState: playlist_IsServicesDiscoveryLoaded( p_playlist,
                                          [o_string cString] ) ? YES : NO];

    i_current_view = VIEW_CATEGORY;
    playlist_ViewUpdate( p_playlist, i_current_view );
    vlc_object_release( p_playlist );
    [self playlistUpdated];
    return;
}

- (IBAction)selectAll:(id)sender
{
    [o_outline_view selectAll: nil];
}

- (IBAction)deleteItem:(id)sender
{
    int i, i_count, i_row;
    NSMutableArray *o_to_delete;
    NSNumber *o_number;

    playlist_t * p_playlist;
    intf_thread_t * p_intf = VLCIntf;

    p_playlist = vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                          FIND_ANYWHERE );

    if ( p_playlist == NULL )
    {
        return;
    }
    o_to_delete = [NSMutableArray arrayWithArray:[[o_outline_view selectedRowEnumerator] allObjects]];
    i_count = [o_to_delete count];

    for( i = 0; i < i_count; i++ )
    {
        playlist_item_t * p_item;
        o_number = [o_to_delete lastObject];
        i_row = [o_number intValue];

        [o_to_delete removeObject: o_number];
        [o_outline_view deselectRow: i_row];

        p_item = (playlist_item_t *)[[o_outline_view itemAtRow: i_row] pointerValue];

        if( p_item->i_children > -1 ) //is a node and not an item
        {
            if( p_playlist->status.i_status != PLAYLIST_STOPPED &&
                [self isItem: p_playlist->status.p_item inNode: p_item] == YES )
            {
                // if current item is in selected node and is playing then stop playlist
                playlist_Stop( p_playlist );
            }
            playlist_NodeDelete( p_playlist, p_item, VLC_TRUE, VLC_FALSE );
        }
        else
        {
            if( p_playlist->status.i_status != PLAYLIST_STOPPED &&
                p_playlist->status.p_item == [[o_outline_view itemAtRow: i_row] pointerValue] )
            {
                playlist_Stop( p_playlist );
            }
            playlist_LockDelete( p_playlist, p_item->input.i_id );
        }
    }
    [self playlistUpdated];
    vlc_object_release( p_playlist );
}

- (IBAction)sortNodeByName:(id)sender
{
    [self sortNode: SORT_TITLE];
}

- (IBAction)sortNodeByAuthor:(id)sender
{
    [self sortNode: SORT_AUTHOR];
}

- (void)sortNode:(int)i_mode
{
    playlist_t * p_playlist = vlc_object_find( VLCIntf, VLC_OBJECT_PLAYLIST,
                                          FIND_ANYWHERE );
    playlist_item_t * p_item;

    if (p_playlist == NULL)
    {
        return;
    }

    if( [o_outline_view selectedRow] > -1 )
    {
        p_item = [[o_outline_view itemAtRow: [o_outline_view selectedRow]]
                                                                pointerValue];
    }
    else
    /*If no item is selected, sort the whole playlist*/
    {
        playlist_view_t * p_view = playlist_ViewFind( p_playlist, i_current_view );
        p_item = p_view->p_root;
    }

    if( p_item->i_children > -1 ) // the item is a node
    {
        vlc_mutex_lock( &p_playlist->object_lock );
        playlist_RecursiveNodeSort( p_playlist, p_item, i_mode, ORDER_NORMAL );
        vlc_mutex_unlock( &p_playlist->object_lock );
    }
    else
    {
        int i;

        for( i = 0 ; i < p_item->i_parents ; i++ )
        {
            if( p_item->pp_parents[i]->i_view == i_current_view )
            {
                vlc_mutex_lock( &p_playlist->object_lock );
                playlist_RecursiveNodeSort( p_playlist,
                        p_item->pp_parents[i]->p_parent, i_mode, ORDER_NORMAL );
                vlc_mutex_unlock( &p_playlist->object_lock );
                break;
            }
        }
    }
    vlc_object_release( p_playlist );
    [self playlistUpdated];
}

- (playlist_item_t *)createItem:(NSDictionary *)o_one_item
{
    intf_thread_t * p_intf = VLCIntf;
    playlist_t * p_playlist = vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                                       FIND_ANYWHERE );

    if( p_playlist == NULL )
    {
        return NULL;
    }
    playlist_item_t *p_item;
    int i;
    BOOL b_rem = FALSE, b_dir = FALSE;
    NSString *o_uri, *o_name;
    NSArray *o_options;
    NSURL *o_true_file;

    /* Get the item */
    o_uri = (NSString *)[o_one_item objectForKey: @"ITEM_URL"];
    o_name = (NSString *)[o_one_item objectForKey: @"ITEM_NAME"];
    o_options = (NSArray *)[o_one_item objectForKey: @"ITEM_OPTIONS"];

    /* Find the name for a disc entry ( i know, can you believe the trouble?) */
    if( ( !o_name || [o_name isEqualToString:@""] ) && [o_uri rangeOfString: @"/dev/"].location != NSNotFound )
    {
        int i_count, i_index;
        struct statfs *mounts = NULL;

        i_count = getmntinfo (&mounts, MNT_NOWAIT);
        /* getmntinfo returns a pointer to static data. Do not free. */
        for( i_index = 0 ; i_index < i_count; i_index++ )
        {
            NSMutableString *o_temp, *o_temp2;
            o_temp = [NSMutableString stringWithString: o_uri];
            o_temp2 = [NSMutableString stringWithCString: mounts[i_index].f_mntfromname];
            [o_temp replaceOccurrencesOfString: @"/dev/rdisk" withString: @"/dev/disk" options:NULL range:NSMakeRange(0, [o_temp length]) ];
            [o_temp2 replaceOccurrencesOfString: @"s0" withString: @"" options:NULL range:NSMakeRange(0, [o_temp2 length]) ];
            [o_temp2 replaceOccurrencesOfString: @"s1" withString: @"" options:NULL range:NSMakeRange(0, [o_temp2 length]) ];

            if( strstr( [o_temp fileSystemRepresentation], [o_temp2 fileSystemRepresentation] ) != NULL )
            {
                o_name = [[NSFileManager defaultManager] displayNameAtPath: [NSString stringWithCString:mounts[i_index].f_mntonname]];
            }
        }
    }
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
        NSMutableString *o_temp;

        buf = (struct statfs *) malloc (sizeof(struct statfs));
        statfs( [o_uri fileSystemRepresentation], buf );
        psz_dev = strdup(buf->f_mntfromname);
        o_temp = [NSMutableString stringWithCString: psz_dev ];
        [o_temp replaceOccurrencesOfString: @"/dev/disk" withString: @"/dev/rdisk" options:NULL range:NSMakeRange(0, [o_temp length]) ];
        [o_temp replaceOccurrencesOfString: @"s0" withString: @"" options:NULL range:NSMakeRange(0, [o_temp length]) ];
        [o_temp replaceOccurrencesOfString: @"s1" withString: @"" options:NULL range:NSMakeRange(0, [o_temp length]) ];
        o_uri = o_temp;
    }

    p_item = playlist_ItemNew( p_intf, [o_uri fileSystemRepresentation], [o_name UTF8String] );
    if( !p_item )
       return NULL;

    if( o_options )
    {
        for( i = 0; i < (int)[o_options count]; i++ )
        {
            playlist_ItemAddOption( p_item, strdup( [[o_options objectAtIndex:i] UTF8String] ) );
        }
    }

    /* Recent documents menu */
    o_true_file = [NSURL fileURLWithPath: o_uri];
    if( o_true_file != nil )
    {
        [[NSDocumentController sharedDocumentController]
            noteNewRecentDocumentURL: o_true_file];
    }

    vlc_object_release( p_playlist );
    return p_item;
}

- (void)appendArray:(NSArray*)o_array atPos:(int)i_position enqueue:(BOOL)b_enqueue
{
    int i_item;
    playlist_t * p_playlist = vlc_object_find( VLCIntf, VLC_OBJECT_PLAYLIST,
                                            FIND_ANYWHERE );
    if( p_playlist == NULL )
    {
        return;
    }

    for( i_item = 0; i_item < (int)[o_array count]; i_item++ )
    {
        playlist_item_t *p_item;
        NSDictionary *o_one_item;

        /* Get the item */
        o_one_item = [o_array objectAtIndex: i_item];
        p_item = [self createItem: o_one_item];
        if( !p_item )
        {
            continue;
        }

        /* Add the item */
        playlist_AddItem( p_playlist, p_item, PLAYLIST_APPEND, i_position == -1 ? PLAYLIST_END : i_position + i_item );

        if( i_item == 0 && !b_enqueue )
        {
            playlist_Control( p_playlist, PLAYLIST_ITEMPLAY, p_item );
        }
    }
    vlc_object_release( p_playlist );
}

- (void)appendNodeArray:(NSArray*)o_array inNode:(playlist_item_t *)p_node atPos:(int)i_position inView:(int)i_view enqueue:(BOOL)b_enqueue
{
    int i_item;
    playlist_t * p_playlist = vlc_object_find( VLCIntf, VLC_OBJECT_PLAYLIST,
                                            FIND_ANYWHERE );
    if( p_playlist == NULL )
    {
        return;
    }

    for( i_item = 0; i_item < (int)[o_array count]; i_item++ )
    {
        playlist_item_t *p_item;
        NSDictionary *o_one_item;

        /* Get the item */
        o_one_item = [o_array objectAtIndex: i_item];
        p_item = [self createItem: o_one_item];
        if( !p_item )
        {
            continue;
        }

        /* Add the item */
        playlist_NodeAddItem( p_playlist, p_item, i_view, p_node, PLAYLIST_APPEND, i_position + i_item );

        if( i_item == 0 && !b_enqueue )
        {
            playlist_Control( p_playlist, PLAYLIST_ITEMPLAY, p_item );
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

    switch( [o_loop_popup indexOfSelectedItem] )
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
             if( val1.b_bool || val2.b_bool )
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

- (NSMutableArray *)subSearchItem:(playlist_item_t *)p_item
{
    playlist_t *p_playlist = vlc_object_find( VLCIntf, VLC_OBJECT_PLAYLIST,
                                                       FIND_ANYWHERE );
    playlist_item_t *p_selected_item;
    int i_current, i_selected_row;

    if( !p_playlist )
        return NULL;

    i_selected_row = [o_outline_view selectedRow];
    if (i_selected_row < 0)
        i_selected_row = 0;

    p_selected_item = (playlist_item_t *)[[o_outline_view itemAtRow:
                                            i_selected_row] pointerValue];

    for( i_current = 0; i_current < p_item->i_children ; i_current++ )
    {
        char *psz_temp;
        NSString *o_current_name, *o_current_author;

        vlc_mutex_lock( &p_playlist->object_lock );
        o_current_name = [NSString stringWithUTF8String:
            p_item->pp_children[i_current]->input.psz_name];
        psz_temp = vlc_input_item_GetInfo( &p_item->input ,
				   _("Meta-information"),_("Artist") );
        o_current_author = [NSString stringWithUTF8String: psz_temp];
        free( psz_temp);
        vlc_mutex_unlock( &p_playlist->object_lock );

        if( p_selected_item == p_item->pp_children[i_current] &&
                    b_selected_item_met == NO )
        {
            b_selected_item_met = YES;
        }
        else if( p_selected_item == p_item->pp_children[i_current] &&
                    b_selected_item_met == YES )
        {
            vlc_object_release( p_playlist );
            return NULL;
        }
        else if( b_selected_item_met == YES &&
                    ( [o_current_name rangeOfString:[o_search_field
                        stringValue] options:NSCaseInsensitiveSearch ].length ||
                      [o_current_author rangeOfString:[o_search_field
                        stringValue] options:NSCaseInsensitiveSearch ].length ) )
        {
            vlc_object_release( p_playlist );
            /*Adds the parent items in the result array as well, so that we can
            expand the tree*/
            return [NSMutableArray arrayWithObject: [NSValue
                            valueWithPointer: p_item->pp_children[i_current]]];
        }
        if( p_item->pp_children[i_current]->i_children > 0 )
        {
            id o_result = [self subSearchItem:
                                            p_item->pp_children[i_current]];
            if( o_result != NULL )
            {
                vlc_object_release( p_playlist );
                [o_result insertObject: [NSValue valueWithPointer:
                                p_item->pp_children[i_current]] atIndex:0];
                return o_result;
            }
        }
    }
    vlc_object_release( p_playlist );
    return NULL;
}

- (IBAction)searchItem:(id)sender
{
    playlist_t * p_playlist = vlc_object_find( VLCIntf, VLC_OBJECT_PLAYLIST,
                                                       FIND_ANYWHERE );
    playlist_view_t * p_view;
    id o_result;

    unsigned int i;
    int i_row = -1;

    b_selected_item_met = NO;

    if( p_playlist == NULL )
        return;

    p_view = playlist_ViewFind( p_playlist, i_current_view );

    if( p_view )
    {
        /*First, only search after the selected item:*
         *(b_selected_item_met = NO)                 */
        o_result = [self subSearchItem:p_view->p_root];
        if( o_result == NULL )
        {
            /* If the first search failed, search again from the beginning */
            o_result = [self subSearchItem:p_view->p_root];
        }
        if( o_result != NULL )
        {
            for( i = 0 ; i < [o_result count] - 1 ; i++ )
            {
                [o_outline_view expandItem: [o_outline_dict objectForKey:
                            [NSString stringWithFormat: @"%p",
                            [[o_result objectAtIndex: i] pointerValue]]]];
            }
            i_row = [o_outline_view rowForItem: [o_outline_dict objectForKey:
                            [NSString stringWithFormat: @"%p",
                            [[o_result objectAtIndex: [o_result count] - 1 ]
                            pointerValue]]]];
        }
        if( i_row > -1 )
        {
            [o_outline_view selectRow:i_row byExtendingSelection: NO];
            [o_outline_view scrollRowToVisible: i_row];
        }
    }
    vlc_object_release( p_playlist );
}

- (NSMenu *)menuForEvent:(NSEvent *)o_event
{
    NSPoint pt;
    vlc_bool_t b_rows;
    vlc_bool_t b_item_sel;

    pt = [o_outline_view convertPoint: [o_event locationInWindow]
                                                 fromView: nil];
    b_item_sel = ( [o_outline_view rowAtPoint: pt] != -1 &&
                   [o_outline_view selectedRow] != -1 );
    b_rows = [o_outline_view numberOfRows] != 0;

    [o_mi_play setEnabled: b_item_sel];
    [o_mi_delete setEnabled: b_item_sel];
    [o_mi_selectall setEnabled: b_rows];
    [o_mi_info setEnabled: b_item_sel];

    return( o_ctx_menu );
}

- (playlist_item_t *)selectedPlaylistItem
{
    return [[o_outline_view itemAtRow: [o_outline_view selectedRow]]
                                                                pointerValue];
}

- (void)outlineView: (NSTableView*)o_tv
                  didClickTableColumn:(NSTableColumn *)o_tc
{
    int i_mode = 0, i_type;
    intf_thread_t *p_intf = VLCIntf;
    playlist_view_t *p_view;

    playlist_t *p_playlist = (playlist_t *)vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                       FIND_ANYWHERE );
    if( p_playlist == NULL )
    {
        return;
    }
    
    /* Check whether the selected table column header corresponds to a
       sortable table column*/
    if( !( o_tc == o_tc_name || o_tc == o_tc_author ) )
    {
        vlc_object_release( p_playlist );
        return;
    }

    p_view = playlist_ViewFind( p_playlist, i_current_view );

    if( o_tc_sortColumn == o_tc )
    {
        b_isSortDescending = !b_isSortDescending;
    }
    else
    {
        b_isSortDescending = VLC_FALSE;
    }

    if( o_tc == o_tc_name )
    {
        i_mode = SORT_TITLE;
    }
    else if( o_tc == o_tc_author )
    {
        i_mode = SORT_AUTHOR;
    }

    if( b_isSortDescending )
    {
        i_type = ORDER_REVERSE;
    }
    else
    {
        i_type = ORDER_NORMAL;
    }

    vlc_mutex_lock( &p_playlist->object_lock );
    playlist_RecursiveNodeSort( p_playlist, p_view->p_root, i_mode, i_type );
    vlc_mutex_unlock( &p_playlist->object_lock );

    vlc_object_release( p_playlist );
    [self playlistUpdated];

    o_tc_sortColumn = o_tc;
    [o_outline_view setHighlightedTableColumn:o_tc];

    if( b_isSortDescending )
    {
        [o_outline_view setIndicatorImage:o_descendingSortingImage
                                                        inTableColumn:o_tc];
    }
    else
    {
        [o_outline_view setIndicatorImage:o_ascendingSortingImage
                                                        inTableColumn:o_tc];
    }
}


- (void)outlineView:(NSOutlineView *)outlineView
                                willDisplayCell:(id)cell
                                forTableColumn:(NSTableColumn *)tableColumn
                                item:(id)item
{
    playlist_t *p_playlist = vlc_object_find( VLCIntf, VLC_OBJECT_PLAYLIST,
                                          FIND_ANYWHERE );
    playlist_item_t *p_item = (playlist_item_t *)[item pointerValue];

    if( !p_playlist ) return;

    if( ( p_item == p_playlist->status.p_item ) ||
            ( p_item->i_children != 0 &&
            [self isItem: p_playlist->status.p_item inNode: p_item] ) )
    {
        [cell setFont: [NSFont boldSystemFontOfSize: 0]];
    }
    else
    {
        [cell setFont: [NSFont systemFontOfSize: 0]];
    }
    vlc_object_release( p_playlist );
}

@end

@implementation VLCPlaylist (NSOutlineViewDataSource)

/* return the number of children for Obj-C pointer item */ /* DONE */
- (int)outlineView:(NSOutlineView *)outlineView numberOfChildrenOfItem:(id)item
{
    int i_return = 0;
    playlist_t * p_playlist = vlc_object_find( VLCIntf, VLC_OBJECT_PLAYLIST,
                                                       FIND_ANYWHERE );
    if( p_playlist == NULL || outlineView != o_outline_view )
        return 0;

    if( item == nil )
    {
        /* root object */
        playlist_view_t *p_view;
        p_view = playlist_ViewFind( p_playlist, i_current_view );
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
    
    if( i_return <= 0 )
        i_return = 0;
    
    return i_return;
}

/* return the child at index for the Obj-C pointer item */ /* DONE */
- (id)outlineView:(NSOutlineView *)outlineView child:(int)index ofItem:(id)item
{
    playlist_item_t *p_return = NULL;
    playlist_t * p_playlist = vlc_object_find( VLCIntf, VLC_OBJECT_PLAYLIST,
                                                       FIND_ANYWHERE );
    NSValue *o_value;

    if( p_playlist == NULL )
        return nil;

    if( item == nil )
    {
        /* root object */
        playlist_view_t *p_view;
        p_view = playlist_ViewFind( p_playlist, i_current_view );
        if( p_view && index < p_view->p_root->i_children && index >= 0 )
            p_return = p_view->p_root->pp_children[index];
    }
    else
    {
        playlist_item_t *p_item = (playlist_item_t *)[item pointerValue];
        if( p_item && index < p_item->i_children && index >= 0 )
            p_return = p_item->pp_children[index];
    }
    
    if( p_playlist->i_size >= 2 )
    {
        [o_status_field setStringValue: [NSString stringWithFormat:
                    _NS("%i items in playlist"), p_playlist->i_size]];
    }
    else
    {
        if( p_playlist->i_size == 0 )
        {
            [o_status_field setStringValue: [NSString stringWithFormat:
                    _NS("no items in playlist"), p_playlist->i_size]];
        }
        else
        {
            [o_status_field setStringValue: [NSString stringWithFormat:
                    _NS("1 item in playlist"), p_playlist->i_size]];
        }
    }

    vlc_object_release( p_playlist );

    o_value = [[NSValue valueWithPointer: p_return] retain];

    [o_outline_dict setObject:o_value forKey:[NSString stringWithFormat:@"%p", p_return]];
    return o_value;
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
        p_view = playlist_ViewFind( p_playlist, i_current_view );
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

    if( i_return <= 0 )
        return NO;
    else
        return YES;
}

/* retrieve the string values for the cells */
- (id)outlineView:(NSOutlineView *)outlineView objectValueForTableColumn:(NSTableColumn *)o_tc byItem:(id)item
{
    id o_value = nil;
    intf_thread_t *p_intf = VLCIntf;
    playlist_t *p_playlist;
    playlist_item_t *p_item;
    
    if( item == nil || ![item isKindOfClass: [NSValue class]] ) return( @"error" );
    
    p_playlist = vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                               FIND_ANYWHERE );
    if( p_playlist == NULL )
    {
        return( @"error" );
    }

    p_item = (playlist_item_t *)[item pointerValue];

    if( p_item == NULL )
    {
        vlc_object_release( p_playlist );
        return( @"error");
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
        psz_temp = vlc_input_item_GetInfo( &p_item->input ,_("Meta-information"),_("Artist") );

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
/*    unsigned int i;

    for( i = 0 ; i < [items count] ; i++ )
    {
        if( [outlineView levelForItem: [items objectAtIndex: i]] == 0 )
        {
            return NO;
        }
    }*/
    return NO;
}

- (NSDragOperation)outlineView:(NSOutlineView *)outlineView validateDrop:(id <NSDraggingInfo>)info proposedItem:(id)item proposedChildIndex:(int)index
{
    NSPasteboard *o_pasteboard = [info draggingPasteboard];

    if( [[o_pasteboard types] containsObject: NSFilenamesPboardType] )
    {
        return NSDragOperationGeneric;
    }
    return NSDragOperationNone;
}

- (BOOL)outlineView:(NSOutlineView *)outlineView acceptDrop:(id <NSDraggingInfo>)info item:(id)item childIndex:(int)index
{
    playlist_t * p_playlist =  vlc_object_find( VLCIntf, VLC_OBJECT_PLAYLIST,
                                                       FIND_ANYWHERE );
    NSPasteboard *o_pasteboard = [info draggingPasteboard];

    if( !p_playlist ) return NO;

    if( [[o_pasteboard types] containsObject: NSFilenamesPboardType] )
    {
        int i;
        playlist_item_t *p_node = [item pointerValue];

        NSArray *o_array = [NSArray array];
        NSArray *o_values = [[o_pasteboard propertyListForType:
                                        NSFilenamesPboardType]
                                sortedArrayUsingSelector:
                                        @selector(caseInsensitiveCompare:)];

        for( i = 0; i < (int)[o_values count]; i++)
        {
            NSDictionary *o_dic;
            o_dic = [NSDictionary dictionaryWithObject:[o_values
                        objectAtIndex:i] forKey:@"ITEM_URL"];
            o_array = [o_array arrayByAddingObject: o_dic];
        }

        if ( item == nil )
        {
            [self appendArray: o_array atPos: index enqueue: YES];
        }
        else if( p_node->i_children == -1 )
        {
            int i_counter;
            playlist_item_t *p_real_node = NULL;

            for( i_counter = 0 ; i_counter < p_node->i_parents ; i_counter++ )
            {
                if( p_node->pp_parents[i_counter]->i_view == i_current_view )
                {
                    p_real_node = p_node->pp_parents[i_counter]->p_parent;
                    break;
                }
                if( i_counter == p_node->i_parents )
                {
                    return NO;
                }
            }
            [self appendNodeArray: o_array inNode: p_real_node
                atPos: index inView: i_current_view enqueue: YES];
        }
        else
        {
            [self appendNodeArray: o_array inNode: p_node
                atPos: index inView: i_current_view enqueue: YES];
        }
        vlc_object_release( p_playlist );
        return YES;
    }
    vlc_object_release( p_playlist );
    return NO;
}

/* Delegate method of NSWindow */
/*- (void)windowWillClose:(NSNotification *)aNotification
{
    [o_btn_playlist setState: NSOffState];
}
*/
@end


