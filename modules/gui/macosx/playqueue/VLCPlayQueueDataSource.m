/*****************************************************************************
 * VLCPlayQueueDataSource.m: MacOS X interface module
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

#import "VLCPlayQueueDataSource.h"

#import "extensions/NSString+Helpers.h"
#import "extensions/NSView+VLCAdditions.h"
#import "main/VLCMain.h"
#import "playqueue/VLCPlayQueueController.h"
#import "playqueue/VLCPlayQueueTableCellView.h"
#import "playqueue/VLCPlayQueueItem.h"
#import "playqueue/VLCPlayQueueModel.h"
#import "library/VLCLibraryDataTypes.h"
#import "views/VLCDragDropView.h"

static NSString *VLCPlayQueueCellIdentifier = @"VLCPlayQueueCellIdentifier";

@interface VLCPlayQueueDataSource ()
{
    VLCPlayQueueModel *_playQueueModel;
}
@end

@implementation VLCPlayQueueDataSource

- (void)setPlayQueueController:(VLCPlayQueueController *)playQueueController
{
    _playQueueController = playQueueController;
    _playQueueModel = _playQueueController.playQueueModel;
}

- (void)setCounterTextField:(NSTextField *)counterTextField
{
    _counterTextField = counterTextField;
    self.counterTextField.stringValue =
        [NSString stringWithFormat:@"%lu", _playQueueModel.numberOfPlayQueueItems];
}

- (void)prepareForUse
{
    [_tableView registerForDraggedTypes:@[
        VLCMediaLibraryMediaItemPasteboardType,
        VLCMediaLibraryMediaItemUTI,
        VLCPlaylistItemPasteboardType,
        NSPasteboardTypeURL,
        NSPasteboardTypeFileURL,
        NSPasteboardTypeString,
        NSFilenamesPboardType
    ]];
}

- (NSInteger)numberOfRowsInTableView:(NSTableView *)tableView
{
    return _playQueueModel.numberOfPlayQueueItems;
}

- (NSView *)tableView:(NSTableView *)tableView viewForTableColumn:(NSTableColumn *)tableColumn row:(NSInteger)row
{
    VLCPlayQueueTableCellView *cellView =
        [tableView makeViewWithIdentifier:VLCPlayQueueCellIdentifier owner:self];

    if (cellView == nil) {
        cellView = [VLCPlayQueueTableCellView fromNibWithOwner:self];
        if (cellView == nil) {
            msg_Err(getIntf(), "Failed to load nib file to show playlist items");
            return nil;
        }
        cellView.identifier = VLCPlayQueueCellIdentifier;
    }

    VLCPlayQueueItem * const item = [_playQueueModel playQueueItemAtIndex:row];
    if (!item) {
        msg_Err(getIntf(), "playlist model did not return an item for representation");
        return cellView;
    }

    cellView.representedPlayQueueItem = item;
    cellView.representsCurrentPlayQueueItem = _playQueueController.currentPlayQueueIndex == (size_t)row;

    return cellView;
}

- (void)playQueueUpdated
{
    const NSUInteger numberOfPlayQueueItems = _playQueueModel.numberOfPlayQueueItems;
    self.dragDropView.hidden = numberOfPlayQueueItems > 0 ? YES : NO;
    self.counterTextField.stringValue = [NSString stringWithFormat:@"%lu", numberOfPlayQueueItems];
    [_tableView reloadData];
}

- (id<NSPasteboardWriting>)tableView:(NSTableView *)tableView pasteboardWriterForRow:(NSInteger)row
{
    NSPasteboardItem * const pboardItem = [[NSPasteboardItem alloc] init];
    VLCPlayQueueItem * const playlistItem = [_playQueueModel playQueueItemAtIndex:row];
    [pboardItem setString:[@(playlistItem.uniqueID) stringValue] forType:VLCPlaylistItemPasteboardType];
    return pboardItem;
}

- (NSDragOperation)tableView:(NSTableView *)tableView
                validateDrop:(id<NSDraggingInfo>)info
                 proposedRow:(NSInteger)row
       proposedDropOperation:(NSTableViewDropOperation)dropOperation
{
    // Make drops at the end of the table go to the end.
    if (row == -1) {
        row = tableView.numberOfRows;
        dropOperation = NSTableViewDropAbove;
        [tableView setDropRow:row dropOperation:dropOperation];
    }

    // We don't ever want to drop onto a row, only between rows.
    if (dropOperation == NSTableViewDropOn) {
        [tableView setDropRow:row + 1 dropOperation:NSTableViewDropAbove];
    }

    return NSDragOperationCopy;
}

- (BOOL)tableView:(NSTableView *)tableView
       acceptDrop:(id<NSDraggingInfo>)info
              row:(NSInteger)row
    dropOperation:(NSTableViewDropOperation)dropOperation
{
    NSString *encodedIDtoMove = [info.draggingPasteboard stringForType:VLCPlaylistItemPasteboardType];
    if (encodedIDtoMove != nil) {
        int64_t uniqueID = [encodedIDtoMove integerValue];
        [_playQueueController moveItemWithID:uniqueID toPosition:row];
        return YES;
    }
    return [VLCFileDragRecognisingView
        handlePasteboardFromDragSessionAsPlayQueueItems:info.draggingPasteboard];
}

- (NSArray<NSTableViewRowAction *> *)tableView:(NSTableView *)tableView
                              rowActionsForRow:(NSInteger)row
                                          edge:(NSTableRowActionEdge)edge
{
    if (edge == NSTableRowActionEdgeTrailing) {
        VLCPlayQueueItem * const item = [_playQueueController.playQueueModel playQueueItemAtIndex:row];
        if (item == nil) {
            return @[];
        }

        NSTableViewRowAction * const removeAction =
            [NSTableViewRowAction rowActionWithStyle:NSTableViewRowActionStyleDestructive
                                                title:_NS("Remove from Play Queue")
                                               handler:^(NSTableViewRowAction * const __unused action, const NSInteger row) {
                NSIndexSet * const indices = [NSIndexSet indexSetWithIndex:row];
                [_playQueueController removeItemsAtIndexes:indices];
            }];
        return @[removeAction];
    }
    return @[];
}

- (void)scrollToCurrentPlayQueueItem
{
    [_tableView scrollRowToVisible:_playQueueController.currentPlayQueueIndex];
}

@end
