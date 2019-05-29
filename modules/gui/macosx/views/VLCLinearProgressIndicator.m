/*****************************************************************************
 * VLCLinearProgressIndicator.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2013, 2019 VLC authors and VideoLAN
 *
 * Authors: Felix Paul Kühne <fkuehne # videolan -dot- org>
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

#import "VLCLinearProgressIndicator.h"
#import "extensions/NSColor+VLCAdditions.h"

@interface VLCLinearProgressIndicator()
{
    NSView *_foregroundView;
}
@end

@implementation VLCLinearProgressIndicator

- (instancetype)initWithFrame:(NSRect)frameRect
{
    self = [super initWithFrame:frameRect];
    if (self) {
        [self setupSubviews];
    }
    return self;
}

- (instancetype)initWithCoder:(NSCoder *)decoder
{
    self = [super initWithCoder:decoder];
    if (self) {
        [self setupSubviews];
    }
    return self;
}

- (void)setupSubviews
{
    self.wantsLayer = YES;
    self.layer.backgroundColor = [NSColor VLClibraryProgressIndicatorBackgroundColor].CGColor;

    CGRect frame = self.frame;
    frame.size.width = 0.;
    _foregroundView = [[NSView alloc] initWithFrame:frame];
    _foregroundView.wantsLayer = YES;
    _foregroundView.layer.backgroundColor = [NSColor VLClibraryProgressIndicatorForegroundColor].CGColor;
    _foregroundView.autoresizingMask = NSViewWidthSizable;
    _foregroundView.translatesAutoresizingMaskIntoConstraints = NO;
    [self addSubview:_foregroundView];
}

- (void)setProgress:(CGFloat)progress
{
    if (progress > 1.) {
        progress = 1.;
    }

    CGRect rect = self.frame;
    rect.size.width = rect.size.width * progress;
    _foregroundView.frame = rect;

    _progress = progress;
}

@end
