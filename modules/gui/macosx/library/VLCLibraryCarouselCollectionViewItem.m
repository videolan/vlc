/*****************************************************************************
 * VLCLibraryCarouselCollectionViewItem.m: MacOS X interface module
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

#import "VLCLibraryCarouselCollectionViewItem.h"

#import "extensions/NSColor+VLCAdditions.h"
#import "extensions/NSFont+VLCAdditions.h"
#import "extensions/NSView+VLCAdditions.h"

#import "library/VLCLibraryDataTypes.h"
#import "library/VLCLibraryImageCache.h"
#import "library/VLCLibraryMenuController.h"
#import "VLCLibraryRepresentedItem.h"

#import "views/VLCImageView.h"
#import "views/VLCLinearProgressIndicator.h"

NSString * const VLCLibraryCarouselCollectionViewItemIdentifier =
    @"VLCLibraryCarouselCollectionViewItemIdentifier";

@implementation VLCLibraryCarouselCollectionViewItem
{
    VLCLibraryMenuController *_menuController;
}

@synthesize representedItem = _representedItem;

- (void)loadView
{
    self.view = [NSView fromNibNamed:NSStringFromClass(self.class)
                           withClass:NSView.class
                           withOwner:self];
    [self setup];
}

- (void)setup
{
    self.highlightBox.borderColor = NSColor.VLCAccentColor;
    self.titleTextField.font = NSFont.VLCLibrarySubsectionHeaderFont;
    self.detailTextField.font = NSFont.VLCLibrarySubsectionSubheaderFont;
    self.annotationTextField.font = NSFont.VLCLibraryItemAnnotationFont;
    self.annotationTextField.textColor = NSColor.VLClibraryAnnotationColor;
    self.annotationTextField.backgroundColor = NSColor.VLClibraryAnnotationBackgroundColor;

    [self prepareForReuse];
}

- (void)setRepresentedItem:(VLCLibraryRepresentedItem *)representedItem
{
    if (representedItem == _representedItem) {
        return;
    }

    _representedItem = representedItem;
    [self updateRepresentation];
}

- (void)setSelected:(BOOL)selected
{
    [super setSelected:selected];
    self.highlightBox.hidden = !selected;
}

- (void)prepareForReuse
{
    [super prepareForReuse];
    self.highlightBox.hidden = YES;
    self.annotationTextField.hidden = YES;
    self.progressIndicator.hidden = YES;
    self.titleTextField.stringValue = @"";
    self.detailTextField.stringValue = @"";
    self.coverImageView.image = nil;
}

- (void)updateRepresentation
{
    [self prepareForReuse];

    const id<VLCMediaLibraryItemProtocol> actualItem = self.representedItem.item;

    __weak typeof(self) weakSelf = self;
    [VLCLibraryImageCache thumbnailForLibraryItem:actualItem withCompletion:^(NSImage * const thumbnail) {
        if (!weakSelf || weakSelf.representedItem.item != actualItem) {
            return;
        }
        weakSelf.coverImageView.image = thumbnail;
    }];

    self.titleTextField.stringValue = actualItem.displayString;
    self.detailTextField.stringValue = actualItem.primaryDetailString;

    if ([actualItem isKindOfClass:VLCMediaLibraryMediaItem.class]) {
        VLCMediaLibraryMediaItem * const mediaItem = (VLCMediaLibraryMediaItem *)actualItem;

        if (mediaItem.mediaType == VLC_ML_MEDIA_TYPE_VIDEO) {
            VLCMediaLibraryTrack * const videoTrack = mediaItem.firstVideoTrack;
            NSString * const resolutionLabel = videoTrack.resolutionLabel;
            if (resolutionLabel) {
                self.annotationTextField.stringValue = resolutionLabel;
                self.annotationTextField.hidden = NO;
            }
        }

        const CGFloat position = mediaItem.progress;
        if (position > 0.05 && position < 0.95) {
            self.progressIndicator.progress = position;
            self.progressIndicator.hidden = NO;
        }
    }
}

- (void)playRepresentedItem
{
    [self.representedItem play];
}

- (void)openContextMenu:(NSEvent *)event
{
    if (_menuController == nil) {
        _menuController = [[VLCLibraryMenuController alloc] init];
    }

    _menuController.representedItems = @[self.representedItem];
    [_menuController popupMenuWithEvent:event forView:self.view];
}

@end
