/*****************************************************************************
 * VLCMediaSourceDataSource.m: MacOS X interface module
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

#import "VLCMediaSourceDataSource.h"

#import "library/VLCInputItem.h"
#import "media-source/VLCMediaSourceCollectionViewItem.h"
#import "media-source/VLCMediaSource.h"
#import "main/VLCMain.h"
#import "playlist/VLCPlaylistController.h"

@interface VLCMediaSourceDataSource()
{
    VLCInputItem *_childRootInput;
    VLCMediaSourceDataSource *_childDataSource;
}
@end

@implementation VLCMediaSourceDataSource

- (void)setNodeToDisplay:(VLCInputNode *)nodeToDisplay
{
    _nodeToDisplay = nodeToDisplay;

    _childRootInput = _nodeToDisplay.inputItem;
    [self.displayedMediaSource preparseInputItemWithinTree:_childRootInput];
}

- (NSInteger)numberOfSectionsInCollectionView:(NSCollectionView *)collectionView
{
    return 1;
}

- (NSInteger)collectionView:(NSCollectionView *)collectionView
     numberOfItemsInSection:(NSInteger)section
{
    if (_nodeToDisplay) {
        return _nodeToDisplay.numberOfChildren;
    }

    return 0;
}

- (NSCollectionViewItem *)collectionView:(NSCollectionView *)collectionView
     itemForRepresentedObjectAtIndexPath:(NSIndexPath *)indexPath
{
    VLCMediaSourceCollectionViewItem *viewItem = [collectionView makeItemWithIdentifier:VLCMediaSourceCellIdentifier forIndexPath:indexPath];

    VLCInputNode *rootNode = _nodeToDisplay;
    NSArray *nodeChildren = rootNode.children;
    VLCInputNode *childNode = nodeChildren[indexPath.item];
    VLCInputItem *childRootInput = childNode.inputItem;
    viewItem.titleTextField.stringValue = childRootInput.name;

    return viewItem;
}

- (void)collectionView:(NSCollectionView *)collectionView didSelectItemsAtIndexPaths:(NSSet<NSIndexPath *> *)indexPaths
{
    NSIndexPath *indexPath = indexPaths.anyObject;
    if (!indexPath) {
        return;
    }
    VLCInputNode *rootNode = self.nodeToDisplay;
    NSArray *nodeChildren = rootNode.children;
    VLCInputNode *childNode = nodeChildren[indexPath.item];
    VLCInputItem *childRootInput = childNode.inputItem;

    if (childRootInput.inputType == ITEM_TYPE_DIRECTORY) {
        self.pathControl.URL = [NSURL URLWithString:[self.pathControl.URL.path stringByAppendingPathComponent:[childRootInput.name stringByAddingPercentEncodingWithAllowedCharacters:[NSCharacterSet URLPathAllowedCharacterSet]]]];
        self.nodeToDisplay = childNode;
        [self.collectionView reloadData];
    } else if (childRootInput.inputType == ITEM_TYPE_FILE) {
        [[[VLCMain sharedInstance] playlistController] addInputItem:childRootInput.vlcInputItem atPosition:-1 startPlayback:YES];
    } else {
        NSAssert(1, @"unhandled input type when browsing media source hierarchy %i", childRootInput.inputType);
        msg_Warn(getIntf(), "unhandled input type when browsing media source hierarchy %i", childRootInput.inputType);
    }
}

@end
