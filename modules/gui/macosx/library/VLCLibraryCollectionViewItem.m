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
}
@end

@implementation VLCLibraryCollectionViewItem

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
    if (@available(macOS 10_14, *)) {
        [[NSApplication sharedApplication] removeObserver:self forKeyPath:@"effectiveAppearance"];
    }
}

- (void)awakeFromNib
{
    [(VLCTrackingView *)self.view setViewToHide:self.playInstantlyButton];
    self.durationTextField.textColor = [NSColor VLClibrarySubtitleColor];
    self.annotationTextField.font = [NSFont VLClibraryCellAnnotationFont];
    self.annotationTextField.textColor = [NSColor VLClibraryAnnotationColor];
    self.annotationTextField.backgroundColor = [NSColor VLClibraryAnnotationBackgroundColor];
    self.unplayedIndicatorTextField.stringValue = _NS("NEW");
    self.unplayedIndicatorTextField.font = [NSFont VLClibraryHighlightCellHighlightLabelFont];
    self.unplayedIndicatorTextField.textColor = [NSColor VLClibraryHighlightColor];

    if (@available(macOS 10_14, *)) {
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
        self.durationTextField.font = [NSFont VLClibraryLargeCellSubtitleFont];
    } else {
        self.mediaTitleTextField.font = [NSFont VLClibrarySmallCellTitleFont];
        self.durationTextField.font = [NSFont VLClibrarySmallCellSubtitleFont];
    }
}

#pragma mark - view representation

- (void)prepareForReuse
{
    [super prepareForReuse];
    _playInstantlyButton.hidden = YES;
    _mediaTitleTextField.stringValue = @"";
    _durationTextField.stringValue = [NSString stringWithTime:0];
    _mediaImageView.image = nil;
    _annotationTextField.hidden = YES;
    _progressIndicator.hidden = YES;
    _unplayedIndicatorTextField.hidden = YES;
}

- (void)setRepresentedMediaItem:(VLCMediaLibraryMediaItem *)representedMediaItem
{
    if (!_libraryController) {
        _libraryController = [[VLCMain sharedInstance] libraryController];
    }

    _representedMediaItem = representedMediaItem;
    [self updateRepresentation];
}

- (void)mediaItemUpdated:(NSNotification *)aNotification
{
    VLCMediaLibraryMediaItem *updatedMediaItem = aNotification.object;
    if (updatedMediaItem == nil || _representedMediaItem == nil) {
        return;
    }
    if (updatedMediaItem.libraryID == _representedMediaItem.libraryID) {
        [self updateRepresentation];
    }
}

- (void)updateRepresentation
{
    if (_representedMediaItem == nil) {
        NSAssert(1, @"no media item assigned for collection view item", nil);
        return;
    }

    _mediaTitleTextField.stringValue = _representedMediaItem.title;
    _durationTextField.stringValue = [NSString stringWithTime:_representedMediaItem.duration / VLCMediaLibraryMediaItemDurationDenominator];

    _mediaImageView.image = [self imageForMedia];

    VLCMediaLibraryTrack *videoTrack = _representedMediaItem.firstVideoTrack;
    [self showVideoSizeIfNeededForWidth:videoTrack.videoWidth andHeight:videoTrack.videoHeight];

    CGFloat position = _representedMediaItem.lastPlaybackPosition;
    if (position > VLCLibraryCollectionViewItemMinimalDisplayedProgress && position < VLCLibraryCollectionViewItemMaximumDisplayedProgress) {
        _progressIndicator.progress = position;
        _progressIndicator.hidden = NO;
    }

    if (_representedMediaItem.playCount == 0) {
        _unplayedIndicatorTextField.hidden = NO;
    }
}

- (NSImage *)imageForMedia
{
    NSImage *image = _representedMediaItem.smallArtworkImage;
    if (!image) {
        image = [NSImage imageNamed: @"noart.png"];
    }
    return image;
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

    [_libraryController appendItemToPlaylist:_representedMediaItem playImmediately:YES];
}

- (IBAction)addToPlaylist:(id)sender
{
    if (!_libraryController) {
        _libraryController = [[VLCMain sharedInstance] libraryController];
    }

    [_libraryController appendItemToPlaylist:_representedMediaItem playImmediately:NO];
}

-(void)mouseDown:(NSEvent *)theEvent
{
    if (theEvent.modifierFlags & NSControlKeyMask) {
        if (!_menuController) {
            _menuController = [[VLCLibraryMenuController alloc] init];
        }
        _menuController.representedMediaItem = self.representedMediaItem;
        [_menuController popupMenuWithEvent:theEvent forView:self.view];
    }

    [super mouseDown:theEvent];
}

- (void)rightMouseDown:(NSEvent *)theEvent
{
    if (!_menuController) {
        _menuController = [[VLCLibraryMenuController alloc] init];
    }
    _menuController.representedMediaItem = self.representedMediaItem;
    [_menuController popupMenuWithEvent:theEvent forView:self.view];

    [super rightMouseDown:theEvent];
}

@end
