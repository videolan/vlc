/*****************************************************************************
 * VLCController.h: VLC.app main controller
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

/* this code needs rework, but it does nice thing */

#import <VLC/VLC.h>
#import "VLCController.h"
#import "VLCCategoryCell.h"
NSArrayController * treeController;
VLCPlaylist * currentPlaylist;
@implementation VLCController
- (void)awakeFromNib
{
    [NSApp setDelegate:self];
    [detailList setTarget:self];
    [detailList setDoubleAction:@selector(detailListItemDoubleClicked:)];

    [detailList setDataSource: [[VLCPlaylistDataSource alloc] initWithPlaylist:nil videoView:videoView]];
    [detailList registerForDraggedTypes: [NSArray arrayWithObjects:NSFilenamesPboardType, NSURLPboardType, nil]];

    [categoryList setTarget:self];
    [categoryList setAction:@selector(categoryListItemClicked:)];
    [[[categoryList tableColumns] objectAtIndex:0] setDataCell:[[[VLCCategoryCell alloc] init] autorelease]];
}

- (void)detailListItemDoubleClicked:(id)sender
{
    if( [sender isKindOfClass:[NSTableView class]] && [sender selectedRow] >= 0)
    {
        [videoView setPlaylist: currentPlaylist];
        [videoView playMedia: [[treeController selectedObjects] objectAtIndex: 0]];
    }

}

- (void)categoryListItemClicked:(id)sender
{
    VLCPlaylist * aPlaylist;
    id selectedItem = [sender itemAtRow:[sender selectedRow]];

    if([selectedItem isKindOfClass:[VLCMediaLibrary class]])
        aPlaylist = [selectedItem allMedia];
    else if([selectedItem isKindOfClass:[VLCMediaDiscoverer class]])
        aPlaylist = [selectedItem playlist];
    else if([selectedItem isKindOfClass:[VLCPlaylist class]])
        aPlaylist = [selectedItem flatPlaylist];
    else
    {
        return;
    }
    currentPlaylist= aPlaylist;
    treeController = [[NSArrayController alloc] init]; /* XXX: We leak */
    
    //[treeController setAutomaticallyPreparesContent: YES];
    [[detailList dataSource] release];
    NSLog(@"currentPlaylist %@", currentPlaylist);
    [detailList setDataSource: [[VLCPlaylistDataSource alloc] initWithPlaylist:aPlaylist videoView:videoView]];
    //[treeController setContent: [[VLCMediaLibrary sharedMediaLibrary] allMedia]];
    [treeController bind:@"contentArray" toObject: aPlaylist
                    withKeyPath:@"items" options:nil];
	NSTableColumn *tableColumn;
    NSMutableDictionary *bindingOptions = [NSMutableDictionary dictionary];

    [bindingOptions setObject:@"metaInformation.title contains[c] $value" forKey:NSPredicateFormatBindingOption];
    [bindingOptions setObject:@"No Title" forKey:NSDisplayNameBindingOption];

    [detailSearchField bind:@"predicate" toObject: treeController
                    withKeyPath:@"filterPredicate" options:bindingOptions];

    //[treeController setChildrenKeyPath:@"subitems.items"];
	
    [bindingOptions removeAllObjects];
	[bindingOptions setObject:@"Search" forKey:@"NSNullPlaceholder"];
		
	/* binding for "title" column */
    tableColumn = [detailList tableColumnWithIdentifier:@"title"];
	
	[tableColumn bind:@"value" toObject: treeController
		  withKeyPath:@"arrangedObjects.metaInformation.title" options:bindingOptions];

}
@end

@implementation VLCController (NSAppDelegating)
- (void)applicationDidFinishLaunching:(NSNotification *)aNotification
{
    [self categoryListItemClicked: categoryList];
}
@end
