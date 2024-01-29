/*****************************************************************************
 * VLCLoadingOverlayView.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2024 VLC authors and VideoLAN
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

#import "VLCLoadingOverlayView.h"

@implementation VLCLoadingOverlayView

- (instancetype)init
{
    self = [super init];
    if (self) {
        [self setup];
    }
    return self;
}

- (instancetype)initWithCoder:(NSCoder *)coder
{
    self = [super initWithCoder:coder];
    if (self) {
        [self setup];
    }
    return self;
}

- (instancetype)initWithFrame:(NSRect)frameRect
{
    self = [super initWithFrame:frameRect];
    if (self) {
        [self setup];
    }
    return self;
}

- (void)setup
{
    self.blendingMode = NSVisualEffectBlendingModeWithinWindow;
    self.material = NSVisualEffectMaterialPopover;

    _indicator = [[NSProgressIndicator alloc] initWithFrame:NSMakeRect(0, 0, 48, 48)];
    self.indicator.translatesAutoresizingMaskIntoConstraints = NO;
    self.indicator.indeterminate = YES;
    self.indicator.style = NSProgressIndicatorStyleSpinning;
    [self addSubview:self.indicator];
    [self addConstraints:@[
        [self.indicator.centerXAnchor constraintEqualToAnchor:self.centerXAnchor],
        [self.indicator.centerYAnchor constraintEqualToAnchor:self.centerYAnchor],
    ]];
}

@end
