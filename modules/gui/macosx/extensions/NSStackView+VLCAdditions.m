/*****************************************************************************
 * NSStackView+VLCAdditions.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2026 VLC authors and VideoLAN
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

#import "NSStackView+VLCAdditions.h"

#import "library/VLCLibraryUIUnits.h"

@implementation NSStackView (VLCAdditions)

- (NSInteger)spacingToken
{
    CGFloat spacing = self.spacing;
    if (spacing == VLCLibraryUIUnits.smallSpacing) {
        return VLCSpacingTokenSmall;
    } else if (spacing == VLCLibraryUIUnits.mediumSpacing) {
        return VLCSpacingTokenMedium;
    } else if (spacing == VLCLibraryUIUnits.largeSpacing) {
        return VLCSpacingTokenLarge;
    }
    return VLCSpacingTokenNone;
}

- (void)setSpacingToken:(NSInteger)spacingToken
{
    CGFloat spacingValue = 0;
    switch (spacingToken) {
        case VLCSpacingTokenSmall:
            spacingValue = VLCLibraryUIUnits.smallSpacing;
            break;
        case VLCSpacingTokenMedium:
            spacingValue = VLCLibraryUIUnits.mediumSpacing;
            break;
        case VLCSpacingTokenLarge:
            spacingValue = VLCLibraryUIUnits.largeSpacing;
            break;
        default:
            return;
    }
    self.spacing = spacingValue;
}

- (NSInteger)edgeInsetsToken
{
    NSEdgeInsets insets = self.edgeInsets;
    if (insets.top == insets.bottom && insets.left == insets.right && insets.top == insets.left) {
        CGFloat insetVal = insets.top;
        if (insetVal == VLCLibraryUIUnits.smallSpacing) {
            return VLCSpacingTokenSmall;
        } else if (insetVal == VLCLibraryUIUnits.mediumSpacing) {
            return VLCSpacingTokenMedium;
        } else if (insetVal == VLCLibraryUIUnits.largeSpacing) {
            return VLCSpacingTokenLarge;
        }
    }
    return VLCSpacingTokenNone;
}

- (void)setEdgeInsetsToken:(NSInteger)edgeInsetsToken
{
    CGFloat spacingValue = 0;
    switch (edgeInsetsToken) {
        case VLCSpacingTokenSmall:
            spacingValue = VLCLibraryUIUnits.smallSpacing;
            break;
        case VLCSpacingTokenMedium:
            spacingValue = VLCLibraryUIUnits.mediumSpacing;
            break;
        case VLCSpacingTokenLarge:
            spacingValue = VLCLibraryUIUnits.largeSpacing;
            break;
        default:
            return;
    }
    self.edgeInsets = NSEdgeInsetsMake(spacingValue, spacingValue, spacingValue, spacingValue);
}

@end
