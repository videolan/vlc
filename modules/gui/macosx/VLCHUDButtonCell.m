/*****************************************************************************
 * VLCHUDButtonCell.m: Custom button cell UI for dark HUD Panels
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

#import "VLCHUDButtonCell.h"

@implementation VLCHUDButtonCell

- (instancetype) initWithCoder:(NSCoder *)coder
{
    self = [super initWithCoder:coder];
    if (self) {
        _disabledGradient  = [[NSGradient alloc] initWithStartingColor:[NSColor colorWithDeviceRed:0.251f green:0.251f blue:0.255f alpha:1.0f]
                                                           endingColor:[NSColor colorWithDeviceRed:0.118f green:0.118f blue:0.118f alpha:1.0f]];
        _normalGradient    = [[NSGradient alloc] initWithStartingColor:[NSColor colorWithDeviceRed:0.251f green:0.251f blue:0.255f alpha:0.7f]
                                                           endingColor:[NSColor colorWithDeviceRed:0.118f green:0.118f blue:0.118f alpha:0.7f]];
        _highlightGradient = [[NSGradient alloc] initWithStartingColor:[NSColor colorWithDeviceRed:0.451f green:0.451f blue:0.455f alpha:1.0f]
                                                           endingColor:[NSColor colorWithDeviceRed:0.318f green:0.318f blue:0.318f alpha:1.0f]];
        _pushedGradient    = [[NSGradient alloc] initWithStartingColor:[NSColor colorWithDeviceRed:0.451f green:0.451f blue:0.455f alpha:1.0f]
                                                           endingColor:[NSColor colorWithDeviceRed:0.318f green:0.318f blue:0.318f alpha:1.0f]];
        _enabledTextColor  = [NSColor whiteColor];
        _disabledTextColor = [NSColor grayColor];
    }
    return self;
}

- (void) drawBezelWithFrame:(NSRect)frame inView:(NSView *)controlView
{
    // Set frame to the correct size
    frame.size.height = self.cellSize.height;

    // Inset rect to have enough room for the stroke
    frame = NSInsetRect(frame, 1, 1);
    if (self.bezelStyle == NSRoundRectBezelStyle) {
        [self drawRoundRectButtonBezelInRect:frame];
    } else {
        [super drawBezelWithFrame:frame inView:controlView];
    }
}

- (NSRect)drawTitle:(NSAttributedString *)title withFrame:(NSRect)frame inView:(NSView *)controlView
{
    NSMutableAttributedString *coloredTitle = [[NSMutableAttributedString alloc]
                                               initWithAttributedString:title];
    if (self.isEnabled) {
        [coloredTitle addAttribute:NSForegroundColorAttributeName
                             value:_enabledTextColor
                             range:NSMakeRange(0, coloredTitle.length)];
    } else {
        [coloredTitle addAttribute:NSForegroundColorAttributeName
                             value:_disabledTextColor
                             range:NSMakeRange(0, coloredTitle.length)];
    }

    return [super drawTitle:coloredTitle withFrame:frame inView:controlView];
}

- (void) drawRoundRectButtonBezelInRect:(NSRect)rect
{
    NSBezierPath *path = [NSBezierPath bezierPathWithRoundedRect:rect xRadius:8.0 yRadius:8.0];
    if (self.highlighted) {
        [_pushedGradient drawInBezierPath:path angle:90.0f];
    } else if (!self.enabled) {
        [_disabledGradient drawInBezierPath:path angle:90.0f];
    } else {
        [_normalGradient drawInBezierPath:path angle:90.0f];
    }
    [[NSColor colorWithCalibratedWhite:1.0 alpha:1.0] setStroke];
    [path setLineWidth:0.5];
    [path stroke];
}

@end
