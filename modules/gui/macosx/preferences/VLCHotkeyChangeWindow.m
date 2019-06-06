/*****************************************************************************
 * VLCHotkeyChangeWindow.m: Preferences Hotkey Window subclass for Mac OS X
 *****************************************************************************
 * Copyright (C) 2008-2014 VLC authors and VideoLAN
 *
 * Authors: Felix Paul Kühne <fkuehne at videolan dot org>
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

#import "VLCHotkeyChangeWindow.h"

#import "preferences/VLCSimplePrefsController.h"

@implementation VLCHotkeyChangeWindow

- (BOOL)acceptsFirstResponder
{
    return YES;
}

- (BOOL)becomeFirstResponder
{
    return YES;
}

- (BOOL)resignFirstResponder
{
    /* We need to stay the first responder or we'll miss the user's input */
    return NO;
}

- (BOOL)performKeyEquivalent:(NSEvent *)theEvent
{
    NSMutableString *tempString = [[NSMutableString alloc] init];
    NSString *keyString = [theEvent characters];

    unichar key = [keyString characterAtIndex:0];
    NSUInteger i_modifiers = [theEvent modifierFlags];

    /* modifiers */
    if (i_modifiers & NSCommandKeyMask)
        [tempString appendString:@"Command-"];
    if (i_modifiers & NSControlKeyMask)
        [tempString appendString:@"Ctrl-"];
    if (i_modifiers & NSShiftKeyMask)
        [tempString appendString:@"Shift-"];
    if (i_modifiers & NSAlternateKeyMask)
        [tempString appendString:@"Alt-"];

    /* non character keys */
    if (key == NSUpArrowFunctionKey)
        [tempString appendString:@"Up"];
    else if (key == NSDownArrowFunctionKey)
        [tempString appendString:@"Down"];
    else if (key == NSLeftArrowFunctionKey)
        [tempString appendString:@"Left"];
    else if (key == NSRightArrowFunctionKey)
        [tempString appendString:@"Right"];
    else if (key == NSF1FunctionKey)
        [tempString appendString:@"F1"];
    else if (key == NSF2FunctionKey)
        [tempString appendString:@"F2"];
    else if (key == NSF3FunctionKey)
        [tempString appendString:@"F3"];
    else if (key == NSF4FunctionKey)
        [tempString appendString:@"F4"];
    else if (key == NSF5FunctionKey)
        [tempString appendString:@"F5"];
    else if (key == NSF6FunctionKey)
        [tempString appendString:@"F6"];
    else if (key == NSF7FunctionKey)
        [tempString appendString:@"F7"];
    else if (key == NSF8FunctionKey)
        [tempString appendString:@"F8"];
    else if (key == NSF9FunctionKey)
        [tempString appendString:@"F9"];
    else if (key == NSF10FunctionKey)
        [tempString appendString:@"F10"];
    else if (key == NSF11FunctionKey)
        [tempString appendString:@"F11"];
    else if (key == NSF12FunctionKey)
        [tempString appendString:@"F12"];
    else if (key == NSInsertFunctionKey)
        [tempString appendString:@"Insert"];
    else if (key == NSHomeFunctionKey)
        [tempString appendString:@"Home"];
    else if (key == NSEndFunctionKey)
        [tempString appendString:@"End"];
    else if (key == NSPageUpFunctionKey)
        [tempString appendString:@"Page Up"];
    else if (key == NSPageDownFunctionKey)
        [tempString appendString:@"Page Down"];
    else if (key == NSMenuFunctionKey)
        [tempString appendString:@"Menu"];
    else if (key == NSTabCharacter)
        [tempString appendString:@"Tab"];
    else if (key == NSCarriageReturnCharacter)
        [tempString appendString:@"Enter"];
    else if (key == NSEnterCharacter)
        [tempString appendString:@"Enter"];
    else if (key == NSDeleteCharacter)
        [tempString appendString:@"Delete"];
    else if (key == NSBackspaceCharacter)
        [tempString appendString:@"Backspace"];
    else if (key == 0x001B)
        [tempString appendString:@"Esc"];
    else if (key == ' ')
        [tempString appendString:@"Space"];
    else if (![[[theEvent charactersIgnoringModifiers] lowercaseString] isEqualToString:@""]) //plain characters
        [tempString appendString:[[theEvent charactersIgnoringModifiers] lowercaseString]];
    else
        return NO;

    return [[[VLCMain sharedInstance] simplePreferences] changeHotkeyTo: tempString];
}

@end
