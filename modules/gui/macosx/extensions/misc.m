/*****************************************************************************
 * misc.m: code not specific to vlc
 *****************************************************************************
 * Copyright (C) 2003-2015 VLC authors and VideoLAN
 *
 * Authors: Jon Lech Johansen <jon-vl@nanocrew.net>
 *          Felix Paul Kühne <fkuehne at videolan dot org>
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

#import "misc.h"
#import "NSString+Helpers.h"

@interface PositionFormatter()
{
    NSCharacterSet *o_forbidden_characters;
}
@end

@implementation PositionFormatter

- (id)init
{
    self = [super init];
    NSMutableCharacterSet *nonNumbers = [[[NSCharacterSet decimalDigitCharacterSet] invertedSet] mutableCopy];
    [nonNumbers removeCharactersInString:@"-:"];
    o_forbidden_characters = [nonNumbers copy];

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

/*****************************************************************************
 * VLCByteCountFormatter addition
 *****************************************************************************/

@implementation VLCByteCountFormatter

+ (NSString *)stringFromByteCount:(long long)byteCount countStyle:(NSByteCountFormatterCountStyle)countStyle
{
    // Use native implementation on >= mountain lion
    Class byteFormatterClass = NSClassFromString(@"NSByteCountFormatter");
    if (byteFormatterClass && [byteFormatterClass respondsToSelector:@selector(stringFromByteCount:countStyle:)]) {
        return [byteFormatterClass stringFromByteCount:byteCount countStyle:NSByteCountFormatterCountStyleFile];
    }

    float devider = 0.;
    float returnValue = 0.;
    NSString *suffix;

    NSNumberFormatter *theFormatter = [[NSNumberFormatter alloc] init];
    [theFormatter setLocale:[NSLocale currentLocale]];
    [theFormatter setAllowsFloats:YES];

    NSString *returnString = @"";

    if (countStyle != NSByteCountFormatterCountStyleDecimal)
        devider = 1024.;
    else
        devider = 1000.;

    if (byteCount < 1000) {
        returnValue = byteCount;
        suffix = _NS("B");
        [theFormatter setMaximumFractionDigits:0];
        goto end;
    }

    if (byteCount < 1000000) {
        returnValue = byteCount / devider;
        suffix = _NS("KB");
        [theFormatter setMaximumFractionDigits:0];
        goto end;
    }

    if (byteCount < 1000000000) {
        returnValue = byteCount / devider / devider;
        suffix = _NS("MB");
        [theFormatter setMaximumFractionDigits:1];
        goto end;
    }

    [theFormatter setMaximumFractionDigits:2];
    if (byteCount < 1000000000000) {
        returnValue = byteCount / devider / devider / devider;
        suffix = _NS("GB");
        goto end;
    }

    returnValue = byteCount / devider / devider / devider / devider;
    suffix = _NS("TB");

end:
    returnString = [NSString stringWithFormat:@"%@ %@", [theFormatter stringFromNumber:[NSNumber numberWithFloat:returnValue]], suffix];

    return returnString;
}

@end
