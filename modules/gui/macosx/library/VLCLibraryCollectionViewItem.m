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

#import "main/VLCMain.h"

#import "library/VLCLibraryController.h"
#import "library/VLCLibraryDataTypes.h"
#import "library/VLCLibraryModel.h"
#import "library/VLCLibraryMenuController.h"
#import "library/VLCLibraryUIUnits.h"

#import "views/VLCImageView.h"
#import "views/VLCLinearProgressIndicator.h"
#import "views/VLCTrackingView.h"

#import "extensions/NSString+Helpers.h"
#import "extensions/NSFont+VLCAdditions.h"
#import "extensions/NSColor+VLCAdditions.h"
#import "extensions/NSView+VLCAdditions.h"

NSString *VLCLibraryCellIdentifier = @"VLCLibraryCellIdentifier";
const CGFloat VLCLibraryCollectionViewItemMinimalDisplayedProgress = 0.05;
const CGFloat VLCLibraryCollectionViewItemMaximumDisplayedProgress = 0.95;

@interface VLCLibraryCollectionViewItem()
{
    VLCLibraryController *_libraryController;
    VLCLibraryMenuController *_menuController;

    NSLayoutConstraint *_videoImageViewAspectRatioConstraint;
}
@end

@implementation VLCLibraryCollectionViewItem

+ (NSSize)defaultSize
{
    CGFloat width = 214;
    return CGSizeMake(width, width + [self bottomTextViewsHeight]);
}

+ (NSSize)defaultVideoItemSize
{
    CGFloat width = 214;
    CGFloat imageViewHeight = (214. / 16.) * 10.;
    return CGSizeMake(width, imageViewHeight + [self bottomTextViewsHeight]);
}

+ (CGFloat)bottomTextViewsHeight
{
    return [VLCLibraryUIUnits smallSpacing] +
           16 +
           [VLCLibraryUIUnits smallSpacing] +
           16 +
           [VLCLibraryUIUnits smallSpacing];
}

- (instancetype)initWithNibName:(NSNibName)nibNameOrNil bundle:(NSBundle *)nibBundleOrNil
{
    self = [super initWithNibName:nibNameOrNil bundle:nibBundleOrNil];
    if (self) {
        NSNotificationCenter *notificationCenter = [NSNotificationCenter defaultCenter];
        [notificationCenter addObserver:self
                               selector:@selector(mediaItemUpdated:)
                                   name:VLCLibraryModelMediaItemUpdated
                                 object:nil];
        [notificationCenter addObserver:self
                               selector:@selector(updateFontBasedOnSetting:)
                                   name:VLCConfigurationChangedNotification
                                 object:nil];
    }
    return self;
}

- (void)dealloc
{
    [[NSNotificationCenter defaultCenter] removeObserver:self];
    if (@available(macOS 10.14, *)) {
        [[NSApplication sharedApplication] removeObserver:self forKeyPath:@"effectiveAppearance"];
    }
}

- (void)awakeFromNib
{
    _videoImageViewAspectRatioConstraint = [NSLayoutConstraint constraintWithItem:_mediaImageView
                                                                        attribute:NSLayoutAttributeHeight
                                                                        relatedBy:NSLayoutRelationEqual
                                                                        toItem:_mediaImageView
                                                                        attribute:NSLayoutAttributeWidth
                                                                        multiplier:10.0/16.0
                                                                        constant:1];
    _videoImageViewAspectRatioConstraint.active = NO;

    [(VLCTrackingView *)self.view setViewToHide:self.playInstantlyButton];
    self.secondaryInfoTextField.textColor = [NSColor VLClibrarySubtitleColor];
    self.annotationTextField.font = [NSFont VLClibraryCellAnnotationFont];
    self.annotationTextField.textColor = [NSColor VLClibraryAnnotationColor];
    self.annotationTextField.backgroundColor = [NSColor VLClibraryAnnotationBackgroundColor];
    self.unplayedIndicatorTextField.stringValue = _NS("NEW");
    self.unplayedIndicatorTextField.font = [NSFont VLClibraryHighlightCellHighlightLabelFont];
    self.highlightBox.borderColor = [NSColor VLCAccentColor];
    self.unplayedIndicatorTextField.textColor = [NSColor VLCAccentColor];

    if (@available(macOS 10.14, *)) {
        [[NSApplication sharedApplication] addObserver:self
                                            forKeyPath:@"effectiveAppearance"
                                               options:0
                                               context:nil];
    }

    [self updateColoredAppearance];
    [self updateFontBasedOnSetting:nil];
    [self prepareForReuse];
}

#pragma mark - dynamic appearance

- (void)observeValueForKeyPath:(NSString *)keyPath
                      ofObject:(id)object
                        change:(NSDictionary<NSKeyValueChangeKey,id> *)change
                       context:(void *)context
{
    [self updateColoredAppearance];
}

- (void)updateColoredAppearance
{
    self.mediaTitleTextField.textColor = self.view.shouldShowDarkAppearance ? [NSColor VLClibraryDarkTitleColor] : [NSColor VLClibraryLightTitleColor];
}

- (void)updateFontBasedOnSetting:(NSNotification *)aNotification
{
    if (config_GetInt("macosx-large-text")) {
        self.mediaTitleTextField.font = [NSFont VLClibraryLargeCellTitleFont];
        self.secondaryInfoTextField.font = [NSFont VLClibraryLargeCellSubtitleFont];
    } else {
        self.mediaTitleTextField.font = [NSFont VLClibrarySmallCellTitleFont];
        self.secondaryInfoTextField.font = [NSFont VLClibrarySmallCellSubtitleFont];
    }
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
    _unplayedIndicatorTextField.hidden = YES;
    _highlightBox.hidden = YES;
}

- (void)setRepresentedItem:(id<VLCMediaLibraryItemProtocol>)representedItem
{
    if (!_libraryController) {
        _libraryController = [[VLCMain sharedInstance] libraryController];
    }

    _representedItem = representedItem;
    [self updateRepresentation];
}

- (void)setSelected:(BOOL)selected
{
    super.selected = selected;
    _highlightBox.hidden = !selected;
}

- (void)mediaItemUpdated:(NSNotification *)aNotification
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
    if (_representedItem == nil) {
        NSAssert(1, @"no item assigned for collection view item", nil);
        return;
    }

    _mediaTitleTextField.stringValue = _representedItem.displayString;
    _secondaryInfoTextField.stringValue = _representedItem.detailString;
    _mediaImageView.image = _representedItem.smallArtworkImage;

    // TODO: Add handling for the other types
    if([_representedItem isKindOfClass:[VLCMediaLibraryMediaItem class]]) {
        VLCMediaLibraryMediaItem *mediaItem = (VLCMediaLibraryMediaItem *)_representedItem;

        if (mediaItem.mediaType == VLC_ML_MEDIA_TYPE_VIDEO) {
            VLCMediaLibraryTrack *videoTrack = mediaItem.firstVideoTrack;
            [self showVideoSizeIfNeededForWidth:videoTrack.videoWidth
                                      andHeight:videoTrack.videoHeight];
            _imageViewAspectRatioConstraint.active = NO;
            _videoImageViewAspectRatioConstraint.active = YES;
        } else {
            _imageViewAspectRatioConstraint.active = YES;
            _videoImageViewAspectRatioConstraint.active = NO;
        }

        CGFloat position = mediaItem.progress;
        if (position > VLCLibraryCollectionViewItemMinimalDisplayedProgress && position < VLCLibraryCollectionViewItemMaximumDisplayedProgress) {
            _progressIndicator.progress = position;
            _progressIndicator.hidden = NO;
        }

        if (mediaItem.playCount == 0) {
            _unplayedIndicatorTextField.hidden = NO;
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

#pragma mark - actions

- (IBAction)playInstantly:(id)sender
{
    if (!_libraryController) {
        _libraryController = [[VLCMain sharedInstance] libraryController];
    }

    // We want to add all the tracks to the playlist but only play the first one immediately,
    // otherwise we will skip straight to the last track of the last album from the artist
    __block BOOL playImmediately = YES;
    [_representedItem iterateMediaItemsWithBlock:^(VLCMediaLibraryMediaItem* mediaItem) {
        [_libraryController appendItemToPlaylist:mediaItem playImmediately:playImmediately];

        if(playImmediately) {
            playImmediately = NO;
        }
    }];
}

- (IBAction)addToPlaylist:(id)sender
{
    if (!_libraryController) {
        _libraryController = [[VLCMain sharedInstance] libraryController];
    }

    [_representedItem iterateMediaItemsWithBlock:^(VLCMediaLibraryMediaItem* mediaItem) {
        [_libraryController appendItemToPlaylist:mediaItem playImmediately:NO];
    }];
}

-(void)mouseDown:(NSEvent *)theEvent
{
    if (theEvent.modifierFlags & NSControlKeyMask) {
        if (!_menuController) {
            _menuController = [[VLCLibraryMenuController alloc] init];
        }

        [_menuController setRepresentedItem:_representedItem];
        [_menuController popupMenuWithEvent:theEvent forView:self.view];
    }

    [super mouseDown:theEvent];
}

- (void)rightMouseDown:(NSEvent *)theEvent
{
    if (!_menuController) {
        _menuController = [[VLCLibraryMenuController alloc] init];
    }

    [_menuController setRepresentedItem:_representedItem];
    [_menuController popupMenuWithEvent:theEvent forView:self.view];

    [super rightMouseDown:theEvent];
}

@end
