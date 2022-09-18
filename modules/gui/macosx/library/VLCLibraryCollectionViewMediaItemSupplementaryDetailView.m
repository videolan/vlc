/*****************************************************************************
 * VLCLibraryCollectionViewMediaItemSupplementaryDetailView.m: MacOS X interface module
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

#import "VLCLibraryCollectionViewMediaItemSupplementaryDetailView.h"

#import "main/VLCMain.h"
#import "library/VLCInputItem.h"
#import "library/VLCLibraryController.h"
#import "library/VLCLibraryDataTypes.h"
#import "library/VLCLibraryModel.h"
#import "library/VLCLibraryMenuController.h"
#import "views/VLCImageView.h"
#import "extensions/NSString+Helpers.h"
#import "extensions/NSFont+VLCAdditions.h"
#import "extensions/NSColor+VLCAdditions.h"
#import "extensions/NSView+VLCAdditions.h"

NSString *const VLCLibraryCollectionViewMediaItemSupplementaryDetailViewIdentifier = @"VLCLibraryCollectionViewMediaItemSupplementaryDetailViewIdentifier";
NSCollectionViewSupplementaryElementKind const VLCLibraryCollectionViewMediaItemSupplementaryDetailViewKind = @"VLCLibraryCollectionViewMediaItemSupplementaryDetailViewIdentifier";

@interface VLCLibraryCollectionViewMediaItemSupplementaryDetailView ()
{
    VLCLibraryController *_libraryController;
}

@end

@implementation VLCLibraryCollectionViewMediaItemSupplementaryDetailView

- (void)awakeFromNib
{
    _mediaItemTitleTextField.font = [NSFont VLCLibrarySupplementaryDetailViewTitleFont];

    if(@available(macOS 10.12.2, *)) {
        _playMediaItemButton.bezelColor = [NSColor VLCAccentColor];
    }
}

- (void)setRepresentedMediaItem:(VLCMediaLibraryMediaItem *)representedMediaItem
{
    _representedMediaItem = representedMediaItem;
    [self updateRepresentation];
}

- (void)updateRepresentation
{
    if (_representedMediaItem == nil) {
        NSAssert(1, @"no media item assigned for collection view item", nil);
        return;
    }

    _mediaItemTitleTextField.stringValue = _representedMediaItem.displayString;
    _mediaItemYearAndDurationTextField.stringValue = [NSString stringWithFormat:@"%u · %@", _representedMediaItem.year, _representedMediaItem.durationString];
    _mediaItemFileNameTextField.stringValue = _representedMediaItem.inputItem.name;
    _mediaItemPathTextField.stringValue = _representedMediaItem.inputItem.decodedMRL;
    _mediaItemArtworkImageView.image = _representedMediaItem.smallArtworkImage;
}

- (IBAction)playAction:(id)sender
{
    if (!_libraryController) {
        _libraryController = [[VLCMain sharedInstance] libraryController];
    }

    [_libraryController appendItemToPlaylist:_representedMediaItem playImmediately:YES];
}

- (IBAction)enqueueAction:(id)sender
{
    if (!_libraryController) {
        _libraryController = [[VLCMain sharedInstance] libraryController];
    }

    [_libraryController appendItemToPlaylist:_representedMediaItem playImmediately:NO];
}

@end
