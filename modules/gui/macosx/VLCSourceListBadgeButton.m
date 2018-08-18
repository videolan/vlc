/*****************************************************************************
 * VLCSourceListBadgeButton.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2018 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Marvin Scholz <epirat07 at gmail dot com>
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

#import "VLCSourceListBadgeButton.h"

@implementation VLCSourceListBadgeButton

/* Ignore clicks on this button as we use it as badge so we want clicks to
 * pass-through us and not handle them.
 */
- (NSView *)hitTest:(NSPoint)point
{
    return nil;
}

- (instancetype)initWithCoder:(NSCoder *)coder
{
    self = [super initWithCoder:coder];
    if (self) {
        [(NSButtonCell*)[self cell] setShowsStateBy:0];
        [(NSButtonCell*)[self cell] setHighlightsBy:0];
    }

    return self;
}

/* Our badges show integer values so make it easier to set those by setting
 * the title depending on the integer value.
 */
- (void)setIntegerValue:(NSInteger)integerValue
{
    [super setIntegerValue:integerValue];

    self.title = [@(integerValue) stringValue];

    if (_hideWhenZero)
        [self setHidden:(integerValue == 0)];
}

/* "Alias" for setIntegerValue for normal int vs NSInteger
 */
- (void)setIntValue:(int)intValue
{
    [self setIntegerValue:intValue];
}

/* Return zero size when hidden
 */
- (NSSize)intrinsicContentSize
{
    if (_hideWhenZero && self.hidden)
        return CGSizeZero;
    return [super intrinsicContentSize];
}

@end
