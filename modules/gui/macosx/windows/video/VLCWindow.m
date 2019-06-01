/*****************************************************************************
 * Windows.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2012-2018 VLC authors and VideoLAN
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

#import "VLCWindow.h"

#import "main/CompatibilityFixes.h"
#import "main/VLCMain.h"
#import "windows/video/VLCVideoWindowCommon.h"
#import "windows/video/VLCVoutView.h"

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
    __weak typeof(self) this = self;
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
    __weak typeof(self) this = self;
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

@end
