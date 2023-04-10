/*****************************************************************************
 * VLCLibrarySongsTableViewSongPlayingTableCellView.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2022 VLC authors and VideoLAN
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

#import "VLCLibrarySongsTableViewSongPlayingTableCellView.h"

#import "library/VLCInputItem.h"
#import "library/VLCLibraryDataTypes.h"
#import "main/VLCMain.h"
#import "playlist/VLCPlayerController.h"
#import "playlist/VLCPlaylistController.h"

@implementation VLCLibrarySongsTableViewSongPlayingTableCellView

- (void)awakeFromNib
{
    NSNotificationCenter *notificationCenter = [NSNotificationCenter defaultCenter];
    [notificationCenter addObserver:self
                           selector:@selector(playStateOrItemChanged:)
                               name:VLCPlayerCurrentMediaItemChanged
                             object:nil];
    [notificationCenter addObserver:self
                           selector:@selector(playStateOrItemChanged:)
                               name:VLCPlayerStateChanged
                             object:nil];
}

- (void)dealloc
{
    [[NSNotificationCenter defaultCenter] removeObserver:self];
}

- (void)prepareForReuse
{
    [super prepareForReuse];
    self.textField.stringValue = @"";
}

- (void)setRepresentedMediaItem:(VLCMediaLibraryMediaItem *)representedMediaItem
{
    _representedMediaItem = representedMediaItem;
    [self updatePlayState];
}

- (BOOL)isCurrentSong
{
    if (!_representedMediaItem ||
        !_representedMediaItem.inputItem ||
        !VLCMain.sharedInstance ||
        !VLCMain.sharedInstance.playlistController ||
        !VLCMain.sharedInstance.playlistController.currentlyPlayingInputItem) {

        return false;
    }

    return [_representedMediaItem.inputItem.MRL isEqualToString:VLCMain.sharedInstance.playlistController.currentlyPlayingInputItem.MRL];
}

- (void)updatePlayState
{
    if (!_representedMediaItem || ![self isCurrentSong]) {
        self.textField.stringValue = @"";
        return;
    }

    NSString *text = @"";

    switch(VLCMain.sharedInstance.playlistController.playerController.playerState) {
        case VLC_PLAYER_STATE_PAUSED:
            text = @"⏸︎";
            break;
        case VLC_PLAYER_STATE_PLAYING:
            text = @"▶";
        default:
            break;
    }

    self.textField.stringValue = text;
}

- (void)playStateOrItemChanged:(id)sender
{
    [self updatePlayState];
}

@end
