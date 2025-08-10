/*****************************************************************************
 * VLCBottomBarView.m
 *****************************************************************************
 * Copyright (C) 2017-2018 VLC authors and VideoLAN
 *
 * Authors: Marvin Scholz <epirat07 at gmail dot com>
 *          Felix Paul Kühne <fkuehne at videolan dot org>
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

#import "VLCBottomBarView.h"
#include "library/VLCLibraryUIUnits.h"

#import "extensions/NSColor+VLCAdditions.h"
#import "extensions/NSView+VLCAdditions.h"
#import "extensions/NSGradient+VLCAdditions.h"

@implementation VLCBottomBarView

- (instancetype)initWithCoder:(NSCoder *)coder
{
    self = [super initWithCoder:coder];

    if (self) {
        [self commonInit];
    }

    return self;
}

- (instancetype)initWithFrame:(NSRect)frameRect
{
    self = [super initWithFrame:frameRect];

    if (self) {
        [self commonInit];
    }

    return self;
}

- (instancetype)init
{
    self = [super init];

    if (self ) {
        [self commonInit];
    }

    return self;
}

- (void)commonInit
{
    self.wantsLayer = YES;
    self.needsDisplay = YES;
    self.drawBorder = YES;
    self.layer.masksToBounds = YES;
}

- (void)drawRect:(NSRect)dirtyRect
{
    [super drawRect:dirtyRect];

    if (!self.drawBorder) {
        return;
    }

    const NSRect barFrame = self.frame;
    const CGFloat cornerRadius = VLCLibraryUIUnits.cornerRadius;
    NSBezierPath * const borderPath = [NSBezierPath bezierPathWithRoundedRect:barFrame 
                                                                      xRadius:cornerRadius 
                                                                      yRadius:cornerRadius];
    
    [NSColor.VLCSubtleBorderColor setStroke];
    borderPath.lineWidth = VLCLibraryUIUnits.borderThickness;
    [borderPath stroke];
}

@end
