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

#import "extensions/NSString+Helpers.h"
#import "extensions/NSFont+VLCAdditions.h"
#import "extensions/NSColor+VLCAdditions.h"
#import "extensions/NSView+VLCAdditions.h"

#import "main/VLCMain.h"

#import "library/VLCInputItem.h"
#import "library/VLCLibraryController.h"
#import "library/VLCLibraryDataTypes.h"
#import "library/VLCLibraryImageCache.h"
#import "library/VLCLibraryModel.h"
#import "library/VLCLibraryMenuController.h"
#import "library/VLCLibraryWindow.h"

#import "views/VLCImageView.h"

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
    _mediaItemTitleTextField.font = NSFont.VLCLibrarySubsectionHeaderFont;
    _mediaItemDetailButton.font = NSFont.VLCLibrarySubsectionSubheaderFont;

    if (@available(macOS 10.14, *)) {
        _mediaItemDetailButton.contentTintColor = NSColor.VLCAccentColor;
    }

    if(@available(macOS 10.12.2, *)) {
        _playMediaItemButton.bezelColor = NSColor.VLCAccentColor;
    }
}

- (void)setRepresentedMediaItem:(VLCMediaLibraryMediaItem *)representedMediaItem
{
    _representedMediaItem = representedMediaItem;
    [self updateRepresentation];
}

- (NSString*)formattedYearAndDurationString
{
    if (_representedMediaItem.year > 0) {
        return [NSString stringWithFormat:@"%u · %@", _representedMediaItem.year, _representedMediaItem.durationString];
    } else if (_representedMediaItem.files.count > 0) {
        VLCMediaLibraryFile *firstFile = _representedMediaItem.files.firstObject;
        time_t fileLastModTime = firstFile.lastModificationDate;
        
        if (fileLastModTime > 0) {
            NSDate *lastModDate = [NSDate dateWithTimeIntervalSince1970:fileLastModTime];
            NSDateComponents *components = [[NSCalendar currentCalendar] components:NSCalendarUnitYear fromDate:lastModDate];
            return [NSString stringWithFormat:@"%ld · %@", components.year, _representedMediaItem.durationString];
        }
    }
    
    return _representedMediaItem.durationString;
}

- (void)updateRepresentation
{
    if (_representedMediaItem == nil) {
        NSAssert(1, @"no media item assigned for collection view item", nil);
        return;
    }

    _mediaItemTitleTextField.stringValue = _representedMediaItem.displayString;
    _mediaItemDetailButton.title = _representedMediaItem.detailString;
    _mediaItemYearAndDurationTextField.stringValue = [self formattedYearAndDurationString];
    _mediaItemFileNameTextField.stringValue = _representedMediaItem.inputItem.name;
    _mediaItemPathTextField.stringValue = _representedMediaItem.inputItem.decodedMRL;

    const BOOL actionableDetail = self.representedMediaItem.actionableDetail;
    self.mediaItemDetailButton.enabled = actionableDetail;
    if (@available(macOS 10.14, *)) {
        self.mediaItemDetailButton.contentTintColor = actionableDetail ? NSColor.VLCAccentColor : NSColor.secondaryLabelColor;
    }
    self.mediaItemDetailButton.action = @selector(detailAction:);

    [VLCLibraryImageCache thumbnailForLibraryItem:_representedMediaItem withCompletion:^(NSImage * const thumbnail) {
        self->_mediaItemArtworkImageView.image = thumbnail;
    }];
}

- (IBAction)playAction:(id)sender
{
    if (!_libraryController) {
        _libraryController = VLCMain.sharedInstance.libraryController;
    }

    [_libraryController appendItemToPlaylist:_representedMediaItem playImmediately:YES];
}

- (IBAction)enqueueAction:(id)sender
{
    if (!_libraryController) {
        _libraryController = VLCMain.sharedInstance.libraryController;
    }

    [_libraryController appendItemToPlaylist:_representedMediaItem playImmediately:NO];
}

- (IBAction)detailAction:(id)sender
{
    if (!self.representedMediaItem.actionableDetail) {
        return;
    }

    VLCLibraryWindow * const libraryWindow = VLCMain.sharedInstance.libraryWindow;
    id<VLCMediaLibraryItemProtocol> libraryItem = self.representedMediaItem.actionableDetailLibraryItem;
    [libraryWindow presentLibraryItem:libraryItem];
}

@end
