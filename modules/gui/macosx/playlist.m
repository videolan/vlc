/*****************************************************************************
 * playlist.m: MacOS X interface module
 *****************************************************************************
* Copyright (C) 2002-2012 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Jon Lech Johansen <jon-vl@nanocrew.net>
 *          Derk-Jan Hartman <hartman at videola/n dot org>
 *          Benjamin Pracht <bigben at videolab dot org>
 *          Felix Paul Kühne <fkuehne at videolan dot org>
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

/* TODO
 * add 'icons' for different types of nodes? (http://www.cocoadev.com/index.pl?IconAndTextInTableCell)
 * reimplement enable/disable item
 */


/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <stdlib.h>                                      /* malloc(), free() */
#include <sys/param.h>                                    /* for MAXPATHLEN */
#include <string.h>
#include <math.h>
#include <sys/mount.h>

#import "intf.h"
#import "wizard.h"
#import "bookmarks.h"
#import "playlistinfo.h"
#import "playlist.h"
#import "controls.h"
#import "misc.h"
#import "open.h"

#include <vlc_keys.h>
#import <vlc_osd.h>
#import <vlc_interface.h>

#include <vlc_url.h>


/*****************************************************************************
 * VLCPlaylistView implementation
 *****************************************************************************/
@implementation VLCPlaylistView

- (NSMenu *)menuForEvent:(NSEvent *)o_event
{
    return( [(VLCPlaylist *)[self delegate] menuForEvent: o_event] );
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
            [(VLCPlaylist *)[self delegate] deleteItem:self];
            break;

        case NSEnterCharacter:
        case NSCarriageReturnCharacter:
            [(VLCPlaylist *)[[VLCMain sharedInstance] playlist] playItem:nil];
            break;

        default:
            [super keyDown: o_event];
            break;
    }
}

- (BOOL)validateMenuItem:(NSMenuItem *)item
{
    if (([self numberOfSelectedRows] >= 1 && [item action] == @selector(delete:)) || [item action] == @selector(selectAll:))
        return YES;

    return NO;
}

- (BOOL) acceptsFirstResponder
{
    return YES;
}

- (BOOL) becomeFirstResponder
{
    [self setNeedsDisplay:YES];
    return YES;
}

- (BOOL) resignFirstResponder
{
    [self setNeedsDisplay:YES];
    return YES;
}

- (IBAction)delete:(id)sender
{
    [[[VLCMain sharedInstance] playlist] deleteItem: sender];
}

@end

/*****************************************************************************
 * VLCPlaylistCommon implementation
 *
 * This class the superclass of the VLCPlaylist and VLCPlaylistWizard.
 * It contains the common methods and elements of these 2 entities.
 *****************************************************************************/
@implementation VLCPlaylistCommon

- (id)init
{
    playlist_t * p_playlist = pl_Get( VLCIntf );
    p_current_root_item = p_playlist->p_local_category;

    self = [super init];
    if ( self != nil )
    {
        o_outline_dict = [[NSMutableDictionary alloc] init];
    }
    return self;
}

- (void)awakeFromNib
{
    playlist_t * p_playlist = pl_Get( VLCIntf );
    [o_outline_view setTarget: self];
    [o_outline_view setDelegate: self];
    [o_outline_view setDataSource: self];
    [o_outline_view setAllowsEmptySelection: NO];
    [o_outline_view expandItem: [o_outline_view itemAtRow:0]];

    [o_outline_view_other setTarget: self];
    [o_outline_view_other setDelegate: self];
    [o_outline_view_other setDataSource: self];
    [o_outline_view_other setAllowsEmptySelection: NO];

    [self initStrings];
}

- (void)initStrings
{
    [[o_tc_name headerCell] setStringValue:_NS("Name")];
    [[o_tc_author headerCell] setStringValue:_NS("Author")];
    [[o_tc_duration headerCell] setStringValue:_NS("Duration")];

    [[o_tc_name_other headerCell] setStringValue:_NS("Name")];
    [[o_tc_author_other headerCell] setStringValue:_NS("Author")];
    [[o_tc_duration_other headerCell] setStringValue:_NS("Duration")];
}

- (void)setPlaylistRoot: (playlist_item_t *)root_item
{
    p_current_root_item = root_item;
    [o_outline_view reloadData];
    [o_outline_view_other reloadData];
}

- (playlist_item_t *)currentPlaylistRoot
{
    return p_current_root_item;
}

- (void)swapPlaylists:(id)newList
{
    if(newList != o_outline_view)
    {
        id o_outline_view_temp = o_outline_view;
        id o_tc_author_temp = o_tc_author;
        id o_tc_duration_temp = o_tc_duration;
        id o_tc_name_temp = o_tc_name;
        o_outline_view = o_outline_view_other;
        o_tc_author = o_tc_author_other;
        o_tc_duration = o_tc_duration_other;
        o_tc_name = o_tc_name_other;
        o_outline_view_other = o_outline_view_temp;
        o_tc_author_other = o_tc_author_temp;
        o_tc_duration_other = o_tc_duration_temp;
        o_tc_name_other = o_tc_name_temp;
    }
}

- (NSOutlineView *)outlineView
{
    return o_outline_view;
}

- (playlist_item_t *)selectedPlaylistItem
{
    return [[o_outline_view itemAtRow: [o_outline_view selectedRow]]
                                                                pointerValue];
}

@end

@implementation VLCPlaylistCommon (NSOutlineViewDataSource)
/* return the number of children for Obj-C pointer item */ /* DONE */
- (NSInteger)outlineView:(NSOutlineView *)outlineView numberOfChildrenOfItem:(id)item
{
    int i_return = 0;
    playlist_item_t *p_item = NULL;
    playlist_t * p_playlist = pl_Get( VLCIntf );
    //assert( outlineView == o_outline_view );

    PL_LOCK;
    if( !item )
    {
        p_item = p_current_root_item;
    }
    else
        p_item = (playlist_item_t *)[item pointerValue];

    if( p_item )
        i_return = p_item->i_children;
    PL_UNLOCK;

    return i_return > 0 ? i_return : 0;
}

/* return the child at index for the Obj-C pointer item */ /* DONE */
- (id)outlineView:(NSOutlineView *)outlineView child:(NSInteger)index ofItem:(id)item
{
    playlist_item_t *p_return = NULL, *p_item = NULL;
    NSValue *o_value;
    playlist_t * p_playlist = pl_Get( VLCIntf );

    PL_LOCK;
    if( item == nil )
    {
        /* root object */
        p_item = p_current_root_item;
    }
    else
    {
        p_item = (playlist_item_t *)[item pointerValue];
    }
    if( p_item && index < p_item->i_children && index >= 0 )
        p_return = p_item->pp_children[index];
    PL_UNLOCK;

    o_value = [o_outline_dict objectForKey:[NSString stringWithFormat: @"%p", p_return]];

    if( o_value == nil )
    {
        /* FIXME: Why is there a warning if that happens all the time and seems
         * to be normal? Add an assert and fix it.
         * msg_Warn( VLCIntf, "playlist item misses pointer value, adding one" ); */
        o_value = [[NSValue valueWithPointer: p_return] retain];
    }
    return o_value;
}

/* is the item expandable */
- (BOOL)outlineView:(NSOutlineView *)outlineView isItemExpandable:(id)item
{
    int i_return = 0;
    playlist_t *p_playlist = pl_Get( VLCIntf );

    PL_LOCK;
    if( item == nil )
    {
        /* root object */
        if( p_current_root_item )
        {
            i_return = p_current_root_item->i_children;
        }
    }
    else
    {
        playlist_item_t *p_item = (playlist_item_t *)[item pointerValue];
        if( p_item )
            i_return = p_item->i_children;
    }
    PL_UNLOCK;

    return (i_return >= 0);
}

/* retrieve the string values for the cells */
- (id)outlineView:(NSOutlineView *)outlineView objectValueForTableColumn:(NSTableColumn *)o_tc byItem:(id)item
{
    id o_value = nil;
    playlist_item_t *p_item;

    /* For error handling */
    static BOOL attempted_reload = NO;

    if( item == nil || ![item isKindOfClass: [NSValue class]] )
    {
        /* Attempt to fix the error by asking for a data redisplay
         * This might cause infinite loop, so add a small check */
        if( !attempted_reload )
        {
            attempted_reload = YES;
            [outlineView reloadData];
        }
        return @"error" ;
    }

    p_item = (playlist_item_t *)[item pointerValue];
    if( !p_item || !p_item->p_input )
    {
        /* Attempt to fix the error by asking for a data redisplay
         * This might cause infinite loop, so add a small check */
        if( !attempted_reload )
        {
            attempted_reload = YES;
            [outlineView reloadData];
        }
        return @"error";
    }

    attempted_reload = NO;

    if( [[o_tc identifier] isEqualToString:@"name"] )
    {
        /* sanity check to prevent the NSString class from crashing */
        char *psz_title =  input_item_GetTitleFbName( p_item->p_input );
        if( psz_title )
        {
            o_value = [NSString stringWithUTF8String: psz_title];
            free( psz_title );
        }
    }
    else if( [[o_tc identifier] isEqualToString:@"artist"] )
    {
        char *psz_artist = input_item_GetArtist( p_item->p_input );
        if( psz_artist )
            o_value = [NSString stringWithUTF8String: psz_artist];
        free( psz_artist );
    }
    else if( [[o_tc identifier] isEqualToString:@"duration"] )
    {
        char psz_duration[MSTRTIME_MAX_SIZE];
        mtime_t dur = input_item_GetDuration( p_item->p_input );
        if( dur != -1 )
        {
            secstotimestr( psz_duration, dur/1000000 );
            o_value = [NSString stringWithUTF8String: psz_duration];
        }
        else
            o_value = @"--:--";
    }
    else if( [[o_tc identifier] isEqualToString:@"status"] )
    {
        if( input_item_HasErrorWhenReading( p_item->p_input ) )
        {
            o_value = [[NSWorkspace sharedWorkspace] iconForFileType:NSFileTypeForHFSTypeCode(kAlertCautionIcon)];
            [o_value setSize: NSMakeSize(16,16)];
        }
    }
    return o_value;
}

@end

/*****************************************************************************
 * VLCPlaylistWizard implementation
 *****************************************************************************/
@implementation VLCPlaylistWizard

- (IBAction)reloadOutlineView
{
    /* Only reload the outlineview if the wizard window is open since this can
       be quite long on big playlists */
    if( [[o_outline_view window] isVisible] )
    {
        [o_outline_view reloadData];
    }
}

@end

/*****************************************************************************
 * extension to NSOutlineView's interface to fix compilation warnings
 * and let us access these 2 functions properly
 * this uses a private Apple-API, but works fine on all current OSX releases
 * keep checking for compatiblity with future releases though
 *****************************************************************************/

@interface NSOutlineView (UndocumentedSortImages)
+ (NSImage *)_defaultTableHeaderSortImage;
+ (NSImage *)_defaultTableHeaderReverseSortImage;
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
        o_nodes_array = [[NSMutableArray alloc] init];
        o_items_array = [[NSMutableArray alloc] init];
    }
    return self;
}

- (void)dealloc
{
    [o_nodes_array release];
    [o_items_array release];
    [super dealloc];
}

- (void)awakeFromNib
{
    playlist_t * p_playlist = pl_Get( VLCIntf );

    int i;

    [super awakeFromNib];

    [o_outline_view setDoubleAction: @selector(playItem:)];
    [o_outline_view_other setDoubleAction: @selector(playItem:)];

    [o_outline_view registerForDraggedTypes:
        [NSArray arrayWithObjects: NSFilenamesPboardType,
        @"VLCPlaylistItemPboardType", nil]];
    [o_outline_view setIntercellSpacing: NSMakeSize (0.0, 1.0)];

    [o_outline_view_other registerForDraggedTypes:
     [NSArray arrayWithObjects: NSFilenamesPboardType,
      @"VLCPlaylistItemPboardType", nil]];
    [o_outline_view_other setIntercellSpacing: NSMakeSize (0.0, 1.0)];

    /* This uses private Apple API which works fine until 10.5.
     * We need to keep checking in the future!
     * These methods are being added artificially to NSOutlineView's interface above */
    o_ascendingSortingImage = [[NSOutlineView class] _defaultTableHeaderSortImage];
    o_descendingSortingImage = [[NSOutlineView class] _defaultTableHeaderReverseSortImage];

    o_tc_sortColumn = nil;
}

- (void)searchfieldChanged:(NSNotification *)o_notification
{
    [o_search_field setStringValue:[[o_notification object] stringValue]];
}

- (void)initStrings
{
    [super initStrings];

    [o_mi_save_playlist setTitle: _NS("Save Playlist...")];
    [o_mi_play setTitle: _NS("Play")];
    [o_mi_delete setTitle: _NS("Delete")];
    [o_mi_recursive_expand setTitle: _NS("Expand Node")];
    [o_mi_selectall setTitle: _NS("Select All")];
    [o_mi_info setTitle: _NS("Media Information...")];
    [o_mi_dl_cover_art setTitle: _NS("Download Cover Art")];
    [o_mi_preparse setTitle: _NS("Fetch Meta Data")];
    [o_mi_revealInFinder setTitle: _NS("Reveal in Finder")];
    [o_mm_mi_revealInFinder setTitle: _NS("Reveal in Finder")];
    [[o_mm_mi_revealInFinder menu] setAutoenablesItems: NO];
    [o_mi_sort_name setTitle: _NS("Sort Node by Name")];
    [o_mi_sort_author setTitle: _NS("Sort Node by Author")];

    [o_search_field setToolTip: _NS("Search in Playlist")];
    [o_search_field_other setToolTip: _NS("Search in Playlist")];

    [o_save_accessory_text setStringValue: _NS("File Format:")];
    [[o_save_accessory_popup itemAtIndex:0] setTitle: _NS("Extended M3U")];
    [[o_save_accessory_popup itemAtIndex:1] setTitle: _NS("XML Shareable Playlist Format (XSPF)")];
    [[o_save_accessory_popup itemAtIndex:2] setTitle: _NS("HTML Playlist")];
}

- (void)swapPlaylists:(id)newList
{
    if(newList != o_outline_view)
    {
        id o_search_field_temp = o_search_field;
        o_search_field = o_search_field_other;
        o_search_field_other = o_search_field_temp;
        [super swapPlaylists:newList];
        [self playlistUpdated];
    }
}

- (void)playlistUpdated
{
    /* Clear indications of any existing column sorting */
    NSUInteger count = [[o_outline_view tableColumns] count];
    for( NSUInteger i = 0 ; i < count ; i++ )
    {
        [o_outline_view setIndicatorImage:nil inTableColumn:
                            [[o_outline_view tableColumns] objectAtIndex:i]];
    }

    [o_outline_view setHighlightedTableColumn:nil];
    o_tc_sortColumn = nil;
    // TODO Find a way to keep the dict size to a minimum
    //[o_outline_dict removeAllObjects];
    [o_outline_view reloadData];
    [[[[VLCMain sharedInstance] wizard] playlistWizard] reloadOutlineView];
    [[[[VLCMain sharedInstance] bookmarks] dataTable] reloadData];

    [self outlineViewSelectionDidChange: nil];
    [[VLCMain sharedInstance] updateMainWindow];
}

- (void)outlineViewSelectionDidChange:(NSNotification *)notification
{
    // FIXME: unsafe
    playlist_item_t * p_item = [[o_outline_view itemAtRow:[o_outline_view selectedRow]] pointerValue];

    if( p_item )
    {
        /* update the state of our Reveal-in-Finder menu items */
        NSMutableString *o_mrl;
        char *psz_uri = input_item_GetURI( p_item->p_input );

        [o_mi_revealInFinder setEnabled: NO];
        [o_mm_mi_revealInFinder setEnabled: NO];
        if( psz_uri )
        {
            o_mrl = [NSMutableString stringWithUTF8String: psz_uri];

            /* perform some checks whether it is a file and if it is local at all... */
            NSRange prefix_range = [o_mrl rangeOfString: @"file:"];
            if( prefix_range.location != NSNotFound )
                [o_mrl deleteCharactersInRange: prefix_range];

            if( [o_mrl characterAtIndex:0] == '/' )
            {
                [o_mi_revealInFinder setEnabled: YES];
                [o_mm_mi_revealInFinder setEnabled: YES];
            }
            free( psz_uri );
        }

        /* update our info-panel to reflect the new item */
        [[[VLCMain sharedInstance] info] updatePanelWithItem:p_item->p_input];
    }
}

- (BOOL)isSelectionEmpty
{
    return [o_outline_view selectedRow] == -1;
}

- (void)updateRowSelection
{
    // FIXME: unsafe
    playlist_t *p_playlist = pl_Get( VLCIntf );
    playlist_item_t *p_item, *p_temp_item;
    NSMutableArray *o_array = [NSMutableArray array];

    PL_LOCK;
    p_item = playlist_CurrentPlayingItem( p_playlist );
    if( p_item == NULL )
    {
        PL_UNLOCK;
        return;
    }

    p_temp_item = p_item;
    while( p_temp_item->p_parent )
    {
        [o_array insertObject: [NSValue valueWithPointer: p_temp_item] atIndex: 0];
        p_temp_item = p_temp_item->p_parent;
    }
    PL_UNLOCK;

    NSUInteger count = [o_array count];
    for( NSUInteger j = 0; j < count - 1; j++ )
    {
        id o_item;
        if( ( o_item = [o_outline_dict objectForKey:
                            [NSString stringWithFormat: @"%p",
                            [[o_array objectAtIndex:j] pointerValue]]] ) != nil )
        {
            [o_outline_view expandItem: o_item];
        }
    }

    id o_item = [o_outline_dict objectForKey:[NSString stringWithFormat: @"%p", p_item]];
    NSInteger i_index = [o_outline_view rowForItem:o_item];
    [o_outline_view selectRowIndexes:[NSIndexSet indexSetWithIndex:i_index] byExtendingSelection:NO];
    [o_outline_view setNeedsDisplay:YES];
}

/* Check if p_item is a child of p_node recursively. We need to check the item
   existence first since OSX sometimes tries to redraw items that have been
   deleted. We don't do it when not required since this verification takes
   quite a long time on big playlists (yes, pretty hacky). */

- (BOOL)isItem: (playlist_item_t *)p_item
                    inNode: (playlist_item_t *)p_node
                    checkItemExistence:(BOOL)b_check
                    locked:(BOOL)b_locked

{
    playlist_t * p_playlist = pl_Get( VLCIntf );
    playlist_item_t *p_temp_item = p_item;

    if( p_node == p_item )
        return YES;

    if( p_node->i_children < 1)
        return NO;

    if ( p_temp_item )
    {
        int i;
        if(!b_locked) PL_LOCK;

        if( b_check )
        {
        /* Since outlineView: willDisplayCell:... may call this function with
           p_items that don't exist anymore, first check if the item is still
           in the playlist. Any cleaner solution welcomed. */
            for( i = 0; i < p_playlist->all_items.i_size; i++ )
            {
                if( ARRAY_VAL( p_playlist->all_items, i) == p_item ) break;
                else if ( i == p_playlist->all_items.i_size - 1 )
                {
                    if(!b_locked) PL_UNLOCK;
                    return NO;
                }
            }
        }

        while( p_temp_item )
        {
            p_temp_item = p_temp_item->p_parent;
            if( p_temp_item == p_node )
            {
                if(!b_locked) PL_UNLOCK;
                return YES;
            }
        }
        if(!b_locked) PL_UNLOCK;
    }
    return NO;
}

- (BOOL)isItem: (playlist_item_t *)p_item
                    inNode: (playlist_item_t *)p_node
                    checkItemExistence:(BOOL)b_check
{
    return [self isItem:p_item inNode:p_node checkItemExistence:b_check locked:NO];
}

/* This method is useful for instance to remove the selected children of an
   already selected node */
- (void)removeItemsFrom:(id)o_items ifChildrenOf:(id)o_nodes
{
    NSUInteger itemCount = [o_items count];
    NSUInteger nodeCount = [o_nodes count];
    for( NSUInteger i = 0 ; i < itemCount ; i++ )
    {
        for ( NSUInteger j = 0 ; j < nodeCount ; j++ )
        {
            if( o_items == o_nodes)
            {
                if( j == i ) continue;
            }
            if( [self isItem: [[o_items objectAtIndex:i] pointerValue]
                    inNode: [[o_nodes objectAtIndex:j] pointerValue]
                    checkItemExistence: NO locked:NO] )
            {
                [o_items removeObjectAtIndex:i];
                /* We need to execute the next iteration with the same index
                   since the current item has been deleted */
                i--;
                break;
            }
        }
    }
}

- (IBAction)savePlaylist:(id)sender
{
    playlist_t * p_playlist = pl_Get( VLCIntf );

    NSSavePanel *o_save_panel = [NSSavePanel savePanel];
    NSString * o_name = [NSString stringWithFormat: @"%@", _NS("Untitled")];

    [o_save_panel setTitle: _NS("Save Playlist")];
    [o_save_panel setPrompt: _NS("Save")];
    [o_save_panel setAccessoryView: o_save_accessory_view];

    if( [o_save_panel runModalForDirectory: nil
            file: o_name] == NSOKButton )
    {
        NSString *o_filename = [[o_save_panel URL] path];

        if( [o_save_accessory_popup indexOfSelectedItem] == 0 )
        {
            NSString * o_real_filename;
            NSRange range;
            range.location = [o_filename length] - [@".m3u" length];
            range.length = [@".m3u" length];

            if( [o_filename compare:@".m3u" options: NSCaseInsensitiveSearch
                                             range: range] != NSOrderedSame )
            {
                o_real_filename = [NSString stringWithFormat: @"%@.m3u", o_filename];
            }
            else
            {
                o_real_filename = o_filename;
            }
            playlist_Export( p_playlist,
                [o_real_filename fileSystemRepresentation],
                p_playlist->p_local_category, "export-m3u" );
        }
        else if( [o_save_accessory_popup indexOfSelectedItem] == 1 )
        {
            NSString * o_real_filename;
            NSRange range;
            range.location = [o_filename length] - [@".xspf" length];
            range.length = [@".xspf" length];

            if( [o_filename compare:@".xspf" options: NSCaseInsensitiveSearch
                                             range: range] != NSOrderedSame )
            {
                o_real_filename = [NSString stringWithFormat: @"%@.xspf", o_filename];
            }
            else
            {
                o_real_filename = o_filename;
            }
            playlist_Export( p_playlist,
                [o_real_filename fileSystemRepresentation],
                p_playlist->p_local_category, "export-xspf" );
        }
        else
        {
            NSString * o_real_filename;
            NSRange range;
            range.location = [o_filename length] - [@".html" length];
            range.length = [@".html" length];

            if( [o_filename compare:@".html" options: NSCaseInsensitiveSearch
                                             range: range] != NSOrderedSame )
            {
                o_real_filename = [NSString stringWithFormat: @"%@.html", o_filename];
            }
            else
            {
                o_real_filename = o_filename;
            }
            playlist_Export( p_playlist,
                [o_real_filename fileSystemRepresentation],
                p_playlist->p_local_category, "export-html" );
        }
    }
}

/* When called retrieves the selected outlineview row and plays that node or item */
- (IBAction)playItem:(id)sender
{
    intf_thread_t * p_intf = VLCIntf;
    playlist_t * p_playlist = pl_Get( p_intf );

    playlist_item_t *p_item;
    playlist_item_t *p_node = NULL;

    // ignore clicks on column header when handling double action
    if( sender != nil && [o_outline_view clickedRow] == -1 )
        return;

    p_item = [[o_outline_view itemAtRow:[o_outline_view selectedRow]] pointerValue];

    PL_LOCK;
    if( p_item )
    {
        if( p_item->i_children == -1 )
        {
            p_node = p_item->p_parent;
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
        playlist_Control( p_playlist, PLAYLIST_VIEWPLAY, pl_Locked, p_node, p_item );
    }
    PL_UNLOCK;
}

- (IBAction)revealItemInFinder:(id)sender
{
    playlist_item_t * p_item = [[o_outline_view itemAtRow:[o_outline_view selectedRow]] pointerValue];
    NSMutableString * o_mrl = nil;

    if(! p_item || !p_item->p_input )
        return;

    char *psz_uri = decode_URI( input_item_GetURI( p_item->p_input ) );
    if( psz_uri )
        o_mrl = [NSMutableString stringWithUTF8String: psz_uri];

    /* perform some checks whether it is a file and if it is local at all... */
    NSRange prefix_range = [o_mrl rangeOfString: @"file:"];
    if( prefix_range.location != NSNotFound )
        [o_mrl deleteCharactersInRange: prefix_range];

    if( [o_mrl characterAtIndex:0] == '/' )
        [[NSWorkspace sharedWorkspace] selectFile: o_mrl inFileViewerRootedAtPath: o_mrl];
}

/* When called retrieves the selected outlineview row and plays that node or item */
- (IBAction)preparseItem:(id)sender
{
    int i_count;
    NSIndexSet *o_selected_indexes;
    intf_thread_t * p_intf = VLCIntf;
    playlist_t * p_playlist = pl_Get( p_intf );
    playlist_item_t *p_item = NULL;

    o_selected_indexes = [o_outline_view selectedRowIndexes];
    i_count = [o_selected_indexes count];

    NSUInteger indexes[i_count];
    [o_selected_indexes getIndexes:indexes maxCount:i_count inIndexRange:nil];
    for (int i = 0; i < i_count; i++)
    {
        p_item = [[o_outline_view itemAtRow:indexes[i]] pointerValue];
        [o_outline_view deselectRow: indexes[i]];

        if( p_item )
        {
            if( p_item->i_children == -1 )
                playlist_PreparseEnqueue( p_playlist, p_item->p_input );
            else
                msg_Dbg( p_intf, "preparsing nodes not implemented" );
        }
    }
    [self playlistUpdated];
}

- (IBAction)downloadCoverArt:(id)sender
{
    int i_count;
    NSIndexSet *o_selected_indexes;
    intf_thread_t * p_intf = VLCIntf;
    playlist_t * p_playlist = pl_Get( p_intf );
    playlist_item_t *p_item = NULL;

    o_selected_indexes = [o_outline_view selectedRowIndexes];
    i_count = [o_selected_indexes count];

    NSUInteger indexes[i_count];
    [o_selected_indexes getIndexes:indexes maxCount:i_count inIndexRange:nil];
    for (int i = 0; i < i_count; i++)
    {
        p_item = [[o_outline_view itemAtRow: indexes[i]] pointerValue];   
        [o_outline_view deselectRow: indexes[i]];

        if( p_item && p_item->i_children == -1 )
            playlist_AskForArtEnqueue( p_playlist, p_item->p_input );
    }
    [self playlistUpdated];
}

- (IBAction)selectAll:(id)sender
{
    [o_outline_view selectAll: nil];
}

- (IBAction)deleteItem:(id)sender
{
    int i_count;
    NSIndexSet *o_selected_indexes;
    playlist_t * p_playlist;
    intf_thread_t * p_intf = VLCIntf;

    o_selected_indexes = [o_outline_view selectedRowIndexes];
    i_count = [o_selected_indexes count];

    p_playlist = pl_Get( p_intf );

    NSUInteger indexes[i_count];
    if (i_count == [o_outline_view numberOfRows])
    {
#ifndef NDEBUG
        msg_Dbg( p_intf, "user selected entire list, deleting current playlist root instead of individual items" );
#endif
        PL_LOCK;
        playlist_NodeDelete( p_playlist, [self currentPlaylistRoot], true, false );
        PL_UNLOCK;
        [self playlistUpdated];
        return;
    }
    [o_selected_indexes getIndexes:indexes maxCount:i_count inIndexRange:nil];
    for (int i = 0; i < i_count; i++)
    {
        id o_item = [o_outline_view itemAtRow: indexes[i]];
        [o_outline_view deselectRow: indexes[i]];

        PL_LOCK;
        playlist_item_t *p_item = [o_item pointerValue];
#ifndef NDEBUG
        msg_Dbg( p_intf, "deleting item %i (of %i) with id \"%i\", pointerValue \"%p\" and %i children", i+1, i_count,
                p_item->p_input->i_id, [o_item pointerValue], p_item->i_children +1 );
#endif

        if( p_item->i_children != -1 )
        //is a node and not an item
        {
            if( playlist_Status( p_playlist ) != PLAYLIST_STOPPED &&
                [self isItem: playlist_CurrentPlayingItem( p_playlist ) inNode: ((playlist_item_t *)[o_item pointerValue])
                        checkItemExistence: NO locked:YES] == YES )
                // if current item is in selected node and is playing then stop playlist
                playlist_Control(p_playlist, PLAYLIST_STOP, pl_Locked );

                playlist_NodeDelete( p_playlist, p_item, true, false );
        }
        else
            playlist_DeleteFromInput( p_playlist, p_item->p_input, pl_Locked );

        PL_UNLOCK;
        [o_outline_dict removeObjectForKey:[NSString stringWithFormat:@"%p", [o_item pointerValue]]];
        [o_item release];
    }

    [self playlistUpdated];
}

- (IBAction)sortNodeByName:(id)sender
{
    [self sortNode: SORT_TITLE];
}

- (IBAction)sortNodeByAuthor:(id)sender
{
    [self sortNode: SORT_ARTIST];
}

- (void)sortNode:(int)i_mode
{
    playlist_t * p_playlist = pl_Get( VLCIntf );
    playlist_item_t * p_item;

    if( [o_outline_view selectedRow] > -1 )
    {
        p_item = [[o_outline_view itemAtRow: [o_outline_view selectedRow]] pointerValue];
    }
    else
    /*If no item is selected, sort the whole playlist*/
    {
        p_item = [self currentPlaylistRoot];
    }

    PL_LOCK;
    if( p_item->i_children > -1 ) // the item is a node
    {
        playlist_RecursiveNodeSort( p_playlist, p_item, i_mode, ORDER_NORMAL );
    }
    else
    {
        playlist_RecursiveNodeSort( p_playlist,
                p_item->p_parent, i_mode, ORDER_NORMAL );
    }
    PL_UNLOCK;
    [self playlistUpdated];
}

- (input_item_t *)createItem:(NSDictionary *)o_one_item
{
    intf_thread_t * p_intf = VLCIntf;
    playlist_t * p_playlist = pl_Get( p_intf );

    input_item_t *p_input;
    BOOL b_rem = FALSE, b_dir = FALSE, b_writable = FALSE;
    NSString *o_uri, *o_name, *o_path;
    NSURL * o_nsurl;
    NSArray *o_options;
    NSURL *o_true_file;

    /* Get the item */
    o_uri = (NSString *)[o_one_item objectForKey: @"ITEM_URL"];
    o_nsurl = [NSURL URLWithString: o_uri];
    o_path = [o_nsurl path];
    o_name = (NSString *)[o_one_item objectForKey: @"ITEM_NAME"];
    o_options = (NSArray *)[o_one_item objectForKey: @"ITEM_OPTIONS"];

    if( [[NSFileManager defaultManager] fileExistsAtPath:o_path isDirectory:&b_dir] && b_dir &&
        [[NSWorkspace sharedWorkspace] getFileSystemInfoForPath:o_path isRemovable: &b_rem
                                                     isWritable:&b_writable isUnmountable:NULL description:NULL type:NULL] && b_rem && !b_writable && [o_nsurl isFileURL] )
    {

        id o_vlc_open = [[VLCMain sharedInstance] open];

        char *diskType = [o_vlc_open getVolumeTypeFromMountPath: o_path];
        msg_Dbg( p_intf, "detected optical media of type '%s' in the file input", diskType );

        if (diskType == kVLCMediaDVD)
        {
            o_uri = [NSString stringWithFormat: @"dvdnav://%@", [o_vlc_open getBSDNodeFromMountPath: o_path]];
        }
        else if (diskType == kVLCMediaVideoTSFolder)
        {
            o_uri = [NSString stringWithFormat: @"dvdnav://%@", o_path];
        }
        else if (diskType == kVLCMediaAudioCD)
        {
            o_uri = [NSString stringWithFormat: @"cdda://%@", [o_vlc_open getBSDNodeFromMountPath: o_path]];
        }
        else if (diskType == kVLCMediaVCD)
        {
            o_uri = [NSString stringWithFormat: @"vcd://%@#0:0", [o_vlc_open getBSDNodeFromMountPath: o_path]];
        }
        else if (diskType == kVLCMediaSVCD)
        {
            o_uri = [NSString stringWithFormat: @"vcd://%@@0:0", [o_vlc_open getBSDNodeFromMountPath: o_path]];
        }
        else if (diskType == kVLCMediaBD || diskType == kVLCMediaBDMVFolder)
        {
            o_uri = [NSString stringWithFormat: @"bluray://%@", o_path];
        }
        else
        {
            msg_Warn( VLCIntf, "unknown disk type, treating %s as regular input", [o_path UTF8String] );
        }

        p_input = input_item_New( [o_uri UTF8String], [[[NSFileManager defaultManager] displayNameAtPath: o_path] UTF8String] );
    }
    else
        p_input = input_item_New( [o_uri fileSystemRepresentation], o_name ? [o_name UTF8String] : NULL );

    if( !p_input )
        return NULL;

    if( o_options )
    {
        NSUInteger count = [o_options count];
        for( NSUInteger i = 0; i < count; i++ )
        {
            input_item_AddOption( p_input, [[o_options objectAtIndex:i] UTF8String],
                                  VLC_INPUT_OPTION_TRUSTED );
        }
    }

    /* Recent documents menu */
    if( o_nsurl != nil && (BOOL)config_GetInt( p_playlist, "macosx-recentitems" ) == YES )
    {
        [[NSDocumentController sharedDocumentController] noteNewRecentDocumentURL: o_nsurl];
    }
    return p_input;
}

- (void)appendArray:(NSArray*)o_array atPos:(int)i_position enqueue:(BOOL)b_enqueue
{
    playlist_t * p_playlist = pl_Get( VLCIntf );
    NSUInteger count = [o_array count];
    BOOL b_usingPlaylist;
    if ([self currentPlaylistRoot] == p_playlist->p_ml_category)
        b_usingPlaylist = NO;
    else
        b_usingPlaylist = YES;

    PL_LOCK;
    for( NSUInteger i_item = 0; i_item < count; i_item++ )
    {
        input_item_t *p_input;
        NSDictionary *o_one_item;

        /* Get the item */
        o_one_item = [o_array objectAtIndex: i_item];
        p_input = [self createItem: o_one_item];
        if( !p_input )
        {
            continue;
        }

        /* Add the item */
        /* FIXME: playlist_AddInput() can fail */

        playlist_AddInput( p_playlist, p_input, PLAYLIST_INSERT, i_position == -1 ? PLAYLIST_END : i_position + i_item, b_usingPlaylist,
         pl_Locked );

        if( i_item == 0 && !b_enqueue )
        {
            playlist_item_t *p_item = playlist_ItemGetByInput( p_playlist, p_input );
            playlist_Control( p_playlist, PLAYLIST_VIEWPLAY, pl_Locked, p_item->p_parent, p_item );
        }

        vlc_gc_decref( p_input );
    }
    PL_UNLOCK;
    [self playlistUpdated];
}

- (void)appendNodeArray:(NSArray*)o_array inNode:(playlist_item_t *)p_node atPos:(int)i_position enqueue:(BOOL)b_enqueue
{
    playlist_t * p_playlist = pl_Get( VLCIntf );
    NSUInteger count = [o_array count];

    for( NSUInteger i_item = 0; i_item < count; i_item++ )
    {
        input_item_t *p_input;
        NSDictionary *o_one_item;

        /* Get the item */
        o_one_item = [o_array objectAtIndex: i_item];
        p_input = [self createItem: o_one_item];

        if( !p_input ) continue;

        /* Add the item */
        PL_LOCK;
        playlist_NodeAddInput( p_playlist, p_input, p_node,
                                      PLAYLIST_INSERT,
                                      i_position == -1 ?
                                      PLAYLIST_END : i_position + i_item,
                                      pl_Locked );


        if( i_item == 0 && !b_enqueue )
        {
            playlist_item_t *p_item;
            p_item = playlist_ItemGetByInput( p_playlist, p_input );
            playlist_Control( p_playlist, PLAYLIST_VIEWPLAY, pl_Locked, p_node, p_item );
        }
        PL_UNLOCK;
        vlc_gc_decref( p_input );
    }
    [self playlistUpdated];
}

- (NSMutableArray *)subSearchItem:(playlist_item_t *)p_item
{
    playlist_t *p_playlist = pl_Get( VLCIntf );
    playlist_item_t *p_selected_item;
    int i_selected_row;

    i_selected_row = [o_outline_view selectedRow];
    if (i_selected_row < 0)
        i_selected_row = 0;

    p_selected_item = (playlist_item_t *)[[o_outline_view itemAtRow: i_selected_row] pointerValue];

    for( NSUInteger i_current = 0; i_current < p_item->i_children ; i_current++ )
    {
        char *psz_temp;
        NSString *o_current_name, *o_current_author;

        PL_LOCK;
        o_current_name = [NSString stringWithUTF8String:
            p_item->pp_children[i_current]->p_input->psz_name];
        psz_temp = input_item_GetInfo( p_item->p_input ,
                   _("Meta-information"),_("Artist") );
        o_current_author = [NSString stringWithUTF8String: psz_temp];
        free( psz_temp);
        PL_UNLOCK;

        if( p_selected_item == p_item->pp_children[i_current] &&
                    b_selected_item_met == NO )
        {
            b_selected_item_met = YES;
        }
        else if( p_selected_item == p_item->pp_children[i_current] &&
                    b_selected_item_met == YES )
        {
            return NULL;
        }
        else if( b_selected_item_met == YES &&
                    ( [o_current_name rangeOfString:[o_search_field
                        stringValue] options:NSCaseInsensitiveSearch].length ||
                      [o_current_author rangeOfString:[o_search_field
                        stringValue] options:NSCaseInsensitiveSearch].length ) )
        {
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
                [o_result insertObject: [NSValue valueWithPointer:
                                p_item->pp_children[i_current]] atIndex:0];
                return o_result;
            }
        }
    }
    return NULL;
}

- (IBAction)searchItem:(id)sender
{
    playlist_t * p_playlist = pl_Get( VLCIntf );
    id o_result;

    int i_row = -1;

    b_selected_item_met = NO;

        /*First, only search after the selected item:*
         *(b_selected_item_met = NO)                 */
    o_result = [self subSearchItem:[self currentPlaylistRoot]];
    if( o_result == NULL )
    {
        /* If the first search failed, search again from the beginning */
        o_result = [self subSearchItem:[self currentPlaylistRoot]];
    }
    if( o_result != NULL )
    {
        int i_start;
        if( [[o_result objectAtIndex: 0] pointerValue] == p_playlist->p_local_category )
            i_start = 1;
        else
            i_start = 0;
        NSUInteger count = [o_result count];

        for( NSUInteger i = i_start ; i < count - 1 ; i++ )
        {
            [o_outline_view expandItem: [o_outline_dict objectForKey:
                        [NSString stringWithFormat: @"%p",
                        [[o_result objectAtIndex: i] pointerValue]]]];
        }
        i_row = [o_outline_view rowForItem: [o_outline_dict objectForKey:
                        [NSString stringWithFormat: @"%p",
                        [[o_result objectAtIndex: count - 1 ]
                        pointerValue]]]];
    }
    if( i_row > -1 )
    {
        [o_outline_view selectRowIndexes:[NSIndexSet indexSetWithIndex:i_row] byExtendingSelection:NO];
        [o_outline_view scrollRowToVisible: i_row];
    }
}

- (IBAction)recursiveExpandNode:(id)sender
{
    id o_item = [o_outline_view itemAtRow: [o_outline_view selectedRow]];
    playlist_item_t *p_item = (playlist_item_t *)[o_item pointerValue];

    if( ![[o_outline_view dataSource] outlineView: o_outline_view
                                                    isItemExpandable: o_item] )
    {
        o_item = [o_outline_dict objectForKey: [NSString stringWithFormat: @"%p", p_item->p_parent]];
    }

    /* We need to collapse the node first, since OSX refuses to recursively
       expand an already expanded node, even if children nodes are collapsed. */
    [o_outline_view collapseItem: o_item collapseChildren: YES];
    [o_outline_view expandItem: o_item expandChildren: YES];
}

- (NSMenu *)menuForEvent:(NSEvent *)o_event
{
    NSPoint pt;
    bool b_rows;
    bool b_item_sel;

    pt = [o_outline_view convertPoint: [o_event locationInWindow]
                                                 fromView: nil];
    int row = [o_outline_view rowAtPoint:pt];
    if( row != -1 )
        [o_outline_view selectRowIndexes:[NSIndexSet indexSetWithIndex:row] byExtendingSelection:NO];

    b_item_sel = ( row != -1 && [o_outline_view selectedRow] != -1 );
    b_rows = [o_outline_view numberOfRows] != 0;

    [o_mi_play setEnabled: b_item_sel];
    [o_mi_delete setEnabled: b_item_sel];
    [o_mi_selectall setEnabled: b_rows];
    [o_mi_info setEnabled: b_item_sel];
    [o_mi_preparse setEnabled: b_item_sel];
    [o_mi_recursive_expand setEnabled: b_item_sel];
    [o_mi_sort_name setEnabled: b_item_sel];
    [o_mi_sort_author setEnabled: b_item_sel];

    return( o_ctx_menu );
}

- (void)outlineView: (NSOutlineView *)o_tv
                  didClickTableColumn:(NSTableColumn *)o_tc
{
    int i_mode, i_type = 0;
    intf_thread_t *p_intf = VLCIntf;

    playlist_t *p_playlist = pl_Get( p_intf );

    /* Check whether the selected table column header corresponds to a
       sortable table column*/
    if( !( o_tc == o_tc_name || o_tc == o_tc_author || o_tc == o_tc_duration ) )
    {
        return;
    }

    if( o_tc_sortColumn == o_tc )
    {
        b_isSortDescending = !b_isSortDescending;
    }
    else
    {
        b_isSortDescending = false;
    }

    if( o_tc == o_tc_name )
    {
        i_mode = SORT_TITLE;
    }
    else if( o_tc == o_tc_author )
    {
        i_mode = SORT_ARTIST;
    }
    else if( o_tc == o_tc_duration )
    {
        i_mode = SORT_DURATION;
    }

    if( b_isSortDescending )
    {
        i_type = ORDER_REVERSE;
    }
    else
    {
        i_type = ORDER_NORMAL;
    }

    PL_LOCK;
    playlist_RecursiveNodeSort( p_playlist, [self currentPlaylistRoot], i_mode, i_type );
    PL_UNLOCK;

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
    /* this method can be called when VLC is already dead, hence the extra checks */
    intf_thread_t * p_intf = VLCIntf;
    if (!p_intf)
        return;
    playlist_t *p_playlist = pl_Get( p_intf );
    if (!p_playlist)
        return;

    id o_playing_item;

    PL_LOCK;
    o_playing_item = [o_outline_dict objectForKey: [NSString stringWithFormat:@"%p",  playlist_CurrentPlayingItem( p_playlist )]];
    PL_UNLOCK;

    if( [self isItem: [o_playing_item pointerValue] inNode:
                        [item pointerValue] checkItemExistence: YES]
                        || [o_playing_item isEqual: item] )
    {
        [cell setFont: [[NSFontManager sharedFontManager] convertFont:[cell font] toHaveTrait:NSBoldFontMask]];
    }
    else
    {
        [cell setFont: [[NSFontManager sharedFontManager] convertFont:[cell font] toNotHaveTrait:NSBoldFontMask]];
    }
}

- (id)playingItem
{
    playlist_t *p_playlist = pl_Get( VLCIntf );

    id o_playing_item;

    PL_LOCK;
    o_playing_item = [o_outline_dict objectForKey: [NSString stringWithFormat:@"%p",  playlist_CurrentPlayingItem( p_playlist )]];
    PL_UNLOCK;

    return o_playing_item;
}

- (NSArray *)draggedItems
{
    return [[o_nodes_array arrayByAddingObjectsFromArray: o_items_array] retain];
}
@end

@implementation VLCPlaylist (NSOutlineViewDataSource)

- (id)outlineView:(NSOutlineView *)outlineView child:(NSInteger)index ofItem:(id)item
{
    id o_value = [super outlineView: outlineView child: index ofItem: item];

    [o_outline_dict setObject:o_value forKey:[NSString stringWithFormat:@"%p", [o_value pointerValue]]];
    return o_value;
}

/* Required for drag & drop and reordering */
- (BOOL)outlineView:(NSOutlineView *)outlineView writeItems:(NSArray *)items toPasteboard:(NSPasteboard *)pboard
{
    playlist_t *p_playlist = pl_Get( VLCIntf );

    /* First remove the items that were moved during the last drag & drop
       operation */
    [o_items_array removeAllObjects];
    [o_nodes_array removeAllObjects];

    NSUInteger itemCount = [items count];

    for( NSUInteger i = 0 ; i < itemCount ; i++ )
    {
        id o_item = [items objectAtIndex: i];

        /* Fill the items and nodes to move in 2 different arrays */
        if( ((playlist_item_t *)[o_item pointerValue])->i_children > 0 )
            [o_nodes_array addObject: o_item];
        else
            [o_items_array addObject: o_item];
    }

    /* Now we need to check if there are selected items that are in already
       selected nodes. In that case, we only want to move the nodes */
    [self removeItemsFrom: o_nodes_array ifChildrenOf: o_nodes_array];
    [self removeItemsFrom: o_items_array ifChildrenOf: o_nodes_array];

    /* We add the "VLCPlaylistItemPboardType" type to be able to recognize
       a Drop operation coming from the playlist. */

    [pboard declareTypes: [NSArray arrayWithObjects:
        @"VLCPlaylistItemPboardType", nil] owner: self];
    [pboard setData:[NSData data] forType:@"VLCPlaylistItemPboardType"];

    return YES;
}

- (NSDragOperation)outlineView:(NSOutlineView *)outlineView validateDrop:(id <NSDraggingInfo>)info proposedItem:(id)item proposedChildIndex:(NSInteger)index
{
    playlist_t *p_playlist = pl_Get( VLCIntf );
    NSPasteboard *o_pasteboard = [info draggingPasteboard];

    if( !p_playlist ) return NSDragOperationNone;

    /* Dropping ON items is not allowed if item is not a node */
    if( item )
    {
        if( index == NSOutlineViewDropOnItemIndex &&
                ((playlist_item_t *)[item pointerValue])->i_children == -1 )
        {
            return NSDragOperationNone;
        }
    }

    /* We refuse to drop an item in anything else than a child of the General
       Node. We still accept items that would be root nodes of the outlineview
       however, to allow drop in an empty playlist. */
    if( !( ([self isItem: [item pointerValue] inNode: p_playlist->p_local_category checkItemExistence: NO] ||
        ( var_CreateGetBool( p_playlist, "media-library" ) && [self isItem: [item pointerValue] inNode: p_playlist->p_ml_category checkItemExistence: NO] ) ) || item == nil ) )
    {
        return NSDragOperationNone;
    }

    /* Drop from the Playlist */
    if( [[o_pasteboard types] containsObject: @"VLCPlaylistItemPboardType"] )
    {
        NSUInteger count = [o_nodes_array count];
        for( NSUInteger i = 0 ; i < count ; i++ )
        {
            /* We refuse to Drop in a child of an item we are moving */
            if( [self isItem: [item pointerValue] inNode:
                    [[o_nodes_array objectAtIndex: i] pointerValue]
                    checkItemExistence: NO] )
            {
                return NSDragOperationNone;
            }
        }
        return NSDragOperationMove;
    }

    /* Drop from the Finder */
    else if( [[o_pasteboard types] containsObject: NSFilenamesPboardType] )
    {
        return NSDragOperationGeneric;
    }
    return NSDragOperationNone;
}

- (BOOL)outlineView:(NSOutlineView *)outlineView acceptDrop:(id <NSDraggingInfo>)info item:(id)item childIndex:(NSInteger)index
{
    playlist_t * p_playlist =  pl_Get( VLCIntf );
    NSPasteboard *o_pasteboard = [info draggingPasteboard];

    /* Drag & Drop inside the playlist */
    if( [[o_pasteboard types] containsObject: @"VLCPlaylistItemPboardType"] )
    {
        int i_row, i_removed_from_node = 0;
        playlist_item_t *p_new_parent, *p_item = NULL;
        NSArray *o_all_items = [o_nodes_array arrayByAddingObjectsFromArray: o_items_array];
        /* If the item is to be dropped as root item of the outline, make it a
           child of the respective general node, if is either the pl or the ml
           Else, choose the proposed parent as parent. */
        if( item == nil )
        {
            if ([self currentPlaylistRoot] == p_playlist->p_local_category || [self currentPlaylistRoot] == p_playlist->p_ml_category) 
                p_new_parent = [self currentPlaylistRoot];
            else
                return NO;
        }
        else
            p_new_parent = [item pointerValue];

        /* Make sure the proposed parent is a node.
           (This should never be true) */
        if( p_new_parent->i_children < 0 )
        {
            return NO;
        }

        NSUInteger count = [o_all_items count];
        for( NSUInteger i = 0; i < count; i++ )
        {
            playlist_item_t *p_old_parent = NULL;
            int i_old_index = 0;

            p_item = [[o_all_items objectAtIndex:i] pointerValue];
            p_old_parent = p_item->p_parent;
            if( !p_old_parent )
            continue;
            /* We may need the old index later */
            if( p_new_parent == p_old_parent )
            {
                for( NSInteger j = 0; j < p_old_parent->i_children; j++ )
                {
                    if( p_old_parent->pp_children[j] == p_item )
                    {
                        i_old_index = j;
                        break;
                    }
                }
            }

            PL_LOCK;
            // Actually detach the item from the old position
            if( playlist_NodeRemoveItem( p_playlist, p_item, p_old_parent ) ==
                VLC_SUCCESS )
            {
                int i_new_index;
                /* Calculate the new index */
                if( index == -1 )
                i_new_index = -1;
                /* If we move the item in the same node, we need to take into
                   account that one item will be deleted */
                else
                {
                    if ((p_new_parent == p_old_parent && i_old_index < index + (int)i) )
                    {
                        i_removed_from_node++;
                    }
                    i_new_index = index + i - i_removed_from_node;
                }
                // Reattach the item to the new position
                playlist_NodeInsert( p_playlist, p_item, p_new_parent, i_new_index );
            }
            PL_UNLOCK;
        }
        [self playlistUpdated];
        i_row = [o_outline_view rowForItem:[o_outline_dict objectForKey:[NSString stringWithFormat: @"%p", [[o_all_items objectAtIndex: 0] pointerValue]]]];

        if( i_row == -1 )
        {
            i_row = [o_outline_view rowForItem:[o_outline_dict objectForKey:[NSString stringWithFormat: @"%p", p_new_parent]]];
        }

        [o_outline_view deselectAll: self];
        [o_outline_view selectRowIndexes:[NSIndexSet indexSetWithIndex:i_row] byExtendingSelection:NO];
        [o_outline_view scrollRowToVisible: i_row];

        return YES;
    }

    else if( [[o_pasteboard types] containsObject: NSFilenamesPboardType] )
    {
        if ([self currentPlaylistRoot] != p_playlist->p_local_category && [self currentPlaylistRoot] != p_playlist->p_ml_category) 
            return NO;

        playlist_item_t *p_node = [item pointerValue];

        NSArray *o_values = [[o_pasteboard propertyListForType: NSFilenamesPboardType]
                                sortedArrayUsingSelector: @selector(caseInsensitiveCompare:)];
        NSUInteger count = [o_values count];
        NSMutableArray *o_array = [NSMutableArray arrayWithCapacity:count];
        input_thread_t * p_input = pl_CurrentInput( VLCIntf );
        BOOL b_returned = NO;

        if (count == 1 && p_input)
        {
            b_returned = input_AddSubtitle( p_input, make_URI([[o_values objectAtIndex:0] UTF8String], NULL), true );
            vlc_object_release( p_input );
            if(!b_returned)
                return YES;
        }
        else if( p_input )
            vlc_object_release( p_input );

        for( NSUInteger i = 0; i < count; i++)
        {
            NSDictionary *o_dic;
            char *psz_uri = make_URI([[o_values objectAtIndex:i] UTF8String], NULL);
            if( !psz_uri )
                continue;

            o_dic = [NSDictionary dictionaryWithObject:[NSString stringWithCString:psz_uri encoding:NSUTF8StringEncoding] forKey:@"ITEM_URL"];

            free( psz_uri );

            [o_array addObject: o_dic];
        }

        if ( item == nil )
        {
            [self appendArray:o_array atPos:index enqueue: YES];
        }
        else
        {
            assert( p_node->i_children != -1 );
            [self appendNodeArray:o_array inNode: p_node atPos:index enqueue:YES];
        }
        return YES;
    }
    return NO;
}
@end


