/*****************************************************************************
 * VLCStackView.m: MacOS X interface module
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

#import "VLCStackView.h"

#import "views/VLCUIUnits.h"

@implementation VLCStackView

- (void)awakeFromNib
{
    [super awakeFromNib];
    [self applySpacingToken];
    [self applyEdgeInsetsToken];
}

- (void)setSpacingToken:(NSInteger)spacingToken
{
    _spacingToken = spacingToken;
    [self applySpacingToken];
}

- (void)setEdgeInsetsToken:(NSInteger)edgeInsetsToken
{
    _edgeInsetsToken = edgeInsetsToken;
    [self applyEdgeInsetsToken];
}

- (void)applySpacingToken
{
    if (_spacingToken == VLCSpacingTokenNone) {
        return;
    }
    
    CGFloat spacingValue = 0;
    switch (_spacingToken) {
        case VLCSpacingTokenSmall:
            spacingValue = VLCUIUnits.smallSpacing;
            break;
        case VLCSpacingTokenMedium:
            spacingValue = VLCUIUnits.mediumSpacing;
            break;
        case VLCSpacingTokenLarge:
            spacingValue = VLCUIUnits.largeSpacing;
            break;
        default:
            return;
    }
    self.spacing = spacingValue;
}

- (void)applyEdgeInsetsToken
{
    if (_edgeInsetsToken == VLCSpacingTokenNone) {
        return;
    }
    
    CGFloat spacingValue = 0;
    switch (_edgeInsetsToken) {
        case VLCSpacingTokenSmall:
            spacingValue = VLCUIUnits.smallSpacing;
            break;
        case VLCSpacingTokenMedium:
            spacingValue = VLCUIUnits.mediumSpacing;
            break;
        case VLCSpacingTokenLarge:
            spacingValue = VLCUIUnits.largeSpacing;
            break;
        default:
            return;
    }
    self.edgeInsets = NSEdgeInsetsMake(spacingValue, spacingValue, spacingValue, spacingValue);
}

@end
