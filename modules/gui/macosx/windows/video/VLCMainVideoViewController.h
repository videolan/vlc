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
@class VLCPlaybackEndViewController;

NS_ASSUME_NONNULL_BEGIN

extern NSString * const VLCUseClassicVideoPlayerLayoutKey;

@interface VLCMainVideoViewController : NSViewController

@property (readwrite, weak) IBOutlet NSView *voutContainingView;

@property (readwrite, weak) IBOutlet VLCVoutView *voutView;
@property (readwrite, weak) IBOutlet NSBox *mainControlsView;
@property (readwrite, weak) IBOutlet VLCMainVideoViewOverlayView *overlayView;
@property (readwrite, weak) IBOutlet NSView *bottomBarView;
@property (readwrite, weak) IBOutlet NSStackView *centralControlsStackView;
@property (readwrite, strong) IBOutlet VLCMainVideoViewControlsBar *controlsBar;
@property (readwrite, weak) IBOutlet NSButton *returnButton;
@property (readwrite, weak) IBOutlet NSButton *playQueueButton;
@property (readwrite, weak) IBOutlet NSLayoutConstraint *returnButtonTopConstraint;
@property (readwrite, weak) IBOutlet NSLayoutConstraint *returnButtonLeadingConstraint;
@property (readwrite, weak) IBOutlet NSLayoutConstraint *playQueueButtonTopConstraint;
@property (readwrite, weak) IBOutlet NSLayoutConstraint *playQueueButtonTrailingConstraint;
@property (readwrite, strong) IBOutlet NSLayoutConstraint *videoViewBottomConstraint;
@property (readwrite, weak) IBOutlet NSVisualEffectView *fakeTitleBar;
@property (readwrite, weak) IBOutlet NSLayoutConstraint *fakeTitleBarHeightConstraint;
@property (readwrite, weak) IBOutlet NSProgressIndicator *loadingIndicator;
@property (readwrite, weak) IBOutlet NSImageView *floatOnTopIndicatorImageView;
@property (readwrite, weak) IBOutlet NSView *classicViewBottomBarContainerView;

@property (readwrite, weak) IBOutlet NSLayoutConstraint *playButtonSizeConstraint;
@property (readwrite, weak) IBOutlet NSLayoutConstraint *prevButtonSizeConstraint;
@property (readwrite, weak) IBOutlet NSLayoutConstraint *nextButtonSizeConstraint;
@property (readwrite, weak) IBOutlet NSLayoutConstraint *jumpBackwardButtonSizeConstraint;
@property (readwrite, weak) IBOutlet NSLayoutConstraint *jumpForwardButtonSizeConstraint;

@property (readwrite, strong) IBOutlet NSLayoutConstraint *centerButtonStackInViewConstraint;
@property (readonly) NSLayoutConstraint *bottomButtonStackViewConstraint;
@property (readonly) NSLayoutConstraint *videoViewBottomToViewConstraint;

@property (readonly, strong) VLCMainVideoViewAudioMediaDecorativeView *audioDecorativeView;
@property (readwrite, nonatomic) BOOL autohideControls;
@property (readwrite, nonatomic) BOOL displayLibraryControls;
@property (readonly) BOOL mouseOnControls;
@property (readonly) BOOL pipIsActive;

@property (readonly) VLCPlaybackEndViewController *playbackEndViewController;
@property (readwrite) void (^endViewDismissHandler)(void);

- (void)showControls;
- (void)hideControls;
- (nullable NSView *)acquireVideoView;
- (void)returnVideoView:(NSView *)videoView;
- (void)displayPlaybackEndView;

- (IBAction)togglePlayQueue:(id)sender;
- (IBAction)returnToLibrary:(id)sender;

@end

NS_ASSUME_NONNULL_END
