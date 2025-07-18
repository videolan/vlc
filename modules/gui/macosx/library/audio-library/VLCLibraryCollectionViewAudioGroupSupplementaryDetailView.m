/*****************************************************************************
 * VLCLibraryCollectionViewAudioGroupSupplementaryDetailView.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2022 VLC authors and VideoLAN
 *
 * Authors: Claudio Cambra <claudio.cambra@gmail.com>
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

#import "VLCLibraryCollectionViewAudioGroupSupplementaryDetailView.h"

#import "extensions/NSFont+VLCAdditions.h"

#import "library/VLCLibraryDataTypes.h"
#import "library/VLCLibraryModel.h"
#import "library/VLCLibraryRepresentedItem.h"

#import "library/audio-library/VLCLibraryAudioGroupDataSource.h"
#import "library/audio-library/VLCLibraryAudioGroupTableViewDelegate.h"

NSString *const VLCLibraryCollectionViewAudioGroupSupplementaryDetailViewIdentifier = @"VLCLibraryCollectionViewAudioGroupSupplementaryDetailViewIdentifier";
NSCollectionViewSupplementaryElementKind const VLCLibraryCollectionViewAudioGroupSupplementaryDetailViewKind = @"VLCLibraryCollectionViewAudioGroupSupplementaryDetailViewIdentifier";

@interface VLCLibraryCollectionViewAudioGroupSupplementaryDetailView ()
{
    VLCLibraryAudioGroupDataSource *_audioGroupAlbumsDataSource;
    VLCLibraryAudioGroupTableViewDelegate *_audioGroupAlbumsTableViewDelegate;
}

@end

@implementation VLCLibraryCollectionViewAudioGroupSupplementaryDetailView

- (void)awakeFromNib
{
    _audioGroupAlbumsDataSource = [[VLCLibraryAudioGroupDataSource alloc] init];
    _audioGroupAlbumsDataSource.tableViews = @[_audioGroupAlbumsTableView];

    _audioGroupAlbumsTableViewDelegate = [[VLCLibraryAudioGroupTableViewDelegate alloc] init];

    _audioGroupAlbumsTableView.dataSource = _audioGroupAlbumsDataSource;
    _audioGroupAlbumsTableView.delegate = _audioGroupAlbumsTableViewDelegate;

    _audioGroupNameTextField.font = NSFont.VLCLibrarySubsectionHeaderFont;

    NSNotificationCenter * const notificationCenter = NSNotificationCenter.defaultCenter;
    [notificationCenter addObserver:self
                           selector:@selector(handleAudioGroupUpdated:)
                               name:VLCLibraryModelAlbumUpdated
                             object:nil];
    [notificationCenter addObserver:self
                           selector:@selector(handleAudioGroupUpdated:)
                               name:VLCLibraryModelArtistUpdated
                             object:nil];
    [notificationCenter addObserver:self
                           selector:@selector(handleAudioGroupUpdated:)
                               name:VLCLibraryModelGenreUpdated
                             object:nil];
}

- (void)handleAudioGroupUpdated:(NSNotification *)notification
{
    NSParameterAssert(notification);

    if (self.representedItem == nil ||
        notification.object == nil ||
        ![notification.object conformsToProtocol:@protocol(VLCMediaLibraryAudioGroupProtocol)]) {

        return;
    }

    const id<VLCMediaLibraryAudioGroupProtocol> audioGroup = (id<VLCMediaLibraryAudioGroupProtocol>)notification.object;
    VLCLibraryRepresentedItem * const representedItem = [[VLCLibraryRepresentedItem alloc] initWithItem:audioGroup parentType:self.representedItem.parentType];
    self.representedItem = representedItem;
}

- (void)updateRepresentation
{
    NSAssert(self.representedItem != nil, @"no media item assigned for collection view item", nil);
    const id<VLCMediaLibraryAudioGroupProtocol> audioGroup = (id<VLCMediaLibraryAudioGroupProtocol>)self.representedItem.item;
    NSAssert(audioGroup != nil, @"audio group should not be nil!");

    _audioGroupNameTextField.stringValue = audioGroup.displayString;
    _audioGroupAlbumsDataSource.representedAudioGroup = audioGroup;
}

@end
