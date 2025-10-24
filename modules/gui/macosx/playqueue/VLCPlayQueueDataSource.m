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
#import "main/VLCMain.h"
#import "playqueue/VLCPlayQueueController.h"
#import "playqueue/VLCPlayQueueTableCellView.h"
#import "playqueue/VLCPlayQueueItem.h"
#import "playqueue/VLCPlayQueueModel.h"
#import "views/VLCDragDropView.h"
#import "library/VLCLibraryDataTypes.h"
#import "library/VLCInputItem.h"
#import "windows/VLCOpenInputMetadata.h"

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
    [_tableView registerForDraggedTypes:@[VLCMediaLibraryMediaItemPasteboardType, VLCMediaLibraryMediaItemUTI, VLCPlaylistItemPasteboardType, NSFilenamesPboardType]];
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
        /* the following code saves us an instance of NSViewController which we don't need */
        NSNib *nib = [[NSNib alloc] initWithNibNamed:@"VLCPlayQueueTableCellView" bundle:nil];
        NSArray *topLevelObjects;
        if (![nib instantiateWithOwner:self topLevelObjects:&topLevelObjects]) {
            msg_Err(getIntf(), "Failed to load nib file to show playlist items");
            return nil;
        }

        for (id topLevelObject in topLevelObjects) {
            if ([topLevelObject isKindOfClass:[VLCPlayQueueTableCellView class]]) {
                cellView = topLevelObject;
                break;
            }
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

    /* check whether the receive data is a library item from the left-hand side */
    NSData *data = [info.draggingPasteboard dataForType:VLCMediaLibraryMediaItemPasteboardType];
    if (!data) {
        data = [info.draggingPasteboard dataForType:VLCMediaLibraryMediaItemUTI];
    }

    if (!data) {
        /* it's not, so check if it is a file handle from the Finder */
        id propertyList = [info.draggingPasteboard propertyListForType:NSFilenamesPboardType];
        if (propertyList == nil) {
            return NO;
        }

        NSArray *mediaPaths = [propertyList sortedArrayUsingSelector:@selector(caseInsensitiveCompare:)];
        NSUInteger mediaCount = [mediaPaths count];
        if (mediaCount > 0) {
            NSMutableArray *metadataArray = [NSMutableArray arrayWithCapacity:mediaCount];
            for (NSString *mediaPath in mediaPaths) {
                VLCOpenInputMetadata *inputMetadata;
                NSURL *url = [NSURL fileURLWithPath:mediaPath isDirectory:NO];
                if (!url) {
                    continue;
                }
                inputMetadata = [[VLCOpenInputMetadata alloc] init];
                inputMetadata.MRLString = url.absoluteString;
                [metadataArray addObject:inputMetadata];
            }
            [_playQueueController addPlayQueueItems:metadataArray];

            return YES;
        }
        return NO;
    }

    /* it is a media library item, so unarchive it and add it to the playlist */
    NSArray *array = nil;
    @try {
        array = [NSKeyedUnarchiver unarchiveObjectWithData:data];
    } @catch (NSException *exception) {
        if ([exception.name isEqualToString:NSInvalidArgumentException]) {
            msg_Err(getIntf(), "Failed to unarchive MediaLibrary Item: %s",
                    [[exception reason] UTF8String]);
            return NO;
        }
        @throw;
    }

    for (VLCMediaLibraryMediaItem *mediaItem in array) {
        [_playQueueController addInputItem:mediaItem.inputItem.vlcInputItem atPosition:row startPlayback:NO];
    }

    return YES;
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
