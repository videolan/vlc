/*****************************************************************************
 * VLCLibraryCollectionViewItem.m: MacOS X interface module
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

#import "VLCLibraryCollectionViewItem.h"

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
#import "library/VLCLibraryUIUnits.h"

#import "main/VLCMain.h"

#import "views/VLCImageView.h"
#import "views/VLCLinearProgressIndicator.h"
#import "views/VLCTrackingView.h"

#import <vlc_configuration.h>

NSString *VLCLibraryCellIdentifier = @"VLCLibraryCellIdentifier";
const CGFloat VLCLibraryCollectionViewItemMinimalDisplayedProgress = 0.05;
const CGFloat VLCLibraryCollectionViewItemMaximumDisplayedProgress = 0.95;

@interface VLCLibraryCollectionViewItem()
{
    VLCLibraryMenuController *_menuController;
    NSLayoutConstraint *_videoImageViewAspectRatioConstraint;
}

@end

@implementation VLCLibraryCollectionViewItem

+ (const NSSize)defaultSize
{
    const CGFloat width = VLCLibraryCollectionViewItem.defaultWidth;
    return CGSizeMake(width, width + self.bottomTextViewsHeight);
}

+ (const NSSize)defaultVideoItemSize
{
    const CGFloat width = VLCLibraryCollectionViewItem.defaultWidth;
    const CGFloat imageViewHeight = width * [VLCLibraryCollectionViewItem videoHeightAspectRatioMultiplier];
    return CGSizeMake(width, imageViewHeight + self.bottomTextViewsHeight);
}

+ (const CGFloat)defaultWidth
{
    return 214.;
}

+ (const CGFloat)bottomTextViewsHeight
{
    return VLCLibraryUIUnits.smallSpacing +
           16 +
           VLCLibraryUIUnits.smallSpacing +
           16 +
           VLCLibraryUIUnits.smallSpacing;
}

+ (const CGFloat)videoHeightAspectRatioMultiplier
{
    return 10. / 16.;
}

- (instancetype)initWithNibName:(NSNibName)nibNameOrNil bundle:(NSBundle *)nibBundleOrNil
{
    self = [super initWithNibName:nibNameOrNil bundle:nibBundleOrNil];
    if (self) {
        NSNotificationCenter *notificationCenter = NSNotificationCenter.defaultCenter;
        [notificationCenter addObserver:self
                               selector:@selector(mediaItemThumbnailGenerated:)
                                   name:VLCLibraryModelMediaItemThumbnailGenerated
                                 object:nil];
    }
    return self;
}

- (void)dealloc
{
    [NSNotificationCenter.defaultCenter removeObserver:self];
    if (@available(macOS 10.14, *)) {
        [NSApplication.sharedApplication removeObserver:self forKeyPath:@"effectiveAppearance"];
    }
}

- (void)awakeFromNib
{
    _videoImageViewAspectRatioConstraint = [NSLayoutConstraint constraintWithItem:_mediaImageView
                                                                        attribute:NSLayoutAttributeHeight
                                                                        relatedBy:NSLayoutRelationEqual
                                                                        toItem:_mediaImageView
                                                                        attribute:NSLayoutAttributeWidth
                                                                       multiplier:[VLCLibraryCollectionViewItem videoHeightAspectRatioMultiplier]
                                                                        constant:1];
    _videoImageViewAspectRatioConstraint.priority = NSLayoutPriorityRequired;
    _videoImageViewAspectRatioConstraint.active = NO;

    [(VLCTrackingView *)self.view setViewToHide:self.playInstantlyButton];
    self.secondaryInfoTextField.textColor = NSColor.VLClibrarySubtitleColor;
    self.annotationTextField.font = [NSFont systemFontOfSize:NSFont.systemFontSize weight:NSFontWeightBold];
    self.annotationTextField.textColor = NSColor.VLClibraryAnnotationColor;
    self.annotationTextField.backgroundColor = NSColor.VLClibraryAnnotationBackgroundColor;
    self.unplayedIndicatorTextField.stringValue = _NS("NEW");
    self.unplayedIndicatorTextField.font = [NSFont systemFontOfSize:NSFont.systemFontSize weight:NSFontWeightBold];
    self.highlightBox.borderColor = NSColor.VLCAccentColor;
    self.unplayedIndicatorTextField.textColor = NSColor.VLCAccentColor;

    if (@available(macOS 10.14, *)) {
        [NSApplication.sharedApplication addObserver:self
                                            forKeyPath:@"effectiveAppearance"
                                               options:NSKeyValueObservingOptionNew
                                               context:nil];
    }

    [self updateColoredAppearance:self.view.effectiveAppearance];
    [self prepareForReuse];
}

#pragma mark - dynamic appearance

- (void)observeValueForKeyPath:(NSString *)keyPath
                      ofObject:(id)object
                        change:(NSDictionary<NSKeyValueChangeKey,id> *)change
                       context:(void *)context
{
    if ([keyPath isEqualToString:@"effectiveAppearance"]) {
        NSAppearance *effectiveAppearance = change[NSKeyValueChangeNewKey];
        [self updateColoredAppearance:effectiveAppearance];
    }
}

- (void)updateColoredAppearance:(NSAppearance*)appearance
{
    NSParameterAssert(appearance);
    BOOL isDark = NO;
    if (@available(macOS 10.14, *)) {
        isDark = [appearance.name isEqualToString:NSAppearanceNameDarkAqua] || [appearance.name isEqualToString:NSAppearanceNameVibrantDark];
    }

    self.mediaTitleTextField.textColor = isDark ? NSColor.VLClibraryDarkTitleColor : NSColor.VLClibraryLightTitleColor;
}

#pragma mark - view representation

- (void)prepareForReuse
{
    [super prepareForReuse];
    _playInstantlyButton.hidden = YES;
    _mediaTitleTextField.stringValue = @"";
    _secondaryInfoTextField.stringValue = [NSString stringWithTime:0];
    _mediaImageView.image = nil;
    _annotationTextField.hidden = YES;
    _progressIndicator.hidden = YES;
    _highlightBox.hidden = YES;

    [self setUnplayedIndicatorHidden:YES];
}

- (void)setRepresentedItem:(VLCLibraryRepresentedItem *)representedItem
{
    _representedItem = representedItem;
    [self updateRepresentation];
}

- (void)setSelected:(BOOL)selected
{
    super.selected = selected;
    _highlightBox.hidden = !selected;
}

- (void)mediaItemThumbnailGenerated:(NSNotification *)aNotification
{
    VLCMediaLibraryMediaItem *updatedMediaItem = aNotification.object;
    if (updatedMediaItem == nil || _representedItem == nil || ![_representedItem isKindOfClass:[VLCMediaLibraryMediaItem class]]) {
        return;
    }

    VLCMediaLibraryMediaItem *mediaItem = (VLCMediaLibraryMediaItem *)_representedItem;
    if(mediaItem && updatedMediaItem.libraryID == mediaItem.libraryID) {
        [self updateRepresentation];
    }
}

- (void)updateRepresentation
{
    NSAssert(self.representedItem != nil, @"no item assigned for collection view item", nil);

    const id<VLCMediaLibraryItemProtocol> actualItem = self.representedItem.item;
    _mediaTitleTextField.stringValue = actualItem.displayString;
    _secondaryInfoTextField.stringValue = actualItem.primaryDetailString;

    [VLCLibraryImageCache thumbnailForLibraryItem:actualItem withCompletion:^(NSImage * const thumbnail) {
        self->_mediaImageView.image = thumbnail;
    }];

    // TODO: Add handling for the other types
    if([actualItem isKindOfClass:[VLCMediaLibraryMediaItem class]]) {
        VLCMediaLibraryMediaItem * const mediaItem = (VLCMediaLibraryMediaItem *)actualItem;

        if (mediaItem.mediaType == VLC_ML_MEDIA_TYPE_VIDEO) {
            VLCMediaLibraryTrack * const videoTrack = mediaItem.firstVideoTrack;
            [self showVideoSizeIfNeededForWidth:videoTrack.videoWidth
                                      andHeight:videoTrack.videoHeight];
            _videoImageViewAspectRatioConstraint.active = YES;
        } else {
            _videoImageViewAspectRatioConstraint.active = NO;
        }

        const CGFloat position = mediaItem.progress;
        if (position > VLCLibraryCollectionViewItemMinimalDisplayedProgress && position < VLCLibraryCollectionViewItemMaximumDisplayedProgress) {
            _progressIndicator.progress = position;
            _progressIndicator.hidden = NO;
        }

        if (mediaItem.playCount == 0) {
            [self setUnplayedIndicatorHidden:NO];
        }
    }
}

- (void)showVideoSizeIfNeededForWidth:(CGFloat)width andHeight:(CGFloat)height
{
    if (width >= VLCMediaLibrary4KWidth || height >= VLCMediaLibrary4KHeight) {
        _annotationTextField.stringValue = _NS("4K");
        _annotationTextField.hidden = NO;
    } else if (width >= VLCMediaLibrary720pWidth || height >= VLCMediaLibrary720pHeight) {
        _annotationTextField.stringValue = _NS("HD");
        _annotationTextField.hidden = NO;
    }
}

- (void)setUnplayedIndicatorHidden:(BOOL)indicatorHidden
{
    _unplayedIndicatorTextField.hidden = indicatorHidden;

    // Set priority of constraints for secondary info label, which is alongside unplayed indicator
    const NSLayoutPriority superViewConstraintPriority = indicatorHidden ? NSLayoutPriorityRequired : NSLayoutPriorityDefaultLow;
    const NSLayoutPriority unplayedIndicatorConstraintPriority = indicatorHidden ? NSLayoutPriorityDefaultLow : NSLayoutPriorityRequired;

    _trailingSecondaryTextToTrailingSuperviewConstraint.priority = superViewConstraintPriority;
    _trailingSecondaryTextToLeadingUnplayedIndicatorConstraint.priority = unplayedIndicatorConstraintPriority;
}

#pragma mark - actions

- (IBAction)playInstantly:(id)sender
{
    [self.representedItem play];
}

- (IBAction)addToPlaylist:(id)sender
{
    [self.representedItem queue];
}

-(void)mouseDown:(NSEvent *)theEvent
{
    if (theEvent.modifierFlags & NSControlKeyMask) {
        if (!_menuController) {
            _menuController = [[VLCLibraryMenuController alloc] init];
        }

        [_menuController setRepresentedItem:self.representedItem];
        [_menuController popupMenuWithEvent:theEvent forView:self.view];
    }

    [super mouseDown:theEvent];
}

- (void)rightMouseDown:(NSEvent *)theEvent
{
    if (!_menuController) {
        _menuController = [[VLCLibraryMenuController alloc] init];
    }

    [_menuController setRepresentedItem:self.representedItem];
    [_menuController popupMenuWithEvent:theEvent forView:self.view];

    [super rightMouseDown:theEvent];
}

@end
