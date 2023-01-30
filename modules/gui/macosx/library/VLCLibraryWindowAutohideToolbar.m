/*****************************************************************************
 * VLCLibraryWindowAutohideToolbar.m: MacOS X interface module
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

#import "VLCLibraryWindowAutohideToolbar.h"

#import "main/VLCMain.h"

#import <vlc_common.h>

@interface VLCLibraryWindowAutohideToolbar()
{
    NSTimer *_hideToolbarTimer;
}
@end

@implementation VLCLibraryWindowAutohideToolbar

- (void)stopAutohideTimer
{
    [_hideToolbarTimer invalidate];
}

- (void)startAutohideTimer
{
    /* Do nothing if timer is already in place */
    if (_hideToolbarTimer.valid) {
        return;
    }

    /* Get timeout and make sure it is not lower than 1 second */
    long long timeToKeepVisibleInSec = MAX(var_CreateGetInteger(getIntf(), "mouse-hide-timeout") / 1000, 1);

    _hideToolbarTimer = [NSTimer scheduledTimerWithTimeInterval:timeToKeepVisibleInSec
                                                         target:self
                                                       selector:@selector(hideToolbar:)
                                                       userInfo:nil
                                                        repeats:NO];
}

- (void)hideToolbar:(id)sender
{
    [self stopAutohideTimer];
    self.visible = NO;
}

- (void)setAutohide:(BOOL)autohide
{
    if (_autohide == autohide) {
        return;
    }

    _autohide = autohide;

    if (autohide) {
        [self startAutohideTimer];
    } else {
        [self stopAutohideTimer];
        self.visible = YES;
    }
}

@end
