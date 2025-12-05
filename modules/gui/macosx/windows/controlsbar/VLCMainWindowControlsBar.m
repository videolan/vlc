/*****************************************************************************
 * VLCMainWindowControlsBar.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2012-2019 VLC authors and VideoLAN
 *
 * Authors: Felix Paul Kühne <fkuehne # videolan dot org>
 *          David Fuhrmann <dfuhrmann # videolan dot org>
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

#import "VLCMainWindowControlsBar.h"
#import "VLCControlsBarCommon.h"

#import "extensions/NSColor+VLCAdditions.h"
#import "extensions/NSImage+VLCAdditions.h"
#import "extensions/NSString+Helpers.h"
#import "extensions/NSView+VLCAdditions.h"

#import "library/VLCInputItem.h"
#import "library/VLCLibraryUIUnits.h"
#import "library/VLCLibraryWindow.h"

#import "main/VLCMain.h"

#import "playqueue/VLCPlayQueueController.h"
#import "playqueue/VLCPlayQueueItem.h"
#import "playqueue/VLCPlayQueueModel.h"
#import "playqueue/VLCPlayerController.h"

#import "views/VLCBottomBarView.h"
#import "views/VLCTimeField.h"
#import "views/VLCTrackingView.h"
#import "views/VLCVolumeSlider.h"

/*****************************************************************************
 * VLCMainWindowControlsBar
 *
 *  Holds all specific outlets, actions and code for the main window controls bar.
 *****************************************************************************/

@interface VLCMainWindowControlsBar()
{
    NSImage *_alwaysMuteImage;

    VLCPlayQueueController *_playQueueController;
    VLCPlayerController *_playerController;
}
@end

@implementation VLCMainWindowControlsBar

- (void)awakeFromNib
{
    [super awakeFromNib];

    if (@available(macOS 26.0, *)) {
#if __MAC_OS_X_VERSION_MAX_ALLOWED >= 260000
        NSGlassEffectView * const glassEffectView = [[NSGlassEffectView alloc] init];
        glassEffectView.translatesAutoresizingMaskIntoConstraints = NO;
        self.bottomBarView.subviews = @[glassEffectView];
        glassEffectView.contentView = self.dropView;
        [glassEffectView applyConstraintsToFillSuperview];
        glassEffectView.cornerRadius = CGFLOAT_MAX;
        self.bottomBarView.drawBorder = NO;
        self.bottomBarView.clipsToBounds = NO;
#endif
    } else {
        self.visualEffectView.wantsLayer = YES;
        self.visualEffectView.layer.cornerRadius = VLCLibraryUIUnits.cornerRadius;
        self.visualEffectView.layer.masksToBounds = YES;
        self.visualEffectView.layer.borderWidth = VLCLibraryUIUnits.borderThickness;
        self.visualEffectView.layer.borderColor = NSColor.VLCSubtleBorderColor.CGColor;
    }

    _playQueueController = VLCMain.sharedInstance.playQueueController;
    _playerController = _playQueueController.playerController;

    NSNotificationCenter *notificationCenter = NSNotificationCenter.defaultCenter;
    [notificationCenter addObserver:self
                           selector:@selector(updatePlaybackControls:)
                               name:VLCPlayerCurrentMediaItemChanged
                             object:nil];
    [notificationCenter addObserver:self
                           selector:@selector(playbackStateChanged:)
                               name:VLCPlayerStateChanged
                             object:nil];

    [self.stopButton setToolTip: _NS("Stop")];
    self.stopButton.accessibilityLabel = self.stopButton.toolTip;
    
    [self.volumeUpButton setToolTip: _NS("Full Volume")];
    self.volumeUpButton.accessibilityLabel = self.volumeUpButton.toolTip;

    if (@available(macOS 11.0, *)) {
        _alwaysMuteImage = [NSImage imageWithSystemSymbolName:@"speaker.minus.fill"
                                     accessibilityDescription:_NS("Mute")];

        [self.stopButton setImage: [NSImage imageWithSystemSymbolName:@"stop.fill"
                                             accessibilityDescription:_NS("Stop")]];
        [self.volumeUpButton setImage: [NSImage imageWithSystemSymbolName:@"speaker.plus.fill"
                                                 accessibilityDescription:_NS("Volume up")]];
    } else {
        _alwaysMuteImage = [NSImage imageNamed:@"VLCVolumeOffTemplate"];

        [self.stopButton setImage: imageFromRes(@"stop")];
        [self.stopButton setAlternateImage: imageFromRes(@"stop-pressed")];
        [self.volumeUpButton setImage: imageFromRes(@"VLCVolumeOnTemplate")];
    }

    [self updateMuteVolumeButtonImage];

    [self playbackStateChanged:nil];
    [self.stopButton setHidden:YES];

    [self.timeField setAlignment: NSCenterTextAlignment];
    [self.trailingTimeField setAlignment: NSCenterTextAlignment];

    [self.thumbnailTrackingView setViewToHide:_openMainVideoViewButtonOverlay];

    if (@available(macOS 10.14, *)) {
        [NSApplication.sharedApplication addObserver:self
                                        forKeyPath:@"effectiveAppearance"
                                            options:NSKeyValueObservingOptionNew
                                            context:nil];
    }
}

- (void)observeValueForKeyPath:(NSString *)keyPath
                      ofObject:(id)object
                        change:(NSDictionary<NSKeyValueChangeKey,id> *)change
                       context:(void *)context
{
    if (@available(macOS 26.0, *)) {
        return;
    } else if (@available(macOS 10.14, *)) {
        NSAppearance * const appearance = change[NSKeyValueChangeNewKey];
        const BOOL isDark = [appearance.name isEqualToString:NSAppearanceNameDarkAqua] || 
                            [appearance.name isEqualToString:NSAppearanceNameVibrantDark];
        self.visualEffectView.layer.borderColor = isDark ?
            NSColor.VLCDarkSubtleBorderColor.CGColor : NSColor.VLCLightSubtleBorderColor.CGColor;
    }
}

#pragma mark -
#pragma mark Extra button actions

- (IBAction)stop:(id)sender
{
    [_playQueueController stopPlayback];
}

// dynamically created next / prev buttons
- (IBAction)prev:(id)sender
{
    [_playQueueController playPreviousItem];
}

- (IBAction)next:(id)sender
{
    [_playQueueController playNextItem];
}

- (IBAction)volumeAction:(id)sender
{
    if (sender == self.volumeUpButton) {
        [_playerController setVolume:VLCVolumeMaximum];
    } else {
        [super volumeAction:sender];
    }
}

- (IBAction)artworkButtonAction:(id)sender
{
    [VLCMain.sharedInstance.libraryWindow enableVideoPlaybackAppearance];
}

#pragma mark -
#pragma mark Extra updaters

- (void)updateVolumeSlider:(NSNotification *)aNotification
{
    [super updateVolumeSlider:aNotification];
    BOOL b_muted = _playerController.mute;
    [self.volumeUpButton setEnabled: !b_muted];
}

- (void)updateCurrentItemDisplayControls:(NSNotification *)aNotification
{
    [super updateCurrentItemDisplayControls:aNotification];

    VLCInputItem * const inputItem = _playerController.currentMedia;
    if (!inputItem) {
        return;
    }

    NSFont * const boldSystemFont = [NSFont boldSystemFontOfSize:12.];
    NSDictionary<NSString *, id> * const boldAttribute = @{NSFontAttributeName: boldSystemFont};

    NSMutableAttributedString * const displayString = [[NSMutableAttributedString alloc] initWithString:inputItem.name attributes:boldAttribute];

    if (inputItem.artist.length != 0) {
        NSAttributedString * const separator = [[NSAttributedString alloc] initWithString:@" · " attributes:boldAttribute];
        [displayString appendAttributedString:separator];

        NSAttributedString * const artistString = [[NSAttributedString alloc] initWithString:inputItem.artist];
        [displayString appendAttributedString:artistString];
    }

    self.playingItemDisplayField.attributedStringValue = displayString;
}

- (void)updateMuteVolumeButtonImage
{
    self.muteVolumeButton.image = _alwaysMuteImage;
}

- (void)playbackStateChanged:(NSNotification *)aNotification
{
    switch (_playerController.playerState) {
        case VLC_PLAYER_STATE_STOPPING:
        case VLC_PLAYER_STATE_STOPPED:
            self.stopButton.enabled = NO;
            break;

        default:
            self.stopButton.enabled = YES;
            break;
    }
}

- (void)updatePlaybackControls:(NSNotification *)notification
{
    const BOOL b_seekable = _playerController.seekable;
    self.prevButton.enabled = b_seekable || _playQueueController.hasPreviousPlayQueueItem;
    self.nextButton.enabled = b_seekable || _playQueueController.hasNextPlayQueueItem;
    [self updateCurrentItemDisplayControls:notification];

    VLCMediaLibraryMediaItem * const currentMlItem = _playerController.currentMediaLibraryItem;
    self.favoriteButton.hidden = currentMlItem == nil;
    self.favoriteButton.state = currentMlItem.favorited ? NSControlStateValueOn : NSControlStateValueOff;
    self.favoriteButton.toolTip = currentMlItem.favorited ? _NS("Unmark as Favorite") : _NS("Mark as Favorite");
}

- (void)toggleFavorite:(id)sender
{
    const NSControlStateValue buttonStartState = self.favoriteButton.state;
    VLCMediaLibraryMediaItem * const mediaItem = [_playerController currentMediaLibraryItem];
    if (mediaItem == nil || [mediaItem toggleFavorite] != VLC_SUCCESS)
        self.favoriteButton.state = buttonStartState;
}

@end
