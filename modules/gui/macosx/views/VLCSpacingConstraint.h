/*****************************************************************************
 * VLCSpacingConstraint.h: MacOS X interface module
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

#import <Cocoa/Cocoa.h>

NS_ASSUME_NONNULL_BEGIN

typedef NS_ENUM(NSInteger, VLCSpacingToken) {
    VLCSpacingTokenNone = 0,
    VLCSpacingTokenSmall = 1,   // smallSpacing (4pt)
    VLCSpacingTokenMedium = 2,  // mediumSpacing (8pt)
    VLCSpacingTokenLarge = 3    // largeSpacing (16pt)
};

IB_DESIGNABLE
@interface VLCSpacingConstraint : NSLayoutConstraint

@property (nonatomic) IBInspectable NSInteger spacingToken;

@end

NS_ASSUME_NONNULL_END
