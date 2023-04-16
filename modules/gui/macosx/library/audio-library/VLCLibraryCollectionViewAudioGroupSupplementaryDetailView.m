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

#import "library/VLCLibraryDataTypes.h"
#import "extensions/NSFont+VLCAdditions.h"

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
    _audioGroupAlbumsTableViewDelegate = [[VLCLibraryAudioGroupTableViewDelegate alloc] init];

    _audioGroupAlbumsTableView.dataSource = _audioGroupAlbumsDataSource;
    _audioGroupAlbumsTableView.delegate = _audioGroupAlbumsTableViewDelegate;
    
    _audioGroupNameTextField.font = [NSFont VLCLibrarySupplementaryDetailViewTitleFont];
}

- (void)setRepresentedAudioGroup:(id<VLCMediaLibraryAudioGroupProtocol>)representedAudioGroup
{
    _representedAudioGroup = representedAudioGroup;
    [self updateRepresentation];
}

- (void)updateRepresentation
{
    if (_representedAudioGroup == nil) {
        NSAssert(1, @"no media item assigned for collection view item", nil);
        return;
    }

    _audioGroupNameTextField.stringValue = _representedAudioGroup.displayString;

    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INTERACTIVE, 0), ^{
        self->_audioGroupAlbumsDataSource.representedListOfAlbums = self->_representedAudioGroup.albums;

        dispatch_async(dispatch_get_main_queue(), ^{
            [self->_audioGroupAlbumsTableView reloadData];
        });
    });
}

@end
