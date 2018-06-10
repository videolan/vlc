/*****************************************************************************
 * Windows.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2012-2018 VLC authors and VideoLAN
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

#import "VLCMain.h"
#import "VLCWindow.h"
#import "VLCVideoWindowCommon.h"
#import "CompatibilityFixes.h"

/*****************************************************************************
 * VLCWindow
 *
 *  Missing extension to NSWindow
 *****************************************************************************/

@interface VLCWindow()
{
    BOOL b_canBecomeKeyWindow;
    BOOL b_isset_canBecomeKeyWindow;
    BOOL b_canBecomeMainWindow;
    BOOL b_isset_canBecomeMainWindow;
}
@end

@implementation VLCWindow

- (id)initWithContentRect:(NSRect)contentRect styleMask:(NSWindowStyleMask)styleMask
                  backing:(NSBackingStoreType)backingType defer:(BOOL)flag
{
    self = [super initWithContentRect:contentRect styleMask:styleMask backing:backingType defer:flag];
    if (self) {
        /* we don't want this window to be restored on relaunch */
        [self setRestorable:NO];
    }
    return self;
}

- (void)setCanBecomeKeyWindow: (BOOL)canBecomeKey
{
    b_isset_canBecomeKeyWindow = YES;
    b_canBecomeKeyWindow = canBecomeKey;
}

- (BOOL)canBecomeKeyWindow
{
    if (b_isset_canBecomeKeyWindow)
        return b_canBecomeKeyWindow;

    return [super canBecomeKeyWindow];
}

- (void)setCanBecomeMainWindow: (BOOL)canBecomeMain
{
    b_isset_canBecomeMainWindow = YES;
    b_canBecomeMainWindow = canBecomeMain;
}

- (BOOL)canBecomeMainWindow
{
    if (b_isset_canBecomeMainWindow)
        return b_canBecomeMainWindow;

    return [super canBecomeMainWindow];
}

- (void)closeAndAnimate:(BOOL)animate
{
    // No animation, just close
    if (!animate) {
        [super close];
        return;
    }

    // Animate window alpha value
    [self setAlphaValue:1.0];
    __unsafe_unretained typeof(self) this = self;
    [NSAnimationContext runAnimationGroup:^(NSAnimationContext *context){
        [[NSAnimationContext currentContext] setDuration:0.9];
        [[this animator] setAlphaValue:0.0];
    } completionHandler:^{
        [this close];
    }];
}

- (void)orderOut:(id)sender animate:(BOOL)animate
{
    if (!animate) {
        [super orderOut:sender];
        return;
    }

    if ([self alphaValue] == 0.0) {
        [super orderOut:self];
        return;
    }
    __unsafe_unretained typeof(self) this = self;
    [NSAnimationContext runAnimationGroup:^(NSAnimationContext *context){
        [[NSAnimationContext currentContext] setDuration:0.5];
        [[this animator] setAlphaValue:0.0];
    } completionHandler:^{
        [this orderOut:self];
    }];
}

- (void)orderFront:(id)sender animate:(BOOL)animate
{
    if (!animate) {
        [super orderFront:sender];
        [self setAlphaValue:1.0];
        return;
    }

    if (![self isVisible]) {
        [self setAlphaValue:0.0];
        [super orderFront:sender];
    } else if ([self alphaValue] == 1.0) {
        [super orderFront:self];
        return;
    }

    [NSAnimationContext beginGrouping];
    [[NSAnimationContext currentContext] setDuration:0.5];
    [[self animator] setAlphaValue:1.0];
    [NSAnimationContext endGrouping];
}

- (VLCVoutView *)videoView
{
    NSArray *o_subViews = [[self contentView] subviews];
    if ([o_subViews count] > 0) {
        id o_vout_view = [o_subViews firstObject];

        if ([o_vout_view class] == [VLCVoutView class])
            return (VLCVoutView *)o_vout_view;
    }

    return nil;
}

- (NSRect)constrainFrameRect:(NSRect)frameRect toScreen:(NSScreen *)screen
{
    if (!screen)
        screen = [self screen];
    NSRect screenRect = [screen frame];
    NSRect constrainedRect = [super constrainFrameRect:frameRect toScreen:screen];

    /*
     * Ugly workaround!
     * With Mavericks, there is a nasty bug resulting in grey bars on top in fullscreen mode.
     * It looks like this is enforced by the os because the window is in the way for the menu bar.
     *
     * According to the documentation, this constraining can be changed by overwriting this
     * method. But in this situation, even the received frameRect is already contrained with the
     * menu bars height substracted. This case is detected here, and the full height is
     * enforced again.
     *
     * See #9469 and radar://15583566
     */

    BOOL b_inFullscreen = [self fullscreen] || ([self respondsToSelector:@selector(inFullscreenTransition)] && [(VLCVideoWindowCommon *)self inFullscreenTransition]);

    if ((OSX_MAVERICKS_AND_HIGHER && !OSX_YOSEMITE_AND_HIGHER) &&
        b_inFullscreen &&
        constrainedRect.size.width == screenRect.size.width &&
        constrainedRect.size.height != screenRect.size.height &&
        fabs(screenRect.size.height - constrainedRect.size.height) <= 25.) {
        msg_Dbg(getIntf(), "Contrain window height %.1f to screen height %.1f",
                constrainedRect.size.height, screenRect.size.height);
        constrainedRect.size.height = screenRect.size.height;
    }

    return constrainedRect;
}

@end
