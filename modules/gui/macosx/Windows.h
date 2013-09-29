/*****************************************************************************
 * Windows.h: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2012-2013 VLC authors and VideoLAN
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
#import "CompatibilityFixes.h"

/*****************************************************************************
 * VLCWindow
 *
 *  Missing extension to NSWindow
 *****************************************************************************/

@class VLCVoutView;

@interface VLCWindow : NSWindow <NSAnimationDelegate>
{
    BOOL b_canBecomeKeyWindow;
    BOOL b_isset_canBecomeKeyWindow;
    BOOL b_canBecomeMainWindow;
    BOOL b_isset_canBecomeMainWindow;
    NSViewAnimation *o_current_animation;

    BOOL              b_has_active_video;

    /* 
     * YES when all animations are over
     * for fullscreen window: always YES
     */
    BOOL              b_fullscreen;
}
@property (readwrite) BOOL canBecomeKeyWindow;
@property (readwrite) BOOL canBecomeMainWindow;

@property (nonatomic, readwrite) BOOL hasActiveVideo;
@property (nonatomic, readwrite) BOOL fullscreen;

/* animate mode is only supported in >=10.4 */
- (void)orderFront: (id)sender animate: (BOOL)animate;

/* animate mode is only supported in >=10.4 */
- (void)orderOut: (id)sender animate: (BOOL)animate;

/* animate mode is only supported in >=10.4 */
- (void)orderOut: (id)sender animate: (BOOL)animate callback:(NSInvocation *)callback;

/* animate mode is only supported in >=10.4 */
- (void)closeAndAnimate: (BOOL)animate;

- (VLCVoutView *)videoView;

@end


static const float f_min_video_height = 70.0;

@class VLCControlsBarCommon;

/*****************************************************************************
 * VLCVideoWindowCommon
 *
 *  Common code for main window, detached window and extra video window
 *****************************************************************************/

@interface VLCVideoWindowCommon : VLCWindow <NSWindowDelegate, NSAnimationDelegate>
{
    NSRect previousSavedFrame;
    BOOL b_dark_interface;

    IBOutlet id o_titlebar_view; // only set in main or detached window
    IBOutlet VLCVoutView *o_video_view;

    IBOutlet VLCControlsBarCommon *o_controls_bar;

    NSSize nativeVideoSize;

    // variables for fullscreen handling
    VLCVideoWindowCommon *o_current_video_window;
    VLCWindow       * o_fullscreen_window;
    NSViewAnimation * o_fullscreen_anim1;
    NSViewAnimation * o_fullscreen_anim2;
    NSViewAnimation * o_makekey_anim;
    NSView          * o_temp_view;

    BOOL              b_window_is_invisible;
    NSInteger i_originalLevel;

    BOOL              b_video_view_was_hidden;

    NSTimer *t_hide_mouse_timer;

    // true when the window is in transition for entering lion fullscreen
    BOOL b_entering_fullscreen_transition;
}

@property (nonatomic, assign) VLCVoutView* videoView;
@property (readonly) VLCControlsBarCommon* controlsBar;
@property (readonly) BOOL enteringFullscreenTransition;

- (void)setWindowLevel:(NSInteger)i_state;

- (void)resizeWindow;
- (void)setNativeVideoSize:(NSSize)size;
- (NSRect)getWindowRectForProposedVideoViewSize:(NSSize)size;


- (void)setTitle:(NSString *)title;

/* fullscreen handling */
- (void)enterFullscreen;
- (void)leaveFullscreen;

/* lion fullscreen handling */
- (void)windowWillEnterFullScreen:(NSNotification *)notification;
- (void)windowDidEnterFullScreen:(NSNotification *)notification;
- (void)windowWillExitFullScreen:(NSNotification *)notification;

@end
