/*****************************************************************************
 * NSString+Helpers.m: Category with helper functions for NSStrings
 *****************************************************************************
 * Copyright (C) 2002-2018 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Jon Lech Johansen <jon-vl@nanocrew.net>
 *          Christophe Massiot <massiot@via.ecp.fr>
 *          Derk-Jan Hartman <hartman at videolan dot org>
 *          Felix Paul Kühne <fkuehne at videolan dot org>
 *          Marvin Scholz <epirat07@gmail.com>
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

#import "NSString+Helpers.h"
#import <Cocoa/Cocoa.h>

#import <vlc_common.h>
#import <vlc_strings.h>

@implementation NSString (Helpers)

+ (instancetype)stringWithTimeFromInput:(input_thread_t *)input
                               negative:(BOOL)negative
{
    NSAssert(input != NULL, @"Input may not be NULL!");

    char psz_time[MSTRTIME_MAX_SIZE];
    vlc_tick_t t = var_GetInteger(input, "time");

    vlc_tick_t dur = input_item_GetDuration(input_GetItem(input));
    if (negative && dur > 0) {
        vlc_tick_t remaining = (dur > t) ? (dur - t) : 0;

        return [NSString stringWithFormat:@"-%s",
                secstotimestr(psz_time, (int)SEC_FROM_VLC_TICK(remaining))];
    } else {
        return [NSString stringWithUTF8String:
                secstotimestr(psz_time, (int)SEC_FROM_VLC_TICK(t))];
    }
}

+ (instancetype)base64StringWithCString:(const char *)cstring
{
    if (cstring == NULL)
        return nil;

    char *encoded_cstring = vlc_b64_encode(cstring);
    if (encoded_cstring == NULL)
        return nil;

    return [[NSString alloc] initWithBytesNoCopy:encoded_cstring
                                          length:strlen(encoded_cstring)
                                        encoding:NSUTF8StringEncoding
                                    freeWhenDone:YES];
}

- (NSString *)base64EncodedString
{
    char *encoded_cstring = vlc_b64_encode(self.UTF8String);
    if (encoded_cstring == NULL)
        return nil;

    return [[NSString alloc] initWithBytesNoCopy:encoded_cstring
                                          length:strlen(encoded_cstring)
                                        encoding:NSUTF8StringEncoding
                                    freeWhenDone:YES];
}

- (NSString *)base64DecodedString
{
    char *decoded_cstring = vlc_b64_decode(self.UTF8String);
    if (decoded_cstring == NULL)
        return nil;

    return [[NSString alloc] initWithBytesNoCopy:decoded_cstring
                                          length:strlen(decoded_cstring)
                                        encoding:NSUTF8StringEncoding
                                    freeWhenDone:YES];
}

- (NSString *)stringWrappedToWidth:(int)width
{
    NSRange glyphRange, effectiveRange, charRange;
    unsigned breaksInserted = 0;

    NSMutableString *wrapped_string = [self mutableCopy];

    NSTextStorage *text_storage =
        [[NSTextStorage alloc] initWithString:wrapped_string
                                   attributes:@{
                                                NSFontAttributeName : [NSFont labelFontOfSize: 0.0]
                                                }];

    NSLayoutManager *layout_manager = [[NSLayoutManager alloc] init];
    NSTextContainer *text_container =
        [[NSTextContainer alloc] initWithContainerSize:NSMakeSize(width, 2000)];

    [layout_manager addTextContainer:text_container];
    [text_storage addLayoutManager:layout_manager];

    glyphRange = [layout_manager glyphRangeForTextContainer:text_container];

    for (NSUInteger glyphIndex = glyphRange.location;
         glyphIndex < NSMaxRange(glyphRange);
         glyphIndex += effectiveRange.length)
    {
        [layout_manager lineFragmentRectForGlyphAtIndex:glyphIndex
                                         effectiveRange:&effectiveRange];

        charRange = [layout_manager characterRangeForGlyphRange:effectiveRange
                                               actualGlyphRange:&effectiveRange];
        if ([wrapped_string lineRangeForRange:
                NSMakeRange(charRange.location + breaksInserted, charRange.length)
             ].length > charRange.length)
        {
            [wrapped_string insertString:@"\n" atIndex:NSMaxRange(charRange) + breaksInserted];
            breaksInserted++;
        }
    }

    return [NSString stringWithString:wrapped_string];
}

@end
