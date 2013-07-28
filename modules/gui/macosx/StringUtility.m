/*****************************************************************************
 * StringUtility.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2002-2012 VLC authors and VideoLAN
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

#import <vlc_input.h>
#import <vlc_keys.h>
#import <vlc_strings.h>

#import "StringUtility.h"
#import "intf.h"

@implementation VLCStringUtility

static VLCStringUtility *_o_sharedInstance = nil;

+ (VLCStringUtility *)sharedInstance
{
    return _o_sharedInstance ? _o_sharedInstance : [[self alloc] init];
}

- (id)init
{
    if (_o_sharedInstance)
        [self dealloc];
    else
        _o_sharedInstance = [super init];

    return _o_sharedInstance;
}

#pragma mark -
#pragma mark String utility

- (NSString *)localizedString:(const char *)psz
{
    NSString * o_str = nil;

    if (psz != NULL) {
        o_str = [NSString stringWithCString: _(psz) encoding:NSUTF8StringEncoding];

        if (o_str == NULL) {
            msg_Err(VLCIntf, "could not translate: %s", psz);
            return(@"");
        }
    } else {
        msg_Warn(VLCIntf, "can't translate empty strings");
        return(@"");
    }

    return(o_str);
}

/* i_width is in pixels */
- (NSString *)wrapString: (NSString *)o_in_string toWidth: (int) i_width
{
    NSMutableString *o_wrapped;
    NSString *o_out_string;
    NSRange glyphRange, effectiveRange, charRange;
    NSRect lineFragmentRect;
    unsigned glyphIndex, breaksInserted = 0;

    NSTextStorage *o_storage = [[NSTextStorage alloc] initWithString: o_in_string
                                                          attributes: [NSDictionary dictionaryWithObjectsAndKeys:
                                                                       [NSFont labelFontOfSize: 0.0], NSFontAttributeName, nil]];
    NSLayoutManager *o_layout_manager = [[NSLayoutManager alloc] init];
    NSTextContainer *o_container = [[NSTextContainer alloc]
                                    initWithContainerSize: NSMakeSize(i_width, 2000)];

    [o_layout_manager addTextContainer: o_container];
    [o_container release];
    [o_storage addLayoutManager: o_layout_manager];
    [o_layout_manager release];

    o_wrapped = [o_in_string mutableCopy];
    glyphRange = [o_layout_manager glyphRangeForTextContainer: o_container];

    for (glyphIndex = glyphRange.location ; glyphIndex < NSMaxRange(glyphRange) ;
        glyphIndex += effectiveRange.length) {
        lineFragmentRect = [o_layout_manager lineFragmentRectForGlyphAtIndex: glyphIndex
                                                              effectiveRange: &effectiveRange];
        charRange = [o_layout_manager characterRangeForGlyphRange: effectiveRange
                                                 actualGlyphRange: &effectiveRange];
        if ([o_wrapped lineRangeForRange:
            NSMakeRange(charRange.location + breaksInserted, charRange.length)].length > charRange.length) {
            [o_wrapped insertString: @"\n" atIndex: NSMaxRange(charRange) + breaksInserted];
            breaksInserted++;
        }
    }
    o_out_string = [NSString stringWithString: o_wrapped];
    [o_wrapped release];
    [o_storage release];

    return o_out_string;
}

- (NSString *)OSXStringKeyToString:(NSString *)theString
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

- (NSString *)getCurrentTimeAsString:(input_thread_t *)p_input negative:(BOOL)b_negative
{
    assert(p_input != nil);
    
    vlc_value_t time;
    char psz_time[MSTRTIME_MAX_SIZE];
    
    var_Get(p_input, "time", &time);
    
    mtime_t dur = input_item_GetDuration(input_GetItem(p_input));
    if (b_negative && dur > 0) {
        mtime_t remaining = 0;
        if (dur > time.i_time)
            remaining = dur - time.i_time;
        return [NSString stringWithFormat: @"-%s", secstotimestr(psz_time, (remaining / 1000000))];
    } else
        return [NSString stringWithUTF8String:secstotimestr(psz_time, (time.i_time / 1000000))];
}

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

- (unsigned int)VLCModifiersToCocoa:(NSString *)theString
{
    unsigned int new = 0;

    if ([theString rangeOfString:@"Command"].location != NSNotFound)
        new |= NSCommandKeyMask;
    if ([theString rangeOfString:@"Alt"].location != NSNotFound)
        new |= NSAlternateKeyMask;
    if ([theString rangeOfString:@"Shift"].location != NSNotFound)
        new |= NSShiftKeyMask;
    if ([theString rangeOfString:@"Ctrl"].location != NSNotFound)
        new |= NSControlKeyMask;
    return new;
}

- (NSString *)VLCKeyToString:(NSString *)theString
{
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
        if ([theString rangeOfString:@"Up"].location != NSNotFound)
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
        else if ([theString rangeOfString:@"Pageup"].location != NSNotFound)
            return [NSString stringWithFormat:@"%C", NSPageUpFunctionKey];
        else if ([theString rangeOfString:@"Pagedown"].location != NSNotFound)
            return [NSString stringWithFormat:@"%C", NSPageDownFunctionKey];
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

- (NSString *)b64Decode:(NSString *)string
{
    char *psz_decoded_string = vlc_b64_decode([string UTF8String]);
    if(!psz_decoded_string)
        return @"";

    NSString *returnStr = [NSString stringWithFormat:@"%s", psz_decoded_string];
    free(psz_decoded_string);

    return returnStr;
}

- (NSString *)b64EncodeAndFree:(char *)psz_string
{
    char *psz_encoded_string = vlc_b64_encode(psz_string);
    free(psz_string);
    if(!psz_encoded_string)
        return @"";

    NSString *returnStr = [NSString stringWithFormat:@"%s", psz_encoded_string];
    free(psz_encoded_string);

    return returnStr;
}


@end
