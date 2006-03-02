/*****************************************************************************
 r playlistinfo.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2002-2005 the VideoLAN team
 * $Id$
 *
 * Authors: Benjamin Pracht <bigben at videolan dot org>
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
 * Preamble
 *****************************************************************************/

#include "intf.h"
#include "playlistinfo.h"
#include "playlist.h"

/*****************************************************************************
 * VLCPlaylistInfo Implementation
 *****************************************************************************/

@implementation VLCInfo

- (id)init
{
    self = [super init];

    if( self != nil )
    {
        p_item = NULL;
    }
    return( self );
}

- (void)awakeFromNib
{
    [o_info_window setExcludedFromWindowsMenu: TRUE];

    [o_info_window setTitle: _NS("Properties")];
    [o_uri_lbl setStringValue: _NS("URI")];
    [o_title_lbl setStringValue: _NS("Title")];
    [o_author_lbl setStringValue: _NS("Author")];
    [o_btn_ok setTitle: _NS("OK")];
    [o_btn_cancel setTitle: _NS("Cancel")];
    
    [[o_tab_view tabViewItemAtIndex: 0] setLabel: _NS("General")];
    [[o_tab_view tabViewItemAtIndex: 1] setLabel: _NS("Advanced Information")];
    [[o_tab_view tabViewItemAtIndex: 2] setLabel: _NS("Statistics")];
    [o_tab_view selectTabViewItemAtIndex: 0];

    /* constants defined in vlc_meta.h */
    [o_genre_lbl setStringValue: _NS(VLC_META_GENRE)];
    [o_copyright_lbl setStringValue: _NS(VLC_META_COPYRIGHT)];
    [o_collection_lbl setStringValue: _NS(VLC_META_COLLECTION)];
    [o_seqNum_lbl setStringValue: _NS(VLC_META_SEQ_NUM)];
    [o_description_lbl setStringValue: _NS(VLC_META_DESCRIPTION)];
    [o_rating_lbl setStringValue: _NS(VLC_META_RATING)];
    [o_date_lbl setStringValue: _NS(VLC_META_DATE)];
    [o_language_lbl setStringValue: _NS(VLC_META_LANGUAGE)];
    [o_nowPlaying_lbl setStringValue: _NS(VLC_META_NOW_PLAYING)];
    [o_publisher_lbl setStringValue: _NS(VLC_META_PUBLISHER)];
}

- (IBAction)togglePlaylistInfoPanel:(id)sender
{
    if( [o_info_window isVisible] )
    {
        [o_info_window orderOut: sender];
    }
    else
    {
        p_item = [[[VLCMain sharedInstance] getPlaylist] selectedPlaylistItem];
        [self initPanel:sender];
    }
}

- (IBAction)toggleInfoPanel:(id)sender
{
    if( [o_info_window isVisible] )
    {
        [o_info_window orderOut: sender];
    }
    else
    {
        intf_thread_t * p_intf = VLCIntf;
        playlist_t * p_playlist = vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                          FIND_ANYWHERE );

        if( p_playlist )
        {
            p_item = p_playlist->status.p_item;
            vlc_object_release( p_playlist );
        }
        [self initPanel:sender];
    }
}

- (void)initPanel:(id)sender
{
    char *psz_temp;
    vlc_mutex_lock( &p_item->input.lock );

    /*fill uri / title / author info */
    if( p_item->input.psz_uri )
    {
        [o_uri_txt setStringValue:
            ([NSString stringWithUTF8String:p_item->input.psz_uri] == nil ) ?
            [NSString stringWithCString:p_item->input.psz_uri] :
            [NSString stringWithUTF8String:p_item->input.psz_uri]];
    }

    if( p_item->input.psz_name )
    {
        [o_title_txt setStringValue:
            ([NSString stringWithUTF8String:p_item->input.psz_name] == nil ) ?
            [NSString stringWithCString:p_item->input.psz_name] :
            [NSString stringWithUTF8String:p_item->input.psz_name]];
    }
    vlc_mutex_unlock( &p_item->input.lock );

    psz_temp = vlc_input_item_GetInfo( &p_item->input, _("Meta-information"), _("Artist") );

    if( psz_temp )
    {
        [o_author_txt setStringValue: [NSString stringWithUTF8String: psz_temp]];
        free( psz_temp );
    }

    /* fill the other fields */
    [self setMeta: VLC_META_GENRE forLabel: o_genre_txt];
    [self setMeta: VLC_META_COPYRIGHT forLabel: o_copyright_txt];
    [self setMeta: VLC_META_COLLECTION forLabel: o_collection_txt];
    [self setMeta: VLC_META_SEQ_NUM forLabel: o_seqNum_txt];
    [self setMeta: VLC_META_DESCRIPTION forLabel: o_description_txt];
    [self setMeta: VLC_META_RATING forLabel: o_rating_txt];
    [self setMeta: VLC_META_DATE forLabel: o_date_txt];
    [self setMeta: VLC_META_LANGUAGE forLabel: o_language_txt];
    [self setMeta: VLC_META_NOW_PLAYING forLabel: o_nowPlaying_txt];
    [self setMeta: VLC_META_PUBLISHER forLabel: o_publisher_txt];

    /* reload the advanced table */
    [[VLCInfoTreeItem rootItem] refresh];
    [o_outline_view reloadData];

    [o_info_window makeKeyAndOrderFront: sender];
}

- (void)setMeta: (char *)meta forLabel: (id)theItem
{
    char *psz_meta = vlc_input_item_GetInfo( &p_item->input, \
        _(VLC_META_INFO_CAT), _(meta) );
    if( psz_meta != NULL && *psz_meta)
        [theItem setStringValue: [NSString stringWithUTF8String: psz_meta]];
    else
        [theItem setStringValue: @"-"];
}

- (IBAction)infoCancel:(id)sender
{
    [o_info_window orderOut: self];
}


- (IBAction)infoOk:(id)sender
{
    intf_thread_t * p_intf = VLCIntf;
    playlist_t * p_playlist = vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                          FIND_ANYWHERE );
    vlc_value_t val;

    if( [self isItemInPlaylist: p_item] )
    {
        vlc_mutex_lock( &p_item->input.lock );

        p_item->input.psz_uri = strdup( [[o_uri_txt stringValue] UTF8String] );
        p_item->input.psz_name = strdup( [[o_title_txt stringValue] UTF8String] );
        vlc_mutex_unlock( &p_item->input.lock );
        vlc_input_item_AddInfo( &p_item->input, _("Meta-information"), _("Artist"), [[o_author_txt stringValue] UTF8String]);
        
        val.b_bool = VLC_TRUE;
        var_Set( p_playlist, "intf-change", val );
    }
    vlc_object_release( p_playlist );
    [o_info_window orderOut: self];
}

- (playlist_item_t *)getItem
{
    return p_item;
}

- (BOOL)isItemInPlaylist:(playlist_item_t *)p_local_item
{
    intf_thread_t * p_intf = VLCIntf;
    playlist_t * p_playlist = vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                          FIND_ANYWHERE );
    int i;

    if( p_playlist == NULL )
    {
        return NO;
    }

    for( i = 0 ; i < p_playlist->i_size ; i++ )
    {
        if( p_playlist->pp_items[i] == p_local_item )
        {
            vlc_object_release( p_playlist );
            return YES;
        }
    }
    vlc_object_release( p_playlist );
    return NO;
}

@end

@implementation VLCInfo (NSMenuValidation)

- (BOOL)validateMenuItem:(NSMenuItem *)o_mi
{
    BOOL bEnabled = TRUE;

    intf_thread_t * p_intf = VLCIntf;
    input_thread_t * p_input = vlc_object_find( p_intf, VLC_OBJECT_INPUT,
                                                       FIND_ANYWHERE );

    if( [[o_mi title] isEqualToString: _NS("Info")] )
    {
        if( p_input == NULL )
        {
            bEnabled = FALSE;
        }
    }
    if( p_input ) vlc_object_release( p_input );

    return( bEnabled );
}

@end

@implementation VLCInfo (NSTableDataSource)

- (int)outlineView:(NSOutlineView *)outlineView numberOfChildrenOfItem:(id)item
{
    return (item == nil) ? [[VLCInfoTreeItem rootItem] numberOfChildren] : [item numberOfChildren];
}

- (BOOL)outlineView:(NSOutlineView *)outlineView isItemExpandable:(id)item {
    return ([item numberOfChildren] > 0);
}

- (id)outlineView:(NSOutlineView *)outlineView child:(int)index ofItem:(id)item
{
    return (item == nil) ? [[VLCInfoTreeItem rootItem] childAtIndex:index] : [item childAtIndex:index];
}

- (id)outlineView:(NSOutlineView *)outlineView objectValueForTableColumn:(NSTableColumn *)tableColumn byItem:(id)item
{
    if ([[tableColumn identifier] isEqualToString:@"0"])
    {
        return (item == nil) ? @"" : (id)[item getName];
    }
    else
    {
        return (item == nil) ? @"" : (id)[item getValue];
    }
}


@end

@implementation VLCInfoTreeItem

static VLCInfoTreeItem *o_root_item = nil;

#define IsALeafNode ((id)-1)

- (id)initWithName: (NSString *)o_item_name value: (NSString *)o_item_value ID: (int)i_id parent:(VLCInfoTreeItem *)o_parent_item
{
    self = [super init];

    if( self != nil )
    {
        o_name = [o_item_name copy];
        o_value = [o_item_value copy];
        i_object_id = i_id;
        o_parent = o_parent_item;
        if( [[VLCMain sharedInstance] getInfo] != nil )
            p_item = [[[VLCMain sharedInstance] getInfo] getItem];
        else
            p_item = NULL;
    }
    return( self );
}

+ (VLCInfoTreeItem *)rootItem {
    if (o_root_item == nil) o_root_item = [[VLCInfoTreeItem alloc] initWithName:@"main" value: @"" ID: 0 parent:nil];
    return o_root_item;
}

- (void)dealloc
{
    if( o_children != IsALeafNode ) [o_children release];
    [o_name release];
    [super dealloc];
}

/* Creates and returns the array of children
 * Loads children incrementally */
- (NSArray *)children
{
    if (o_children == NULL)
    {
        int i;

        if( [[[VLCMain sharedInstance] getInfo] isItemInPlaylist: p_item] )
        {
            if( self == o_root_item )
            {
                vlc_mutex_lock( &p_item->input.lock );
                o_children = [[NSMutableArray alloc] initWithCapacity:
                                                p_item->input.i_categories];
                for (i = 0 ; i < p_item->input.i_categories ; i++)
                {
                    [o_children addObject:[[VLCInfoTreeItem alloc]
                        initWithName: [NSString stringWithUTF8String:
                            p_item->input.pp_categories[i]->psz_name]
                        value: @""
                        ID: i
                        parent: self]];
                }
                vlc_mutex_unlock( &p_item->input.lock );
            }
            else if( o_parent == o_root_item )
            {
                vlc_mutex_lock( &p_item->input.lock );
                o_children = [[NSMutableArray alloc] initWithCapacity:
                    p_item->input.pp_categories[i_object_id]->i_infos];

                for (i = 0 ; i < p_item->input.pp_categories[i_object_id]->i_infos ; i++)
                {
                    [o_children addObject:[[VLCInfoTreeItem alloc]
                    initWithName: [NSString stringWithUTF8String:
                            p_item->input.pp_categories[i_object_id]->pp_infos[i]->psz_name]
                        value: [NSString stringWithUTF8String:
                            p_item->input.pp_categories[i_object_id]->pp_infos[i]->psz_value]
                        ID: i
                        parent: self]];
                }
                vlc_mutex_unlock( &p_item->input.lock );
            }
            else
            {
                o_children = IsALeafNode;
            }
        }
    }
    return o_children;
}

- (NSString *)getName
{
    return o_name;
}

- (NSString *)getValue
{
    return o_value;
}

- (VLCInfoTreeItem *)childAtIndex:(int)i_index {
    return [[self children] objectAtIndex:i_index];
}

- (int)numberOfChildren {
    id i_tmp = [self children];
    return ( i_tmp == IsALeafNode ) ? (-1) : (int)[i_tmp count];
}

/*- (int)selectedPlaylistItem
{
    return i_item;
}
*/
- (void)refresh
{
    p_item = [[[VLCMain sharedInstance] getInfo] getItem];
    if( o_children != NULL )
    {
        [o_children release];
        o_children = NULL;
    }
}

@end

