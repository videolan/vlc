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

#import <vlc_common.h>
#import <vlc_strings.h>

@implementation NSString (Helpers)

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

@end
