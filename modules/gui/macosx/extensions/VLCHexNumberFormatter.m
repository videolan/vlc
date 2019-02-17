/*****************************************************************************
 * VLCHexNumberFormatter.m
 *****************************************************************************
 * Copyright (C) 2017 VLC authors and VideoLAN
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

#import "VLCHexNumberFormatter.h"

@implementation VLCHexNumberFormatter

- (NSString *)stringForObjectValue:(id)obj
{
    if (![obj isKindOfClass:[NSNumber class]]) {
        return nil;
    }

    NSString *string = [NSString stringWithFormat:@"%06" PRIX32, [obj intValue]];
    return string;
}

- (BOOL)getObjectValue:(out id  _Nullable __autoreleasing *)obj
             forString:(NSString *)string
      errorDescription:(out NSString *__autoreleasing  _Nullable *)error
{
    unsigned result = 0;
    NSScanner *scanner = [NSScanner scannerWithString:string];
    BOOL success = [scanner scanHexInt:&result];

    *obj = (success) ? [NSNumber numberWithInt:result] : nil;
    return success;
}

@end
