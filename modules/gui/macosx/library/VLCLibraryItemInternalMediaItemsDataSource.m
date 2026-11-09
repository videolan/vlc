/*****************************************************************************
 * VLCLibraryItemInternalMediaItemsDataSource.m: MacOS X interface module
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

#import "VLCLibraryItemInternalMediaItemsDataSource.h"

#import "extensions/NSPasteboardItem+VLCAdditions.h"
#import "library/VLCLibraryDataTypes.h"
#import "library/VLCLibraryModel.h"

const CGFloat VLCLibraryInternalMediaItemRowHeight = 40.;

@interface VLCLibraryItemInternalMediaItemsDataSource ()

@property (readwrite, atomic) id<VLCMediaLibraryItemProtocol> internalItem;
@property (readwrite, atomic) NSArray<VLCMediaLibraryMediaItem*> *internalMediaItems;

// Nil when there is no current track fetch. Each fetch creates a dictionary
// to save child updates for its incoming rows, which may be different objects
// from the displayed rows. Publishing the fetched rows or clearing the parent
// sets this back to nil. A newer fetch replaces the dictionary so the older
// result can be discarded.
@property (nonatomic) NSMutableDictionary<NSNumber *, VLCMediaLibraryMediaItem *> *pendingMediaUpdates;

@end

@implementation VLCLibraryItemInternalMediaItemsDataSource

- (instancetype)init
{
    self = [super init];
    if (self) {
        [NSNotificationCenter.defaultCenter addObserver:self
                                               selector:@selector(mediaItemUpdated:)
                                                   name:VLCLibraryModelMediaItemUpdated
                                                 object:nil];
    }
    return self;
}

- (void)mediaItemUpdated:(NSNotification *)notification
{
    VLCMediaLibraryMediaItem * const mediaItem = notification.object;
    self.pendingMediaUpdates[@(mediaItem.libraryID)] = mediaItem;
    for (VLCMediaLibraryMediaItem * const item in self.internalMediaItems) {
        if (item.libraryID == mediaItem.libraryID) {
            [item updatePlaybackStateFromMediaItem:mediaItem];
        }
    }
}

- (id<VLCMediaLibraryItemProtocol>)representedItem
{
    return self.internalItem;
}

- (void)setRepresentedItem:(id<VLCMediaLibraryItemProtocol>)representedItem
{
    if (representedItem == nil) {
        self.pendingMediaUpdates = nil;
        self.internalItem = nil;
        self.internalMediaItems = nil;
        return;
    }
    [self setRepresentedItem:representedItem withCompletion:nil];
}

- (void)setRepresentedItem:(id<VLCMediaLibraryItemProtocol>)item
            withCompletion:(nullable void (^)(void))completionHandler
{
    NSAssert(NSThread.isMainThread, @"Track lists are presented on the main thread");
    NSMutableDictionary<NSNumber *, VLCMediaLibraryMediaItem *> * const updates = NSMutableDictionary.dictionary;
    self.pendingMediaUpdates = updates;

    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INTERACTIVE, 0), ^{
        NSArray<VLCMediaLibraryMediaItem *> * const mediaItems = item.mediaItems;

        dispatch_async(dispatch_get_main_queue(), ^{
            if (self.pendingMediaUpdates != updates) {
                return;
            }
            for (VLCMediaLibraryMediaItem * const mediaItem in mediaItems) {
                VLCMediaLibraryMediaItem * const update = updates[@(mediaItem.libraryID)];
                if (update != nil) {
                    [mediaItem updatePlaybackStateFromMediaItem:update];
                }
            }
            self.pendingMediaUpdates = nil;
            self.internalItem = item;
            self.internalMediaItems = mediaItems;
            if (completionHandler != nil) {
                completionHandler();
            }
        });
    });
}

- (NSInteger)numberOfRowsInTableView:(NSTableView *)tableView
{
    return self.representedItem == nil ? 0 : self.internalMediaItems.count;
}

- (id<NSPasteboardWriting>)tableView:(NSTableView *)tableView pasteboardWriterForRow:(NSInteger)row
{
    const id<VLCMediaLibraryItemProtocol> libraryItem =
        [self libraryItemAtRow:row forTableView:tableView];
    return [NSPasteboardItem pasteboardItemWithLibraryItem:libraryItem];
}

- (id<VLCMediaLibraryItemProtocol>)libraryItemAtRow:(NSInteger)row
                                       forTableView:(NSTableView *)tableView
{
    if (row < 0 || (NSUInteger)row >= self.internalMediaItems.count) {
        return nil;
    }

    return self.internalMediaItems[row];
}

- (NSInteger)rowForLibraryItem:(id<VLCMediaLibraryItemProtocol>)libraryItem
{
    if (libraryItem == nil) {
        return NSNotFound;
    }

    return [self.internalMediaItems indexOfObjectPassingTest:^BOOL(VLCMediaLibraryMediaItem * const item, const NSUInteger __unused index, BOOL * const __unused stop) {
        return item.libraryID == libraryItem.libraryID;
    }];
}

- (VLCMediaLibraryParentGroupType)currentParentType
{
    if ([self.internalItem isKindOfClass:VLCMediaLibraryPlaylist.class]) {
        return VLCMediaLibraryParentGroupTypePlaylist;
    }
    return VLCMediaLibraryParentGroupTypeAlbum;
}

- (id<VLCMediaLibraryItemProtocol>)parentItemForTableView:(NSTableView *)tableView
{
    if ([self.internalItem isKindOfClass:VLCMediaLibraryPlaylist.class]) {
        return self.internalItem;
    }
    return nil;
}

@end
