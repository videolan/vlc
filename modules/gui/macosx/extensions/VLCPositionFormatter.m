/*****************************************************************************
 * VLCPositionFormatter.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2013, 2019 VLC authors and VideoLAN
 *
 * Author: David Fuhrmann <dfuhrmann # videolan.org>
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

#import "VLCPositionFormatter.h"

@interface VLCPositionFormatter()
{
    NSCharacterSet *o_forbidden_characters;
}
@end

@implementation VLCPositionFormatter

- (id)init
{
    self = [super init];
    if (self) {
        NSMutableCharacterSet *nonNumbers = [[[NSCharacterSet decimalDigitCharacterSet] invertedSet] mutableCopy];
        [nonNumbers removeCharactersInString:@"-:"];
        o_forbidden_characters = [nonNumbers copy];
    }
    return self;
}

- (NSString*)stringForObjectValue:(id)obj
{
    if([obj isKindOfClass:[NSString class]])
        return obj;
    if([obj isKindOfClass:[NSNumber class]])
        return [obj stringValue];

    return nil;
}

- (BOOL)getObjectValue:(id*)obj forString:(NSString*)string errorDescription:(NSString**)error
{
    *obj = [string copy];
    return YES;
}

- (BOOL)isPartialStringValid:(NSString*)partialString newEditingString:(NSString**)newString errorDescription:(NSString**)error
{
    if ([partialString rangeOfCharacterFromSet:o_forbidden_characters options:NSLiteralSearch].location != NSNotFound) {
        return NO;
    } else {
        return YES;
    }
}

@end
