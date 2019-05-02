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

@implementation VLCLinearProgressIndicator

- (void)drawRect:(NSRect)dirtyRect
{
    [super drawRect:dirtyRect];

    CGRect rect = NSRectToCGRect(dirtyRect);

    CGContextClearRect([NSGraphicsContext currentContext].CGContext, rect);

    NSColor *drawingColor = [NSColor VLClibraryHighlightColor];

    NSBezierPath* bezierPath = [NSBezierPath bezierPath];

    float progress_width = self.progress * rect.size.width;

    // Create our triangle
    [bezierPath moveToPoint:CGPointMake(progress_width - rect.size.height + 3., 2.)];
    [bezierPath lineToPoint:CGPointMake(progress_width - (rect.size.height/2), rect.size.height - 5.)];
    [bezierPath lineToPoint:CGPointMake(progress_width - 3., 2.)];
    [bezierPath closePath];

    // Set the display for the path, and stroke it
    bezierPath.lineWidth = 6.;
    [drawingColor setStroke];
    [bezierPath stroke];
    [drawingColor setFill];
    [bezierPath fill];
}

- (BOOL)isFlipped
{
    return YES;
}

@end
