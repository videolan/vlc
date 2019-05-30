/*****************************************************************************
 * NSColor+VLCAdditions.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2019 VLC authors and VideoLAN
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

#import "NSColor+VLCAdditions.h"

@implementation NSColor (VLCAdditions)

+ (instancetype)VLClibraryHighlightColor
{
    return [NSColor colorWithRed:1. green:136./255. blue:.0 alpha:1.];
}

+ (instancetype)VLClibraryLightTitleColor
{
    return [NSColor colorWithRed:0.15 green:0.16 blue:0.17 alpha:1.];
}

+ (instancetype)VLClibraryDarkTitleColor
{
    return [NSColor colorWithRed:0.85 green:0.84 blue:0.83 alpha:1.];
}

+ (instancetype)VLClibrarySubtitleColor
{
    return [NSColor colorWithRed:0.52 green:0.57 blue:0.61 alpha:1.];
}

+ (instancetype)VLClibraryAnnotationColor
{
    return [NSColor whiteColor];
}

+ (instancetype)VLClibraryAnnotationBackgroundColor
{
    return [NSColor colorWithRed:0. green:0. blue:0. alpha:.4];
}

+ (instancetype)VLClibrarySeparatorLightColor
{
    return [NSColor colorWithRed:0.89 green:0.91 blue:0.93 alpha:1.];
}

+ (instancetype)VLClibrarySeparatorDarkColor
{
    return [NSColor colorWithRed:0.11 green:0.09 blue:0.07 alpha:1.];
}

+ (instancetype)VLClibraryProgressIndicatorBackgroundColor
{
    return [NSColor colorWithRed:37./255. green:41./255. blue:44./255. alpha:.8];
}

+ (instancetype)VLClibraryProgressIndicatorForegroundColor
{
    return [NSColor colorWithRed:246./255. green:127./255. blue:0. alpha:1.];
}

@end
