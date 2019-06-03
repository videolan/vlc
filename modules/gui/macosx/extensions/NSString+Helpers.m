/*****************************************************************************
 * NSString+Helpers.m: Category with helper functions for NSStrings
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

#import "NSString+Helpers.h"

#import <vlc_strings.h>
#import <vlc_actions.h>

#import <sys/param.h>
#import <sys/mount.h>

#import <IOKit/storage/IOMedia.h>
#import <IOKit/storage/IOCDMedia.h>
#import <IOKit/storage/IODVDMedia.h>
#import <IOKit/storage/IOBDMedia.h>

NSString *const kVLCMediaAudioCD = @"AudioCD";
NSString *const kVLCMediaDVD = @"DVD";
NSString *const kVLCMediaVCD = @"VCD";
NSString *const kVLCMediaSVCD = @"SVCD";
NSString *const kVLCMediaBD = @"Blu-ray";
NSString *const kVLCMediaVideoTSFolder = @"VIDEO_TS";
NSString *const kVLCMediaBDMVFolder = @"BDMV";
NSString *const kVLCMediaUnknown = @"Unknown";

@implementation NSString (Helpers)

+ (instancetype)stringWithDuration:(vlc_tick_t)duration
                       currentTime:(vlc_tick_t)time
                          negative:(BOOL)negative
{
    char psz_time[MSTRTIME_MAX_SIZE];

    if (negative && duration > 0) {
        vlc_tick_t remaining = (duration > time) ? (duration - time) : 0;

        return [NSString stringWithFormat:@"-%s",
                secstotimestr(psz_time, (int)SEC_FROM_VLC_TICK(remaining))];
    } else {
        return [NSString stringWithUTF8String:
                secstotimestr(psz_time, (int)SEC_FROM_VLC_TICK(time))];
    }
}

+ (instancetype)stringWithTimeFromTicks:(vlc_tick_t)time
{
    char psz_time[MSTRTIME_MAX_SIZE];
    return [NSString stringWithUTF8String:
            secstotimestr(psz_time, (int)SEC_FROM_VLC_TICK(time))];
}

+ (instancetype)stringWithTime:(long long int)time
{
    if (time > 0) {
        if (time > 3600)
            return [NSString stringWithFormat:@"%s%01ld:%02ld:%02ld",
                    time < 0 ? "-" : "",
                    (long) (time / 3600),
                    (long)((time / 60) % 60),
                    (long) (time % 60)];
        else
            return [NSString stringWithFormat:@"%s%02ld:%02ld",
                    time < 0 ? "-" : "",
                    (long)((time / 60) % 60),
                    (long) (time % 60)];
    } else {
        // Return a string that represents an undefined time.
        return @"--:--";
    }
}

+ (NSInteger)timeInSecondsFromStringWithColons:(NSString *)aString
{
    NSArray *components = [aString componentsSeparatedByString:@":"];
    NSUInteger componentCount = [components count];
    NSInteger returnValue = 0;
    switch (componentCount) {
        case 3:
            returnValue = [[components firstObject] intValue] * 3600 + [[components objectAtIndex:1] intValue] * 60 + [[components objectAtIndex:2] intValue];
            break;

        case 2:
            returnValue = [[components firstObject] intValue] * 60 + [[components objectAtIndex:1] intValue];
            break;

        default:
            returnValue = [[components firstObject] intValue];
            break;
    }

    return returnValue;
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

#pragma mark -
#pragma mark Key Shortcuts

static struct
{
    unichar i_nskey;
    unsigned int i_vlckey;
} nskeys_to_vlckeys[] =
{
    { NSUpArrowFunctionKey, KEY_UP },
    { NSDownArrowFunctionKey, KEY_DOWN },
    { NSLeftArrowFunctionKey, KEY_LEFT },
    { NSRightArrowFunctionKey, KEY_RIGHT },
    { NSF1FunctionKey, KEY_F1 },
    { NSF2FunctionKey, KEY_F2 },
    { NSF3FunctionKey, KEY_F3 },
    { NSF4FunctionKey, KEY_F4 },
    { NSF5FunctionKey, KEY_F5 },
    { NSF6FunctionKey, KEY_F6 },
    { NSF7FunctionKey, KEY_F7 },
    { NSF8FunctionKey, KEY_F8 },
    { NSF9FunctionKey, KEY_F9 },
    { NSF10FunctionKey, KEY_F10 },
    { NSF11FunctionKey, KEY_F11 },
    { NSF12FunctionKey, KEY_F12 },
    { NSInsertFunctionKey, KEY_INSERT },
    { NSHomeFunctionKey, KEY_HOME },
    { NSEndFunctionKey, KEY_END },
    { NSPageUpFunctionKey, KEY_PAGEUP },
    { NSPageDownFunctionKey, KEY_PAGEDOWN },
    { NSMenuFunctionKey, KEY_MENU },
    { NSTabCharacter, KEY_TAB },
    { NSCarriageReturnCharacter, KEY_ENTER },
    { NSEnterCharacter, KEY_ENTER },
    { NSBackspaceCharacter, KEY_BACKSPACE },
    { NSDeleteCharacter, KEY_DELETE },
    {0,0}
};

unsigned int CocoaKeyToVLC(unichar i_key)
{
    unsigned int i;

    for (i = 0; nskeys_to_vlckeys[i].i_nskey != 0; i++) {
        if (nskeys_to_vlckeys[i].i_nskey == i_key) {
            return nskeys_to_vlckeys[i].i_vlckey;
        }
    }
    return (unsigned int)i_key;
}

/* takes a good old const c string and converts it to NSString without UTF8 loss */

NSString *toNSStr(const char *str) {
    return str != NULL ? [NSString stringWithUTF8String:str] : @"";
}

NSImage *imageFromRes(NSString *name)
{
    return [NSImage imageNamed:name];
}

bool fixIntfSettings(void)
{
    NSMutableString * o_workString;
    NSRange returnedRange;
    NSRange fullRange;
    BOOL b_needsRestart = NO;

    #define fixpref(pref) \
    o_workString = [[NSMutableString alloc] initWithFormat:@"%s", config_GetPsz(pref)]; \
    if ([o_workString length] > 0) \
    { \
        returnedRange = [o_workString rangeOfString:@"macosx" options: NSCaseInsensitiveSearch]; \
        if (returnedRange.location != NSNotFound) \
        { \
            if ([o_workString isEqualToString:@"macosx"]) \
                [o_workString setString:@""]; \
            fullRange = NSMakeRange(0, [o_workString length]); \
            [o_workString replaceOccurrencesOfString:@":macosx" withString:@"" options: NSCaseInsensitiveSearch range: fullRange]; \
            fullRange = NSMakeRange(0, [o_workString length]); \
            [o_workString replaceOccurrencesOfString:@"macosx:" withString:@"" options: NSCaseInsensitiveSearch range: fullRange]; \
            \
            config_PutPsz(pref, [o_workString UTF8String]); \
            b_needsRestart = YES; \
        } \
    }

    fixpref("control");
    fixpref("extraintf");
#undef fixpref

    return b_needsRestart;
}

NSString * getVolumeTypeFromMountPath(NSString *mountPath)
{
    struct statfs stf;
    int ret = statfs([mountPath fileSystemRepresentation], &stf);
    if (ret != 0) {
        return @"";
    }

    /* the matching dictionary wants the BSD name without its path, so strip it */
    NSString *bsdMount = [NSString stringWithUTF8String:stf.f_mntfromname];
    NSString *bsdName = [bsdMount lastPathComponent];

    CFMutableDictionaryRef matchingDict = IOBSDNameMatching(kIOMasterPortDefault, 0, [bsdName UTF8String]);
    NSString *returnValue;

    io_service_t service = IOServiceGetMatchingService(kIOMasterPortDefault, matchingDict);
    if (IO_OBJECT_NULL != service) {
        if (IOObjectConformsTo(service, kIOCDMediaClass)) {
            returnValue = kVLCMediaAudioCD;
        } else if (IOObjectConformsTo(service, kIODVDMediaClass)) {
            returnValue = kVLCMediaDVD;
        } else if (IOObjectConformsTo(service, kIOBDMediaClass)) {
            returnValue = kVLCMediaBD;
        }
        IOObjectRelease(service);

        if (returnValue) {
            return returnValue;
        }
    }

    if ([mountPath rangeOfString:@"VIDEO_TS" options:NSCaseInsensitiveSearch | NSBackwardsSearch].location != NSNotFound) {
        returnValue = kVLCMediaVideoTSFolder;
    } else if ([mountPath rangeOfString:@"BDMV" options:NSCaseInsensitiveSearch | NSBackwardsSearch].location != NSNotFound) {
        returnValue = kVLCMediaBDMVFolder;
    } else {
        // NSFileManager is not thread-safe, don't use defaultManager outside of the main thread
        NSFileManager *fileManager = [[NSFileManager alloc] init];

        NSArray *directoryContents = [fileManager contentsOfDirectoryAtPath:mountPath error:nil];
        NSUInteger directoryContentCount = [directoryContents count];
        for (NSUInteger i = 0; i < directoryContentCount; i++) {
            NSString *currentFile = directoryContents[i];
            NSString *fullPath = [mountPath stringByAppendingPathComponent:currentFile];

            BOOL isDirectory;
            if ([fileManager fileExistsAtPath:fullPath isDirectory:&isDirectory] && isDirectory)
            {
                if ([currentFile caseInsensitiveCompare:@"SVCD"] == NSOrderedSame) {
                    returnValue = kVLCMediaSVCD;
                    break;
                }
                if ([currentFile caseInsensitiveCompare:@"VCD"] == NSOrderedSame) {
                    returnValue = kVLCMediaVCD;
                    break;
                }
                if ([currentFile caseInsensitiveCompare:@"BDMV"] == NSOrderedSame) {
                    returnValue = kVLCMediaBDMVFolder;
                    break;
                }
                if ([currentFile caseInsensitiveCompare:@"VIDEO_TS"] == NSOrderedSame) {
                    returnValue = kVLCMediaVideoTSFolder;
                    break;
                }
            }
        }

        if (!returnValue)
            returnValue = kVLCMediaVideoTSFolder;
    }

    return returnValue;
}

NSString * getBSDNodeFromMountPath(NSString *mountPath)
{
    struct statfs stf;
    int ret = statfs([mountPath fileSystemRepresentation], &stf);
    if (ret != 0) {
        return @"";
    }
    /* the provided BSD mount path doesn't include the r prefix we need */
    NSString *bsdMount = [NSString stringWithUTF8String:stf.f_mntfromname];
    NSString *bsdName = [bsdMount lastPathComponent];
    NSString *fixedBsdName = [NSString stringWithFormat:@"/dev/r%@", bsdName];

    return fixedBsdName;
}

NSString * OSXStringKeyToString(NSString *theString)
{
    if (![theString isEqualToString:@""]) {
        /* remove cruft */
        if ([theString characterAtIndex:([theString length] - 1)] != 0x2b)
            theString = [theString stringByReplacingOccurrencesOfString:@"+" withString:@""];
        else {
            theString = [theString stringByReplacingOccurrencesOfString:@"+" withString:@""];
            theString = [NSString stringWithFormat:@"%@+", theString];
        }
        if ([theString characterAtIndex:([theString length] - 1)] != 0x2d)
            theString = [theString stringByReplacingOccurrencesOfString:@"-" withString:@""];
        else {
            theString = [theString stringByReplacingOccurrencesOfString:@"-" withString:@""];
            theString = [NSString stringWithFormat:@"%@-", theString];
        }
        /* modifiers */
        theString = [theString stringByReplacingOccurrencesOfString:@"Command" withString: [NSString stringWithUTF8String:"\xE2\x8C\x98"]];
        theString = [theString stringByReplacingOccurrencesOfString:@"Alt" withString: [NSString stringWithUTF8String:"\xE2\x8C\xA5"]];
        theString = [theString stringByReplacingOccurrencesOfString:@"Shift" withString: [NSString stringWithUTF8String:"\xE2\x87\xA7"]];
        theString = [theString stringByReplacingOccurrencesOfString:@"Ctrl" withString: [NSString stringWithUTF8String:"\xE2\x8C\x83"]];
        /* show non-character keys correctly */
        theString = [theString stringByReplacingOccurrencesOfString:@"Right" withString:[NSString stringWithUTF8String:"\xE2\x86\x92"]];
        theString = [theString stringByReplacingOccurrencesOfString:@"Left" withString:[NSString stringWithUTF8String:"\xE2\x86\x90"]];
        theString = [theString stringByReplacingOccurrencesOfString:@"Page Up" withString:[NSString stringWithUTF8String:"\xE2\x87\x9E"]];
        theString = [theString stringByReplacingOccurrencesOfString:@"Page Down" withString:[NSString stringWithUTF8String:"\xE2\x87\x9F"]];
        theString = [theString stringByReplacingOccurrencesOfString:@"Up" withString:[NSString stringWithUTF8String:"\xE2\x86\x91"]];
        theString = [theString stringByReplacingOccurrencesOfString:@"Down" withString:[NSString stringWithUTF8String:"\xE2\x86\x93"]];
        theString = [theString stringByReplacingOccurrencesOfString:@"Enter" withString:[NSString stringWithUTF8String:"\xe2\x86\xb5"]];
        theString = [theString stringByReplacingOccurrencesOfString:@"Tab" withString:[NSString stringWithUTF8String:"\xe2\x87\xa5"]];
        theString = [theString stringByReplacingOccurrencesOfString:@"Delete" withString:[NSString stringWithUTF8String:"\xe2\x8c\xab"]];        /* capitalize plain characters to suit the menubar's look */
        theString = [theString capitalizedString];
    }
    else
        theString = [NSString stringWithString:_NS("Not Set")];
    return theString;
}

NSString * VLCKeyToString(char *theChar)
{
    if (theChar == NULL) {
        return @"";
    }
    NSString *theString = toNSStr(theChar);

    if (![theString isEqualToString:@""]) {
        if ([theString characterAtIndex:([theString length] - 1)] != 0x2b)
            theString = [theString stringByReplacingOccurrencesOfString:@"+" withString:@""];
        else {
            theString = [theString stringByReplacingOccurrencesOfString:@"+" withString:@""];
            theString = [NSString stringWithFormat:@"%@+", theString];
        }
        if ([theString characterAtIndex:([theString length] - 1)] != 0x2d)
            theString = [theString stringByReplacingOccurrencesOfString:@"-" withString:@""];
        else {
            theString = [theString stringByReplacingOccurrencesOfString:@"-" withString:@""];
            theString = [NSString stringWithFormat:@"%@-", theString];
        }
        theString = [theString stringByReplacingOccurrencesOfString:@"Command" withString:@""];
        theString = [theString stringByReplacingOccurrencesOfString:@"Alt" withString:@""];
        theString = [theString stringByReplacingOccurrencesOfString:@"Shift" withString:@""];
        theString = [theString stringByReplacingOccurrencesOfString:@"Ctrl" withString:@""];
    }

#ifdef __clang__
#pragma GCC diagnostic ignored "-Wformat"
#endif
    if ([theString length] > 1) {
        if ([theString rangeOfString:@"Page Up"].location != NSNotFound)
            return [NSString stringWithFormat:@"%C", NSPageUpFunctionKey];
        else if ([theString rangeOfString:@"Page Down"].location != NSNotFound)
            return [NSString stringWithFormat:@"%C", NSPageDownFunctionKey];
        else if ([theString rangeOfString:@"Up"].location != NSNotFound)
            return [NSString stringWithFormat:@"%C", NSUpArrowFunctionKey];
        else if ([theString rangeOfString:@"Down"].location != NSNotFound)
            return [NSString stringWithFormat:@"%C", NSDownArrowFunctionKey];
        else if ([theString rangeOfString:@"Right"].location != NSNotFound)
            return [NSString stringWithFormat:@"%C", NSRightArrowFunctionKey];
        else if ([theString rangeOfString:@"Left"].location != NSNotFound)
            return [NSString stringWithFormat:@"%C", NSLeftArrowFunctionKey];
        else if ([theString rangeOfString:@"Enter"].location != NSNotFound)
            return [NSString stringWithFormat:@"%C", NSEnterCharacter]; // we treat NSCarriageReturnCharacter as aquivalent
        else if ([theString rangeOfString:@"Insert"].location != NSNotFound)
            return [NSString stringWithFormat:@"%C", NSInsertFunctionKey];
        else if ([theString rangeOfString:@"Home"].location != NSNotFound)
            return [NSString stringWithFormat:@"%C", NSHomeFunctionKey];
        else if ([theString rangeOfString:@"End"].location != NSNotFound)
            return [NSString stringWithFormat:@"%C", NSEndFunctionKey];
        else if ([theString rangeOfString:@"Menu"].location != NSNotFound)
            return [NSString stringWithFormat:@"%C", NSMenuFunctionKey];
        else if ([theString rangeOfString:@"Tab"].location != NSNotFound)
            return [NSString stringWithFormat:@"%C", NSTabCharacter];
        else if ([theString rangeOfString:@"Backspace"].location != NSNotFound)
            return [NSString stringWithFormat:@"%C", NSBackspaceCharacter];
        else if ([theString rangeOfString:@"Delete"].location != NSNotFound)
            return [NSString stringWithFormat:@"%C", NSDeleteCharacter];
        else if ([theString rangeOfString:@"F12"].location != NSNotFound)
            return [NSString stringWithFormat:@"%C", NSF12FunctionKey];
        else if ([theString rangeOfString:@"F11"].location != NSNotFound)
            return [NSString stringWithFormat:@"%C", NSF11FunctionKey];
        else if ([theString rangeOfString:@"F10"].location != NSNotFound)
            return [NSString stringWithFormat:@"%C", NSF10FunctionKey];
        else if ([theString rangeOfString:@"F9"].location != NSNotFound)
            return [NSString stringWithFormat:@"%C", NSF9FunctionKey];
        else if ([theString rangeOfString:@"F8"].location != NSNotFound)
            return [NSString stringWithFormat:@"%C", NSF8FunctionKey];
        else if ([theString rangeOfString:@"F7"].location != NSNotFound)
            return [NSString stringWithFormat:@"%C", NSF7FunctionKey];
        else if ([theString rangeOfString:@"F6"].location != NSNotFound)
            return [NSString stringWithFormat:@"%C", NSF6FunctionKey];
        else if ([theString rangeOfString:@"F5"].location != NSNotFound)
            return [NSString stringWithFormat:@"%C", NSF5FunctionKey];
        else if ([theString rangeOfString:@"F4"].location != NSNotFound)
            return [NSString stringWithFormat:@"%C", NSF4FunctionKey];
        else if ([theString rangeOfString:@"F3"].location != NSNotFound)
            return [NSString stringWithFormat:@"%C", NSF3FunctionKey];
        else if ([theString rangeOfString:@"F2"].location != NSNotFound)
            return [NSString stringWithFormat:@"%C", NSF2FunctionKey];
        else if ([theString rangeOfString:@"F1"].location != NSNotFound)
            return [NSString stringWithFormat:@"%C", NSF1FunctionKey];
        else if ([theString rangeOfString:@"Space"].location != NSNotFound)
            return @" ";
        /* note that we don't support esc here, since it is reserved for leaving fullscreen */
    }
#ifdef __clang__
#pragma GCC diagnostic warning "-Wformat"
#endif

    return theString;
}

unsigned int VLCModifiersToCocoa(char *theChar)
{
    unsigned int new = 0;

    if (strstr(theChar, "Command") != NULL)
        new |= NSCommandKeyMask;
    if (strstr(theChar, "Alt") != NULL)
        new |= NSAlternateKeyMask;
    if (strstr(theChar, "Shift") != NULL)
        new |= NSShiftKeyMask;
    if (strstr(theChar, "Ctrl") != NULL)
        new |= NSControlKeyMask;

    return new;
}
