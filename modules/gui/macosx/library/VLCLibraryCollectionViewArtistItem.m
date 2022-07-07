/*****************************************************************************
 * VLCLibraryCollectionViewArtistItem.m: MacOS X interface module
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

#import "VLCLibraryCollectionViewArtistItem.h"

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

NSString *VLCLibraryArtistCellIdentifier = @"VLCLibraryArtistCellIdentifier";

@interface VLCLibraryCollectionViewArtistItem()
{
    VLCLibraryController *_libraryController;
    VLCLibraryMenuController *_menuController;
}
@end

@implementation VLCLibraryCollectionViewArtistItem

@synthesize mediaTitleTextField = _mediaTitleTextField;
@synthesize annotationTextField = _annotationTextField;
@synthesize unplayedIndicatorTextField = _unplayedIndicatorTextField;
@synthesize durationTextField = _durationTextField;
@synthesize mediaImageView = _mediaImageView;
@synthesize playInstantlyButton = _playInstantlyButton;
@synthesize addToPlaylistButton = _addToPlaylistButton;
@synthesize progressIndicator = _progressIndicator;

- (instancetype)initWithNibName:(NSNibName)nibNameOrNil bundle:(NSBundle *)nibBundleOrNil
{
    self = [super initWithNibName:nibNameOrNil bundle:nibBundleOrNil];
    if (self) {
        NSNotificationCenter *notificationCenter = [NSNotificationCenter defaultCenter];
        // TODO: Update artist on a VLCLibraryModelArtistUpdated signal
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
    [(VLCTrackingView *)self.view setViewToHide:self.playInstantlyButton];
    self.durationTextField.textColor = [NSColor VLClibrarySubtitleColor];
    self.annotationTextField.font = [NSFont VLClibraryCellAnnotationFont];
    self.annotationTextField.textColor = [NSColor VLClibraryAnnotationColor];
    self.annotationTextField.backgroundColor = [NSColor VLClibraryAnnotationBackgroundColor];
    self.unplayedIndicatorTextField.stringValue = _NS("NEW");
    self.unplayedIndicatorTextField.font = [NSFont VLClibraryHighlightCellHighlightLabelFont];
    self.unplayedIndicatorTextField.textColor = [NSColor VLClibraryHighlightColor];

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

- (void)setRepresentedArtist:(VLCMediaLibraryArtist *)representedArtist
{
    if (!_libraryController) {
        _libraryController = [[VLCMain sharedInstance] libraryController];
    }

    _representedArtist = representedArtist;
    [self updateRepresentation];
}

- (void)artistUpdated:(NSNotification *)aNotification
{
    VLCMediaLibraryArtist *updatedArtist = aNotification.object;
    if (updatedArtist == nil || _representedArtist == nil) {
        return;
    }
    if (updatedArtist.artistID == _representedArtist.artistID) {
        [self updateRepresentation];
    }
}

- (void)updateRepresentation
{
    if (_representedArtist == nil) {
        NSAssert(1, @"no artist assigned for collection view item", nil);
        return;
    }

    _mediaTitleTextField.stringValue = _representedArtist.name;

    NSString *countMetadataString;
    if (_representedArtist.numberOfAlbums > 1) {
        countMetadataString = [NSString stringWithFormat:_NS("%u albums"), _representedArtist.numberOfAlbums];
    } else {
        countMetadataString = _NS("1 album");
    }
    if (_representedArtist.numberOfTracks > 1) {
        countMetadataString = [countMetadataString stringByAppendingFormat:@", %@", [NSString stringWithFormat:_NS("%u songs"), _representedArtist.numberOfTracks]];
    } else {
        countMetadataString = [countMetadataString stringByAppendingFormat:@", %@", _NS("1 song")];
    }

    _durationTextField.stringValue = countMetadataString;

    _mediaImageView.image = [self imageForMedia];

    // TODO: Show artist tracks progress with progress indicator
}

- (NSImage *)imageForMedia
{
    NSImage *image = nil;
    if (_representedArtist.artworkMRL.length > 0) {
        image = [[NSImage alloc] initWithContentsOfURL:[NSURL URLWithString:_representedArtist.artworkMRL]];
    }
    if (!image) {
        image = [NSImage imageNamed: @"noart.png"];
    }
    return image;
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
    [_representedArtist iterateMediaItemsWithBlock:^(VLCMediaLibraryMediaItem* mediaItem) {
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

    [_representedArtist iterateMediaItemsWithBlock:^(VLCMediaLibraryMediaItem* mediaItem) {
        [_libraryController appendItemToPlaylist:mediaItem playImmediately:NO];
    }];
}

-(void)mouseDown:(NSEvent *)theEvent
{
    if (theEvent.modifierFlags & NSControlKeyMask) {
        if (!_menuController) {
            _menuController = [[VLCLibraryMenuController alloc] init];
        }
        [_menuController setRepresentedArtist:self.representedArtist];
        [_menuController popupMenuWithEvent:theEvent forView:self.view];
    }

    [super mouseDown:theEvent];
}

- (void)rightMouseDown:(NSEvent *)theEvent
{
    if (!_menuController) {
        _menuController = [[VLCLibraryMenuController alloc] init];
    }
    [_menuController setRepresentedArtist:self.representedArtist];
    [_menuController popupMenuWithEvent:theEvent forView:self.view];

    [super rightMouseDown:theEvent];
}

@end
