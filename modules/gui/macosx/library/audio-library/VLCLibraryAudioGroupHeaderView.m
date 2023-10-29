/*****************************************************************************
 * VLCLibraryAudioGroupHeaderView.m: MacOS X interface module
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

#import "VLCLibraryAudioGroupHeaderView.h"

#import "extensions/NSColor+VLCAdditions.h"
#import "extensions/NSString+Helpers.h"

#import "main/VLCMain.h"

#import "library/VLCLibraryController.h"
#import "library/VLCLibraryDataTypes.h"
#import "library/VLCLibraryRepresentedItem.h"

NSString * const VLCLibraryAudioGroupHeaderViewIdentifier = @"VLCLibraryAudioGroupHeaderViewIdentifier";

@implementation VLCLibraryAudioGroupHeaderView

+ (CGSize)defaultHeaderSize
{
    return CGSizeMake(690., 86.);
}

- (void)awakeFromNib
{
    if (@available(macOS 10.14, *)) {
        _playButton.bezelColor = NSColor.VLCAccentColor;
    }

    _backgroundBox.borderColor = NSColor.VLCSubtleBorderColor;
}

- (void)updateRepresentation
{
    const id<VLCMediaLibraryItemProtocol> actualItem = self.representedItem.item;
    if (actualItem == nil) {
        _titleTextField.stringValue = _NS("Unknown");
        _detailTextField.stringValue = _NS("Unknown");
        return;
    }

    _titleTextField.stringValue = actualItem.displayString;
    _detailTextField.stringValue = actualItem.detailString;
}

- (void)setRepresentedItem:(VLCLibraryRepresentedItem *)representedItem
{
    if (representedItem == _representedItem) {
        return;
    }

    _representedItem = representedItem;
    [self updateRepresentation];
}

- (IBAction)play:(id)sender
{
    VLCLibraryController * const libraryController = VLCMain.sharedInstance.libraryController;

    // We want to add all the tracks to the playlist but only play the first one immediately,
    // otherwise we will skip straight to the last track of the last album from the artist
    __block BOOL playImmediately = YES;
    [self.representedItem.item iterateMediaItemsWithBlock:^(VLCMediaLibraryMediaItem* mediaItem) {
        [libraryController appendItemToPlaylist:mediaItem playImmediately:playImmediately];

        if(playImmediately) {
            playImmediately = NO;
        }
    }];
}

- (IBAction)enqueue:(id)sender
{
    VLCLibraryController * const libraryController = VLCMain.sharedInstance.libraryController;

    [self.representedItem.item iterateMediaItemsWithBlock:^(VLCMediaLibraryMediaItem* mediaItem) {
        [libraryController appendItemToPlaylist:mediaItem playImmediately:NO];
    }];
}

@end
