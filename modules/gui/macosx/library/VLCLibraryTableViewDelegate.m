/*****************************************************************************
 * VLCLibraryTableViewDelegate.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2023 VLC authors and VideoLAN
 *
 * Authors: Claudio Cambra <developer@claudiocambra.com>
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

#import "VLCLibraryTableViewDelegate.h"

#import "extensions/NSColor+VLCAdditions.h"
#import "extensions/NSString+Helpers.h"

#import "library/VLCInputItem.h"
#import "library/VLCLibraryController.h"
#import "library/VLCLibraryDataTypes.h"
#import "library/VLCLibraryRepresentedItem.h"
#import "library/VLCLibraryTableCellView.h"
#import "library/VLCLibraryTableCellViewProtocol.h"
#import "library/VLCLibraryTableViewDataSource.h"

#include "main/VLCMain.h"

#import "playqueue/VLCPlayQueueController.h"

@implementation VLCLibraryTableViewDelegate

- (instancetype)init
{
    self = [super init];
    if (self) {
        self.cellViewIdentifier = @"VLCLibraryTableViewCellIdentifier";
        self.cellViewClass = [VLCLibraryTableCellView class];
    }
    return self;
}

- (NSView *)tableView:(NSTableView *)tableView viewForTableColumn:(NSTableColumn *)tableColumn row:(NSInteger)row
{
    if (![tableView.dataSource conformsToProtocol:@protocol(VLCLibraryTableViewDataSource)]) {
        return nil;
    }

    NSObject<VLCLibraryTableViewDataSource> * const vlcDataSource = (NSObject<VLCLibraryTableViewDataSource>*)tableView.dataSource;
    NSAssert(vlcDataSource != nil, @"Should be a valid data source");

    NSView<VLCLibraryTableCellViewProtocol> * cellView = (NSView<VLCLibraryTableCellViewProtocol> *)[tableView makeViewWithIdentifier:self.cellViewIdentifier owner:self];

    if (cellView == nil && [self.cellViewClass respondsToSelector:@selector(fromNibWithOwner:)]) {
        cellView = [self.cellViewClass fromNibWithOwner:self];
        cellView.identifier = self.cellViewIdentifier;
    }

    NSObject<VLCMediaLibraryItemProtocol> * const libraryItem = [vlcDataSource libraryItemAtRow:row forTableView:tableView];
    if (libraryItem != nil) {
        VLCLibraryRepresentedItem * const representedItem = 
            [[VLCLibraryRepresentedItem alloc] initWithItem:libraryItem
                                                 parentType:vlcDataSource.currentParentType];
        [cellView setRepresentedItem:representedItem];
    }
    return cellView;
}

- (NSArray<NSTableViewRowAction *> *)tableView:(NSTableView *)tableView
                              rowActionsForRow:(NSInteger)row
                                          edge:(NSTableRowActionEdge)edge
{
    if (![tableView.dataSource conformsToProtocol:@protocol(VLCLibraryTableViewDataSource)]) {
        return @[];
    }

    NSObject<VLCLibraryTableViewDataSource> * const vlcDataSource =
        (NSObject<VLCLibraryTableViewDataSource>*)tableView.dataSource;
    NSAssert(vlcDataSource != nil, @"Should be a valid data source");

    NSObject<VLCMediaLibraryItemProtocol> * const libraryItem =
        [vlcDataSource libraryItemAtRow:row forTableView:tableView];

    if (edge == NSTableRowActionEdgeLeading) {
        NSTableViewRowAction * const appendToPlayQueueAction =
            [NSTableViewRowAction rowActionWithStyle:NSTableViewRowActionStyleRegular
                                               title:_NS("Append to Play Queue")
                                             handler:^(NSTableViewRowAction * const __unused action, const NSInteger __unused row) {
                VLCLibraryController * const libraryController = VLCMain.sharedInstance.libraryController;
                for (VLCMediaLibraryMediaItem * const mediaItem in libraryItem.mediaItems) {
                    [libraryController appendItemToPlayQueue:mediaItem playImmediately:NO];
                }
        }];
        appendToPlayQueueAction.backgroundColor = NSColor.VLCAccentColor;
        return @[appendToPlayQueueAction];
    } else if (edge == NSTableRowActionEdgeTrailing) {
        const BOOL isFavorited = libraryItem.favorited;
        NSString * const displayString =
            isFavorited ? _NS("Remove from Favorites") : _NS("Add to Favorites");

        NSTableViewRowAction * const favoriteAction =
            [NSTableViewRowAction rowActionWithStyle:NSTableViewRowActionStyleRegular
                                               title:displayString
                                             handler:^(NSTableViewRowAction * const __unused action, const NSInteger __unused row) {
            [libraryItem setFavorite:!isFavorited];
        }];
        favoriteAction.backgroundColor =
            isFavorited ? NSColor.systemBlueColor : NSColor.systemRedColor;
        return @[favoriteAction];
    }
    return @[];
}

@end
