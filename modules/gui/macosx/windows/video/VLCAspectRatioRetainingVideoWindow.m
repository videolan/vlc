/*****************************************************************************
 * VLCAspectRatioRetainingVideoWindow.m: MacOS X interface module
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

#import "VLCAspectRatioRetainingVideoWindow.h"

#import "main/VLCMain.h"
#import "playlist/VLCPlayerController.h"
#import "views/VLCMainVideoView.h"
#import "windows/video/VLCVoutView.h"

@implementation VLCAspectRatioRetainingVideoWindow

#pragma mark -
#pragma mark Video window resizing logic

- (NSRect)getWindowRectForProposedVideoViewSize:(NSSize)size
{
    NSSize windowMinSize = [self minSize];
    NSRect screenFrame = [[self screen] visibleFrame];

    NSRect topleftbase = NSMakeRect(0, [self frame].size.height, 0, 0);
    NSPoint topleftscreen = [self convertRectToScreen: topleftbase].origin;

    CGFloat f_width = size.width;
    CGFloat f_height = size.height;
    if (f_width < windowMinSize.width)
        f_width = windowMinSize.width;
    if (f_height < VLCVideoWindowCommonMinimalHeight)
        f_height = VLCVideoWindowCommonMinimalHeight;

    /* Calculate the window's new size */
    NSRect new_frame;
    new_frame.size.width = [self frame].size.width - [self.videoView frame].size.width + f_width;
    new_frame.size.height = [self frame].size.height - [self.videoView frame].size.height + f_height;
    new_frame.origin.x = topleftscreen.x;
    new_frame.origin.y = topleftscreen.y - new_frame.size.height;

    /* make sure the window doesn't exceed the screen size the window is on */
    if (new_frame.size.width > screenFrame.size.width) {
        new_frame.size.width = screenFrame.size.width;
        new_frame.origin.x = screenFrame.origin.x;
    }
    if (new_frame.size.height > screenFrame.size.height) {
        new_frame.size.height = screenFrame.size.height;
        new_frame.origin.y = screenFrame.origin.y;
    }
    if (new_frame.origin.y < screenFrame.origin.y)
        new_frame.origin.y = screenFrame.origin.y;

    CGFloat right_screen_point = screenFrame.origin.x + screenFrame.size.width;
    CGFloat right_window_point = new_frame.origin.x + new_frame.size.width;
    if (right_window_point > right_screen_point)
        new_frame.origin.x -= (right_window_point - right_screen_point);

    return new_frame;
}

- (void)resizeWindow
{
    // VLC_WINDOW_SET_SIZE is triggered when exiting fullscreen. This event is ignored here
    // to avoid interference with the animation.
    if ([self isInNativeFullscreen] || [self fullscreen] || self.inFullscreenTransition) {
        return;
    }

    NSRect window_rect = [self getWindowRectForProposedVideoViewSize:self.nativeVideoSize];
    [[self animator] setFrame:window_rect display:YES];
}

- (void)setNativeVideoSize:(NSSize)size
{
    _nativeVideoSize = size;

    if (var_InheritBool(getIntf(), "macosx-video-autoresize") && !var_InheritBool(getIntf(), "video-wallpaper")) {
        [self resizeWindow];
    }
}

- (NSSize)windowWillResize:(NSWindow *)window toSize:(NSSize)proposedFrameSize
{
    if (![self.playerController activeVideoPlayback] || self.nativeVideoSize.width == 0. || self.nativeVideoSize.height == 0. || window != self)
        return proposedFrameSize;

    // needed when entering lion fullscreen mode
    if (self.inFullscreenTransition || [self fullscreen] || [self isInNativeFullscreen])
        return proposedFrameSize;

    if ([self.videoView isHidden])
        return proposedFrameSize;

    if ([self.playerController aspectRatioIsLocked]) {
        NSRect videoWindowFrame = [self frame];
        NSRect viewRect = [self.videoView convertRect:[self.videoView bounds] toView: nil];
        NSRect contentRect = [self contentRectForFrameRect:videoWindowFrame];
        CGFloat marginy = viewRect.origin.y + videoWindowFrame.size.height - contentRect.size.height;
        CGFloat marginx = contentRect.size.width - viewRect.size.width;

        proposedFrameSize.height = (proposedFrameSize.width - marginx) * self.nativeVideoSize.height / self.nativeVideoSize.width + marginy;
    }

    return proposedFrameSize;
}

@end
