/*****************************************************************************
 * VLCPlayQueueTableViewCell.m: MacOS X interface module
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

#import "VLCPlayQueueTableCellView.h"

#import "extensions/NSColor+VLCAdditions.h"
#import "extensions/NSFont+VLCAdditions.h"
#import "extensions/NSImage+VLCAdditions.h"
#import "extensions/NSString+Helpers.h"

#import "library/VLCLibraryImageCache.h"
#import "library/VLCInputItem.h"

#import "main/VLCMain.h"

#import "playqueue/VLCPlayQueueItem.h"

#import "views/VLCImageView.h"

#import <vlc_common.h>
#import <vlc_configuration.h>

NSString * const VLCDisplayTrackNumberPlayQueueKey = @"VLCDisplayTrackNumberPlayQueueKey";
NSString * const VLCDisplayTrackNumberPlayQueueSettingChanged = @"VLCDisplayTrackNumberPlayQueueSettingChanged";

@implementation VLCPlayQueueTableCellView

- (void)dealloc
{
    [NSNotificationCenter.defaultCenter removeObserver:self];
}

- (void)awakeFromNib
{
    [super awakeFromNib];
    [NSNotificationCenter.defaultCenter addObserver:self
                                           selector:@selector(updateRepresentation)
                                               name:VLCDisplayTrackNumberPlayQueueSettingChanged
                                             object:nil];
}

- (void)prepareForReuse
{
    [super prepareForReuse];
    [self updateFonts];
    [self updateColouredElements];
}

- (void)setRepresentsCurrentPlayQueueItem:(BOOL)representsCurrentPlayQueueItem
{
    _representsCurrentPlayQueueItem = representsCurrentPlayQueueItem;

    [self updateFonts];
    [self updateColouredElements];
}

- (void)updateFonts
{
    NSFont * const displayedFont = _representsCurrentPlayQueueItem ?
        [NSFont boldSystemFontOfSize:NSFont.systemFontSize] :
        [NSFont systemFontOfSize:NSFont.systemFontSize];

    NSFont * const sublineDisplayedFont = _representsCurrentPlayQueueItem ?
        [NSFont boldSystemFontOfSize:NSFont.smallSystemFontSize] :
        [NSFont systemFontOfSize:NSFont.smallSystemFontSize];

    self.mediaTitleTextField.font = displayedFont;
    self.secondaryMediaTitleTextField.font = displayedFont;
    self.durationTextField.font = displayedFont;
    self.artistTextField.font = sublineDisplayedFont;
}

- (void)updateColouredElements
{
    self.audioMediaTypeIndicator.textColor = _representsCurrentPlayQueueItem ?
        NSColor.labelColor :
        NSColor.secondaryLabelColor;
}

- (void)setRepresentedPlayQueueItem:(VLCPlayQueueItem *)item
{
    _representedPlayQueueItem = item;
    [self updateRepresentation];
}

- (void)updateRepresentation
{
    VLCPlayQueueItem * const item = self.representedPlayQueueItem;

    __weak typeof(self) weakSelf = self;
    [VLCLibraryImageCache thumbnailForPlayQueueItem:item withCompletion:^(NSImage * const thumbnail) {
        if (!weakSelf || item != weakSelf.representedPlayQueueItem) {
            return;
        }
        weakSelf.audioArtworkImageView.image = thumbnail;
        weakSelf.mediaImageView.image = thumbnail;
    }];

    const BOOL validArtistString = item.artistName && item.artistName.length > 0;
    const BOOL validAlbumString = item.albumName && item.albumName.length > 0;
    const BOOL validTitleString = item.inputItem && item.inputItem.title && (item.inputItem.title.length > 0);

    NSString *playTitle = item.title;
    if (validTitleString) {
        NSString * const trackNumberString = item.inputItem.trackNumber;
        if ([NSUserDefaults.standardUserDefaults boolForKey:VLCDisplayTrackNumberPlayQueueKey] && trackNumberString && trackNumberString.length > 0 && ![trackNumberString isEqualToString:@"0"]) {
            playTitle = [NSString stringWithFormat:@"%@ · %@", trackNumberString, item.inputItem.title];
        } else {
            playTitle = item.inputItem.title;
        }
    }

    NSString *songDetailString = @"";

    if (validArtistString && validAlbumString) {
        songDetailString = [NSString stringWithFormat:@"%@ · %@", item.artistName, item.albumName];
    } else if (validArtistString) {
        songDetailString = item.artistName;
    }

    if (songDetailString && ![songDetailString isEqualToString:@""]) {
        self.mediaTitleTextField.hidden = YES;
        self.secondaryMediaTitleTextField.hidden = NO;
        self.artistTextField.hidden = NO;
        self.secondaryMediaTitleTextField.stringValue = playTitle;
        self.artistTextField.stringValue = songDetailString;
        self.audioMediaTypeIndicator.hidden = NO;

        self.audioArtworkImageView.hidden = NO;
        self.mediaImageView.hidden = YES;
    } else {
        self.mediaTitleTextField.hidden = NO;
        self.secondaryMediaTitleTextField.hidden = YES;
        self.artistTextField.hidden = YES;
        self.mediaTitleTextField.stringValue = playTitle;
        self.audioMediaTypeIndicator.hidden = YES;

        self.audioArtworkImageView.hidden = YES;
        self.mediaImageView.hidden = NO;
    }

    self.durationTextField.stringValue = [NSString stringWithTimeFromTicks:item.duration];
}

@end
