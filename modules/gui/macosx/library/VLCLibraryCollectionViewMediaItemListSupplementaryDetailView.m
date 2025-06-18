/*****************************************************************************
 * VLCLibraryCollectionViewMediaItemListSupplementaryDetailView.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2021 VLC authors and VideoLAN
 *
 * Authors: Samuel Bassaly <shkshk90 # gmail -dot- com>
 *          Claudio Cambra <developer@claudiocambra.com>
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

#import "VLCLibraryCollectionViewMediaItemListSupplementaryDetailView.h"

#import "extensions/NSString+Helpers.h"
#import "extensions/NSFont+VLCAdditions.h"
#import "extensions/NSColor+VLCAdditions.h"
#import "extensions/NSView+VLCAdditions.h"

#import "library/VLCLibraryController.h"
#import "library/VLCLibraryDataTypes.h"
#import "library/VLCLibraryImageCache.h"
#import "library/VLCLibraryItemInternalMediaItemsDataSource.h"
#import "library/VLCLibraryModel.h"
#import "library/VLCLibraryMenuController.h"
#import "library/VLCLibraryRepresentedItem.h"
#import "library/VLCLibraryWindow.h"

#import "library/audio-library/VLCLibraryAlbumTracksTableViewDelegate.h"

#import "main/VLCMain.h"

#import "views/VLCImageView.h"

NSString * const VLCLibraryCollectionViewMediaItemListSupplementaryDetailViewIdentifier =
    @"VLCLibraryCollectionViewMediaItemListSupplementaryDetailViewIdentifier";
NSCollectionViewSupplementaryElementKind const VLCLibraryCollectionViewMediaItemListSupplementaryDetailViewKind =
    @"VLCLibraryCollectionViewMediaItemListSupplementaryDetailViewIdentifier";

@interface VLCLibraryCollectionViewMediaItemListSupplementaryDetailView ()
{
    VLCLibraryItemInternalMediaItemsDataSource *_tracksDataSource;
    VLCLibraryAlbumTracksTableViewDelegate *_tracksTableViewDelegate;
    VLCLibraryController *_libraryController;
}

@end

@implementation VLCLibraryCollectionViewMediaItemListSupplementaryDetailView

- (void)awakeFromNib
{
    _tracksDataSource = [[VLCLibraryItemInternalMediaItemsDataSource alloc] init];
    _tracksTableViewDelegate = [[VLCLibraryAlbumTracksTableViewDelegate alloc] init];

    self.tableView.dataSource = _tracksDataSource;
    self.tableView.delegate = _tracksTableViewDelegate;
    self.tableView.rowHeight = VLCLibraryInternalMediaItemRowHeight;

    self.titleTextField.font = NSFont.VLCLibrarySubsectionHeaderFont;
    self.primaryDetailTextButton.font = NSFont.VLCLibrarySubsectionSubheaderFont;
    self.secondaryDetailTextButton.font = NSFont.VLCLibrarySubsectionSubheaderFont;

    self.primaryDetailTextButton.action = @selector(primaryDetailAction:);
    self.secondaryDetailTextButton.action = @selector(secondaryDetailAction:);

    if (@available(macOS 10.14, *)) {
        self.primaryDetailTextButton.contentTintColor = NSColor.VLCAccentColor;
        self.secondaryDetailTextButton.contentTintColor = NSColor.secondaryLabelColor;
    }

    if(@available(macOS 10.12.2, *)) {
        self.playButton.bezelColor = NSColor.VLCAccentColor;
    }

    NSNotificationCenter * const notificationCenter = NSNotificationCenter.defaultCenter;
    [notificationCenter addObserver:self
                           selector:@selector(handleItemUpdated:)
                               name:VLCLibraryModelAlbumUpdated
                             object:nil];
    [notificationCenter addObserver:self
                           selector:@selector(handleItemUpdated:)
                               name:VLCLibraryModelArtistUpdated
                             object:nil];
    [notificationCenter addObserver:self
                           selector:@selector(handleItemUpdated:)
                               name:VLCLibraryModelGenreUpdated
                             object:nil];
    [notificationCenter addObserver:self
                           selector:@selector(handleItemUpdated:)
                               name:VLCLibraryModelPlaylistUpdated
                             object:nil];
}

- (void)handleItemUpdated:(NSNotification *)notification
{
    NSParameterAssert(notification);
    const id<VLCMediaLibraryItemProtocol> item = notification.object;
    if (self.representedItem == nil ||
        item == nil ||
        ![self.representedItem.item.class isKindOfClass:item.class] ||
        self.representedItem.item.libraryID != item.libraryID) {
        return;
    }

    VLCLibraryRepresentedItem * const representedItem = 
        [[VLCLibraryRepresentedItem alloc] initWithItem:item
                                             parentType:self.representedItem.parentType];
    self.representedItem = representedItem;
}

- (void)updateRepresentation
{
    NSAssert(self.representedItem != nil, @"no media item assigned for collection view item", nil);
    const id<VLCMediaLibraryItemProtocol> item = self.representedItem.item;

    self.titleTextField.stringValue = item.displayString;
    self.primaryDetailTextButton.title = item.primaryDetailString;
    self.secondaryDetailTextButton.title = item.secondaryDetailString;

    if ([item isKindOfClass:VLCMediaLibraryAlbum.class]) {
        const int year = [(VLCMediaLibraryAlbum *)item year];
        if (year != 0) {
            self.yearAndDurationTextField.stringValue =
                [NSString stringWithFormat:@"%u · %@", year, item.durationString];
        } else {
            self.yearAndDurationTextField.stringValue = item.durationString;
        }
    } else {
        self.yearAndDurationTextField.stringValue = item.durationString;
    }

    const BOOL primaryActionableDetail = item.primaryActionableDetail;
    const BOOL secondaryActionableDetail = item.secondaryActionableDetail;
    self.primaryDetailTextButton.enabled = primaryActionableDetail;
    self.secondaryDetailTextButton.enabled = secondaryActionableDetail;
    if (@available(macOS 10.14, *)) {
        self.primaryDetailTextButton.contentTintColor =
            primaryActionableDetail ? NSColor.VLCAccentColor : NSColor.secondaryLabelColor;
        self.secondaryDetailTextButton.contentTintColor =
            secondaryActionableDetail ? NSColor.secondaryLabelColor : NSColor.tertiaryLabelColor;
    }

    __weak typeof(self) weakSelf = self; // Prevent retain cycle
    [VLCLibraryImageCache thumbnailForLibraryItem:item withCompletion:^(NSImage * const thumbnail) {
        if (!weakSelf || weakSelf.representedItem.item != item) {
            return;
        }
        weakSelf.artworkImageView.image = thumbnail;
    }];

    [_tracksDataSource setRepresentedItem:item withCompletion:^{
        if (weakSelf) {
            [weakSelf.tableView reloadData];
        }
    }];
}

- (IBAction)playAction:(id)sender
{
    [self.representedItem play];
}

- (IBAction)enqueueAction:(id)sender
{
    [self.representedItem queue];
}

- (IBAction)primaryDetailAction:(id)sender
{
    const id<VLCMediaLibraryItemProtocol> item = self.representedItem.item;
    if (item == nil || !item.primaryActionableDetail) {
        return;
    }

    VLCLibraryWindow * const libraryWindow = VLCMain.sharedInstance.libraryWindow;
    const id<VLCMediaLibraryItemProtocol> libraryItem = item.primaryActionableDetailLibraryItem;
    [libraryWindow presentLibraryItem:libraryItem];
}

- (IBAction)secondaryDetailAction:(id)sender
{
    const id<VLCMediaLibraryItemProtocol> item = self.representedItem.item;
    if (item == nil || !item.secondaryActionableDetail) {
        return;
    }

    VLCLibraryWindow * const libraryWindow = VLCMain.sharedInstance.libraryWindow;
    const id<VLCMediaLibraryItemProtocol> libraryItem = item.secondaryActionableDetailLibraryItem;
    [libraryWindow presentLibraryItem:libraryItem];
}

@end
