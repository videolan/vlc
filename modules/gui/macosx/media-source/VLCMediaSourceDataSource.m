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
#import "library/VLCLibraryTableCellView.h"
#import "media-source/VLCMediaSourceCollectionViewItem.h"
#import "media-source/VLCMediaSource.h"
#import "main/VLCMain.h"
#import "playlist/VLCPlaylistController.h"
#import "views/VLCImageView.h"
#import "extensions/NSString+Helpers.h"

@interface VLCMediaSourceDataSource()
{
    VLCInputItem *_childRootInput;
}
@end

@implementation VLCMediaSourceDataSource

- (void)setNodeToDisplay:(VLCInputNode *)nodeToDisplay
{
    _nodeToDisplay = nodeToDisplay;

    _childRootInput = _nodeToDisplay.inputItem;
    [self.displayedMediaSource preparseInputItemWithinTree:_childRootInput];
}

- (void)setupViews
{
    [self.tableView setDoubleAction:@selector(tableViewAction:)];
    [self.tableView setTarget:self];
}

#pragma mark - collection view data source and delegation

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

    viewItem.representedInputItem = childRootInput;

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

    [self performActionForNode:childNode allowPlayback:YES];
}

#pragma mark - table view data source and delegation

- (NSInteger)numberOfRowsInTableView:(NSTableView *)tableView
{
    if (_nodeToDisplay) {
        return _nodeToDisplay.numberOfChildren;
    }

    return 0;
}

- (NSView *)tableView:(NSTableView *)tableView viewForTableColumn:(NSTableColumn *)tableColumn row:(NSInteger)row
{
    VLCLibraryTableCellView *cellView = [tableView makeViewWithIdentifier:@"VLCMediaSourceTableViewCellIdentifier" owner:self];

    if (cellView == nil) {
        /* the following code saves us an instance of NSViewController which we don't need */
        NSNib *nib = [[NSNib alloc] initWithNibNamed:@"VLCLibraryTableCellView" bundle:nil];
        NSArray *topLevelObjects;
        if (![nib instantiateWithOwner:self topLevelObjects:&topLevelObjects]) {
            NSAssert(1, @"Failed to load nib file to show audio library items");
            return nil;
        }

        for (id topLevelObject in topLevelObjects) {
            if ([topLevelObject isKindOfClass:[VLCLibraryTableCellView class]]) {
                cellView = topLevelObject;
                break;
            }
        }
        cellView.identifier = @"VLCMediaSourceTableViewCellIdentifier";
    }

    VLCInputNode *rootNode = _nodeToDisplay;
    NSArray *nodeChildren = rootNode.children;
    VLCInputNode *childNode = nodeChildren[row];
    VLCInputItem *childRootInput = childNode.inputItem;
    cellView.representedInputItem = childRootInput;

    return cellView;
}

- (void)tableViewSelectionDidChange:(NSNotification *)notification
{
    NSInteger selectedIndex = self.tableView.selectedRow;
    if (selectedIndex < 0) {
        return;
    }
    VLCInputNode *rootNode = self.nodeToDisplay;
    NSArray *nodeChildren = rootNode.children;
    VLCInputNode *childNode = nodeChildren[selectedIndex];

    [self performActionForNode:childNode allowPlayback:NO];
}

- (void)tableViewAction:(id)sender
{
    NSInteger selectedIndex = self.tableView.selectedRow;
    if (selectedIndex < 0) {
        return;
    }

    VLCInputNode *rootNode = self.nodeToDisplay;
    NSArray *nodeChildren = rootNode.children;
    VLCInputNode *childNode = nodeChildren[selectedIndex];

    [self performActionForNode:childNode allowPlayback:YES];
}

#pragma mark - generic actions

- (void)performActionForNode:(VLCInputNode *)node allowPlayback:(BOOL)allowPlayback
{
    VLCInputItem *childRootInput = node.inputItem;

    if (childRootInput.inputType == ITEM_TYPE_DIRECTORY || childRootInput.inputType == ITEM_TYPE_NODE) {
        self.pathControl.URL = [NSURL URLWithString:[self.pathControl.URL.path stringByAppendingPathComponent:[childRootInput.name stringByAddingPercentEncodingWithAllowedCharacters:[NSCharacterSet URLPathAllowedCharacterSet]]]];
        self.nodeToDisplay = node;
        [self reloadData];
    } else if (childRootInput.inputType == ITEM_TYPE_FILE && allowPlayback) {
        [[[VLCMain sharedInstance] playlistController] addInputItem:childRootInput.vlcInputItem atPosition:-1 startPlayback:YES];
    }
}

- (void)reloadData
{
    if (_gridViewMode) {
        [self.collectionView reloadData];
    } else {
        [self.tableView reloadData];
    }
}

@end
