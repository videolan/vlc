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

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#import "VLCLibraryMediaSourceViewNavigationStack.h"
#import "VLCMediaSourceBaseDataSource.h"
#import "VLCMediaSourceCollectionViewItem.h"
#import "VLCMediaSource.h"

#import "extensions/NSString+Helpers.h"

#import "library/VLCInputItem.h"
#import "library/VLCInputNodePathControl.h"
#import "library/VLCInputNodePathControlItem.h"
#import "library/VLCLibraryTableCellView.h"
#import "library/VLCLibraryUIUnits.h"
#import "library/VLCLibraryWindow.h"

#import "main/VLCMain.h"

#import "playqueue/VLCPlayQueueController.h"

#import "views/VLCImageView.h"

NSString * const VLCMediaSourceDataSourceNodeChanged = @"VLCMediaSourceDataSourceNodeChanged";

@interface VLCMediaSourceDataSource()
{
    VLCInputItem *_childRootInput;
}

@property (readwrite) dispatch_source_t observedPathDispatchSource;

@end

@implementation VLCMediaSourceDataSource

- (dispatch_source_t)observeLocalUrl:(NSURL *)url
                      forVnodeEvents:(dispatch_source_vnode_flags_t)eventsFlags
                    withEventHandler:(dispatch_block_t)eventHandlerBlock
{
    const uintptr_t descriptor = open(url.path.UTF8String, O_EVTONLY);
    if (descriptor == -1) {
        return nil;
    }
    struct stat fileStat;
    const int statResult = fstat(descriptor, &fileStat);

    const dispatch_queue_t globalQueue =
        dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0);
    const dispatch_source_t fileDispatchSource =
        dispatch_source_create(DISPATCH_SOURCE_TYPE_VNODE, descriptor, eventsFlags, globalQueue);
    dispatch_source_set_event_handler(fileDispatchSource, eventHandlerBlock);
    dispatch_source_set_cancel_handler(fileDispatchSource, ^{
        close(descriptor);
    });
    dispatch_resume(fileDispatchSource);
    return fileDispatchSource;
}

- (void)setNodeToDisplay:(nonnull VLCInputNode*)nodeToDisplay
{
    NSAssert(nodeToDisplay, @"Nil node to display, will not set");
    _nodeToDisplay = nodeToDisplay;

    input_item_node_t * const inputNode = nodeToDisplay.vlcInputItemNode;
    NSURL * const nodeUrl = [NSURL URLWithString:nodeToDisplay.inputItem.MRL];
    [self.displayedMediaSource generateChildNodesForDirectoryNode:inputNode withUrl:nodeUrl];

    [self reloadData];

    const __weak typeof(self) weakSelf = self;

    self.observedPathDispatchSource = [self observeLocalUrl:nodeUrl
                                             forVnodeEvents:DISPATCH_VNODE_WRITE | DISPATCH_VNODE_DELETE | DISPATCH_VNODE_RENAME
                                           withEventHandler:^{
        const uintptr_t eventFlags = dispatch_source_get_data(weakSelf.observedPathDispatchSource);
        if (eventFlags & DISPATCH_VNODE_DELETE || eventFlags & DISPATCH_VNODE_RENAME) {
            dispatch_async(dispatch_get_main_queue(), ^{
                [weakSelf.parentBaseDataSource homeButtonAction:weakSelf];
            });
        } else {
            dispatch_async(dispatch_get_main_queue(), ^{
                [weakSelf.displayedMediaSource generateChildNodesForDirectoryNode:inputNode
                                                                          withUrl:nodeUrl];
                [weakSelf reloadData];
            });
        }
    }];
}

- (void)setupViews
{
    [self.tableView setDoubleAction:@selector(tableViewAction:)];
    [self.tableView setTarget:self];
}

- (VLCInputNode *)inputNodeForIndexPath:(NSIndexPath *)indexPath
{
    VLCInputNode * const rootNode = self.nodeToDisplay;
    NSArray * const nodeChildren = rootNode.children;
    return nodeChildren[indexPath.item];
}

- (NSArray<VLCInputItem *> *)mediaSourceInputItemsAtIndexPaths:(NSSet<NSIndexPath *> *const)indexPaths
{
    NSMutableArray<VLCInputItem *> * const inputItems =
        [NSMutableArray arrayWithCapacity:indexPaths.count];

    for (NSIndexPath * const indexPath in indexPaths) {
        VLCInputNode * const inputNode = [self inputNodeForIndexPath:indexPath];
        VLCInputItem * const inputItem = inputNode.inputItem;
        [inputItems addObject:inputItem];
    }

    return inputItems.copy;
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
    if (indexPaths.count != 1) {
        return;
    }

    NSIndexPath * const indexPath = indexPaths.anyObject;
    if (!indexPath) {
        return;
    }
    VLCInputNode * const childNode = [self inputNodeForIndexPath:indexPath];
    [self performActionForNode:childNode allowPlayback:YES];
}

- (NSSize)collectionView:(NSCollectionView *)collectionView
                  layout:(NSCollectionViewLayout *)collectionViewLayout
  sizeForItemAtIndexPath:(NSIndexPath *)indexPath
{
    VLCLibraryCollectionViewFlowLayout *collectionViewFlowLayout = (VLCLibraryCollectionViewFlowLayout*)collectionViewLayout;
    NSAssert(collectionViewLayout, @"This should be a flow layout and thus a valid pointer");
    return [VLCLibraryUIUnits adjustedCollectionViewItemSizeForCollectionView:collectionView
                                                                   withLayout:collectionViewFlowLayout
                                                         withItemsAspectRatio:VLCLibraryCollectionViewItemAspectRatioDefaultItem];
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
    VLCLibraryTableCellView * const cellView = [tableView makeViewWithIdentifier:VLCLibraryTableCellViewIdentifier owner:self];
    cellView.representedInputItem = [self mediaSourceInputItemAtRow:row];
    return cellView;
}

- (void)tableViewSelectionDidChange:(NSNotification *)notification
{
    NSInteger selectedIndex = self.tableView.selectedRow;
    if (selectedIndex < 0) {
        return;
    }

    VLCInputNode *childNode = [self mediaSourceInputNodeAtRow:selectedIndex];
    [self performActionForNode:childNode allowPlayback:NO];
}

- (void)tableViewAction:(id)sender
{
    NSInteger selectedIndex = self.tableView.selectedRow;
    if (selectedIndex < 0) {
        return;
    }

    VLCInputNode *childNode = [self mediaSourceInputNodeAtRow:selectedIndex];
    [self performActionForNode:childNode allowPlayback:YES];
}

- (VLCInputNode*)mediaSourceInputNodeAtRow:(NSInteger)tableViewRow
{
    if (_nodeToDisplay == nil) {
        return nil;
    }

    VLCInputNode *rootNode = _nodeToDisplay;
    NSArray *nodeChildren = rootNode.children;

    if (nodeChildren == nil || nodeChildren.count == 0) {
        return nil;
    }

    return nodeChildren[tableViewRow];
}

- (VLCInputItem*)mediaSourceInputItemAtRow:(NSInteger)tableViewRow
{
    VLCInputNode *childNode = [self mediaSourceInputNodeAtRow:tableViewRow];

    if (childNode == nil) {
        return nil;
    }

    return childNode.inputItem;
}

#pragma mark - generic actions

- (void)performActionForNode:(VLCInputNode *)node allowPlayback:(BOOL)allowPlayback
{
    if(node == nil || node.inputItem == nil) {
        return;
    }

    VLCInputItem *childRootInput = node.inputItem;

    if (childRootInput.inputType == ITEM_TYPE_DIRECTORY || childRootInput.inputType == ITEM_TYPE_NODE) {
        VLCInputNodePathControlItem *nodePathItem = [[VLCInputNodePathControlItem alloc] initWithInputNode:node];
        [self.pathControl appendInputNodePathControlItem:nodePathItem];

        [self.displayedMediaSource preparseInputNodeWithinTree:node];
        self.nodeToDisplay = node;

        [self.navigationStack appendCurrentLibraryState];
    } else if (childRootInput.inputType == ITEM_TYPE_FILE && allowPlayback) {
        [VLCMain.sharedInstance.playQueueController addInputItem:childRootInput.vlcInputItem atPosition:-1 startPlayback:YES];
    }
}

- (void)reloadData
{
    if (!_collectionView.hidden) {
        [_collectionView reloadData];
    }

    if(!_tableView.hidden) {
        [_tableView reloadData];
    }

    [NSNotificationCenter.defaultCenter postNotificationName:VLCMediaSourceDataSourceNodeChanged
                                                      object:self];
}

@end
