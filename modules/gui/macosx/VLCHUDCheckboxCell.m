/*****************************************************************************
 * VLCHUDCheckboxCell.m: Custom checkbox cell UI for dark HUD Panels
 *****************************************************************************
 * Copyright (C) 2016 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Marvin Scholz <epirat07 -at- gmail -dot- com>
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

#import "VLCHUDCheckboxCell.h"

@implementation VLCHUDCheckboxCell

- (instancetype) initWithCoder:(NSCoder *)coder
{
    self = [super initWithCoder:coder];
    if (self) {
        _normalGradient    = [[NSGradient alloc] initWithStartingColor:[NSColor colorWithDeviceRed:0.251f green:0.251f blue:0.255f alpha:1.0f]
                                                           endingColor:[NSColor colorWithDeviceRed:0.118f green:0.118f blue:0.118f alpha:1.0f]];
        _highlightGradient = [[NSGradient alloc] initWithStartingColor:[NSColor colorWithDeviceRed:0.451f green:0.451f blue:0.455f alpha:1.0f]
                                                           endingColor:[NSColor colorWithDeviceRed:0.318f green:0.318f blue:0.318f alpha:1.0f]];
        _pushedGradient    = [[NSGradient alloc] initWithStartingColor:[NSColor colorWithDeviceRed:0.451f green:0.451f blue:0.455f alpha:1.0f]
                                                           endingColor:[NSColor colorWithDeviceRed:0.318f green:0.318f blue:0.318f alpha:1.0f]];
    }
    return self;
}

- (void) drawImage:(NSImage *)image withFrame:(NSRect)frame inView:(NSView *)controlView
{
    // Set frame size correctly
    NSRect backgroundFrame = frame;
    if (self.controlSize == NSSmallControlSize) {
        frame.origin.x += 4;
        frame.origin.y += 2.5;
        frame.size.width -= 5;
        frame.size.height -= 5;
        backgroundFrame = NSInsetRect(frame, 1.5, 1.5);
    } else if (self.controlSize == NSMiniControlSize) {
        frame.origin.x += 2.5;
        frame.origin.y += 4;
        frame.size.width -= 5;
        frame.size.height -= 5;
        backgroundFrame = NSInsetRect(frame, 2, 2);
    }

    // Set fill frame
    NSBezierPath *backgroundPath = [NSBezierPath bezierPathWithRoundedRect:backgroundFrame
                                                                   xRadius:2
                                                                   yRadius:2];

    // Draw border and background
    [NSColor.whiteColor setStroke];
    [backgroundPath setLineWidth:1];
    [backgroundPath stroke];

    if([self isEnabled]) {
        [_normalGradient drawInBezierPath:backgroundPath angle:90.0];
    } else {
        [[NSColor lightGrayColor] setFill];
        NSRectFill(backgroundFrame);
    }

    // Drawing inner contents
    //NSRect fillFrame = NSInsetRect(frame, 4, 4);
    if([self isHighlighted]) {

        //[[NSColor colorWithCalibratedWhite:0.9f alpha:1.0f] setFill];
    } else {
        //[[NSColor colorWithCalibratedWhite:0.8f alpha:1.0f] setFill];
    }
    //NSRectFill(fillFrame);

    // Now drawing tick
    if ([self intValue]) {
        if([self isEnabled]) {
            NSBezierPath* bezierPath = [NSBezierPath bezierPath];
            [bezierPath moveToPoint: NSMakePoint(NSMinX(frame) + 3.0, NSMidY(frame) - 2.0)];
            [bezierPath lineToPoint: NSMakePoint(NSMidX(frame), NSMidY(frame) + 2.0)];
            [bezierPath lineToPoint: NSMakePoint((NSMinX(frame) + NSWidth(frame) - 1), NSMinY(frame) - 2.0)];
            [bezierPath setLineWidth: 1.5];
            [bezierPath stroke];
        } else {
            //[[NSColor blackColor] setFill];
        }
        //NSRectFill(NSInsetRect(frame, 6, 6));
    }
}

@end
