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

@property (readonly) NSImageSymbolConfiguration *mainButtonsSymbolConfig API_AVAILABLE(macos(26.0));

@end

@implementation VLCMainVideoViewControlsBar

- (void)awakeFromNib
{
    [super awakeFromNib];

    self.bookmarksButton.toolTip = _NS("Bookmarks");
    self.bookmarksButton.accessibilityLabel = self.bookmarksButton.toolTip;

    self.subtitlesButton.toolTip = _NS("Subtitles");
    self.subtitlesButton.accessibilityLabel = self.subtitlesButton.toolTip;

    self.audioButton.toolTip = _NS("Audio");
    self.audioButton.accessibilityLabel = self.audioButton.toolTip;

    self.videoButton.toolTip = _NS("Video");
    self.videoButton.accessibilityLabel = self.videoButton.toolTip;

    self.playbackRateButton.toolTip = _NS("Playback Rate");
    self.playbackRateButton.accessibilityLabel = self.playbackRateButton.toolTip;

    self.floatOnTopButton.toolTip = _NS("Float on Top");
    self.floatOnTopButton.accessibilityLabel = self.floatOnTopButton.toolTip;

    self.pipButton.toolTip = _NS("Picture in Picture");
    self.pipButton.accessibilityLabel = self.pipButton.toolTip;

    if (@available(macOS 26.0, *)) {
#if __MAC_OS_X_VERSION_MAX_ALLOWED >= 260000
        _mainButtonsSymbolConfig = [NSImageSymbolConfiguration configurationWithPaletteColors:@[NSColor.whiteColor]];

        NSPointerArray * const buttons = NSPointerArray.weakObjectsPointerArray;
        [buttons addPointer:(__bridge void *)self.playButton];
        [buttons addPointer:(__bridge void *)self.backwardButton];
        [buttons addPointer:(__bridge void *)self.forwardButton];
        [buttons addPointer:(__bridge void *)self.jumpBackwardButton];
        [buttons addPointer:(__bridge void *)self.jumpForwardButton];
        [buttons addPointer:(__bridge void *)self.bookmarksButton];
        [buttons addPointer:(__bridge void *)self.subtitlesButton];
        [buttons addPointer:(__bridge void *)self.audioButton];
        [buttons addPointer:(__bridge void *)self.videoButton];
        [buttons addPointer:(__bridge void *)self.fullscreenButton];
        [buttons addPointer:(__bridge void *)self.floatOnTopButton];
        [buttons addPointer:(__bridge void *)self.playbackRateButton];
        [buttons addPointer:(__bridge void *)self.pipButton];
        [buttons addPointer:(__bridge void *)self.muteVolumeButton];
        [buttons compact];
        
        for (NSButton * const button in buttons) {
            button.bordered = YES;
            button.borderShape = NSControlBorderShapeCapsule;
            button.bezelStyle = NSBezelStyleGlass;
            button.layer = [CALayer layer];
            button.image = [button.image imageWithSymbolConfiguration:_mainButtonsSymbolConfig];
            button.alternateImage = [button.alternateImage imageWithSymbolConfiguration:_mainButtonsSymbolConfig];
        }
#endif
    }

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
    [notificationCenter addObserver:self
                           selector:@selector(updateAvailableButtons:)
                               name:VLCPlayerCurrentMediaItemChanged
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

    if (@available(macOS 26.0, *)) {
        NSMutableAttributedString * const colorTitle = [[NSMutableAttributedString alloc] initWithAttributedString:self.playbackRateButton.attributedTitle];
        const NSRange titleRange = NSMakeRange(0, colorTitle.length);
        [colorTitle addAttribute:NSForegroundColorAttributeName value:NSColor.whiteColor range:titleRange];
        self.playbackRateButton.attributedTitle = colorTitle;
    }
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

- (void)updateAvailableButtons:(id)sender
{
    const BOOL currentItemIsAudio = _playerController.currentMediaIsAudioOnly;
    self.videoButton.hidden = currentItemIsAudio;
    self.subtitlesButton.hidden = currentItemIsAudio;
}

- (void)playerStateUpdated:(NSNotification *)notification
{
    [super playerStateUpdated:notification];
    if (@available(macOS 26.0, *)) {
        if (self.mainButtonsSymbolConfig == nil)
            return;
        self.playButton.image = [self.playButton.image imageWithSymbolConfiguration:self.mainButtonsSymbolConfig];
        self.playButton.alternateImage = [self.playButton.alternateImage imageWithSymbolConfiguration:self.mainButtonsSymbolConfig];
    }
}

- (void)updateCurrentItemDisplayControls:(NSNotification *)notification
{
    [super updateCurrentItemDisplayControls:notification];
    if (@available(macOS 26.0, *)) {
        if (self.mainButtonsSymbolConfig == nil)
            return;
        self.forwardButton.image = [self.forwardButton.image imageWithSymbolConfiguration:self.mainButtonsSymbolConfig];
        self.forwardButton.alternateImage = [self.forwardButton.alternateImage imageWithSymbolConfiguration:self.mainButtonsSymbolConfig];
        self.backwardButton.image = [self.backwardButton.image imageWithSymbolConfiguration:self.mainButtonsSymbolConfig];
        self.backwardButton.alternateImage = [self.backwardButton.alternateImage imageWithSymbolConfiguration:self.mainButtonsSymbolConfig];
        self.jumpForwardButton.image = [self.jumpForwardButton.image imageWithSymbolConfiguration:self.mainButtonsSymbolConfig];
        self.jumpForwardButton.alternateImage = [self.jumpForwardButton.alternateImage imageWithSymbolConfiguration:self.mainButtonsSymbolConfig];
        self.jumpBackwardButton.image = [self.jumpBackwardButton.image imageWithSymbolConfiguration:self.mainButtonsSymbolConfig];
        self.jumpBackwardButton.alternateImage = [self.jumpBackwardButton.alternateImage imageWithSymbolConfiguration:self.mainButtonsSymbolConfig];
    }
}

- (void)updateMuteVolumeButtonImage
{
    [super updateMuteVolumeButtonImage];
    if (@available(macOS 26.0, *)) {
        if (self.mainButtonsSymbolConfig == nil)
            return;
        self.muteVolumeButton.image = [self.muteVolumeButton.image imageWithSymbolConfiguration:self.mainButtonsSymbolConfig];
    }
}

@end
