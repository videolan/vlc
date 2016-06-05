/*****************************************************************************
 * VLCHUDRadiobuttonCell.m: Custom radiobutton cell UI for dark HUD Panels
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

#import "VLCHUDRadiobuttonCell.h"

@implementation VLCHUDRadiobuttonCell

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
        _textColor         = [NSColor whiteColor];
    }
    return self;
}

- (void) drawImage:(NSImage *)image withFrame:(NSRect)frame inView:(NSView *)controlView
{
    // Set text color
    NSMutableAttributedString *colorTitle = [[NSMutableAttributedString alloc] initWithAttributedString:self.attributedTitle];
    NSRange titleRange = NSMakeRange(0, [colorTitle length]);
    [colorTitle addAttribute:NSForegroundColorAttributeName value:_textColor range:titleRange];
    [self setAttributedTitle:colorTitle];

    // Set frame size correctly
    NSRect backgroundFrame = frame;
    NSRect innerFrame = frame;
    if (self.controlSize == NSSmallControlSize) {
        frame.origin.x += 3.5;
        frame.origin.y += 4;
        frame.size.width -= 7;
        frame.size.height -= 7;
        innerFrame = NSInsetRect(frame, 2, 2);
    } else if (self.controlSize == NSMiniControlSize) {
        frame.origin.x += 2;
        frame.origin.y += 4;
        frame.size.width -= 6.5;
        frame.size.height -= 6.5;
        innerFrame = NSInsetRect(frame, 4, 4);
    }

    // Set fill frame
    NSBezierPath *backgroundPath = [NSBezierPath bezierPathWithOvalInRect:frame];

    // Draw border and background
    [NSColor.whiteColor setStroke];
    [backgroundPath setLineWidth:1.5];
    [backgroundPath stroke];

    if ([self isEnabled]) {
        if ([self isHighlighted]) {
            [_normalGradient drawInBezierPath:backgroundPath angle:90.0];
        } else {
            [_pushedGradient drawInBezierPath:backgroundPath angle:90.0];
        }
    } else {
        [[NSColor lightGrayColor] setFill];
        [backgroundPath fill];
    }

    // Draw dot
    if ([self integerValue] && [self isEnabled]) {
        NSBezierPath* bezierPath = [NSBezierPath bezierPathWithOvalInRect:innerFrame];
        [[NSColor whiteColor] setFill];
        [bezierPath fill];
    }
}

- (NSRect)drawTitle:(NSAttributedString *)title withFrame:(NSRect)frame inView:(NSView *)controlView
{
    NSMutableAttributedString *coloredTitle = [[NSMutableAttributedString alloc]
                                               initWithAttributedString:title];
    if (self.isEnabled) {
        [coloredTitle addAttribute:NSForegroundColorAttributeName
                             value:[NSColor whiteColor]
                             range:NSMakeRange(0, coloredTitle.length)];
    } else {
        [coloredTitle addAttribute:NSForegroundColorAttributeName
                             value:[NSColor grayColor]
                             range:NSMakeRange(0, coloredTitle.length)];
    }

    return [super drawTitle:coloredTitle withFrame:frame inView:controlView];
}

@end
