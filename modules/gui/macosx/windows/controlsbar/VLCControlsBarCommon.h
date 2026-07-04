/*****************************************************************************
 * VLCControlsBarCommon.h: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2012-2019 VLC authors and VideoLAN
 *
 * Authors: Felix Paul Kühne <fkuehne -at- videolan -dot- org>
 *          David Fuhrmann <david dot fuhrmann at googlemail dot com>
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

#import <Cocoa/Cocoa.h>

@class VLCDragDropView;
@class VLCPlaybackProgressSlider;
@class VLCVolumeSlider;
@class VLCTimeField;
@class VLCImageView;
@class VLCBottomBarView;

/*****************************************************************************
 * VLCControlsBarCommon
 *
 *  Holds all outlets, actions and code common for controls bar in detached
 *  and in main window.
 *****************************************************************************/

@interface VLCControlsBarCommon : NSObject

@property (readwrite, weak) IBOutlet VLCDragDropView *dropView;

@property (readwrite, weak) IBOutlet NSButton *playButton;
@property (readwrite, weak) IBOutlet NSButton *backwardButton;
@property (readwrite, weak) IBOutlet NSButton *forwardButton;
@property (readwrite, weak) IBOutlet NSButton *jumpBackwardButton;
@property (readwrite, weak) IBOutlet NSButton *jumpForwardButton;

@property (readwrite, weak) IBOutlet NSLayoutConstraint *jumpBackwardButtonWidthConstraint;
@property (readwrite, weak) IBOutlet NSLayoutConstraint *jumpForwardButtonWidthConstraint;
@property (readwrite, weak) IBOutlet NSLayoutConstraint *jumpBackwardButtonSpacingConstraint;
@property (readwrite, weak) IBOutlet NSLayoutConstraint *jumpForwardButtonSpacingConstraint;

@property (readwrite, weak) IBOutlet VLCPlaybackProgressSlider *timeSlider;
@property (readwrite, weak) IBOutlet VLCVolumeSlider *volumeSlider;
@property (readwrite, weak) IBOutlet NSButton *muteVolumeButton;

@property (readwrite, weak) IBOutlet VLCImageView *artworkImageView;
@property (readwrite, weak) IBOutlet NSButton *artworkButton;
@property (readwrite, weak) IBOutlet NSTextField *playingItemDisplayField;
@property (readwrite, weak) IBOutlet NSTextField *detailLabel;
@property (readwrite, weak) IBOutlet VLCTimeField *timeField;
@property (readwrite, weak) IBOutlet VLCTimeField *trailingTimeField;

@property (readwrite, weak) IBOutlet NSButton *fullscreenButton;
@property (readwrite, weak) IBOutlet NSLayoutConstraint *fullscreenButtonWidthConstraint;

@property (readwrite, weak) IBOutlet NSButton *pipButton;
@property (readwrite, weak) IBOutlet NSLayoutConstraint *pipButtonWidthConstraint;

@property (readwrite, weak) IBOutlet VLCBottomBarView *bottomBarView;

@property (readonly) BOOL nativeFullscreenMode;

- (CGFloat)height;

- (IBAction)play:(id)sender;
- (IBAction)bwd:(id)sender;
- (IBAction)fwd:(id)sender;
- (IBAction)jumpBackward:(id)sender;
- (IBAction)jumpForward:(id)sender;

- (IBAction)timeSliderAction:(id)sender;
- (IBAction)volumeAction:(id)sender;
- (IBAction)fullscreen:(id)sender;
- (IBAction)onPipButtonClick:(id)sender;

- (void)update;
- (void)updateMuteVolumeButtonImage;

- (void)updateTimeSlider:(NSNotification *)aNotification;
- (void)updateVolumeSlider:(NSNotification *)aNotification;
- (void)updateMuteVolumeButton:(NSNotification *)aNotification;
- (void)updateCurrentItemDisplayControls:(NSNotification *)aNotification;

- (void)playerStateUpdated:(NSNotification *)notification;
- (void)updateCurrentItemDisplayControls:(NSNotification *)notification;

@end
