/*****************************************************************************
 * VLCCategoryListDataSource.h: VLC.app custom outline view
 *****************************************************************************
 * Copyright (C) 2007 Pierre d'Herbemont
 * Copyright (C) 2007 the VideoLAN team
 * $Id$
 *
 * Authors: Pierre d'Herbemont <pdherbemont # videolan.org>
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

#import <VLC/VLC.h>
#import "VLCCategoryListDataSource.h"

@implementation VLCCategoryListDataSource
- (id)init
{
    if( self = [super init] )
    {
        /* No need to release, cause we keep it around forever */
        mainCategories = [[NSArray arrayWithObjects: [VLCMediaLibrary sharedMediaLibrary],
                                                     @"Services Discovery",
                                                     @"Playlists", nil] retain];
    }
    return self;
}

- (id)outlineView:(NSOutlineView *)aOutlineView child:(int)index ofItem:(id)item
{
    if( !item )
    {
        return [mainCategories objectAtIndex: index];
    }
    if ([item isKindOfClass: [NSString class]])
    {
        if([item isEqualToString: @"Services Discovery"])
            return [[[VLCServicesDiscoverer sharedDiscoverer] services] objectAtIndex: index];
        if([item isEqualToString: @"Playlists"])
            return [[[[VLCMediaLibrary sharedMediaLibrary] playlists] objectAtIndex: index] retain]; /* XXX: this is a leak, but this code needs rework */
    }
    if ([item isKindOfClass: [VLCMediaLibrary class]])
        return nil;
#if 0
    if ([item isKindOfClass: [VLCMediaDiscoverer class]])
    {
        return nil;
    }
#endif
    if ([item isKindOfClass: [VLCPlaylist class]])
    {
        return [[[item sublists] objectAtIndex: index] retain];  /* XXX: this is a leak, but this code needs rework */
    }
    return nil;
}

- (BOOL)outlineView:(NSOutlineView *)aOutlineView isItemExpandable:(id)item
{
    if (!item)
        return YES;

    if ([item isKindOfClass: [NSString class]])
    {
        if([item isEqualToString: @"Services Discovery"])
            return YES;
        if([item isEqualToString: @"Playlists"])
            return YES;
    }
    if ([item isKindOfClass: [VLCMediaLibrary class]])
        return NO;

    if ([item isKindOfClass: [VLCPlaylist class]])
    {
        return [[item sublists] count] > 0;
    }

    return NO;
}

- (int)outlineView:(NSOutlineView *)aOutlineView numberOfChildrenOfItem:(id)item
{
    if (!item)
        return [mainCategories count];
    if ([item isKindOfClass: [NSString class]])
    {
        if([item isEqualToString: @"Playlists"])
            return [[[VLCMediaLibrary sharedMediaLibrary] playlists] count];
        if([item isEqualToString: @"Services Discovery"])
            return [[[VLCServicesDiscoverer sharedDiscoverer] services] count];
    }
    if ([item isKindOfClass: [VLCPlaylist class]])
        return [[item playlists] count];

    return 0;
}

- (id)outlineView:(NSOutlineView *)aOutlineView objectValueForTableColumn:(NSTableColumn *)tableColumn byItem:(id)item
{
    if (!item)
        return nil;
    if( [[tableColumn identifier] isEqualToString:@"name"] )
    {
        if ( [item isKindOfClass: [NSString class]] )
            return item;
        if ( [item isKindOfClass: [VLCPlaylist class]])
            return [[[item providerMedia] metaInformation] objectForKey:VLCMetaInformationTitle];
        if ( [item isKindOfClass: [VLCMediaDiscoverer class]])
            return [item localizedName];
        if ( [item isKindOfClass: [VLCMediaLibrary class]])
            return @"Library";
    }
    else if ( [item isKindOfClass: [NSString class]] && [[tableColumn identifier] isEqualToString:@"icon"])
        return nil;

    return nil;
}
@end

@implementation VLCCategoryListDataSource (OutlineViewDelegating)
- (BOOL)outlineView:(NSOutlineView *)outlineView shouldSelectItem:(id)item
{
    if ([item isKindOfClass: [NSString class]])
    {
        if([item isEqualToString: @"Library"])
            return YES;
        if([item isEqualToString: @"Services Discovery"])
            return NO;
        if([item isEqualToString: @"Playlists"])
            return NO;
    }
    return YES;
}
- (void)outlineView:(NSOutlineView *)outlineView willDisplayCell:(id)cell forTableColumn:(NSTableColumn *)tableColumn item:(id)item
{
    if ([item isKindOfClass: [VLCMediaDiscoverer class]])
        [cell setImage: [NSImage imageNamed:@"vlc_stream_16px"]];
    else
        [cell setImage: nil];
}
@end
