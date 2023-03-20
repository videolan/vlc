/*****************************************************************************
 * VLCLibraryCollectionViewAlbumSupplementaryDetailView.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2021 VLC authors and VideoLAN
 *
 * Authors: Samuel Bassaly <shkshk90 # gmail -dot- com>
 *          Claudio Cambra <claudio.cambra@gmail.com>
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

#import "main/VLCMain.h"
#import "library/VLCLibraryController.h"
#import "library/VLCLibraryDataTypes.h"
#import "library/VLCLibraryModel.h"
#import "library/VLCLibraryMenuController.h"
#import "views/VLCImageView.h"
#import "extensions/NSString+Helpers.h"
#import "extensions/NSFont+VLCAdditions.h"
#import "extensions/NSColor+VLCAdditions.h"
#import "extensions/NSView+VLCAdditions.h"

#import "library/audio-library/VLCLibraryAlbumTracksDataSource.h"
#import "library/audio-library/VLCLibraryAlbumTracksTableViewDelegate.h"
#import "library/audio-library/VLCLibraryAlbumTableCellView.h"

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

@synthesize representedAlbum = _representedAlbum;
@synthesize albumTitleTextField = _albumTitleTextField;
@synthesize albumDetailsTextField = _albumDetailsTextField;
@synthesize albumArtworkImageView = _albumArtworkImageView;
@synthesize albumTracksTableView = _albumTracksTableView;

- (void)awakeFromNib
{
    _tracksDataSource = [[VLCLibraryAlbumTracksDataSource alloc] init];
    _tracksTableViewDelegate = [[VLCLibraryAlbumTracksTableViewDelegate alloc] init];

    _albumTracksTableView.dataSource = _tracksDataSource;
    _albumTracksTableView.delegate = _tracksTableViewDelegate;
    _albumTracksTableView.rowHeight = VLCLibraryTracksRowHeight;
    
    _albumTitleTextField.font = [NSFont VLCLibrarySupplementaryDetailViewTitleFont];
    _albumDetailsTextField.font = [NSFont VLCLibrarySupplementaryDetailViewSubtitleFont];

    _albumDetailsTextField.textColor = [NSColor VLCAccentColor];

    if(@available(macOS 10.12.2, *)) {
        _playAlbumButton.bezelColor = [NSColor VLCAccentColor];
    }
}

- (void)setRepresentedAlbum:(VLCMediaLibraryAlbum *)representedAlbum
{
    _representedAlbum = representedAlbum;
    [self updateRepresentation];
}

- (void)updateRepresentation
{
    if (_representedAlbum == nil) {
        NSAssert(1, @"no media item assigned for collection view item", nil);
        return;
    }

    _albumTitleTextField.stringValue = _representedAlbum.displayString;
    _albumDetailsTextField.stringValue = _representedAlbum.artistName;
    _albumYearAndDurationTextField.stringValue = [NSString stringWithFormat:@"%u · %@", _representedAlbum.year, _representedAlbum.durationString];
    _albumArtworkImageView.image = _representedAlbum.smallArtworkImage;
    _tracksDataSource.representedAlbum = _representedAlbum;

    [_albumTracksTableView reloadData];
}

- (IBAction)playAction:(id)sender
{
    if (!_libraryController) {
        _libraryController = [[VLCMain sharedInstance] libraryController];
    }

    // We want to add all the tracks to the playlist but only play the first one immediately,
    // otherwise we will skip straight to the last track of the last album from the artist
    __block BOOL playImmediately = YES;
    [_representedAlbum iterateMediaItemsWithBlock:^(VLCMediaLibraryMediaItem* mediaItem) {
        [_libraryController appendItemToPlaylist:mediaItem playImmediately:playImmediately];

        if(playImmediately) {
            playImmediately = NO;
        }
    }];
}

- (IBAction)enqueueAction:(id)sender
{
    if (!_libraryController) {
        _libraryController = [[VLCMain sharedInstance] libraryController];
    }

    [_representedAlbum iterateMediaItemsWithBlock:^(VLCMediaLibraryMediaItem* mediaItem) {
        [_libraryController appendItemToPlaylist:mediaItem playImmediately:NO];
    }];
}

@end
