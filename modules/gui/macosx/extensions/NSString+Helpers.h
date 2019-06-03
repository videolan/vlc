/*****************************************************************************
 * NSString+Helpers.h: Category with helper functions for NSStrings
 *****************************************************************************
 * Copyright (C) 2002-2019 VLC authors and VideoLAN
 *
 * Authors: Jon Lech Johansen <jon-vl@nanocrew.net>
 *          Christophe Massiot <massiot@via.ecp.fr>
 *          Derk-Jan Hartman <hartman at videolan dot org>
 *          Felix Paul Kühne <fkuehne at videolan dot org>
 *          Marvin Scholz <epirat07@gmail.com>
 *          David Fuhrmann <dfuhrmann # videolan.org>
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
#import <vlc_input.h>

#define _NS(s) ((s) ? toNSStr(vlc_gettext(s)) : @"")

/* Get an alternate version of the string.
 * This string is stored as '1:string' but when displayed it only displays
 * the translated string. the translation should be '1:translatedstring' though */
#define _ANS(s) [((s) ? toNSStr(vlc_gettext(s)) : @"") substringFromIndex:2]

extern NSString *const kVLCMediaAudioCD;
extern NSString *const kVLCMediaDVD;
extern NSString *const kVLCMediaVCD;
extern NSString *const kVLCMediaSVCD;
extern NSString *const kVLCMediaBD;
extern NSString *const kVLCMediaVideoTSFolder;
extern NSString *const kVLCMediaBDMVFolder;
extern NSString *const kVLCMediaUnknown;

NSString *toNSStr(const char *str);

/**
 * Takes the first value of an cocoa key string, and converts it to VLCs int representation.
 */
unsigned int CocoaKeyToVLC(unichar i_key);

/**
 * Fix certain settings strings before saving
 */
bool fixIntfSettings(void);

/**
 * Gets an image resource
 */
NSImage *imageFromRes(NSString *name);

@interface NSString (Helpers)

/**
 Creates an NSString with the current time of the \c input_item_t

 This method allocates and initializes an NSString with the current
 elapsed or remaining time of the given input.

 \param the duration
 \param the current time
 \param negative   If YES, calculate remaining instead of elapsed time
 */
+ (instancetype)stringWithDuration:(vlc_tick_t)duration
                       currentTime:(vlc_tick_t)time
                          negative:(BOOL)negative;

/**
 Creates an NSString with the given time in seconds

 This method allocates and initializes an NSString with the given
 time formatted as playback time.

 \param time   Time in seconds
 */
+ (instancetype)stringWithTime:(long long int)time;

/**
 Creates an NSString with the given time in VLC ticks

 This method allocates and initializes an NSString with the given
 time formatted as displayable time

 \param time   Time in VLC ticks
 */
+ (instancetype)stringWithTimeFromTicks:(vlc_tick_t)time;

/**
 Returns a time in seconds from strings formatted with colons (aka ##, ##:##, ##:##:##)

 \param aString the string to parse
 */
+ (NSInteger)timeInSecondsFromStringWithColons:(NSString *)aString;

/**
 Creates an NSString from the given null-terminated C string
 buffer encoded as base64

 This method allocates and initializes an NSString with the
 provided C string encoded as base64.
 */
+ (instancetype)base64StringWithCString:(const char *)cstring;

/**
 Base64 encoded copy of the string

 Encode the string as Base64 string and return the result or
 nil on failure.
 */
- (NSString *)base64EncodedString;

/**
 Base64 decoded copy of the string

 Decode the string as Base64 string and return the result or
 nil on failure.
 */
- (NSString *)base64DecodedString;

/**
 Returns a copy of the receiver string, wrapped to the specified width

 This method returns a copy of the receiver string, wrapped to the given
 width in pixels.

 \param width Width in pixel
 */
- (NSString *)stringWrappedToWidth:(int)width;

@end

/**
 Base64 decode the given NSString

 Decodes the given Base64 encoded NSString or returns and empty
 NSString in case of failure.

 \warning Compatibility function, do not use in new code!
 */
static inline NSString *B64DecNSStr(NSString *s) {
    NSString *res = [s base64DecodedString];

    return (res == nil) ? @"" : res;
}

/**
 Base64 encode the given C String and free it

 Base64 encodes the given C string and frees it, returns and empty
 NSString in case of failure.
 The given string is freed regardless if an error occured or not.

 \warning Compatibility function, do not use in new code!
 */
static inline NSString *B64EncAndFree(char *cs) {
    NSString *res = [NSString base64StringWithCString:cs];
    free(cs);

    return (res == nil) ? @"" : res;
}

NSString * getVolumeTypeFromMountPath(NSString *mountPath);

NSString * getBSDNodeFromMountPath(NSString *mountPath);

/**
 * Converts VLC key string to a prettified version, for hotkey settings.
 * The returned string adapts similar how its done within the cocoa framework when setting this
 * key to menu items.
 */
NSString * OSXStringKeyToString(NSString *theString);

/**
 * Converts VLC key string to cocoa modifiers which can be used as setKeyEquivalent for menu items
 */
NSString * VLCKeyToString(char *theChar);

/**
 * Converts VLC key to cocoa string which can be used as setKeyEquivalentModifierMask for menu items
 */
unsigned int VLCModifiersToCocoa(char *theChar);
