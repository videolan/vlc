/*****************************************************************************
 * VLCLibraryCollectionViewAlbumSupplementaryDetailView.m: MacOS X interface module
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

#import "VLCLibraryCollectionViewAlbumSupplementaryDetailView.h"

#import "extensions/NSString+Helpers.h"
#import "extensions/NSFont+VLCAdditions.h"
#import "extensions/NSColor+VLCAdditions.h"
#import "extensions/NSView+VLCAdditions.h"

#import "library/VLCLibraryController.h"
#import "library/VLCLibraryDataTypes.h"
#import "library/VLCLibraryImageCache.h"
#import "library/VLCLibraryModel.h"
#import "library/VLCLibraryMenuController.h"
#import "library/VLCLibraryRepresentedItem.h"
#import "library/VLCLibraryWindow.h"

#import "library/audio-library/VLCLibraryAlbumTracksDataSource.h"
#import "library/audio-library/VLCLibraryAlbumTracksTableViewDelegate.h"
#import "library/audio-library/VLCLibraryAlbumTableCellView.h"

#import "main/VLCMain.h"

#import "views/VLCImageView.h"

NSString *const VLCLibraryCollectionViewAlbumSupplementaryDetailViewIdentifier = @"VLCLibraryCollectionViewAlbumSupplementaryDetailViewIdentifier";
NSCollectionViewSupplementaryElementKind const VLCLibraryCollectionViewAlbumSupplementaryDetailViewKind = @"VLCLibraryCollectionViewAlbumSupplementaryDetailViewIdentifier";

@interface VLCLibraryCollectionViewAlbumSupplementaryDetailView ()
{
    VLCLibraryAlbumTracksDataSource *_tracksDataSource;
    VLCLibraryAlbumTracksTableViewDelegate *_tracksTableViewDelegate;
    VLCLibraryController *_libraryController;
}

@end

@implementation VLCLibraryCollectionViewAlbumSupplementaryDetailView

- (void)awakeFromNib
{
    _tracksDataSource = [[VLCLibraryAlbumTracksDataSource alloc] init];
    _tracksTableViewDelegate = [[VLCLibraryAlbumTracksTableViewDelegate alloc] init];

    _albumTracksTableView.dataSource = _tracksDataSource;
    _albumTracksTableView.delegate = _tracksTableViewDelegate;
    _albumTracksTableView.rowHeight = VLCLibraryTracksRowHeight;

    _albumTitleTextField.font = NSFont.VLCLibrarySubsectionHeaderFont;
    self.albumPrimaryDetailTextButton.font = NSFont.VLCLibrarySubsectionSubheaderFont;
    self.albumSecondaryDetailTextButton.font = NSFont.VLCLibrarySubsectionSubheaderFont;

    self.albumPrimaryDetailTextButton.action = @selector(primaryDetailAction:);
    self.albumSecondaryDetailTextButton.action = @selector(secondaryDetailAction:);

    if (@available(macOS 10.14, *)) {
        self.albumPrimaryDetailTextButton.contentTintColor = NSColor.VLCAccentColor;
        self.albumSecondaryDetailTextButton.contentTintColor = NSColor.secondaryLabelColor;
    }

    if(@available(macOS 10.12.2, *)) {
        _playAlbumButton.bezelColor = NSColor.VLCAccentColor;
    }

    NSNotificationCenter * const notificationCenter = NSNotificationCenter.defaultCenter;
    [notificationCenter addObserver:self
                           selector:@selector(handleAlbumUpdated:)
                               name:VLCLibraryModelAlbumUpdated
                             object:nil];
}

- (void)handleAlbumUpdated:(NSNotification *)notification
{
    NSParameterAssert(notification);
    if (self.representedItem == nil) {
        return;
    }

    VLCMediaLibraryAlbum * const album = (VLCMediaLibraryAlbum *)notification.object;
    if (album == nil || self.representedItem.item.libraryID != album.libraryID) {
        return;
    }

    VLCLibraryRepresentedItem * const representedItem = [[VLCLibraryRepresentedItem alloc] initWithItem:album parentType:self.representedItem.parentType];
    self.representedItem = representedItem;
}

- (void)updateRepresentation
{
    NSAssert(self.representedItem != nil, @"no media item assigned for collection view item", nil);
    VLCMediaLibraryAlbum * const album = (VLCMediaLibraryAlbum *)self.representedItem.item;
    NSAssert(album != nil, @"represented item is not an album", nil);

    _albumTitleTextField.stringValue = album.displayString;
    _albumPrimaryDetailTextButton.title = album.artistName;
    _albumSecondaryDetailTextButton.title = album.genreString;
    _albumYearAndDurationTextField.stringValue = [NSString stringWithFormat:@"%u · %@", album.year, album.durationString];

    const BOOL primaryActionableDetail = album.primaryActionableDetail;
    const BOOL secondaryActionableDetail = album.secondaryActionableDetail;
    self.albumPrimaryDetailTextButton.enabled = primaryActionableDetail;
    self.albumSecondaryDetailTextButton.enabled = secondaryActionableDetail;
    if (@available(macOS 10.14, *)) {
        self.albumPrimaryDetailTextButton.contentTintColor = primaryActionableDetail ? NSColor.VLCAccentColor : NSColor.secondaryLabelColor;
        self.albumSecondaryDetailTextButton.contentTintColor = secondaryActionableDetail ? NSColor.secondaryLabelColor : NSColor.tertiaryLabelColor;
    }

    [VLCLibraryImageCache thumbnailForLibraryItem:album withCompletion:^(NSImage * const thumbnail) {
        self->_albumArtworkImageView.image = thumbnail;
    }];

    __weak typeof(self) weakSelf = self; // Prevent retain cycle
    [_tracksDataSource setRepresentedAlbum:album withCompletion:^{
        __strong typeof(self) strongSelf = weakSelf;

        if (strongSelf) {
            [strongSelf->_albumTracksTableView reloadData];
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
    VLCMediaLibraryAlbum * const album = (VLCMediaLibraryAlbum *)self.representedItem.item;
    if (album == nil || !album.primaryActionableDetail) {
        return;
    }

    VLCLibraryWindow * const libraryWindow = VLCMain.sharedInstance.libraryWindow;
    const id<VLCMediaLibraryItemProtocol> libraryItem = album.primaryActionableDetailLibraryItem;
    [libraryWindow presentLibraryItem:libraryItem];
}

- (IBAction)secondaryDetailAction:(id)sender
{
    VLCMediaLibraryAlbum * const album = (VLCMediaLibraryAlbum *)self.representedItem.item;
    if (album == nil || !album.secondaryActionableDetail) {
        return;
    }

    VLCLibraryWindow * const libraryWindow = VLCMain.sharedInstance.libraryWindow;
    const id<VLCMediaLibraryItemProtocol> libraryItem = album.secondaryActionableDetailLibraryItem;
    [libraryWindow presentLibraryItem:libraryItem];
}

@end
