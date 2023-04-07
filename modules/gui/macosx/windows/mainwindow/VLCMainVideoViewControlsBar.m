/*****************************************************************************
 * VLCMainVideoViewControlsBar.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2023 VLC authors and VideoLAN
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

#import "VLCMainVideoViewControlsBar.h"

#import "extensions/NSString+Helpers.h"

#import "main/VLCMain.h"

#import "panels/VLCBookmarksWindowController.h"

#import "playlist/VLCPlaylistController.h"
#import "playlist/VLCPlayerController.h"

@implementation VLCMainVideoViewControlsBar

- (void)awakeFromNib
{
    [super awakeFromNib];

    _bookmarksButton.toolTip = _NS("Bookmarks");
    _bookmarksButton.accessibilityLabel = _bookmarksButton.toolTip;

    [self updateSubtitleButtonVisibility];
    [self updateAudioTracksButtonVisibility];
}

- (IBAction)openBookmarks:(id)sender
{
    [VLCMain.sharedInstance.bookmarks toggleWindow:sender];
}

- (void)updateSubtitleButtonVisibility
{
     NSArray * const subtitleTracks = VLCMain.sharedInstance.playlistController.playerController.subtitleTracks;
    _subtitlesButton.hidden = subtitleTracks.count == 0;
}

- (void)updateAudioTracksButtonVisibility
{
    NSArray * const audioTracks = VLCMain.sharedInstance.playlistController.playerController.audioTracks;
   _audioTracksButton.hidden = audioTracks.count == 0;
}

@end
