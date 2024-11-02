/*****************************************************************************
 * VLCTimeFormatter.m: MacOS X interface module
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

#import "VLCTimeFormatter.h"

#import "extensions/NSString+Helpers.h"

@implementation VLCTimeFormatter

- (NSString *)stringForObjectValue:(id)obj
{
    NSParameterAssert([obj isKindOfClass:NSNumber.class]);
    NSNumber * const time = obj;
    return [NSString stringWithTime:time.longLongValue];
}

- (BOOL)getObjectValue:(out id _Nullable *)obj
             forString:(NSString *)string
      errorDescription:(out NSString * _Nullable *)error
{
    NSArray<NSString *> * const components = [string componentsSeparatedByString:@":"];
    const NSUInteger componentCount = components.count;
    if (componentCount <= 1 || componentCount > 3) {
        *error = @"Cannot get bookmark time as invalid string format for time was received";
        return NO;
    }

    size_t time = 0;

    if (componentCount == 1) {
        time = components.firstObject.longLongValue * 1000;
    } else if (componentCount == 2) {
        time = components.firstObject.longLongValue * 60 +
               [components objectAtIndex:1].longLongValue * 1000;
    } else if (componentCount == 3) {
        time = components.firstObject.longLongValue * 3600 +
               [components objectAtIndex:1].longLongValue * 60 +
               [components objectAtIndex:2].longLongValue * 1000;
    }

    *obj = [NSNumber numberWithLongLong:time];
    return YES;
}

@end
