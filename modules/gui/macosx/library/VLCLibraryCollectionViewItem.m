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
#import "library/VLCLibraryModel.h"
#import "library/VLCLibraryDataTypes.h"
#import "views/VLCImageView.h"
#import "extensions/NSString+Helpers.h"
#import "extensions/NSFont+VLCAdditions.h"
#import "extensions/NSColor+VLCAdditions.h"

NSString *VLCLibraryCellIdentifier = @"VLCLibraryCellIdentifier";

@interface VLCLibraryCollectionViewItem()
{
    VLCLibraryController *_libraryController;
}
@end

@implementation VLCLibraryCollectionViewItem

- (instancetype)initWithNibName:(NSNibName)nibNameOrNil bundle:(NSBundle *)nibBundleOrNil
{
    self = [super initWithNibName:nibNameOrNil bundle:nibBundleOrNil];
    if (self) {
        [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(mediaItemUpdated:) name:VLCLibraryModelMediaItemUpdated object:nil];
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
    self.playInstantlyButton.hidden = YES;
    [(VLCLibraryCollectionViewTrackingView *)self.view setButtonToHide:self.playInstantlyButton];
    self.mediaTitleTextField.font = [NSFont VLClibraryCellTitleFont];
    self.durationTextField.font = [NSFont VLClibraryCellSubtitleFont];
    self.durationTextField.textColor = [NSColor VLClibrarySubtitleColor];

    if (@available(macOS 10_14, *)) {
        [[NSApplication sharedApplication] addObserver:self
                                            forKeyPath:@"effectiveAppearance"
                                               options:0
                                               context:nil];
    }

    [self updateColoredAppearance];
}

- (void)observeValueForKeyPath:(NSString *)keyPath
                      ofObject:(id)object
                        change:(NSDictionary<NSKeyValueChangeKey,id> *)change
                       context:(void *)context
{
    [self updateColoredAppearance];
}

- (void)updateColoredAppearance
{
    if (@available(macOS 10_14, *)) {
        if ([self.view.effectiveAppearance.name isEqualToString:NSAppearanceNameDarkAqua]) {
            self.mediaTitleTextField.textColor = [NSColor VLClibraryDarkTitleColor];
        } else {
            self.mediaTitleTextField.textColor = [NSColor VLClibraryLightTitleColor];
        }
    } else {
        self.mediaTitleTextField.textColor = [NSColor VLClibraryLightTitleColor];
    }
}

#pragma mark - view representation

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
        _mediaTitleTextField.stringValue = @"";
        _durationTextField.stringValue = [NSString stringWithTime:0];
        _mediaImageView.image = [NSImage imageNamed: @"noart.png"];
        return;
    }

    _mediaTitleTextField.stringValue = _representedMediaItem.title;
    _durationTextField.stringValue = [NSString stringWithTime:_representedMediaItem.duration / 1000];

    NSImage *image;
    if (_representedMediaItem.artworkGenerated) {
        image = [[NSImage alloc] initWithContentsOfURL:[NSURL URLWithString:_representedMediaItem.artworkMRL]];
    } else {
        [_libraryController attemptToGenerateThumbnailForMediaItem:_representedMediaItem];
    }
    if (!image) {
        image = [NSImage imageNamed: @"noart.png"];
    }
    _mediaImageView.image = image;
}

#pragma mark - actions

- (IBAction)playInstantly:(id)sender
{
    if (!_libraryController) {
        _libraryController = [[VLCMain sharedInstance] libraryController];
    }

    NSIndexPath *indexPath = [[self collectionView] indexPathForItem:self];
    [_libraryController appendItemAtIndexPathToPlaylist:indexPath playImmediately:YES];
}

- (IBAction)addToPlaylist:(id)sender
{
    if (!_libraryController) {
        _libraryController = [[VLCMain sharedInstance] libraryController];
    }

    NSIndexPath *indexPath = [[self collectionView] indexPathForItem:self];
    [_libraryController appendItemAtIndexPathToPlaylist:indexPath playImmediately:NO];
}

@end

@interface VLCLibraryCollectionViewTrackingView ()
{
    NSTrackingArea *_trackingArea;
}
@end

@implementation VLCLibraryCollectionViewTrackingView

- (void)mouseExited:(NSEvent *)event
{
    self.buttonToHide.hidden = YES;
}

- (void)mouseEntered:(NSEvent *)event
{
    self.buttonToHide.hidden = NO;
}

- (void)updateTrackingAreas
{
    [super updateTrackingAreas];
    if(_trackingArea != nil) {
        [self removeTrackingArea:_trackingArea];
    }

    NSTrackingAreaOptions trackingAreaOptions = (NSTrackingMouseEnteredAndExited | NSTrackingActiveAlways);
    _trackingArea = [[NSTrackingArea alloc] initWithRect:[self bounds]
                                                 options:trackingAreaOptions
                                                   owner:self
                                                userInfo:nil];
    [self addTrackingArea:_trackingArea];
}

@end
