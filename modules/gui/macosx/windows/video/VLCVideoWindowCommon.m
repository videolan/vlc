/*****************************************************************************
 * Windows.m: MacOS X interface module
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

#import "VLCVideoWindowCommon.h"

#import "extensions/NSScreen+VLCAdditions.h"
#import "extensions/NSString+Helpers.h"

#import "library/VLCLibraryWindow.h"
#import "library/VLCInputItem.h"

#import "main/CompatibilityFixes.h"
#import "main/VLCMain.h"

#import "playlist/VLCPlaylistController.h"
#import "playlist/VLCPlayerController.h"

#import "windows/video/VLCMainVideoViewController.h"
#import "windows/video/VLCVideoOutputProvider.h"
#import "windows/video/VLCVoutView.h"

const CGFloat VLCVideoWindowCommonMinimalHeight = 70.;
NSString *VLCVideoWindowShouldShowFullscreenController = @"VLCVideoWindowShouldShowFullscreenController";
NSString *VLCVideoWindowDidEnterFullscreen = @"VLCVideoWindowDidEnterFullscreen";
NSString *VLCWindowShouldShowController = @"VLCWindowShouldShowController";

/*****************************************************************************
 * VLCVideoWindowCommon
 *
 *  Common code for main window, detached window and extra video window
 *****************************************************************************/

@interface VLCVideoWindowCommon()
{
    // variables for fullscreen handling
    VLCVideoWindowCommon *o_current_video_window;
    VLCWindow       * o_fullscreen_window;
    NSViewAnimation * o_fullscreen_anim1;
    NSViewAnimation * o_fullscreen_anim2;
    NSView          * o_temp_view;

    NSInteger i_originalLevel;

    BOOL b_video_view_was_hidden;

    NSRect _frameBeforeLionFullscreen;
}

- (void)customZoom:(id)sender;
- (void)hasBecomeFullscreen;
- (void)hasEndedFullscreen;
@end

@implementation VLCVideoWindowCommon

#pragma mark -
#pragma mark Init

- (id)initWithContentRect:(NSRect)contentRect styleMask:(NSWindowStyleMask)styleMask
                  backing:(NSBackingStoreType)backingType defer:(BOOL)flag
{
    self = [super initWithContentRect:contentRect styleMask:styleMask
                              backing:backingType defer:flag];

    if (self) {
        /* we want to be moveable regardless of our style */
        [self setMovableByWindowBackground: YES];
        [self setCanBecomeKeyWindow:YES];

        o_temp_view = [[NSView alloc] init];
        [o_temp_view setAutoresizingMask:NSViewHeightSizable | NSViewWidthSizable];

        _playerController = [[[VLCMain sharedInstance] playlistController] playerController];
        _videoViewController = [[VLCMainVideoViewController alloc] init];
    }

    return self;
}

- (void)dealloc
{
    [[NSNotificationCenter defaultCenter] removeObserver:self];
}

- (void)awakeFromNib
{
    if (_videoViewController == nil) {
        _videoViewController = [[VLCMainVideoViewController alloc] init];
    }

    NSNotificationCenter *notificationCenter = [NSNotificationCenter defaultCenter];
    [notificationCenter addObserver:self
                           selector:@selector(mediaMetadataChanged:)
                               name:VLCPlayerMetadataChangedForCurrentMedia
                             object:nil];
    [notificationCenter addObserver:self
                           selector:@selector(mediaMetadataChanged:)
                               name:VLCPlayerCurrentMediaItemChanged
                             object:nil];
    [self mediaMetadataChanged:nil];

    BOOL b_nativeFullscreenMode = var_InheritBool(getIntf(), "macosx-nativefullscreenmode");

    if (b_nativeFullscreenMode) {
        [self setCollectionBehavior: NSWindowCollectionBehaviorFullScreenPrimary];
    } else {
        // Native fullscreen seems to be default on El Capitan, this disables it explicitly
        [self setCollectionBehavior: NSWindowCollectionBehaviorFullScreenAuxiliary];
    }

    [super awakeFromNib];
}

- (void)mediaMetadataChanged:(NSNotification *)aNotification
{
    VLCPlaylistController *playlistController = [[VLCMain sharedInstance] playlistController];
    VLCInputItem *inputItem = [playlistController currentlyPlayingInputItem];
    if (inputItem == NULL || _playerController.playerState == VLC_PLAYER_STATE_STOPPED) {
        [self setTitle:_NS("VLC media player")];
        self.representedURL = nil;
        return;
    }

    NSString *title = inputItem.title;
    NSString *nowPlaying = inputItem.nowPlaying;
    if (nowPlaying) {
        [self setTitle:[NSString stringWithFormat:@"%@ — %@", title, nowPlaying]];
    } else {
        [self setTitle:title];
    }

    self.representedURL = [NSURL URLWithString:inputItem.MRL];
}

- (void)setTitle:(NSString *)title
{
    if (!title || [title length] < 1)
        return;

    [super setTitle: title];
}

#pragma mark -
#pragma mark zoom / minimize / close

- (BOOL)validateMenuItem:(NSMenuItem *)menuItem
{
    SEL s_menuAction = [menuItem action];

    if ((s_menuAction == @selector(performClose:)) || (s_menuAction == @selector(performMiniaturize:)) || (s_menuAction == @selector(performZoom:)))
        return YES;

    return [super validateMenuItem:menuItem];
}

- (BOOL)windowShouldClose:(id)sender
{
    return YES;
}

- (void)performClose:(id)sender
{
    if (!([self styleMask] & NSTitledWindowMask)) {
        [[NSNotificationCenter defaultCenter] postNotificationName:NSWindowWillCloseNotification object:self];

        [self close];
    } else
        [super performClose: sender];
}

- (void)performMiniaturize:(id)sender
{
    if (!([self styleMask] & NSTitledWindowMask))
        [self miniaturize: sender];
    else
        [super performMiniaturize: sender];
}

- (void)performZoom:(id)sender
{
    if (!([self styleMask] & NSTitledWindowMask))
        [self customZoom: sender];
    else
        [super performZoom: sender];
}

- (void)zoom:(id)sender
{
    if (!([self styleMask] & NSTitledWindowMask))
        [self customZoom: sender];
    else
        [super zoom: sender];
}

/**
 * Given a proposed frame rectangle, return a modified version
 * which will fit inside the screen.
 *
 * This method is based upon NSWindow.m, part of the GNUstep GUI Library, licensed under LGPLv2+.
 *    Authors:  Scott Christley <scottc@net-community.com>, Venkat Ajjanagadde <venkat@ocbi.com>,
 *              Felipe A. Rodriguez <far@ix.netcom.com>, Richard Frith-Macdonald <richard@brainstorm.co.uk>
 *    Copyright (C) 1996 Free Software Foundation, Inc.
 */
- (NSRect) customConstrainFrameRect: (NSRect)frameRect toScreen: (NSScreen*)screen
{
    NSRect screenRect = [screen visibleFrame];
    CGFloat difference;

    /* Move top edge of the window inside the screen */
    difference = NSMaxY (frameRect) - NSMaxY (screenRect);
    if (difference > 0) {
        frameRect.origin.y -= difference;
    }

    /* If the window is resizable, resize it (if needed) so that the
     bottom edge is on the screen or can be on the screen when the user moves
     the window */
    difference = NSMaxY (screenRect) - NSMaxY (frameRect);
    if (self.styleMask & NSResizableWindowMask) {
        CGFloat difference2;

        difference2 = screenRect.origin.y - frameRect.origin.y;
        difference2 -= difference;
        // Take in account the space between the top of window and the top of the
        // screen which can be used to move the bottom of the window on the screen
        if (difference2 > 0) {
            frameRect.size.height -= difference2;
            frameRect.origin.y += difference2;
        }

        /* Ensure that resizing doesn't makewindow smaller than minimum */
        difference2 = [self minSize].height - frameRect.size.height;
        if (difference2 > 0) {
            frameRect.size.height += difference2;
            frameRect.origin.y -= difference2;
        }
    }

    return frameRect;
}

#define DIST 3

/**
 Zooms the receiver.   This method calls the delegate method
 windowShouldZoom:toFrame: to determine if the window should
 be allowed to zoom to full screen.
 *
 * This method is based upon NSWindow.m, part of the GNUstep GUI Library, licensed under LGPLv2+.
 *    Authors:  Scott Christley <scottc@net-community.com>, Venkat Ajjanagadde <venkat@ocbi.com>,
 *              Felipe A. Rodriguez <far@ix.netcom.com>, Richard Frith-Macdonald <richard@brainstorm.co.uk>
 *    Copyright (C) 1996 Free Software Foundation, Inc.
 */
- (void) customZoom: (id)sender
{
    NSRect maxRect = [[self screen] visibleFrame];
    NSRect currentFrame = [self frame];

    if ([[self delegate] respondsToSelector: @selector(windowWillUseStandardFrame:defaultFrame:)]) {
        maxRect = [[self delegate] windowWillUseStandardFrame: self defaultFrame: maxRect];
    }

    maxRect = [self customConstrainFrameRect: maxRect toScreen: [self screen]];

    // Compare the new frame with the current one
    if ((fabs(NSMaxX(maxRect) - NSMaxX(currentFrame)) < DIST)
        && (fabs(NSMaxY(maxRect) - NSMaxY(currentFrame)) < DIST)
        && (fabs(NSMinX(maxRect) - NSMinX(currentFrame)) < DIST)
        && (fabs(NSMinY(maxRect) - NSMinY(currentFrame)) < DIST)) {
        // Already in zoomed mode, reset user frame, if stored
        if ([self frameAutosaveName] != nil) {
            [self setFrame: self.previousSavedFrame display: YES animate: YES];
            [self saveFrameUsingName: [self frameAutosaveName]];
        }
        return;
    }

    if ([self frameAutosaveName] != nil) {
        [self saveFrameUsingName: [self frameAutosaveName]];
        self.previousSavedFrame = [self frame];
    }

    [self setFrame: maxRect display: YES animate: YES];
}

# pragma mark -
# pragma mark Window level handling

- (void)setWindowLevel:(NSInteger)i_state
{
    if (var_InheritBool(getIntf(), "video-wallpaper") || [self level] < NSNormalWindowLevel)
        return;

    if (!self.fullscreen && !_inFullscreenTransition)
        [self setLevel: i_state];

    // save it for restore if window is currently minimized or in fullscreen
    i_originalLevel = i_state;
}

- (void)windowWillMiniaturize:(NSNotification *)notification
{
    // Set level to normal as a workaround for Mavericks bug causing window
    // to vanish from screen, see radar://15473716
    i_originalLevel = [self level];
    [self setLevel: NSNormalWindowLevel];
}

- (void)windowDidDeminiaturize:(NSNotification *)notification
{
    [self setLevel: i_originalLevel];
}

#pragma mark -
#pragma mark Lion native fullscreen handling

-(NSArray*)customWindowsToEnterFullScreenForWindow:(NSWindow *)window
{
    if (window == self) {
        return [NSArray arrayWithObject:window];
    }

    return nil;
}

- (NSArray*)customWindowsToExitFullScreenForWindow:(NSWindow*)window
{
    if (window == self) {
        return [NSArray arrayWithObject:window];
    }

    return nil;
}

- (void)window:window startCustomAnimationToEnterFullScreenWithDuration:(NSTimeInterval)duration
{
    [window setStyleMask:([window styleMask] | NSFullScreenWindowMask)];

    NSScreen *screen = [window screen];
    NSRect screenFrame = [screen frame];

    [NSAnimationContext runAnimationGroup:^(NSAnimationContext *context) {
        [context setDuration:0.5 * duration];
        [[window animator] setFrame:screenFrame display:YES];
    } completionHandler:nil];
}

- (void)window:window startCustomAnimationToExitFullScreenWithDuration:(NSTimeInterval)duration
{
    [window setStyleMask:([window styleMask] & ~NSFullScreenWindowMask)];
    [[window animator] setFrame:_frameBeforeLionFullscreen display:YES animate:YES];

    [NSAnimationContext runAnimationGroup:^(NSAnimationContext *context) {
        [context setDuration:0.5 * duration];
        [[window animator] setFrame:self->_frameBeforeLionFullscreen display:YES animate:YES];
    } completionHandler:nil];
}

- (void)windowWillEnterFullScreen:(NSNotification *)notification
{
    _windowShouldExitFullscreenWhenFinished = [_playerController activeVideoPlayback];

    NSInteger i_currLevel = [self level];
    // self.fullscreen and _inFullscreenTransition must not be true yet
    [[[VLCMain sharedInstance] voutProvider] updateWindowLevelForHelperWindows: NSNormalWindowLevel];
    [self setLevel:NSNormalWindowLevel];
    i_originalLevel = i_currLevel;

    _inFullscreenTransition = YES;

    _playerController.fullscreen = YES;

    _frameBeforeLionFullscreen = [self frame];

    if ([self hasActiveVideo]) {
        vout_thread_t *p_vout = [_playerController videoOutputThreadForKeyWindow];
        if (p_vout) {
            var_SetBool(p_vout, "fullscreen", true);
            vout_Release(p_vout);
        }
    }

    [self setMovableByWindowBackground: NO];
}

- (void)windowDidEnterFullScreen:(NSNotification *)notification
{
    // Indeed, we somehow can have an "inactive" fullscreen (but a visible window!).
    // But this creates some problems when leaving fs over remote intfs, so activate app here.
    [NSApp activateIgnoringOtherApps:YES];

    [self setFullscreen: YES];
    _inFullscreenTransition = NO;

    if ([self hasActiveVideo]) {
        NSNotificationCenter *notificationCenter = [NSNotificationCenter defaultCenter];
        if (![_videoViewController.view isHidden]) {
            [notificationCenter postNotificationName:VLCVideoWindowShouldShowFullscreenController object:self];
        }
    }

    for (__kindof NSView *view in [_videoViewController.view subviews]) {
        if ([view respondsToSelector:@selector(reshape)])
            [view reshape];
    }
}

- (void)windowWillExitFullScreen:(NSNotification *)notification
{
    _inFullscreenTransition = YES;
    [self setFullscreen: NO];

    if ([self hasActiveVideo]) {
        _playerController.fullscreen = NO;

        vout_thread_t *p_vout = [_playerController videoOutputThreadForKeyWindow];
        if (p_vout) {
            var_SetBool(p_vout, "fullscreen", false);
            vout_Release(p_vout);
        }
    }

    [NSCursor setHiddenUntilMouseMoves: NO];

    [self setMovableByWindowBackground: YES];
}

- (void)windowDidExitFullScreen:(NSNotification *)notification
{
    _inFullscreenTransition = NO;

    [[[VLCMain sharedInstance] voutProvider] updateWindowLevelForHelperWindows: i_originalLevel];
    [self setLevel:i_originalLevel];
}

#pragma mark -
#pragma mark Fullscreen Logic

- (void)enterFullscreenWithAnimation:(BOOL)b_animation
{
    NSMutableDictionary *dict1, *dict2;
    NSScreen *screen;
    NSRect screen_rect;
    NSRect rect;
    BOOL blackout_other_displays = var_InheritBool(getIntf(), "macosx-black");

    screen = [NSScreen screenWithDisplayID:(CGDirectDisplayID)var_InheritInteger(getIntf(), "macosx-vdev")];

    if (!screen) {
        msg_Dbg(getIntf(), "chosen screen isn't present, using current screen for fullscreen mode");
        screen = [self screen];
    }
    if (!screen) {
        msg_Dbg(getIntf(), "Using deepest screen");
        screen = [NSScreen deepestScreen];
    }

    screen_rect = [screen frame];

    if (blackout_other_displays)
        [screen blackoutOtherScreens];

    /* Make sure we don't see the window flashes in float-on-top mode */
    NSInteger i_currLevel = [self level];
    // self.fullscreen must not be true yet
    [[[VLCMain sharedInstance] voutProvider] updateWindowLevelForHelperWindows: NSNormalWindowLevel];
    [self setLevel:NSNormalWindowLevel];
    i_originalLevel = i_currLevel; // would be overwritten by previous call

    /* Only create the o_fullscreen_window if we are not in the middle of the zooming animation */
    if (!o_fullscreen_window) {
        /* We can't change the styleMask of an already created NSWindow, so we create another window, and do eye catching stuff */

        rect = [[_videoViewController.view superview] convertRect: [_videoViewController.view frame] toView: nil]; /* Convert to Window base coord */
        rect.origin.x += [self frame].origin.x;
        rect.origin.y += [self frame].origin.y;
        o_fullscreen_window = [[VLCWindow alloc] initWithContentRect:rect styleMask: NSBorderlessWindowMask backing:NSBackingStoreBuffered defer:YES];
        [o_fullscreen_window setBackgroundColor: [NSColor blackColor]];
        [o_fullscreen_window setCanBecomeKeyWindow: YES];
        [o_fullscreen_window setCanBecomeMainWindow: YES];
        [o_fullscreen_window setHasActiveVideo: YES];
        [o_fullscreen_window setFullscreen: YES];

        /* Make sure video view gets visible in case the playlist was visible before */
        b_video_view_was_hidden = [_videoViewController.view isHidden];
        [_videoViewController.view setHidden: NO];
        _videoViewController.view.translatesAutoresizingMaskIntoConstraints = YES;

        if (!b_animation) {
            /* We don't animate if we are not visible, instead we
             * simply fade the display */
            CGDisplayFadeReservationToken token;

            if (blackout_other_displays) {
                CGAcquireDisplayFadeReservation(kCGMaxDisplayReservationInterval, &token);
                CGDisplayFade(token, 0.5, kCGDisplayBlendNormal, kCGDisplayBlendSolidColor, 0, 0, 0, YES);
            }

            [NSAnimationContext beginGrouping];
            [[_videoViewController.view superview] replaceSubview:_videoViewController.view with:o_temp_view];
            [o_temp_view setFrame:[_videoViewController.view frame]];
            [[o_fullscreen_window contentView] addSubview:_videoViewController.view];
            [_videoViewController.view setFrame: [[o_fullscreen_window contentView] frame]];
            [_videoViewController.view setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
            [NSAnimationContext endGrouping];

            [screen setFullscreenPresentationOptions];

            [o_fullscreen_window setFrame:screen_rect display:YES animate:NO];

            [o_fullscreen_window orderFront:self animate:YES];

            [o_fullscreen_window setLevel:NSNormalWindowLevel];

            if (blackout_other_displays) {
                CGDisplayFade(token, 0.3, kCGDisplayBlendSolidColor, kCGDisplayBlendNormal, 0, 0, 0, NO);
                CGReleaseDisplayFadeReservation(token);
            }

            /* Will release the lock */
            [self hasBecomeFullscreen];

            return;
        }

        /* Make sure we don't see the _videoView disappearing of the screen during this operation */
        [NSAnimationContext beginGrouping];
        [[_videoViewController.view superview] replaceSubview:_videoViewController.view with:o_temp_view];
        [o_temp_view setFrame:[_videoViewController.view frame]];
        [[o_fullscreen_window contentView] addSubview:_videoViewController.view];
        [_videoViewController.view setFrame: [[o_fullscreen_window contentView] frame]];
        [_videoViewController.view setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];

        [o_fullscreen_window makeKeyAndOrderFront:self];
        [NSAnimationContext endGrouping];
    }

    /* We are in fullscreen (and no animation is running) */
    if ([self fullscreen]) {
        /* Make sure we are hidden */
        [self orderOut: self];

        return;
    }

    if (o_fullscreen_anim1) {
        [o_fullscreen_anim1 stopAnimation];
    }
    if (o_fullscreen_anim2) {
        [o_fullscreen_anim2 stopAnimation];
    }

    [screen setFullscreenPresentationOptions];

    dict1 = [[NSMutableDictionary alloc] initWithCapacity:2];
    dict2 = [[NSMutableDictionary alloc] initWithCapacity:3];

    [dict1 setObject:self forKey:NSViewAnimationTargetKey];
    [dict1 setObject:NSViewAnimationFadeOutEffect forKey:NSViewAnimationEffectKey];

    [dict2 setObject:o_fullscreen_window forKey:NSViewAnimationTargetKey];
    [dict2 setObject:[NSValue valueWithRect:[o_fullscreen_window frame]] forKey:NSViewAnimationStartFrameKey];
    [dict2 setObject:[NSValue valueWithRect:screen_rect] forKey:NSViewAnimationEndFrameKey];

    /* Strategy with NSAnimation allocation:
     - Keep at most 2 animation at a time
     - leaveFullscreen/enterFullscreen are the only responsible for releasing and alloc-ing
     */
    o_fullscreen_anim1 = [[NSViewAnimation alloc] initWithViewAnimations:[NSArray arrayWithObject:dict1]];
    o_fullscreen_anim2 = [[NSViewAnimation alloc] initWithViewAnimations:[NSArray arrayWithObject:dict2]];

    [o_fullscreen_anim1 setAnimationBlockingMode: NSAnimationNonblocking];
    [o_fullscreen_anim1 setDuration: 0.3];
    [o_fullscreen_anim1 setFrameRate: 30];
    [o_fullscreen_anim2 setAnimationBlockingMode: NSAnimationNonblocking];
    [o_fullscreen_anim2 setDuration: 0.2];
    [o_fullscreen_anim2 setFrameRate: 30];

    [o_fullscreen_anim2 setDelegate: self];
    [o_fullscreen_anim2 startWhenAnimation: o_fullscreen_anim1 reachesProgress: 1.0];

    [o_fullscreen_anim1 startAnimation];
    /* fullscreenAnimation will be unlocked when animation ends */

    _inFullscreenTransition = YES;
}

- (void)hasBecomeFullscreen
{
    if ([[_videoViewController.view subviews] count] > 0)
        [o_fullscreen_window makeFirstResponder: [[_videoViewController.view subviews] firstObject]];

    [o_fullscreen_window makeKeyWindow];
    [o_fullscreen_window setAcceptsMouseMovedEvents: YES];

    NSNotificationCenter *notificationCenter = [NSNotificationCenter defaultCenter];
    [notificationCenter postNotificationName:VLCVideoWindowDidEnterFullscreen object:self];
    [notificationCenter postNotificationName:VLCVideoWindowShouldShowFullscreenController object:self];

    if ([self isVisible])
        [self orderOut: self];

    _inFullscreenTransition = NO;
    [self setFullscreen:YES];
}

- (void)leaveFullscreenWithAnimation:(BOOL)b_animation
{
    NSMutableDictionary *dict1, *dict2;
    NSRect frame;
    BOOL blackout_other_displays = var_InheritBool(getIntf(), "macosx-black");

    /* We always try to do so */
    [NSScreen unblackoutScreens];

    [[_videoViewController.view window] makeKeyAndOrderFront: nil];

    /* Don't do anything if o_fullscreen_window is already closed */
    if (!o_fullscreen_window) {
        return;
    }

    [[o_fullscreen_window screen] setNonFullscreenPresentationOptions];

    if (o_fullscreen_anim1) {
        [o_fullscreen_anim1 stopAnimation];
        o_fullscreen_anim1 = nil;
    }
    if (o_fullscreen_anim2) {
        [o_fullscreen_anim2 stopAnimation];
        o_fullscreen_anim2 = nil;
    }

    _inFullscreenTransition = YES;
    [self setFullscreen:NO];

    if (!b_animation) {
        /* We don't animate if we are not visible, instead we
         * simply fade the display */
        CGDisplayFadeReservationToken token;

        if (blackout_other_displays) {
            CGAcquireDisplayFadeReservation(kCGMaxDisplayReservationInterval, &token);
            CGDisplayFade(token, 0.3, kCGDisplayBlendNormal, kCGDisplayBlendSolidColor, 0, 0, 0, YES);
        }

        [self setAlphaValue:1.0];
        [self orderFront: self];

        /* Will release the lock */
        [self hasEndedFullscreen];

        if (blackout_other_displays) {
            CGDisplayFade(token, 0.5, kCGDisplayBlendSolidColor, kCGDisplayBlendNormal, 0, 0, 0, NO);
            CGReleaseDisplayFadeReservation(token);
        }

        return;
    }

    [self setAlphaValue: 0.0];
    [self orderFront: self];
    [[_videoViewController.view window] orderFront: self];

    frame = [[o_temp_view superview] convertRect: [o_temp_view frame] toView: nil]; /* Convert to Window base coord */
    frame.origin.x += [self frame].origin.x;
    frame.origin.y += [self frame].origin.y;

    dict2 = [[NSMutableDictionary alloc] initWithCapacity:2];
    [dict2 setObject:self forKey:NSViewAnimationTargetKey];
    [dict2 setObject:NSViewAnimationFadeInEffect forKey:NSViewAnimationEffectKey];

    o_fullscreen_anim2 = [[NSViewAnimation alloc] initWithViewAnimations:[NSArray arrayWithObject:dict2]];

    [o_fullscreen_anim2 setAnimationBlockingMode: NSAnimationNonblocking];
    [o_fullscreen_anim2 setDuration: 0.3];
    [o_fullscreen_anim2 setFrameRate: 30];

    [o_fullscreen_anim2 setDelegate: self];

    dict1 = [[NSMutableDictionary alloc] initWithCapacity:3];

    [dict1 setObject:o_fullscreen_window forKey:NSViewAnimationTargetKey];
    [dict1 setObject:[NSValue valueWithRect:[o_fullscreen_window frame]] forKey:NSViewAnimationStartFrameKey];
    [dict1 setObject:[NSValue valueWithRect:frame] forKey:NSViewAnimationEndFrameKey];

    o_fullscreen_anim1 = [[NSViewAnimation alloc] initWithViewAnimations:[NSArray arrayWithObject:dict1]];

    [o_fullscreen_anim1 setAnimationBlockingMode: NSAnimationNonblocking];
    [o_fullscreen_anim1 setDuration: 0.2];
    [o_fullscreen_anim1 setFrameRate: 30];
    [o_fullscreen_anim2 startWhenAnimation: o_fullscreen_anim1 reachesProgress: 1.0];

    /* Make sure o_fullscreen_window is the frontmost window */
    [o_fullscreen_window orderFront: self];

    [o_fullscreen_anim1 startAnimation];
    /* fullscreenAnimation will be unlocked when animation ends */
}

- (void)hasEndedFullscreen
{
    _inFullscreenTransition = NO;

    /* This function is private and should be only triggered at the end of the fullscreen change animation */
    /* Make sure we don't see the _videoView disappearing of the screen during this operation */
    [NSAnimationContext beginGrouping];
    [_videoViewController.view removeFromSuperviewWithoutNeedingDisplay];
    [[o_temp_view superview] replaceSubview:o_temp_view with:_videoViewController.view];
    // TODO Replace tmpView by an existing view (e.g. middle view)
    // TODO Use constraints for fullscreen window, reinstate constraints once the video view is added to the main window again
    [_videoViewController.view setFrame:[o_temp_view frame]];
    if ([[_videoViewController.view subviews] count] > 0)
        [self makeFirstResponder: [[_videoViewController.view subviews] firstObject]];

    [_videoViewController.view setHidden: b_video_view_was_hidden];

    [self makeKeyAndOrderFront:self];

    [o_fullscreen_window orderOut: self];
    [NSAnimationContext endGrouping];

    o_fullscreen_window = nil;

    [[[VLCMain sharedInstance] voutProvider] updateWindowLevelForHelperWindows: i_originalLevel];
    [self setLevel:i_originalLevel];

    [self setAlphaValue: config_GetFloat("macosx-opaqueness")];
}

- (void)animationDidEnd:(NSAnimation*)animation
{
    NSArray *viewAnimations;
    if ([animation currentValue] < 1.0)
        return;

    /* Fullscreen ended or started (we are a delegate only for leaveFullscreen's/enterFullscren's anim2) */
    viewAnimations = [o_fullscreen_anim2 viewAnimations];
    if ([viewAnimations count] >=1 &&
        [[[viewAnimations firstObject] objectForKey: NSViewAnimationEffectKey] isEqualToString:NSViewAnimationFadeInEffect]) {
        /* Fullscreen ended */
        [self hasEndedFullscreen];
    } else {
    /* Fullscreen started */
        [self hasBecomeFullscreen];
    }
}

@end
