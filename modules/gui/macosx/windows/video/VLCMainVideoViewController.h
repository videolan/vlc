/*****************************************************************************
 * VLCMainVideoViewController.h: MacOS X interface module
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

#import <Cocoa/Cocoa.h>

#import <windows/video/VLCVoutView.h>

@class VLCMainVideoViewOverlayView;
@class VLCMainVideoViewAudioMediaDecorativeView;
@class VLCMainVideoViewControlsBar;

NS_ASSUME_NONNULL_BEGIN

@interface VLCMainVideoViewController : NSViewController

@property (readwrite, strong) IBOutlet VLCVoutView *voutView;
@property (readwrite, strong) IBOutlet NSBox *mainControlsView;
@property (readwrite, strong) IBOutlet VLCMainVideoViewOverlayView *overlayView;
@property (readwrite, strong) IBOutlet VLCMainVideoViewAudioMediaDecorativeView *audioDecorativeView;
@property (readwrite, strong) IBOutlet NSView *bottomBarView;
@property (readwrite, strong) IBOutlet NSStackView *centralControlsStackView;
@property (readwrite, strong) IBOutlet VLCMainVideoViewControlsBar *controlsBar;
@property (readwrite, strong) IBOutlet NSButton *returnButton;
@property (readwrite, strong) IBOutlet NSButton *playlistButton;
@property (readwrite, strong) IBOutlet NSLayoutConstraint *returnButtonTopConstraint;
@property (readwrite, strong) IBOutlet NSLayoutConstraint *returnButtonLeadingConstraint;
@property (readwrite, strong) IBOutlet NSLayoutConstraint *playlistButtonTopConstraint;
@property (readwrite, strong) IBOutlet NSLayoutConstraint *playlistButtonTrailingConstraint;
@property (readwrite, strong) IBOutlet NSVisualEffectView *fakeTitleBar;
@property (readwrite, strong) IBOutlet NSLayoutConstraint *fakeTitleBarHeightConstraint;

@property (readwrite, nonatomic) BOOL autohideControls;
@property (readwrite, nonatomic) BOOL displayLibraryControls;
@property (readonly) BOOL mouseOnControls;

- (void)showControls;
- (void)hideControls;

- (IBAction)togglePlaylist:(id)sender;
- (IBAction)returnToLibrary:(id)sender;

@end

NS_ASSUME_NONNULL_END
