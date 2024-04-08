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

#import "library/VLCLibraryDataTypes.h"

#import "main/VLCMain.h"

#import "menus/VLCMainMenu.h"

#import "panels/VLCBookmarksWindowController.h"

#import "playlist/VLCPlaylistController.h"
#import "playlist/VLCPlayerController.h"

#import "views/VLCWrappableTextField.h"

#import "windows/video/VLCMainVideoViewController.h"
#import "windows/video/VLCVideoOutputProvider.h"
#import "windows/video/VLCVideoWindowCommon.h"

@interface VLCMainVideoViewControlsBar ()
{
    VLCPlaylistController *_playlistController;
    VLCPlayerController *_playerController;
}
@end

@implementation VLCMainVideoViewControlsBar

- (void)awakeFromNib
{
    [super awakeFromNib];

    _bookmarksButton.toolTip = _NS("Bookmarks");
    _bookmarksButton.accessibilityLabel = _bookmarksButton.toolTip;

    _subtitlesButton.toolTip = _NS("Subtitle settings");
    _subtitlesButton.accessibilityLabel = _subtitlesButton.toolTip;

    _audioButton.toolTip = _NS("Audio settings");
    _audioButton.accessibilityLabel = _audioButton.toolTip;

    _playlistController = VLCMain.sharedInstance.playlistController;
    _playerController = _playlistController.playerController;

    NSNotificationCenter * const notificationCenter = NSNotificationCenter.defaultCenter;
    [notificationCenter addObserver:self
                           selector:@selector(updateDetailLabel:)
                               name:VLCPlayerCurrentMediaItemChanged
                             object:nil];
    [notificationCenter addObserver:self
                           selector:@selector(updateFloatOnTopButton:)
                               name:VLCWindowFloatOnTopChangedNotificationName
                             object:nil];
}

- (void)updateDetailLabel:(NSNotification *)notification
{

    VLCMediaLibraryMediaItem * const mediaItem = [VLCMediaLibraryMediaItem mediaItemForURL:_playerController.URLOfCurrentMediaItem];
    if (!mediaItem) {
        return;
    }

    _detailLabel.hidden = [mediaItem.primaryDetailString isEqualToString:@""] ||
                          [mediaItem.primaryDetailString isEqualToString:mediaItem.durationString];
    _detailLabel.stringValue = mediaItem.primaryDetailString;
}

- (IBAction)openBookmarks:(id)sender
{
    [VLCMain.sharedInstance.bookmarks toggleWindow:sender];
}

- (IBAction)openSubtitlesMenu:(id)sender
{
    NSMenu * const menu = VLCMain.sharedInstance.mainMenu.subtitlesMenu;
    [menu popUpMenuPositioningItem:nil
                        atLocation:_subtitlesButton.frame.origin
                            inView:((NSView *)sender).superview];
}

- (IBAction)openAudioMenu:(id)sender
{
    NSMenu * const menu = VLCMain.sharedInstance.mainMenu.audioMenu;
    [menu popUpMenuPositioningItem:nil
                        atLocation:_audioButton.frame.origin
                            inView:((NSView *)sender).superview];
}

- (IBAction)toggleFloatOnTop:(id)sender
{
    VLCVideoWindowCommon * const window = (VLCVideoWindowCommon *)self.floatOnTopButton.window;
    if (window == nil) {
        return;
    }
    vout_thread_t * const p_vout = window.videoViewController.voutView.voutThread;
    if (!p_vout) {
        return;
    }
    var_ToggleBool(p_vout, "video-on-top");
    vout_Release(p_vout);
}

- (void)updateFloatOnTopButton:(NSNotification *)notification
{
    VLCVideoWindowCommon * const videoWindow = (VLCVideoWindowCommon *)notification.object;
    NSAssert(videoWindow != nil, @"Received video window should not be nil!");
    NSDictionary<NSString *, NSNumber *> * const userInfo = notification.userInfo;
    NSAssert(userInfo != nil, @"Received user info should not be nil!");
    NSNumber * const enabledNumberWrapper = userInfo[VLCWindowFloatOnTopEnabledNotificationKey];
    NSAssert(enabledNumberWrapper != nil, @"Received user info enabled wrapper should not be nil!");

    if (@available(macOS 10.14, *)) {
        self.floatOnTopButton.contentTintColor =
            enabledNumberWrapper.boolValue ? NSColor.controlAccentColor : NSColor.controlTextColor;
    }
}

@end
