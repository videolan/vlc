/*****************************************************************************
 * VLCLibraryTableCellView.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2019 VLC authors and VideoLAN
 *
 * Authors: Felix Paul Kühne <fkuehne # videolan -dot- org>
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

#import "VLCLibraryTableCellView.h"

#import "extensions/NSImage+VLCAdditions.h"
#import "extensions/NSFont+VLCAdditions.h"
#import "extensions/NSString+Helpers.h"
#import "extensions/NSView+VLCAdditions.h"

#import "library/VLCInputItem.h"
#import "library/VLCLibraryController.h"
#import "library/VLCLibraryDataTypes.h"
#import "library/VLCLibraryImageCache.h"
#import "library/VLCLibraryModel.h"
#import "library/VLCLibraryRepresentedItem.h"

#import "library/VLCLibraryMediaGroupDescriptor.h"

#import "main/VLCMain.h"

#import "playqueue/VLCPlayQueueController.h"

#import "views/VLCImageView.h"
#import "views/VLCTrackingView.h"

NSString * const VLCLibraryTableCellViewIdentifier = @"VLCLibraryTableCellViewIdentifier";

@interface VLCLibraryTableCellView ()
{
    VLCImageViewContentGravity _mediaArtworkContentGravity;
}

@property (readwrite, strong) IBOutlet NSLayoutConstraint *representedImageAspectRatioConstraint;
@property (readwrite, nonatomic) BOOL audioArtworkLayout;

@end

@implementation VLCLibraryTableCellView

- (void)awakeFromNib
{
    NSAssert(self.representedImageAspectRatioConstraint != nil, @"Artwork aspect-ratio constraint must be connected");
    if (self.audioMediaTypeIndicator != nil) {
        // The video nib starts with a widescreen constraint.
        _representedImageAspectRatio = VLCLibraryTableCellImageAspectRatioVideo;
    }
    _mediaArtworkContentGravity = self.representedImageView.contentGravity;
    self.markUnplayedButton.target = self;
    self.markUnplayedButton.action = @selector(markAsUnplayed:);
    self.markUnplayedButton.bordered = YES;
    self.markUnplayedButton.bezelStyle = NSBezelStyleRecessed;
    self.markUnplayedButton.showsBorderOnlyWhileMouseInside = YES;
    [self prepareForReuse];

    if (@available(macOS 26.0, *)) {
#if __MAC_OS_X_VERSION_MAX_ALLOWED >= 260000
        self.playInstantlyButton.bordered = YES;
        self.playInstantlyButton.bezelStyle = NSBezelStyleGlass;
        self.playInstantlyButton.borderShape = NSControlBorderShapeCircle;
        self.playInstantlyButton.image = [NSImage imageWithSystemSymbolName:@"play.fill" accessibilityDescription:nil];
        self.playInstantlyButton.imageScaling = NSImageScaleProportionallyUpOrDown;
        self.playInstantlyButton.controlSize = NSControlSizeExtraLarge;
#endif
    }
}

- (void)prepareForReuse
{
    [super prepareForReuse];
    _representedItem = nil;
    _representedInputItem = nil;
    self.showsMarkUnplayedButton = NO;
    self.markUnplayedButton.image = nil;
    self.markUnplayedButton.toolTip = nil;
    self.markUnplayedButton.accessibilityLabel = nil;
    self.detailMode = VLCLibraryTableCellDetailModePrimaryDetail;
    if (self.audioMediaTypeIndicator == nil) {
        self.representedImageAspectRatio = VLCLibraryTableCellImageAspectRatioSquare;
    }
    self.representedImageView.image = nil;
    self.audioArtworkLayout = NO;
    self.primaryTitleTextField.hidden = YES;
    self.secondaryTitleTextField.hidden = YES;
    self.singlePrimaryTitleTextField.hidden = YES;
    self.trackingView.viewToHide = nil;
    self.playInstantlyButton.hidden = YES;
}

- (void)setRepresentedImageAspectRatio:(VLCLibraryTableCellImageAspectRatio)representedImageAspectRatio
{
    if (_representedImageAspectRatio == representedImageAspectRatio) {
        return;
    }

    _representedImageAspectRatio = representedImageAspectRatio;
    if (self.representedImageAspectRatioConstraint == nil) {
        return;
    }

    CGFloat multiplier = 1.;
    switch (representedImageAspectRatio) {
        case VLCLibraryTableCellImageAspectRatioSquare:
            multiplier = 1.;
            break;
        case VLCLibraryTableCellImageAspectRatioVideo:
            multiplier = 16. / 9.;
            break;
    }

    NSLayoutConstraint * const previousConstraint = self.representedImageAspectRatioConstraint;
    NSLayoutConstraint * const newConstraint =
        [self.representedImageView.widthAnchor
            constraintEqualToAnchor:self.representedImageView.heightAnchor
                         multiplier:multiplier];
    newConstraint.priority = previousConstraint.priority;
    newConstraint.identifier = previousConstraint.identifier;

    previousConstraint.active = NO;
    newConstraint.active = YES;
    self.representedImageAspectRatioConstraint = newConstraint;
}

- (void)setAudioArtworkLayout:(BOOL)audio
{
    _audioArtworkLayout = audio;
    self.audioMediaTypeIndicator.hidden = !audio;
    if (self.audioMediaTypeIndicator != nil) {
        self.representedImageAspectRatio = audio
            ? VLCLibraryTableCellImageAspectRatioSquare
            : VLCLibraryTableCellImageAspectRatioVideo;
    }
    self.representedImageView.contentGravity = audio
        ? VLCImageViewContentGravityResizeAspect
        : _mediaArtworkContentGravity;
}

- (void)setShowsMarkUnplayedButton:(BOOL)showsMarkUnplayedButton
{
    NSAssert(!showsMarkUnplayedButton || self.markUnplayedButton != nil,
             @"Mark as Unplayed button must be connected when enabled");
    _showsMarkUnplayedButton = showsMarkUnplayedButton;
    [self updateMarkUnplayedButton];
}

- (void)updateMarkUnplayedButton
{
    self.markUnplayedButton.hidden = YES;
    if (!self.showsMarkUnplayedButton ||
        ![self.representedItem.item isKindOfClass:VLCMediaLibraryMediaItem.class]) {
        return;
    }

    VLCMediaLibraryMediaItem * const currentItem =
        [VLCMediaLibraryMediaItem mediaItemForLibraryID:self.representedItem.item.libraryID];
    if (currentItem.lastPlayedDate <= 0) {
        return;
    }

    NSString *actionTitle;
    NSString *symbolName;
    switch (currentItem.mediaType) {
        case VLC_ML_MEDIA_TYPE_AUDIO:
            actionTitle = _NS("Mark as Unheard");
            if (@available(macOS 26.0, *)) {
                symbolName = @"music.note.slash";
            } else {
                symbolName = @"speaker.slash";
            }
            break;
        case VLC_ML_MEDIA_TYPE_VIDEO:
            actionTitle = _NS("Mark as Unseen");
            symbolName = @"eye.slash";
            break;
        default:
            actionTitle = _NS("Mark as Unplayed");
            symbolName = @"arrow.counterclockwise";
            break;
    }

    NSImage *image = nil;
    if (@available(macOS 11.0, *)) {
        image = [NSImage imageWithSystemSymbolName:symbolName accessibilityDescription:actionTitle];
    }
    self.markUnplayedButton.image = image ?: [NSImage imageNamed:NSImageNameRefreshTemplate];
    self.markUnplayedButton.toolTip = actionTitle;
    self.markUnplayedButton.accessibilityLabel = actionTitle;
    self.markUnplayedButton.hidden = NO;
}

- (void)setRepresentedItem:(VLCLibraryRepresentedItem *)representedItem
{
    _representedItem = representedItem;
    _representedInputItem = nil; // Reset and ensure the change is obvious

    id<VLCMediaLibraryItemProtocol> const actualItem = representedItem.item;
    NSAssert(actualItem != nil, @"Should not update nil represented item!");
    self.audioArtworkLayout = actualItem.firstMediaItem.mediaType == VLC_ML_MEDIA_TYPE_AUDIO;

    self.trackingView.viewToHide = self.playInstantlyButton;
    self.playInstantlyButton.action = @selector(playMediaItemInstantly:);
    self.playInstantlyButton.target = self;

    __weak typeof(self) weakSelf = self;
    [VLCLibraryImageCache thumbnailForLibraryItem:actualItem withCompletion:^(NSImage * const thumbnail) {
        if (!weakSelf || weakSelf.representedItem.item != actualItem) {
            return;
        }
        weakSelf.representedImageView.image = thumbnail;
    }];

    NSString * const primaryDetailString = [self primaryDetailStringForItem:actualItem];
    if (primaryDetailString.length > 0) {
        self.primaryTitleTextField.hidden = NO;
        self.primaryTitleTextField.stringValue = actualItem.displayString;
        self.secondaryTitleTextField.hidden = NO;
        self.secondaryTitleTextField.stringValue = primaryDetailString;
    } else {
        self.singlePrimaryTitleTextField.hidden = NO;
        self.singlePrimaryTitleTextField.stringValue = actualItem.displayString;
    }
    [self updateMarkUnplayedButton];
}

- (void)setRepresentedInputItem:(VLCInputItem *)representedInputItem
{
    _representedInputItem = representedInputItem;
    _representedItem = nil; // Reset and ensure the change is obvious
    self.audioArtworkLayout = NO;
    [self updateMarkUnplayedButton];

    self.singlePrimaryTitleTextField.hidden = NO;
    self.singlePrimaryTitleTextField.stringValue = _representedInputItem.name;

    __weak typeof(self) weakSelf = self;
    [VLCLibraryImageCache thumbnailForInputItem:self->_representedInputItem withCompletion:^(NSImage * const thumbnail) {
        VLCLibraryTableCellView * const strongSelf = weakSelf;
        if (!strongSelf || representedInputItem != strongSelf->_representedInputItem) {
            return;
        }
        strongSelf->_representedImageView.image = thumbnail;
    }];

    self.trackingView.viewToHide = self.playInstantlyButton;
    self.playInstantlyButton.action = @selector(playInputItemInstantly:);
    self.playInstantlyButton.target = self;
}

- (void)setRepresentedVideoLibrarySection:(NSUInteger)section
{
    // We need to adjust the selected row value to match the backing enum.
    // Additionally, we hide recents when there are no recent media items.
    VLCLibraryModel * const model = VLCMain.sharedInstance.libraryController.libraryModel;
    const BOOL anyRecents = model.numberOfRecentMedia > 0;
    const NSUInteger sectionBase = VLCMediaLibraryParentGroupTypeRecentVideos;
    const NSUInteger sectionAdjustment = anyRecents ? sectionBase : sectionBase + 1;

    NSString *sectionString = @"";
    switch(section + sectionAdjustment) { // Group 0 is Invalid, so add one
        case VLCMediaLibraryParentGroupTypeRecentVideos:
            sectionString = _NS("Recents");
            break;
        case VLCMediaLibraryParentGroupTypeVideoLibrary:
            sectionString = _NS("Library");
            break;
        default:
            NSAssert(1, @"Reached unreachable case for video library section");
            break;
    }

    self.singlePrimaryTitleTextField.hidden = NO;
    self.singlePrimaryTitleTextField.stringValue = sectionString;
    self.representedImageView.image = NSImage.VLCNoArtImage;

    _representedVideoLibrarySection = section;
}

- (NSString *)primaryDetailStringForItem:(id<VLCMediaLibraryItemProtocol>)item
{
    switch (self.detailMode) {
        case VLCLibraryTableCellDetailModePrimaryDetail:
            return item.primaryDetailString;
        case VLCLibraryTableCellDetailModeDuration:
            return item.durationString;
    }
}

- (void)playMediaItemInstantly:(id)sender
{
    [self.representedItem play];
}

- (void)markAsUnplayed:(id)sender
{
    id<VLCMediaLibraryItemProtocol> const item = self.representedItem.item;
    if (!self.showsMarkUnplayedButton || self.markUnplayedButton.hidden ||
        ![item isKindOfClass:VLCMediaLibraryMediaItem.class]) {
        return;
    }

    if ([(VLCMediaLibraryMediaItem *)item markAsUnplayed] == VLC_SUCCESS) {
        self.markUnplayedButton.hidden = YES;
    }
}

- (void)playInputItemInstantly:(id)sender
{
    [VLCMain.sharedInstance.playQueueController addInputItem:_representedInputItem.vlcInputItem atPosition:-1 startPlayback:YES];
}

@end
