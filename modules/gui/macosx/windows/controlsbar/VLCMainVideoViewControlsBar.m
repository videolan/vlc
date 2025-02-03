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

#import "playqueue/VLCPlayQueueController.h"
#import "playqueue/VLCPlayerController.h"

#import "windows/video/VLCMainVideoViewController.h"
#import "windows/video/VLCVideoOutputProvider.h"
#import "windows/video/VLCVideoWindowCommon.h"

@interface VLCMainVideoViewControlsBar ()
{
    VLCPlayQueueController *_playQueueController;
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

    self.videoButton.toolTip = _NS("Video settings");
    self.videoButton.accessibilityLabel = self.videoButton.toolTip;

    self.playbackRateButton.toolTip = _NS("Playback rate");
    self.playbackRateButton.accessibilityLabel = self.playbackRateButton.toolTip;

    _playQueueController = VLCMain.sharedInstance.playQueueController;
    _playerController = _playQueueController.playerController;

    NSNotificationCenter * const notificationCenter = NSNotificationCenter.defaultCenter;
    [notificationCenter addObserver:self
                           selector:@selector(floatOnTopChanged:)
                               name:VLCWindowFloatOnTopChangedNotificationName
                             object:nil];
    [notificationCenter addObserver:self
                           selector:@selector(playbackRateChanged:)
                               name:VLCPlayerRateChanged
                             object:nil];
    [notificationCenter addObserver:self
                           selector:@selector(playbackRateChanged:)
                               name:VLCPlayerCapabilitiesChanged
                             object:nil];

    [self update];
}

- (void)update
{
    [super update];
    [self updateFloatOnTopButton];
    [self updatePlaybackRateButton];
}

- (void)floatOnTopChanged:(NSNotification *)notification
{
    VLCVideoWindowCommon * const videoWindow = (VLCVideoWindowCommon *)notification.object;
    NSAssert(videoWindow != nil, @"Received video window should not be nil!");
    VLCVideoWindowCommon * const selfVideoWindow =
        (VLCVideoWindowCommon *)self.floatOnTopButton.window;

    if (videoWindow != selfVideoWindow) {
        return;
    }

    [self updateFloatOnTopButton];
}

- (void)updateFloatOnTopButton
{
    VLCVideoWindowCommon * const videoWindow = (VLCVideoWindowCommon *)self.floatOnTopButton.window;
    if (videoWindow == nil) {
        return;
    }

    VLCVoutView * const voutView = videoWindow.videoViewController.voutView;
    NSAssert(voutView != nil, @"Vout view should not be nil!");
    vout_thread_t * const voutThread = voutView.voutThread;

    if (voutThread == NULL) {
        return;
    }

    const bool floatOnTopEnabled = var_GetBool(voutThread, "video-on-top");

    if (@available(macOS 10.14, *)) {
        self.floatOnTopButton.contentTintColor =
            floatOnTopEnabled ? NSColor.controlAccentColor : nil;
    }
}

- (void)playbackRateChanged:(NSNotification *)notification
{
    [self updatePlaybackRateButton];
}

- (void)updatePlaybackRateButton
{
    self.playbackRateButton.title =
        [NSString stringWithFormat:@"%.1fx", _playerController.playbackRate];
    self.playbackRateButton.enabled = _playerController.rateChangable;
}

- (IBAction)openPlaybackRate:(id)sender
{
    NSSlider * const playbackRateSlider = [[NSSlider alloc] init];
    playbackRateSlider.frame = NSMakeRect(0, 0, 272, 17);
    playbackRateSlider.target = self;
    playbackRateSlider.action = @selector(rateSliderAction:);
    playbackRateSlider.minValue = -34.;
    playbackRateSlider.maxValue = 34.;
    playbackRateSlider.sliderType = NSSliderTypeLinear;
    playbackRateSlider.numberOfTickMarks = 17;
    playbackRateSlider.controlSize = NSControlSizeSmall;

    const double value = 17.0 * log(_playerController.playbackRate) / log(2.);
    const int sliderIntValue = (int)((value > 0) ? value + 0.5 : value - 0.5);
    playbackRateSlider.intValue = sliderIntValue;

    NSMenuItem * const menuItem = [[NSMenuItem alloc] init];
    menuItem.title = _NS("Playback rate");
    menuItem.view = playbackRateSlider;

    NSMenu * const menu = [[NSMenu alloc] initWithTitle:_NS("Playback rate")];
    [menu addItem:menuItem];
    [menu popUpMenuPositioningItem:nil
                        atLocation:self.playbackRateButton.frame.origin
                            inView:((NSView *)sender).superview];
}

- (IBAction)rateSliderAction:(id)sender
{
    NSSlider * const slider = (NSSlider *)sender;
    _playerController.playbackRate = pow(2, (double)slider.intValue / 17);
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

- (IBAction)openVideoMenu:(id)sender
{
    NSMenu * const menu = VLCMain.sharedInstance.mainMenu.videoMenu;
    [menu popUpMenuPositioningItem:nil
                        atLocation:self.videoButton.frame.origin
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

@end
