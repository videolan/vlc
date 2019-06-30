/*****************************************************************************
 * VLCMediaSourceCollectionViewItem.m: MacOS X interface module
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

#import "VLCMediaSourceCollectionViewItem.h"

#import "main/VLCMain.h"
#import "views/VLCImageView.h"
#import "views/VLCTrackingView.h"
#import "extensions/NSString+Helpers.h"
#import "extensions/NSFont+VLCAdditions.h"
#import "extensions/NSColor+VLCAdditions.h"
#import "extensions/NSView+VLCAdditions.h"
#import "library/VLCInputItem.h"
#import "playlist/VLCPlaylistController.h"

NSString *VLCMediaSourceCellIdentifier = @"VLCLibraryCellIdentifier";

@implementation VLCMediaSourceCollectionViewItem

- (instancetype)initWithNibName:(NSNibName)nibNameOrNil bundle:(NSBundle *)nibBundleOrNil
{
    self = [super initWithNibName:nibNameOrNil bundle:nibBundleOrNil];
    if (self) {
        NSNotificationCenter *notificationCenter = [NSNotificationCenter defaultCenter];
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
    self.annotationTextField.font = [NSFont VLClibraryCellAnnotationFont];
    self.annotationTextField.textColor = [NSColor VLClibraryAnnotationColor];
    self.annotationTextField.backgroundColor = [NSColor VLClibraryAnnotationBackgroundColor];

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
    } else {
        self.mediaTitleTextField.font = [NSFont VLClibrarySmallCellTitleFont];
    }
}

#pragma mark - view representation

- (void)prepareForReuse
{
    [super prepareForReuse];
    _playInstantlyButton.hidden = YES;
    _mediaTitleTextField.stringValue = @"";
    _annotationTextField.hidden = YES;
    _mediaImageView.image = nil;
    _addToPlaylistButton.hidden = NO;
}

- (void)setRepresentedInputItem:(VLCInputItem *)representedInputItem
{
    _representedInputItem = representedInputItem;
    [self updateRepresentation];
}

- (void)updateRepresentation
{
    if (_representedInputItem == nil) {
        NSAssert(1, @"no input item assigned for collection view item", nil);
        return;
    }

    _mediaTitleTextField.stringValue = _representedInputItem.name;
    NSURL *artworkURL = _representedInputItem.artworkURL;
    NSImage *placeholderImage = [self imageForInputItem];
    if (artworkURL) {
        [_mediaImageView setImageURL:artworkURL placeholderImage:placeholderImage];
    } else {
        _mediaImageView.image = placeholderImage;
    }

    switch (_representedInputItem.inputType) {
        case ITEM_TYPE_DIRECTORY:
            _annotationTextField.stringValue = _NS("Folder");
            _annotationTextField.hidden = NO;
            break;

        case ITEM_TYPE_STREAM:
            _annotationTextField.stringValue = _NS("Stream");
            _annotationTextField.hidden = NO;
            break;

        case ITEM_TYPE_PLAYLIST:
            _annotationTextField.stringValue = _NS("Playlist");
            _annotationTextField.hidden = NO;
            break;

        case ITEM_TYPE_DISC:
            _annotationTextField.stringValue = _NS("Disk");
            _annotationTextField.hidden = NO;
            break;

        default:
            break;
    }
}

- (NSImage *)imageForInputItem
{
    NSImage *image;
    if (_representedInputItem.inputType == ITEM_TYPE_DIRECTORY) {
        image = [NSImage imageNamed:NSImageNameFolder];
    }

    if (!image) {
        image = [NSImage imageNamed: @"noart.png"];
    }
    return image;
}

#pragma mark - actions

- (IBAction)playInstantly:(id)sender
{
    [[[VLCMain sharedInstance] playlistController] addInputItem:_representedInputItem.vlcInputItem atPosition:-1 startPlayback:YES];
}

- (IBAction)addToPlaylist:(id)sender
{
    [[[VLCMain sharedInstance] playlistController] addInputItem:_representedInputItem.vlcInputItem atPosition:-1 startPlayback:NO];
}

@end
