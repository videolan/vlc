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
@class VLCSlider;
@class VLCTimeField;
@class VLCBottomBarView;

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

@property (readwrite, strong) IBOutlet VLCSlider *timeSlider;

@property (readwrite, strong) IBOutlet VLCTimeField *timeField;
@property (readwrite, strong) IBOutlet NSButton *fullscreenButton;
@property (readwrite, strong) IBOutlet NSLayoutConstraint *fullscreenButtonWidthConstraint;

@property (readwrite, strong) IBOutlet VLCBottomBarView *bottomBarView;

@property (readonly) BOOL nativeFullscreenMode;

- (CGFloat)height;

- (IBAction)play:(id)sender;
- (IBAction)bwd:(id)sender;
- (IBAction)fwd:(id)sender;

- (IBAction)timeSliderAction:(id)sender;
- (IBAction)fullscreen:(id)sender;

@end
