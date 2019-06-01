/*****************************************************************************
 * VLCPlaylistTableViewCell.m: MacOS X interface module
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

#import "VLCPlaylistTableCellView.h"
#import "extensions/NSString+Helpers.h"
#import "extensions/NSFont+VLCAdditions.h"
#import "playlist/VLCPlaylistItem.h"
#import "views/VLCImageView.h"
#import "main/VLCMain.h"

@interface VLCPlaylistTableCellView ()
{
    NSFont *_displayedFont;
    NSFont *_displayedBoldFont;
}
@end

@implementation VLCPlaylistTableCellView

- (void)awakeFromNib
{
    [self updateFontsBasedOnSetting:nil];
    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(updateFontsBasedOnSetting:)
                                                 name:VLCConfigurationChangedNotification
                                               object:nil];
}

- (void)dealloc
{
    [[NSNotificationCenter defaultCenter] removeObserver:self];
}

- (void)setRepresentsCurrentPlaylistItem:(BOOL)representsCurrentPlaylistItem
{
    _representsCurrentPlaylistItem = representsCurrentPlaylistItem;
    NSFont *displayedFont = _representsCurrentPlaylistItem ? _displayedBoldFont : _displayedFont;
    self.mediaTitleTextField.font = displayedFont;
    self.secondaryMediaTitleTextField.font = displayedFont;
    self.artistTextField.font = _displayedFont;
    self.durationTextField.font = _displayedFont;
}

- (void)setRepresentedPlaylistItem:(VLCPlaylistItem *)item
{
    NSString *artist = item.artistName;
    if (artist && artist.length > 0) {
        self.mediaTitleTextField.hidden = YES;
        self.secondaryMediaTitleTextField.hidden = NO;
        self.artistTextField.hidden = NO;
        self.secondaryMediaTitleTextField.stringValue = item.title;
        self.artistTextField.stringValue = artist;
        self.audioArtworkImageView.image = item.artworkImage;
        self.audioMediaTypeIndicator.hidden = NO;
        self.mediaImageView.hidden = YES;
    } else {
        self.mediaTitleTextField.hidden = NO;
        self.secondaryMediaTitleTextField.hidden = YES;
        self.artistTextField.hidden = YES;
        self.mediaTitleTextField.stringValue = item.title;
        self.mediaImageView.image = item.artworkImage;
        self.audioArtworkImageView.hidden = YES;
        self.audioMediaTypeIndicator.hidden = YES;
    }

    self.durationTextField.stringValue = [NSString stringWithTimeFromTicks:item.duration];

    _representedPlaylistItem = item;
}

- (void)updateFontsBasedOnSetting:(NSNotification *)aNotification
{
    BOOL largeText = config_GetInt("macosx-large-text");
    if (largeText) {
        _displayedFont = [NSFont VLCplaylistLabelFont];
        _displayedBoldFont = [NSFont VLCplaylistSelectedItemLabelFont];
    } else {
        _displayedFont = [NSFont VLCsmallPlaylistLabelFont];
        _displayedBoldFont = [NSFont VLCsmallPlaylistSelectedItemLabelFont];
    }
    [self setRepresentsCurrentPlaylistItem:_representsCurrentPlaylistItem];
}

@end
