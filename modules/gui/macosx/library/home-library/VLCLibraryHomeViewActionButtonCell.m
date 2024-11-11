/*****************************************************************************
 * VLCLibraryHomeViewActionButtonCell.m: MacOS X interface module
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

#import "VLCLibraryHomeViewActionButtonCell.h"

#import "extensions/NSColor+VLCAdditions.h"

@implementation VLCLibraryHomeViewActionButtonCell

- (void)drawWithFrame:(NSRect)cellFrame inView:(NSView *)controlView
{
    [NSColor.VLCSubtleBorderColor setStroke];
    [NSColor.windowBackgroundColor setFill];

    NSBezierPath * const separatorPath = NSBezierPath.bezierPath;
    [separatorPath moveToPoint:NSMakePoint(NSMinX(cellFrame), NSMaxY(cellFrame))];
    [separatorPath lineToPoint:NSMakePoint(NSMaxX(cellFrame), NSMaxY(cellFrame))];
    [separatorPath lineToPoint:NSMakePoint(NSMaxX(cellFrame), NSMinY(cellFrame))];
    [separatorPath lineToPoint:NSMakePoint(NSMinX(cellFrame), NSMinY(cellFrame))];
    [separatorPath lineToPoint:NSMakePoint(NSMinX(cellFrame), NSMaxY(cellFrame))];
    separatorPath.lineWidth = 1.0;

    [separatorPath stroke];
    [separatorPath fill];

    [self.image drawInRect:cellFrame];
}

@end
