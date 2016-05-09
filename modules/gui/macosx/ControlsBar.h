/*****************************************************************************
 * ControlsBar.h: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2012-2016 VLC authors and VideoLAN
 * $Id$
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
#import "misc.h"

@class VLCFSPanel;
@class VLCResizeControl;

/*****************************************************************************
 * VLCControlsBarCommon
 *
 *  Holds all outlets, actions and code common for controls bar in detached
 *  and in main window.
 *****************************************************************************/

@interface VLCControlsBarCommon : NSObject

@property (readwrite, strong) IBOutlet VLCDragDropView *dropView;

@property (readwrite, strong) IBOutlet NSButton *playButton;
@property (readwrite, strong) IBOutlet NSButton *backwardButton;
@property (readwrite, strong) IBOutlet NSButton *forwardButton;

@property (readwrite, strong) IBOutlet VLCProgressView *progressView;
@property (readwrite, strong) IBOutlet TimeLineSlider *timeSlider;
@property (readwrite, strong) IBOutlet VLCThreePartImageView *timeSliderGradientView;
@property (readwrite, strong) IBOutlet VLCThreePartImageView *timeSliderBackgroundView;
@property (readwrite, strong) IBOutlet NSProgressIndicator *progressBar;

@property (readwrite, strong) IBOutlet VLCTimeField *timeField;
@property (readwrite, strong) IBOutlet NSButton *fullscreenButton;
@property (readwrite, strong) IBOutlet VLCResizeControl *resizeView;

@property (readwrite, strong) IBOutlet VLCThreePartImageView *bottomBarView;

@property (readonly) BOOL darkInterface;
@property (readonly) BOOL nativeFullscreenMode;

- (CGFloat)height;
- (void)toggleForwardBackwardMode:(BOOL)b_alt;

- (IBAction)play:(id)sender;
- (IBAction)bwd:(id)sender;
- (IBAction)fwd:(id)sender;

- (IBAction)timeSliderAction:(id)sender;
- (IBAction)fullscreen:(id)sender;

- (void)updateTimeSlider;
- (void)drawFancyGradientEffectForTimeSlider;
- (void)updateControls;
- (void)setPause;
- (void)setPlay;
- (void)setFullscreenState:(BOOL)b_fullscreen;

@end


/*****************************************************************************
 * VLCMainWindowControlsBar
 *
 *  Holds all specific outlets, actions and code for the main window controls bar.
 *****************************************************************************/

@interface VLCMainWindowControlsBar : VLCControlsBarCommon

@property (readwrite, strong) IBOutlet NSButton *stopButton;

@property (readwrite, strong) IBOutlet NSButton *playlistButton;
@property (readwrite, strong) IBOutlet NSButton *repeatButton;
@property (readwrite, strong) IBOutlet NSButton *shuffleButton;

@property (readwrite, strong) IBOutlet VLCVolumeSliderCommon * volumeSlider;
@property (readwrite, strong) IBOutlet NSImageView *volumeTrackImageView;
@property (readwrite, strong) IBOutlet NSButton *volumeDownButton;
@property (readwrite, strong) IBOutlet NSButton *volumeUpButton;

@property (readwrite, strong) IBOutlet NSButton *effectsButton;

- (IBAction)stop:(id)sender;

- (IBAction)shuffle:(id)sender;
- (IBAction)volumeAction:(id)sender;
- (IBAction)effects:(id)sender;

- (void)setRepeatOne;
- (void)setRepeatAll;
- (void)setRepeatOff;
- (IBAction)repeat:(id)sender;

- (void)setShuffle;
- (IBAction)shuffle:(id)sender;

- (IBAction)togglePlaylist:(id)sender;

- (void)toggleEffectsButton;
- (void)toggleJumpButtons;
- (void)togglePlaymodeButtons;

- (void)updateVolumeSlider;
- (void)updateControls;

@end

