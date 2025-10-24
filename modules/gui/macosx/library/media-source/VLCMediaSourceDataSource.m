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
#import "extensions/NSTableCellView+VLCAdditions.h"

#import "library/VLCInputItem.h"
#import "library/VLCInputNodePathControl.h"
#import "library/VLCInputNodePathControlItem.h"
#import "library/VLCLibraryImageCache.h"
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

- (instancetype)initWithParentBaseDataSource:(VLCMediaSourceBaseDataSource *)parentBaseDataSource
{
    self = [super init];
    if (self)
        self.parentBaseDataSource = parentBaseDataSource;
    return self;
}

- (dispatch_source_t)observeLocalUrl:(NSURL *)url
                      forVnodeEvents:(dispatch_source_vnode_flags_t)eventsFlags
                    withEventHandler:(dispatch_block_t)eventHandlerBlock
{
    const uintptr_t descriptor = open(url.path.UTF8String, O_EVTONLY);
    if (descriptor == (uintptr_t)-1) {
        return nil;
    }
    struct stat fileStat;
    const int statResult = fstat(descriptor, &fileStat);
    if (statResult == -1) {
        NSLog(@"Failed to stat file %@: %s", url.path, strerror(errno));
        return nil;
    }

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

    NSParameterAssert(self.parentBaseDataSource);
    if (self.parentBaseDataSource.mediaSourceMode == VLCMediaSourceModeLAN) {
        NSURL * const nodeUrl = [NSURL URLWithString:nodeToDisplay.inputItem.MRL];
        NSError * const error =
            [self.displayedMediaSource generateChildNodesForDirectoryNode:inputNode
                                                                  withUrl:nodeUrl];
        if (error) {
            NSAlert * const alert = [NSAlert alertWithError:error];
            alert.alertStyle = NSAlertStyleCritical;
            [alert runModal];
            return;
        }

        const __weak typeof(self) weakSelf = self;
        self.observedPathDispatchSource = [self observeLocalUrl:nodeUrl
                                                forVnodeEvents:DISPATCH_VNODE_WRITE | 
                                                               DISPATCH_VNODE_DELETE |
                                                               DISPATCH_VNODE_RENAME
                                            withEventHandler:^{
            const uintptr_t eventFlags =
                dispatch_source_get_data(weakSelf.observedPathDispatchSource);
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

    [self reloadData];
}

- (void)setupViews
{
    [self.tableView setDoubleAction:@selector(tableViewAction:)];
    [self.tableView setTarget:self];
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
    VLCMediaSourceCollectionViewItem *viewItem = [collectionView makeItemWithIdentifier:VLCMediaSourceCellIdentifier forIndexPath:indexPath];

    VLCInputNode *rootNode = _nodeToDisplay;
    NSArray *nodeChildren = rootNode.children;
    if (nodeChildren == nil) {
        NSLog(@"No children for node %@, cannot provide correctly setup viewItem", rootNode);
        return viewItem;
    }
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

- (NSView *)tableView:(NSTableView *)tableView
   viewForTableColumn:(NSTableColumn *)tableColumn
                  row:(NSInteger)row
{
    VLCInputNode * const inputNode = [self mediaSourceInputNodeAtRow:row];

    if ([tableColumn.identifier isEqualToString:@"VLCMediaSourceTableNameColumn"]) {
        VLCLibraryTableCellView * const cellView =
            [tableView makeViewWithIdentifier:VLCLibraryTableCellViewIdentifier owner:self];
        [VLCLibraryImageCache thumbnailForInputItem:inputNode.inputItem
                                     withCompletion:^(NSImage * _Nullable image) {
            cellView.representedImageView.image = image;
        }];
        cellView.primaryTitleTextField.hidden = YES;
        cellView.secondaryTitleTextField.hidden = YES;
        cellView.singlePrimaryTitleTextField.hidden = NO;
        cellView.singlePrimaryTitleTextField.stringValue = inputNode.inputItem.name;
        return cellView;
    }

     // Only present count view for folders
    if ([tableColumn.identifier isEqualToString:@"VLCMediaSourceTableCountColumn"] &&
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

    if ([tableColumn.identifier isEqualToString:@"VLCMediaSourceTableCountColumn"]) {
        if (inputNode.numberOfChildren == 0) {
            cellView.textField.stringValue = NSTR("Loading…");
            dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
                NSURL * const inputNodeUrl = [NSURL URLWithString:inputNode.inputItem.MRL];
                input_item_node_t * const p_inputNode = inputNode.vlcInputItemNode;
                NSError * const error =
                    [self.displayedMediaSource generateChildNodesForDirectoryNode:p_inputNode
                                                                          withUrl:inputNodeUrl];
                if (error)
                    return;

                dispatch_async(dispatch_get_main_queue(), ^{
                    cellView.textField.stringValue =
                        [NSString stringWithFormat:@"%i items", inputNode.numberOfChildren];
                });
            });
        } else {
            cellView.textField.stringValue =
                [NSString stringWithFormat:@"%i items", inputNode.numberOfChildren];
        }
    } else if ([tableColumn.identifier isEqualToString:@"VLCMediaSourceTableKindColumn"]) {
        NSString *typeName = NSTR("Unknown");
        switch (inputNode.inputItem.inputType) {
            case ITEM_TYPE_UNKNOWN:
                typeName = NSTR("Unknown");
                break;
            case ITEM_TYPE_FILE:
            {
                NSString * const filePath = inputNode.inputItem.MRL;
                NSString * const extension = filePath.pathExtension.lowercaseString;
                if (extension.length > 0) {
                    typeName = [NSString stringWithFormat:@"%@ File", extension.capitalizedString];

                    const CFStringRef extCF = (__bridge CFStringRef)extension;
                    const CFStringRef uti = UTTypeCreatePreferredIdentifierForTag(kUTTagClassFilenameExtension, extCF, NULL);
                    if (uti) {
                        CFStringRef descriptionCF = UTTypeCopyDescription(uti);
                        if (descriptionCF) {
                            typeName = CFBridgingRelease(descriptionCF);
                        }
                        CFRelease(uti);
                    }
                } else {
                    typeName = NSTR("File");
                }
                break;
            }
            case ITEM_TYPE_DIRECTORY:
                typeName = NSTR("Directory");
                break;
            case ITEM_TYPE_DISC:
                typeName = NSTR("Disc");
                break;
            case ITEM_TYPE_CARD:
                typeName = NSTR("Card");
                break;
            case ITEM_TYPE_STREAM:
                typeName = NSTR("Stream");
                break;
            case ITEM_TYPE_PLAYLIST:
                typeName = NSTR("Playlist");
                break;
            case ITEM_TYPE_NODE:
                typeName = NSTR("Node");
                break;
            case ITEM_TYPE_NUMBER:
                typeName = NSTR("Undefined");
                break;
        }
        cellView.textField.stringValue = typeName;
    }
    return cellView;
}

- (void)tableViewSelectionDidChange:(NSNotification *)notification
{
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

- (nullable VLCInputNode *)mediaSourceInputNodeAtRow:(NSInteger)tableViewRow
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
