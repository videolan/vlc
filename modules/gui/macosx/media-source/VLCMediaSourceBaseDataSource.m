/*****************************************************************************
 * VLCMediaSourceBaseDataSource.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2019 VLC authors and VideoLAN
 *
 * Authors: Felix Paul Kühne <fkuehne # videolan -dot- org>
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

#import "VLCMediaSourceBaseDataSource.h"

#import "media-source/VLCMediaSourceProvider.h"
#import "media-source/VLCMediaSource.h"
#import "media-source/VLCMediaSourceCollectionViewItem.h"

#import "main/VLCMain.h"
#import "library/VLCInputItem.h"
#import "extensions/NSString+Helpers.h"

@interface VLCMediaSourceBaseDataSource ()
{
    NSArray *_mediaSources;
}
@end

@implementation VLCMediaSourceBaseDataSource

- (instancetype)init
{
    self = [super init];
    if (self) {
        _mediaSources = @[];
        NSNotificationCenter *notificationCenter = [NSNotificationCenter defaultCenter];
        [notificationCenter addObserver:self
                               selector:@selector(mediaSourceChildrenReset:)
                                   name:VLCMediaSourceChildrenReset
                                 object:nil];
        [notificationCenter addObserver:self
                               selector:@selector(mediaSourceChildrenAdded:)
                                   name:VLCMediaSourceChildrenAdded
                                 object:nil];
        [notificationCenter addObserver:self
                               selector:@selector(mediaSourceChildrenRemoved:)
                                   name:VLCMediaSourceChildrenRemoved
                                 object:nil];
    }
    return self;
}

- (void)dealloc
{
    [[NSNotificationCenter defaultCenter] removeObserver:self];
}

- (void)loadMediaSources
{
    NSArray *mediaSourcesOnLAN = [VLCMediaSourceProvider listOfMediaSourcesForCategory:SD_CAT_LAN];
    NSUInteger count = mediaSourcesOnLAN.count;
    if (count > 0) {
        for (NSUInteger x = 0; x < count; x++) {
            VLCMediaSource *mediaSource = mediaSourcesOnLAN[x];
            VLCInputNode *rootNode = [mediaSource rootNode];
            [mediaSource preparseInputItemWithinTree:rootNode.inputItem];
        }
    }
    _mediaSources = mediaSourcesOnLAN;
    [self.collectionView reloadData];
}

- (NSInteger)numberOfSectionsInCollectionView:(NSCollectionView *)collectionView
{
    return _mediaSources.count;
}

- (NSInteger)collectionView:(NSCollectionView *)collectionView
     numberOfItemsInSection:(NSInteger)section
{
    VLCMediaSource *mediaSource = _mediaSources[section];
    VLCInputNode *rootNode = mediaSource.rootNode;
    return rootNode.numberOfChildren;
}

- (NSCollectionViewItem *)collectionView:(NSCollectionView *)collectionView
     itemForRepresentedObjectAtIndexPath:(NSIndexPath *)indexPath
{
    VLCMediaSourceCollectionViewItem *viewItem = [collectionView makeItemWithIdentifier:VLCMediaSourceCellIdentifier forIndexPath:indexPath];

    VLCMediaSource *mediaSource = _mediaSources[indexPath.section];
    VLCInputNode *rootNode = mediaSource.rootNode;
    NSArray *nodeChildren = rootNode.children;
    VLCInputNode *childNode = nodeChildren[indexPath.item];
    VLCInputItem *childRootInput = childNode.inputItem;
    viewItem.titleTextField.stringValue = childRootInput.name;

    return viewItem;
}

- (void)collectionView:(NSCollectionView *)collectionView didSelectItemsAtIndexPaths:(NSSet<NSIndexPath *> *)indexPaths
{
    NSLog(@"media source selection changed: %@", indexPaths);
}

#pragma mark - VLCMediaSource Delegation

- (void)mediaSourceChildrenReset:(NSNotification *)aNotification
{
    msg_Dbg(getIntf(), "Reset nodes: %s", [[aNotification.object description] UTF8String]);
    [self reloadDataForNotification:aNotification];
}

- (void)mediaSourceChildrenAdded:(NSNotification *)aNotification
{
    msg_Dbg(getIntf(), "Received new nodes: %s", [[aNotification.object description] UTF8String]);
    [self reloadDataForNotification:aNotification];
}

- (void)mediaSourceChildrenRemoved:(NSNotification *)aNotification
{
    msg_Dbg(getIntf(), "Removed nodes: %s", [[aNotification.object description] UTF8String]);
    [self reloadDataForNotification:aNotification];
}

- (void)reloadDataForNotification:(NSNotification *)aNotification
{
    NSInteger index = [_mediaSources indexOfObject:aNotification.object];
    [self.collectionView reloadSections:[NSIndexSet indexSetWithIndex:index]];
}

@end
