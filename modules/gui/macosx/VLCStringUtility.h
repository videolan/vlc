/*****************************************************************************
 * VLCStringUtility.h: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2002-2014 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Jon Lech Johansen <jon-vl@nanocrew.net>
 *          Christophe Massiot <massiot@via.ecp.fr>
 *          Derk-Jan Hartman <hartman at videolan dot org>
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

#import <Cocoa/Cocoa.h>

#import <vlc_common.h>
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
unsigned int CocoaKeyToVLC(unichar i_key);

/**
 * Gets an image resource
 */
NSImage *imageFromRes(NSString *name);

@interface VLCStringUtility : NSObject

+ (VLCStringUtility *)sharedInstance;

- (NSString *)stringForTime:(long long int)time;

- (NSString *)OSXStringKeyToString:(NSString *)theString;
- (NSString *)VLCKeyToString:(NSString *)theString;
- (unsigned int)VLCModifiersToCocoa:(NSString *)theString;

- (NSString *)getVolumeTypeFromMountPath:(NSString *)mountPath;
- (NSString *)getBSDNodeFromMountPath:(NSString *)mountPath;

@end
