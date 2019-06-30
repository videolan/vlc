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
#import "media-source/VLCMediaSourceDeviceCollectionViewItem.h"
#import "media-source/VLCMediaSourceCollectionViewItem.h"
#import "media-source/VLCMediaSourceDataSource.h"

#import "main/VLCMain.h"
#import "views/VLCImageView.h"
#import "library/VLCInputItem.h"
#import "extensions/NSString+Helpers.h"

@interface VLCMediaSourceBaseDataSource () <NSCollectionViewDataSource, NSCollectionViewDelegate>
{
    NSArray *_mediaSources;
    VLCMediaSourceDataSource *_childDataSource;
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

- (void)setupViews
{
    self.collectionView.dataSource = self;
    self.collectionView.delegate = self;
    [self.collectionView registerClass:[VLCMediaSourceDeviceCollectionViewItem class] forItemWithIdentifier:VLCMediaSourceDeviceCellIdentifier];
    [self.collectionView registerClass:[VLCMediaSourceCollectionViewItem class] forItemWithIdentifier:VLCMediaSourceCellIdentifier];

    self.homeButton.action = @selector(homeButtonAction:);
    self.homeButton.target = self;
    self.pathControl.URL = nil;
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
    VLCMediaSourceDeviceCollectionViewItem *viewItem = [collectionView makeItemWithIdentifier:VLCMediaSourceDeviceCellIdentifier forIndexPath:indexPath];

    VLCMediaSource *mediaSource = _mediaSources[indexPath.section];
    VLCInputNode *rootNode = mediaSource.rootNode;
    NSArray *nodeChildren = rootNode.children;
    VLCInputNode *childNode = nodeChildren[indexPath.item];
    VLCInputItem *childRootInput = childNode.inputItem;
    viewItem.titleTextField.stringValue = childRootInput.name;

    NSURL *artworkURL = childRootInput.artworkURL;
    NSImage *placeholder = [NSImage imageNamed:@"NSApplicationIcon"];
    if (artworkURL) {
        [viewItem.mediaImageView setImageURL:artworkURL placeholderImage:placeholder];
    } else {
        viewItem.mediaImageView.image = placeholder;
    }

    return viewItem;
}

- (void)collectionView:(NSCollectionView *)collectionView didSelectItemsAtIndexPaths:(NSSet<NSIndexPath *> *)indexPaths
{
    NSIndexPath *indexPath = indexPaths.anyObject;
    if (!indexPath) {
        return;
    }
    VLCMediaSource *mediaSource = _mediaSources[indexPath.section];
    VLCInputNode *rootNode = mediaSource.rootNode;
    NSArray *nodeChildren = rootNode.children;
    VLCInputNode *childNode = nodeChildren[indexPath.item];
    VLCInputItem *childRootInput = childNode.inputItem;
    self.pathControl.URL = [NSURL URLWithString:[NSString stringWithFormat:@"vlc://%@", [childRootInput.name stringByAddingPercentEncodingWithAllowedCharacters:[NSCharacterSet URLPathAllowedCharacterSet]]]];
    _childDataSource = [[VLCMediaSourceDataSource alloc] init];
    _childDataSource.displayedMediaSource = mediaSource;
    _childDataSource.nodeToDisplay = childNode;
    _childDataSource.collectionView = self.collectionView;
    _childDataSource.pathControl = self.pathControl;
    self.collectionView.dataSource = _childDataSource;
    self.collectionView.delegate = _childDataSource;
    [self.collectionView reloadData];
}

- (IBAction)homeButtonAction:(id)sender
{
    self.collectionView.dataSource = self;
    self.collectionView.delegate = self;
    [self.collectionView reloadData];
    _childDataSource = nil;
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
    if (self.collectionView.dataSource == self) {
        NSInteger index = [_mediaSources indexOfObject:aNotification.object];
        [self.collectionView reloadSections:[NSIndexSet indexSetWithIndex:index]];
    } else {
        [self.collectionView reloadData];
    }
}

@end
