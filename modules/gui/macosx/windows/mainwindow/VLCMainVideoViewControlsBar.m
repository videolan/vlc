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

#import "menus/VLCMainMenu.h"

#import "panels/VLCBookmarksWindowController.h"

#import "playlist/VLCPlaylistController.h"
#import "playlist/VLCPlayerController.h"

@implementation VLCMainVideoViewControlsBar

- (void)awakeFromNib
{
    [super awakeFromNib];

    _bookmarksButton.toolTip = _NS("Bookmarks");
    _bookmarksButton.accessibilityLabel = _bookmarksButton.toolTip;

    [self updateItemDependentButtons:self];

    NSNotificationCenter * const notificationCenter = [NSNotificationCenter defaultCenter];
    [notificationCenter addObserver:self
                           selector:@selector(updateItemDependentButtons:)
                               name:VLCPlayerCurrentMediaItemChanged
                             object:nil];
    [notificationCenter addObserver:self
                           selector:@selector(updateItemDependentButtons:)
                               name:VLCPlayerTrackListChanged
                             object:nil];
}

- (IBAction)openBookmarks:(id)sender
{
    [VLCMain.sharedInstance.bookmarks toggleWindow:sender];
}

- (IBAction)openSubtitlesMenu:(id)sender
{
    NSMenu *menu = VLCMain.sharedInstance.mainMenu.subtitlesMenu;
    [menu popUpMenuPositioningItem:nil
                        atLocation:_subtitlesButton.frame.origin
                            inView:((NSView *)sender).superview];
}

- (void)updateItemDependentButtons:(id)sender
{
    [self updateSubtitleButtonVisibility];
    [self updateAudioTracksButtonVisibility];
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
