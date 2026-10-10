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

#import "VLCLibraryMediaSourceViewNavigationStack.h"
#import "VLCMediaSource.h"
#import "VLCMediaSourceBaseDataSource.h"
#import "VLCMediaSourceTableViewSorting.h"

#import "extensions/NSPasteboardItem+VLCAdditions.h"
#import "extensions/NSString+Helpers.h"
#import "extensions/NSTableCellView+VLCAdditions.h"

#import "library/VLCInputItem.h"
#import "library/VLCInputNode.h"
#import "library/VLCInputNodePathControl.h"
#import "library/VLCInputNodePathControlItem.h"
#import "library/VLCLibraryImageCache.h"
#import "library/VLCLibraryTableCellView.h"
#import "library/VLCLibraryWindow.h"

#import "library/media-source/VLCMediaSourceCollectionViewItem.h"

#import "main/VLCMain.h"

#import "playqueue/VLCPlayQueueController.h"

#import "views/VLCFileDragRecognisingView.h"
#import "views/VLCImageView.h"
#import "views/VLCUIUnits.h"

NSString * const VLCMediaSourceDataSourceNodeChanged = @"VLCMediaSourceDataSourceNodeChanged";
NSString * const VLCMediaSourceDataSourceLoadingStarted = @"VLCMediaSourceDataSourceLoadingStarted";
NSString * const VLCMediaSourceDataSourceLoadingEnded = @"VLCMediaSourceDataSourceLoadingEnded";

@interface VLCMediaSourceDataSource()
{
    VLCInputItem *_childRootInput;
    // Table row order; grid items continue to use nodeToDisplay.children.
    NSArray<VLCInputNode *> *_tableViewNodes;
    BOOL _ignoringTableViewSelectionChanges;
}

@property (readwrite) dispatch_source_t observedPathDispatchSource;
@property (readwrite, strong, nullable, nonatomic) id<VLCMediaSourceNodeObservation> nodeObservation;
@property (readwrite, strong) NSMutableSet<NSValue *> *preparingInputItemIdentifiers;
@property (readwrite, strong) NSMutableSet<NSValue *> *preparedInputItemIdentifiers;
@property (readwrite, strong) NSMutableSet<NSValue *> *unavailableInputItemIdentifiers;
@property (readwrite, strong) NSMutableDictionary<NSValue *, NSNumber *> *childCountsByInputItemIdentifier;
@property (readwrite) dispatch_queue_t childCountQueue;

@end

static NSValue * _Nullable inputItemIdentifier(VLCInputItem * _Nullable const inputItem)
{
    return inputItem.vlcInputItem == NULL
        ? nil
        : [NSValue valueWithPointer:inputItem.vlcInputItem];
}

@implementation VLCMediaSourceDataSource

- (instancetype)initWithParentBaseDataSource:(VLCMediaSourceBaseDataSource *)parentBaseDataSource
{
    self = [super init];
    if (self) {
        self.parentBaseDataSource = parentBaseDataSource;
        self.preparingInputItemIdentifiers = NSMutableSet.set;
        self.preparedInputItemIdentifiers = NSMutableSet.set;
        self.unavailableInputItemIdentifiers = NSMutableSet.set;
        self.childCountsByInputItemIdentifier = NSMutableDictionary.dictionary;
        dispatch_queue_attr_t const childCountQueueAttributes =
            dispatch_queue_attr_make_with_qos_class(DISPATCH_QUEUE_SERIAL, QOS_CLASS_UTILITY, 0);
        self.childCountQueue = dispatch_queue_create("org.videolan.vlc.media-source-child-count",
                                                     childCountQueueAttributes);
        NSNotificationCenter * const notificationCenter = NSNotificationCenter.defaultCenter;
        [notificationCenter addObserver:self
                               selector:@selector(mediaSourceChildrenChanged:)
                                   name:VLCMediaSourceChildrenReset
                                 object:nil];
        [notificationCenter addObserver:self
                               selector:@selector(mediaSourceChildrenChanged:)
                                   name:VLCMediaSourceChildrenAdded
                                 object:nil];
        [notificationCenter addObserver:self
                               selector:@selector(mediaSourceChildrenChanged:)
                                   name:VLCMediaSourceChildrenRemoved
                                 object:nil];
        [notificationCenter addObserver:self
                               selector:@selector(preparseStateChanged:)
                                   name:VLCMediaSourcePreparsingStarted
                                 object:nil];
        [notificationCenter addObserver:self
                               selector:@selector(preparseStateChanged:)
                                   name:VLCMediaSourcePreparsingEnded
                                 object:nil];
    }
    return self;
}

- (void)dealloc
{
    [self.nodeObservation cancel];
    [NSNotificationCenter.defaultCenter removeObserver:self];
}

- (void)preparseStateChanged:(NSNotification *)notification
{
    if (notification.object != self.displayedMediaSource)
        return;

    VLCInputItem * const inputItem = notification.userInfo[VLCMediaSourcePreparseInputItemKey];
    NSValue * const identifier = inputItemIdentifier(inputItem);
    if (identifier == nil) {
        return;
    }

    const BOOL preparseStarted = [notification.name isEqualToString:VLCMediaSourcePreparsingStarted];
    if (preparseStarted) {
        [self.preparingInputItemIdentifiers addObject:identifier];
    } else {
        [self.preparingInputItemIdentifiers removeObject:identifier];

        NSNumber * const status = notification.userInfo[VLCMediaSourcePreparseStatusKey];
        if (status == nil || status.intValue == VLC_SUCCESS) {
            [self.preparedInputItemIdentifiers addObject:identifier];
            [self.unavailableInputItemIdentifiers removeObject:identifier];
        } else {
            [self.unavailableInputItemIdentifiers addObject:identifier];
            [self.preparedInputItemIdentifiers removeObject:identifier];
        }
    }

    NSValue * const displayedNodeIdentifier = inputItemIdentifier(self.nodeToDisplay.inputItem);
    if (![identifier isEqual:displayedNodeIdentifier]) {
        if (!preparseStarted) {
            [self reloadCountForInputItemIdentifier:identifier];
        }
        return;
    }

    if (!preparseStarted) {
        [self.nodeToDisplay clearChildrenCache];
    }

    if (self.parentBaseDataSource.childDataSource != self) {
        return;
    }

    if (!preparseStarted) {
        [self reloadData];
    }

    NSString * const loadingNotificationName = preparseStarted
        ? VLCMediaSourceDataSourceLoadingStarted
        : VLCMediaSourceDataSourceLoadingEnded;
    [NSNotificationCenter.defaultCenter postNotificationName:loadingNotificationName
                                                      object:self];
}

- (nullable NSNumber *)knownChildCountForInputNode:(VLCInputNode *)inputNode
{
    if (inputNode.inputItem.inputType != ITEM_TYPE_DIRECTORY) {
        return nil;
    }

    NSValue * const identifier = inputItemIdentifier(inputNode.inputItem);
    if ([self.unavailableInputItemIdentifiers containsObject:identifier]) {
        return nil;
    }

    const int numberOfChildren = inputNode.numberOfChildren;
    if (numberOfChildren > 0 || [self.preparedInputItemIdentifiers containsObject:identifier]) {
        return @(numberOfChildren);
    }
    return self.childCountsByInputItemIdentifier[identifier];
}

- (void)reloadCountForInputItemIdentifier:(NSValue *)identifier
{
    NSArray<VLCInputNode *> * const children = _tableViewNodes;
    const NSUInteger row = [children indexOfObjectPassingTest:^BOOL(VLCInputNode * const childNode,
                                                                    NSUInteger __unused idx,
                                                                    BOOL * const __unused stop) {
        return [inputItemIdentifier(childNode.inputItem) isEqual:identifier];
    }];
    if (row == NSNotFound) {
        return;
    }

    [children[row] clearChildrenCache];
    if (self.tableView.dataSource != self) {
        return;
    }

    VLCMediaSourceTableViewSorting * const sorting =
        [[VLCMediaSourceTableViewSorting alloc] initWithSortDescriptors:self.tableView.sortDescriptors];
    // A changed count can move rows when Count is the primary sort or a tie breaker.
    // Reload to apply that ordering and restore selection; otherwise refresh only the count cell.
    if ([sorting sortsByKey:VLCMediaSourceTableCountSortDescriptorKey]) {
        [self reloadData];
        return;
    }

    const NSInteger countColumn = [self.tableView columnWithIdentifier:VLCMediaSourceTableCountColumnIdentifier];
    if (self.tableView.hidden || countColumn == -1) {
        return;
    }

    [self.tableView reloadDataForRowIndexes:[NSIndexSet indexSetWithIndex:row]
                              columnIndexes:[NSIndexSet indexSetWithIndex:countColumn]];
}

- (void)clearDisplayedNodeChildrenCaches
{
    [self.nodeToDisplay clearChildrenCache];
    for (VLCInputNode * const childNode in self.nodeToDisplay.children) {
        [childNode clearChildrenCache];
    }
}

- (void)mediaSourceChildrenChanged:(NSNotification *)notification
{
    if (notification.object != self.displayedMediaSource)
        return;
    dispatch_async(dispatch_get_main_queue(), ^{
        [self clearDisplayedNodeChildrenCaches];
        // Navigation history retains inactive data sources that also receive these notifications.
        if (self.parentBaseDataSource.childDataSource != self) {
            return;
        }
        [self reloadData];
    });
}

- (void)setNodeToDisplay:(nonnull VLCInputNode*)nodeToDisplay
{
    NSAssert(nodeToDisplay, @"Nil node to display, will not set");
    _nodeToDisplay = nodeToDisplay;

    NSParameterAssert(self.parentBaseDataSource);
    [self.nodeObservation cancel];

    const __weak typeof(self) weakSelf = self;
    self.nodeObservation = [self.displayedMediaSource observeInputNode:nodeToDisplay
                                                              onChange:^(VLCMediaSourceNodeChange change) {
        const typeof(self) strongSelf = weakSelf;
        if (strongSelf == nil || strongSelf.parentBaseDataSource.childDataSource != strongSelf) {
            return;
        }
        switch (change) {
            case VLCMediaSourceNodeChangeChildrenUpdated:
                [strongSelf reloadData];
                break;
            case VLCMediaSourceNodeChangeInvalidated:
                [strongSelf.parentBaseDataSource homeButtonAction:strongSelf];
                break;
        }
    }];

    // Initial node configuration precedes attaching the shared views.
    if (self.tableView == nil) {
        [self updateTableViewNodes];
    } else {
        [self reloadData];
    }
    NSValue * const identifier = inputItemIdentifier(nodeToDisplay.inputItem);
    NSString * const loadingNotificationName =
        [self.preparingInputItemIdentifiers containsObject:identifier]
        ? VLCMediaSourceDataSourceLoadingStarted
        : VLCMediaSourceDataSourceLoadingEnded;
    [NSNotificationCenter.defaultCenter postNotificationName:loadingNotificationName
                                                      object:self];
}

- (BOOL)hasDisplayedItems
{
    return _nodeToDisplay.numberOfChildren > 0;
}

- (void)setupViews
{
    [self updateTableViewNodes];
    [self.tableView setDoubleAction:@selector(tableViewAction:)];
    [self.tableView setTarget:self];
    [self.tableView registerForDraggedTypes:@[NSFilenamesPboardType]];
    [self.tableView setDraggingSourceOperationMask:NSDragOperationCopy forLocal:NO];
    [self.tableView setDraggingSourceOperationMask:NSDragOperationCopy forLocal:YES];
}

- (nullable VLCInputNode *)inputNodeForIndexPath:(NSIndexPath *)indexPath
{
    VLCInputNode * const rootNode = self.nodeToDisplay;
    NSArray * const nodeChildren = rootNode.children;
    return nodeChildren ? nodeChildren[indexPath.item] : nil;
}

- (NSArray<VLCInputItem *> *)mediaSourceInputItemsAtIndexPaths:(NSSet<NSIndexPath *> *const)indexPaths
{
    NSMutableArray<VLCInputItem *> * const inputItems =
        [NSMutableArray arrayWithCapacity:indexPaths.count];

    for (NSIndexPath * const indexPath in indexPaths) {
        VLCInputNode * const inputNode = [self inputNodeForIndexPath:indexPath];
        if (!inputNode) {
            continue;
        }
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
    VLCMediaSourceCollectionViewItem * const viewItem =
        [collectionView makeItemWithIdentifier:VLCMediaSourceCollectionViewItemIdentifier
                                       forIndexPath:indexPath];

    VLCInputNode *rootNode = _nodeToDisplay;
    NSArray *nodeChildren = rootNode.children;
    if (nodeChildren == nil) {
        NSLog(@"No children for node %@, cannot provide correctly setup viewItem", rootNode);
        return viewItem;
    }
    VLCInputNode *childNode = nodeChildren[indexPath.item];
    VLCInputItem *childRootInput = childNode.inputItem;

    viewItem.representedItem = childRootInput;

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
    if (childNode) {
        [self performActionForNode:childNode allowPlayback:YES];
    }
}

- (NSSize)collectionView:(NSCollectionView *)collectionView
                  layout:(NSCollectionViewLayout *)collectionViewLayout
  sizeForItemAtIndexPath:(NSIndexPath *)indexPath
{
    VLCLibraryCollectionViewFlowLayout *collectionViewFlowLayout = (VLCLibraryCollectionViewFlowLayout*)collectionViewLayout;
    NSAssert(collectionViewLayout, @"This should be a flow layout and thus a valid pointer");
    return [VLCUIUnits adjustedCollectionViewItemSizeForCollectionView:collectionView
                                                            withLayout:collectionViewFlowLayout
                                                  withItemsAspectRatio:VLCLibraryCollectionViewItemAspectRatioDefaultItem];
}

#pragma mark - table view data source and delegation

- (void)updateTableViewNodes
{
    VLCMediaSourceTableViewSorting * const sorting =
        [[VLCMediaSourceTableViewSorting alloc] initWithSortDescriptors:self.tableView.sortDescriptors ?: @[]];
    _tableViewNodes = [sorting sortedItems:self.nodeToDisplay.children ?: @[]
                              valueForItem:^id(VLCInputNode *node, NSString *key) {
        VLCInputItem * const inputItem = node.inputItem;
        if ([key isEqualToString:VLCMediaSourceTableNameSortDescriptorKey]) {
            return inputItem.name ?: @"";
        }
        if ([key isEqualToString:VLCMediaSourceTableCountSortDescriptorKey]) {
            return [self knownChildCountForInputNode:node] ?: @(-1);
        }
        if ([key isEqualToString:VLCMediaSourceTableKindSortDescriptorKey]) {
            return inputItem.inputTypeDescription;
        }
        NSAssert([key isEqualToString:VLCMediaSourceTableTagsSortDescriptorKey],
                 @"Unexpected media source sort key: %@", key);
        return [inputItem.finderTags componentsJoinedByString:@", "] ?: @"";
    }];
}

- (NSInteger)numberOfRowsInTableView:(NSTableView *)tableView
{
    if (_nodeToDisplay) {
        return _tableViewNodes.count;
    }

    return 0;
}

- (void)tableView:(NSTableView *)tableView sortDescriptorsDidChange:(NSArray<NSSortDescriptor *> *)oldDescriptors
{
    [self reloadData];
}

- (NSView *)tableView:(NSTableView *)tableView
   viewForTableColumn:(NSTableColumn *)tableColumn
                  row:(NSInteger)row
{
    VLCInputNode * const inputNode = [self mediaSourceInputNodeAtRow:row];

    if ([tableColumn.identifier isEqualToString:VLCMediaSourceTableNameColumnIdentifier]) {
        VLCLibraryTableCellView * const cellView =
            [tableView makeViewWithIdentifier:VLCLibraryTableCellViewIdentifier owner:self];
        cellView.representedInputItem = inputNode.inputItem;
        cellView.primaryTitleTextField.hidden = YES;
        cellView.secondaryTitleTextField.hidden = YES;
        return cellView;
    }

     // Only present count view for folders
    if ([tableColumn.identifier isEqualToString:VLCMediaSourceTableCountColumnIdentifier] &&
        inputNode.inputItem.inputType != ITEM_TYPE_DIRECTORY) {
        return nil;
    }

    static NSString * const basicCellViewIdentifier = @"BasicTableCellViewIdentifier";
    NSTableCellView *cellView =
        [tableView makeViewWithIdentifier:basicCellViewIdentifier owner:self];
    if (cellView == nil) {
        cellView =
            [NSTableCellView tableCellViewWithIdentifier:basicCellViewIdentifier showingString:@""];
    }
    NSAssert(cellView, @"Cell view should not be nil");

    if ([tableColumn.identifier isEqualToString:VLCMediaSourceTableCountColumnIdentifier]) {
        NSValue * const identifier = inputItemIdentifier(inputNode.inputItem);
        NSNumber * const knownChildCount = [self knownChildCountForInputNode:inputNode];
        if ([self.unavailableInputItemIdentifiers containsObject:identifier]) {
            cellView.textField.stringValue = NSTR("Unavailable");
        } else if (knownChildCount != nil) {
            cellView.textField.stringValue =
                [NSString stringWithFormat:@"%li items", knownChildCount.integerValue];
        } else {
            cellView.textField.stringValue = NSTR("Loading…");
            if (![self.preparingInputItemIdentifiers containsObject:identifier]) {
                [self.preparingInputItemIdentifiers addObject:identifier];
                VLCMediaSource * const mediaSource = self.displayedMediaSource;
                dispatch_async(self.childCountQueue, ^{
                    NSError *error = nil;
                    NSNumber * const childCount = [mediaSource childCountForInputNode:inputNode
                                                                                error:&error];
                    if (childCount == nil && error == nil) {
                        [mediaSource preparseInputNodeWithinTree:inputNode];
                        return;
                    }

                    dispatch_async(dispatch_get_main_queue(), ^{
                        [self.preparingInputItemIdentifiers removeObject:identifier];
                        if (error != nil) {
                            [self.unavailableInputItemIdentifiers addObject:identifier];
                        } else if (childCount != nil) {
                            [self.unavailableInputItemIdentifiers removeObject:identifier];
                            self.childCountsByInputItemIdentifier[identifier] = childCount;
                        }
                        [self reloadCountForInputItemIdentifier:identifier];
                    });
                });
            }
        }
    } else if ([tableColumn.identifier isEqualToString:VLCMediaSourceTableKindColumnIdentifier]) {
        cellView.textField.stringValue = inputNode.inputItem.inputTypeDescription;
    } else if ([tableColumn.identifier isEqualToString:VLCMediaSourceTableTagsColumnIdentifier]) {
        static NSString * const basicCellViewIdentifier = @"BasicTableCellViewIdentifier";
        NSTableCellView *cellView = [tableView makeViewWithIdentifier:basicCellViewIdentifier
                                                                owner:self];
        if (cellView == nil) {
            cellView = [NSTableCellView tableCellViewWithIdentifier:basicCellViewIdentifier
                                                      showingString:@""];
        }

        NSArray<NSString *> * const tags = inputNode.inputItem.finderTags;
        if (tags.count > 0) {
            cellView.textField.stringValue = [tags componentsJoinedByString:@", "];
        } else {
            cellView.textField.stringValue = @"";
        }
        return cellView;
    }
    return cellView;
}

- (void)tableViewSelectionDidChange:(NSNotification *)notification
{
    if (_ignoringTableViewSelectionChanges) {
        return;
    }
    NSInteger selectedIndex = self.tableView.selectedRow;
    if (selectedIndex < 0) {
        return;
    }

    VLCInputNode *childNode = [self mediaSourceInputNodeAtRow:selectedIndex];
    if (childNode) {
        [self performActionForNode:childNode allowPlayback:NO];
    }
}

- (void)tableViewAction:(id)sender
{
    NSInteger selectedIndex = self.tableView.selectedRow;
    if (selectedIndex < 0) {
        return;
    }

    VLCInputNode *childNode = [self mediaSourceInputNodeAtRow:selectedIndex];
    if (childNode) {
        [self performActionForNode:childNode allowPlayback:YES];
    }
}

- (id<NSPasteboardWriting>)tableView:(NSTableView *)tableView pasteboardWriterForRow:(NSInteger)row
{
    VLCInputItem * const inputItem = [self mediaSourceInputItemAtRow:row];
    return [self.parentBaseDataSource pasteboardWriterForInputItem:inputItem];
}

- (NSDragOperation)tableView:(NSTableView *)tableView
                validateDrop:(id<NSDraggingInfo>)info
                 proposedRow:(NSInteger)row
       proposedDropOperation:(NSTableViewDropOperation)dropOperation
{
    const id propertyList = [info.draggingPasteboard propertyListForType:NSFilenamesPboardType];
    if (propertyList == nil) {
        return NSDragOperationNone;
    }

    [tableView setDropRow:-1 dropOperation:NSTableViewDropOn];
    return NSDragOperationCopy;
}

- (BOOL)tableView:(NSTableView *)tableView
       acceptDrop:(id<NSDraggingInfo>)info
              row:(NSInteger)row
    dropOperation:(NSTableViewDropOperation)dropOperation
{
    return [VLCFileDragRecognisingView
        handlePasteboardFromDragSessionAsPlayQueueItems:info.draggingPasteboard];
}

- (nullable VLCInputNode *)mediaSourceInputNodeAtRow:(NSInteger)tableViewRow
{
    if (_nodeToDisplay == nil) {
        return nil;
    }

    if (tableViewRow < 0 || (NSUInteger)tableViewRow >= _tableViewNodes.count) {
        return nil;
    }
    return _tableViewNodes[tableViewRow];
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

    if (childRootInput.inputType == ITEM_TYPE_DIRECTORY || childRootInput.inputType == ITEM_TYPE_NODE || childRootInput.inputType == ITEM_TYPE_PLAYLIST) {
        VLCInputNodePathControlItem *nodePathItem = [[VLCInputNodePathControlItem alloc] initWithInputNode:node];
        [self.pathControl appendInputNodePathControlItem:nodePathItem];

        NSError * const error = [self.displayedMediaSource preparseInputNodeWithinTree:node];
        if (error) {
            NSAlert * const alert = [NSAlert alertWithError:error];
            alert.alertStyle = NSAlertStyleCritical;
            [alert runModal];
            return;
        }
        self.nodeToDisplay = node;

        [self.navigationStack appendCurrentLibraryState];
    } else if (childRootInput.inputType == ITEM_TYPE_FILE && allowPlayback) {
        [VLCMain.sharedInstance.playQueueController addInputItem:childRootInput.vlcInputItem atPosition:-1 startPlayback:YES];
    }
}

- (void)reloadData
{
    NSParameterAssert(self.tableView.dataSource == self);

    if (!_collectionView.hidden) {
        [_collectionView reloadData];
    }

    // Row indexes change when sorting; preserve selection by input-item identity.
    NSMutableSet<NSValue *> * const selectedInputItemIdentifiers = NSMutableSet.set;
    [self.tableView.selectedRowIndexes enumerateIndexesUsingBlock:^(NSUInteger row, BOOL * const __unused stop) {
        if (row >= self->_tableViewNodes.count) {
            return;
        }

        NSValue * const identifier = inputItemIdentifier(self->_tableViewNodes[row].inputItem);
        if (identifier == nil) {
            return;
        }
        [selectedInputItemIdentifiers addObject:identifier];
    }];
    [self updateTableViewNodes];
    _ignoringTableViewSelectionChanges = YES;
    [self.tableView reloadData];
    NSMutableIndexSet * const selectedRows = NSMutableIndexSet.indexSet;
    [_tableViewNodes enumerateObjectsUsingBlock:^(VLCInputNode * const node, NSUInteger row, BOOL * const __unused stop) {
        if ([selectedInputItemIdentifiers containsObject:inputItemIdentifier(node.inputItem)]) {
            [selectedRows addIndex:row];
        }
    }];
    // Restoring selection must not trigger folder navigation.
    [self.tableView selectRowIndexes:selectedRows byExtendingSelection:NO];
    _ignoringTableViewSelectionChanges = NO;

    [NSNotificationCenter.defaultCenter postNotificationName:VLCMediaSourceDataSourceNodeChanged
                                                      object:self];
}

@end
