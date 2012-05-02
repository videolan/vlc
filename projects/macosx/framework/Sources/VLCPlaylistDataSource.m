/*****************************************************************************
 * VLCPlaylistDataSource.m: VLC.framework VLCPlaylistDataSource implementation
 *****************************************************************************
 * Copyright (C) 2007 Pierre d'Herbemont
 * Copyright (C) 2007 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Pierre d'Herbemont <pdherbemont # videolan.org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#import <VLC/VLCPlaylistDataSource.h>
#import "VLCEventManager.h"

@implementation VLCPlaylistDataSource

- (id)init
{
    if (self = [super init])
    {
        playlist = nil;
        videoView = nil;
    }
    return self;
}

- (id)initWithPlaylist:(VLCPlaylist *)aPlaylist
{
    if (self = [super init])
    {

        playlist = [aPlaylist retain];

        [[NSNotificationCenter defaultCenter] addObserver: self selector: @selector(playlistDidChange:) name:VLCPlaylistItemAdded  object:nil];
        [[NSNotificationCenter defaultCenter] addObserver: self selector: @selector(itemDidAddSubitem:) name:VLCMediaSubItemAdded  object:nil];
        [[NSNotificationCenter defaultCenter] addObserver: self selector: @selector(itemDidChange:) name:VLCPlaylistItemChanged   object:nil];
        videoView = nil;
        outlineView = nil;

    }
    return self;
}

- (id)initWithPlaylist:(VLCPlaylist *)aPlaylist videoView:(VLCVideoView *)aVideoView;
{
    if (self = [super init])
    {
        playlist = [aPlaylist retain];

        [[NSNotificationCenter defaultCenter] addObserver: self selector: @selector(playlistDidChange:) name:VLCPlaylistItemAdded object:nil];
        [[NSNotificationCenter defaultCenter] addObserver: self selector: @selector(itemDidAddSubitem:) name:VLCMediaSubItemAdded object:nil];
        [[NSNotificationCenter defaultCenter] addObserver: self selector: @selector(itemDidChange:) name:VLCPlaylistItemChanged   object:nil];

        videoView = [aVideoView retain];
        /* Will be automatically set if an outline view ask us data,
         * be careful not to connect two outlineView to this object or this goes wrong. */
        outlineView = nil;
    }
    return self;
}

- (void)dealloc
{
    [[NSNotificationCenter defaultCenter] removeObserver: self];

    if (playlist)
        [playlist release];
    if (videoView)
        [videoView release];

    [super dealloc];
}

- (VLCPlaylist *)playlist
{
    return playlist;
}

- (VLCVideoView *)videoView
{
    return videoView;
}
@end



@implementation VLCPlaylistDataSource (OutlineViewDataSource)
- (BOOL)  outlineView: (NSOutlineView *)ov isItemExpandable: (id)item { return NO; }
- (int)   outlineView: (NSOutlineView *)ov numberOfChildrenOfItem:(id)item { return 0; }
- (id)    outlineView: (NSOutlineView *)ov child:(int)index ofItem:(id)item { return nil; }
- (id)    outlineView: (NSOutlineView *)ov objectValueForTableColumn:(NSTableColumn*)col byItem:(id)item { return nil; }
@end

@implementation VLCPlaylistDataSource (TableViewDropping)
/* Dummy implementation cause we need them */
- (int)numberOfRowsInTableView:(NSTableView *)aTableView { return 0; }
- (id)tableView:(NSTableView *)aTableView objectValueForTableColumn:(NSTableColumn *)aTableColumn row:(int)rowIndex {return nil;}

- (NSDragOperation)tableView:(NSTableView*)tv validateDrop:(id <NSDraggingInfo>)info proposedRow:(int)row proposedDropOperation:(NSTableViewDropOperation)op
{
    return NSDragOperationEvery; /* This is for now */
}

- (BOOL)tableView:(NSTableView *)aTableView acceptDrop:(id <NSDraggingInfo>)info
            row:(int)row dropOperation:(NSTableViewDropOperation)operation
{
    int i;
    NSArray  * droppedItems = [[info draggingPasteboard] propertyListForType: NSFilenamesPboardType];
 
    for (i = 0; i < [droppedItems count]; i++)
    {
        NSString * filename = [droppedItems objectAtIndex:i];
        
        [[self playlist] insertMedia:[VLCMedia mediaWithURL:filename] atIndex:row+i];
    }

}
@end

@interface NSObject (UnknownBindingsObject)
/* OutlineViewDataSourceDropping and bindings hack */
- (id)observedObject;
@end

@implementation VLCPlaylistDataSource (OutlineViewDataSourceDropping)
- (BOOL)outlineView:(NSOutlineView *)aOutlineView acceptDrop:(id <NSDraggingInfo>)info item:(id)item childIndex:(int)index
{
    NSArray  * droppedItems = [[info draggingPasteboard] propertyListForType: NSFilenamesPboardType];
    VLCPlaylist * aPlaylist;
    int i;

    if (!item)
        item = [self playlist]; /* The root object is our playlist */
    else
        item = [item observedObject];

    if (![item  isMemberOfClass:[VLCPlaylist class]])
        return NO;

    if (index < 0) /* No precise item given, put it as the first one */
        index = 0;

    aPlaylist = item;

    if (!droppedItems)
    {
        /* XXX: localization */
        NSRunCriticalAlertPanelRelativeToWindow(@"Error", @"Unable to drop the provided item.", @"OK", nil, nil, [outlineView window]);
        return NO;
    }

    for (i = 0; i < [droppedItems count]; i++)
    {
        NSString * filename = [droppedItems objectAtIndex:i];
        
        [aPlaylist insertMedia:[VLCMedia mediaWithURL:filename] atIndex:index+i];
    }
    return YES;
}

- (NSDragOperation)outlineView:(NSOutlineView *)aOutlineView validateDrop:(id <NSDraggingInfo>)info proposedItem:(id)item proposedChildIndex:(int)index
{
    return NSDragOperationEvery;
}
@end