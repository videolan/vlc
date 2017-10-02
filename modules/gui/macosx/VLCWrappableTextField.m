/*****************************************************************************
 * VLCWrappableTextField.m
 *****************************************************************************
 * Copyright (C) 2017 VideoLAN and authors
 * Author:       David Fuhrmann <dfuhrmann at videolan dot org>
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

#import "VLCWrappableTextField.h"


@implementation VLCWrappableTextField

- (NSSize)intrinsicContentSize
{
    if (![self.cell wraps]) {
        return [super intrinsicContentSize];
    }

    // Try to get minimum height needed, by assuming unlimited height being
    // (theoretically) possible.
    NSRect frame = [self frame];
    frame.size.height = CGFLOAT_MAX;

    CGFloat height = [self.cell cellSizeForBounds:frame].height;

    return NSMakeSize(frame.size.width, height);
}

- (void)textDidChange:(NSNotification *)notification
{
    [super textDidChange:notification];
    [self invalidateIntrinsicContentSize];
}

@end