/*****************************************************************************
 * prefs_widgets.m: Preferences controls
 *****************************************************************************
 * Copyright (C) 2002-2003 VideoLAN
 * $Id$
 *
 * Authors: Derk-Jan Hartman <hartman at videolan.org>
 *          Jérôme Decoodt <djc at videolan.org>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <stdlib.h>                                      /* malloc(), free() */
#include <string.h>

#include <vlc/vlc.h>
#include "vlc_keys.h"

#include "intf.h"
#include "prefs_widgets.h"

#define PREFS_WRAP 300
#define OFFSET_RIGHT 20
#define OFFSET_BETWEEN 2

#define MACOS_VERSION [[[NSDictionary dictionaryWithContentsOfFile: \
            @"/System/Library/CoreServices/SystemVersion.plist"] \
            objectForKey: @"ProductVersion"] floatValue]

#define UPWARDS_WHITE_ARROW                 "\xE2\x87\xA7" 
#define OPTION_KEY                          "\xE2\x8C\xA5"
#define UP_ARROWHEAD                        "\xE2\x8C\x83"
#define PLACE_OF_INTEREST_SIGN              "\xE2\x8C\x98"

#define POPULATE_A_KEY( o_menu, string, value )                             \
{                                                                           \
    NSMenuItem *o_mi;                                                       \
/*  Normal */                                                               \
    o_mi = [[NSMenuItem alloc] initWithTitle:string                         \
        action:nil keyEquivalent:@""];                                      \
    [o_mi setKeyEquivalentModifierMask:                                     \
        0];                                                                 \
    [o_mi setAlternate: NO];                                                \
    [o_mi setTag:                                                           \
        ( value )];                                                         \
    [o_menu addItem: o_mi];                                                 \
if( MACOS_VERSION >= 10.3 )                                                 \
{                                                                           \
/*  Ctrl */                                                                 \
    o_mi = [[NSMenuItem alloc] initWithTitle:                               \
        [[NSString stringWithUTF8String:                                    \
            UP_ARROWHEAD                                                    \
        ] stringByAppendingString: string]                                  \
        action:nil keyEquivalent:@""];                                      \
    [o_mi setKeyEquivalentModifierMask:                                     \
        NSControlKeyMask];                                                  \
    [o_mi setAlternate: YES];                                               \
    [o_mi setTag:                                                           \
        KEY_MODIFIER_CTRL | ( value )];                                     \
    [o_menu addItem: o_mi];                                                 \
/* Ctrl+Alt */                                                              \
    o_mi = [[NSMenuItem alloc] initWithTitle:                               \
        [[NSString stringWithUTF8String:                                    \
            UP_ARROWHEAD OPTION_KEY                                         \
        ] stringByAppendingString: string]                                  \
        action:nil keyEquivalent:@""];                                      \
    [o_mi setKeyEquivalentModifierMask:                                     \
        NSControlKeyMask | NSAlternateKeyMask];                             \
    [o_mi setAlternate: YES];                                               \
    [o_mi setTag:                                                           \
        (KEY_MODIFIER_CTRL | KEY_MODIFIER_ALT) | ( value )];                \
    [o_menu addItem: o_mi];                                                 \
/* Ctrl+Shift */                                                            \
    o_mi = [[NSMenuItem alloc] initWithTitle:                               \
        [[NSString stringWithUTF8String:                                    \
            UP_ARROWHEAD UPWARDS_WHITE_ARROW                                \
        ] stringByAppendingString: string]                                  \
        action:nil keyEquivalent:@""];                                      \
    [o_mi setKeyEquivalentModifierMask:                                     \
       NSControlKeyMask | NSShiftKeyMask];                                  \
    [o_mi setAlternate: YES];                                               \
    [o_mi setTag:                                                           \
        (KEY_MODIFIER_CTRL | KEY_MODIFIER_SHIFT) | ( value )];              \
    [o_menu addItem: o_mi];                                                 \
/* Ctrl+Apple */                                                            \
    o_mi = [[NSMenuItem alloc] initWithTitle:                               \
        [[NSString stringWithUTF8String:                                    \
            UP_ARROWHEAD PLACE_OF_INTEREST_SIGN                             \
        ] stringByAppendingString: string]                                  \
        action:nil keyEquivalent:@""];                                      \
    [o_mi setKeyEquivalentModifierMask:                                     \
        NSControlKeyMask | NSCommandKeyMask];                               \
    [o_mi setAlternate: YES];                                               \
    [o_mi setTag:                                                           \
        (KEY_MODIFIER_CTRL | KEY_MODIFIER_COMMAND) | ( value )];            \
    [o_menu addItem: o_mi];                                                 \
/* Ctrl+Alt+Shift */                                                        \
    o_mi = [[NSMenuItem alloc] initWithTitle:                               \
        [[NSString stringWithUTF8String:                                    \
            UP_ARROWHEAD OPTION_KEY UPWARDS_WHITE_ARROW                     \
        ] stringByAppendingString: string]                                  \
        action:nil keyEquivalent:@""];                                      \
    [o_mi setKeyEquivalentModifierMask:                                     \
        NSControlKeyMask | NSAlternateKeyMask | NSShiftKeyMask];            \
    [o_mi setAlternate: YES];                                               \
    [o_mi setTag:                                                           \
        (KEY_MODIFIER_CTRL | KEY_MODIFIER_ALT | KEY_MODIFIER_SHIFT) |       \
             ( value )];                                                    \
    [o_menu addItem: o_mi];                                                 \
/* Ctrl+Alt+Apple */                                                        \
    o_mi = [[NSMenuItem alloc] initWithTitle:                               \
        [[NSString stringWithUTF8String:                                    \
            UP_ARROWHEAD OPTION_KEY PLACE_OF_INTEREST_SIGN                  \
        ] stringByAppendingString: string]                                  \
        action:nil keyEquivalent:@""];                                      \
    [o_mi setKeyEquivalentModifierMask:                                     \
        NSControlKeyMask | NSAlternateKeyMask | NSCommandKeyMask];          \
    [o_mi setAlternate: YES];                                               \
    [o_mi setTag:                                                           \
        (KEY_MODIFIER_CTRL | KEY_MODIFIER_ALT | KEY_MODIFIER_COMMAND) |     \
            ( value )];                                                     \
    [o_menu addItem: o_mi];                                                 \
/* Ctrl+Shift+Apple */                                                      \
    o_mi = [[NSMenuItem alloc] initWithTitle:                               \
        [[NSString stringWithUTF8String:                                    \
            UP_ARROWHEAD UPWARDS_WHITE_ARROW PLACE_OF_INTEREST_SIGN         \
        ] stringByAppendingString: string]                                  \
        action:nil keyEquivalent:@""];                                      \
    [o_mi setKeyEquivalentModifierMask:                                     \
        NSControlKeyMask | NSShiftKeyMask | NSCommandKeyMask];              \
    [o_mi setAlternate: YES];                                               \
    [o_mi setTag:                                                           \
        (KEY_MODIFIER_CTRL | KEY_MODIFIER_SHIFT | KEY_MODIFIER_COMMAND) |   \
            ( value )];                                                     \
    [o_menu addItem: o_mi];                                                 \
/* Ctrl+Alt+Shift+Apple */                                                  \
    o_mi = [[NSMenuItem alloc] initWithTitle:                               \
        [[NSString stringWithUTF8String:                                    \
            UP_ARROWHEAD OPTION_KEY UPWARDS_WHITE_ARROW                     \
                PLACE_OF_INTEREST_SIGN                                      \
        ] stringByAppendingString: string]                                  \
        action:nil keyEquivalent:@""];                                      \
    [o_mi setKeyEquivalentModifierMask:                                     \
        NSControlKeyMask | NSAlternateKeyMask | NSShiftKeyMask |            \
            NSCommandKeyMask];                                              \
    [o_mi setAlternate: YES];                                               \
    [o_mi setTag:                                                           \
        (KEY_MODIFIER_CTRL | KEY_MODIFIER_ALT | KEY_MODIFIER_SHIFT |        \
            KEY_MODIFIER_COMMAND) | ( value )];                             \
    [o_menu addItem: o_mi];                                                 \
/* Alt */                                                                   \
    o_mi = [[NSMenuItem alloc] initWithTitle:                               \
        [[NSString stringWithUTF8String:                                    \
            OPTION_KEY                                                      \
        ] stringByAppendingString: string]                                  \
        action:nil keyEquivalent:@""];                                      \
    [o_mi setKeyEquivalentModifierMask:                                     \
        NSAlternateKeyMask];                                                \
    [o_mi setAlternate: YES];                                               \
    [o_mi setTag:                                                           \
        KEY_MODIFIER_ALT | ( value )];                                      \
    [o_menu addItem: o_mi];                                                 \
/* Alt+Shift */                                                             \
    o_mi = [[NSMenuItem alloc] initWithTitle:                               \
        [[NSString stringWithUTF8String:                                    \
            OPTION_KEY UPWARDS_WHITE_ARROW                                  \
        ] stringByAppendingString: string]                                  \
        action:nil keyEquivalent:@""];                                      \
    [o_mi setKeyEquivalentModifierMask:                                     \
        NSAlternateKeyMask | NSShiftKeyMask];                               \
    [o_mi setAlternate: YES];                                               \
    [o_mi setTag:                                                           \
        (KEY_MODIFIER_ALT | KEY_MODIFIER_SHIFT) | ( value )];               \
    [o_menu addItem: o_mi];                                                 \
/* Alt+Apple */                                                             \
    o_mi = [[NSMenuItem alloc] initWithTitle:                               \
        [[NSString stringWithUTF8String:                                    \
            OPTION_KEY PLACE_OF_INTEREST_SIGN                               \
        ] stringByAppendingString: string]                                  \
        action:nil keyEquivalent:@""];                                      \
    [o_mi setKeyEquivalentModifierMask:                                     \
        NSAlternateKeyMask | NSCommandKeyMask];                             \
    [o_mi setAlternate: YES];                                               \
    [o_mi setTag:                                                           \
        (KEY_MODIFIER_ALT | KEY_MODIFIER_COMMAND) | ( value )];             \
    [o_menu addItem: o_mi];                                                 \
/* Alt+Shift+Apple */                                                       \
    o_mi = [[NSMenuItem alloc] initWithTitle:                               \
        [[NSString stringWithUTF8String:                                    \
            OPTION_KEY UPWARDS_WHITE_ARROW PLACE_OF_INTEREST_SIGN           \
        ] stringByAppendingString: string]                                  \
        action:nil keyEquivalent:@""];                                      \
    [o_mi setKeyEquivalentModifierMask:                                     \
        NSAlternateKeyMask | NSShiftKeyMask | NSCommandKeyMask];            \
    [o_mi setAlternate: YES];                                               \
    [o_mi setTag:                                                           \
        (KEY_MODIFIER_ALT | KEY_MODIFIER_SHIFT | KEY_MODIFIER_COMMAND) |    \
            ( value )];                                                     \
    [o_menu addItem: o_mi];                                                 \
/* Shift */                                                                 \
    o_mi = [[NSMenuItem alloc] initWithTitle:                               \
        [[NSString stringWithUTF8String:                                    \
            UPWARDS_WHITE_ARROW                                             \
        ] stringByAppendingString: string]                                  \
        action:nil keyEquivalent:@""];                                      \
    [o_mi setKeyEquivalentModifierMask:                                     \
        NSShiftKeyMask];                                                    \
    [o_mi setAlternate: YES];                                               \
    [o_mi setTag:                                                           \
        KEY_MODIFIER_SHIFT | ( value )];                                    \
    [o_menu addItem: o_mi];                                                 \
/* Shift+Apple */                                                           \
    o_mi = [[NSMenuItem alloc] initWithTitle:                               \
        [[NSString stringWithUTF8String:                                    \
            UPWARDS_WHITE_ARROW PLACE_OF_INTEREST_SIGN                      \
        ] stringByAppendingString: string]                                  \
        action:nil keyEquivalent:@""];                                      \
    [o_mi setKeyEquivalentModifierMask:                                     \
        NSShiftKeyMask | NSCommandKeyMask];                                 \
    [o_mi setAlternate: YES];                                               \
    [o_mi setTag:                                                           \
        (KEY_MODIFIER_SHIFT | KEY_MODIFIER_COMMAND) | ( value )];           \
    [o_menu addItem: o_mi];                                                 \
/* Apple */                                                                 \
    o_mi = [[NSMenuItem alloc] initWithTitle:                               \
        [[NSString stringWithUTF8String:                                    \
        PLACE_OF_INTEREST_SIGN                                              \
        ] stringByAppendingString: string]                                  \
        action:nil keyEquivalent:@""];                                      \
    [o_mi setKeyEquivalentModifierMask:                                     \
        NSCommandKeyMask];                                                  \
    [o_mi setAlternate: YES];                                               \
    [o_mi setTag:                                                           \
        KEY_MODIFIER_COMMAND | ( value )];                                  \
    [o_menu addItem: o_mi];                                                 \
}                                                                           \
}

#define ADD_LABEL( o_label, superFrame, x_offset, my_y_offset, label )      \
{                                                                           \
    NSRect s_rc = superFrame;                                               \
    s_rc.size.height = 17;                                                  \
    s_rc.origin.x = x_offset - 3;                                           \
    s_rc.origin.y = superFrame.size.height - 17 + my_y_offset;              \
    o_label = [[[NSTextField alloc] initWithFrame: s_rc] retain];           \
    [o_label setDrawsBackground: NO];                                       \
    [o_label setBordered: NO];                                              \
    [o_label setEditable: NO];                                              \
    [o_label setSelectable: NO];                                            \
    [o_label setStringValue: label];                                        \
    [o_label setFont:[NSFont systemFontOfSize:0]];                          \
    [o_label sizeToFit];                                                    \
}

#define ADD_TEXTFIELD( o_textfield, superFrame, x_offset, my_y_offset,      \
    my_width, tooltip, init_value )                                         \
{                                                                           \
    NSRect s_rc = superFrame;                                               \
    s_rc.origin.x = x_offset;                                               \
    s_rc.origin.y = my_y_offset;                                            \
    s_rc.size.height = 22;                                                  \
    s_rc.size.width = my_width;                                             \
    o_textfield = [[[NSTextField alloc] initWithFrame: s_rc] retain];       \
    [o_textfield setFont:[NSFont systemFontOfSize:0]];                      \
    [o_textfield setToolTip: tooltip];                                      \
    [o_textfield setStringValue: init_value];                               \
}

#define ADD_COMBO( o_combo, superFrame, x_offset, my_y_offset, x2_offset,   \
    tooltip )                                                               \
{                                                                           \
    NSRect s_rc = superFrame;                                               \
    s_rc.origin.x = x_offset + 2;                                           \
    s_rc.origin.y = my_y_offset;                                            \
    s_rc.size.height = 26;                                                  \
    s_rc.size.width = superFrame.size.width + 2 - s_rc.origin.x -           \
        (x2_offset);                                                        \
    o_combo = [[[NSComboBox alloc] initWithFrame: s_rc] retain];            \
    [o_combo setFont:[NSFont systemFontOfSize:0]];                          \
    [o_combo setToolTip: tooltip];                                          \
    [o_combo setUsesDataSource:TRUE];                                       \
    [o_combo setDataSource:self];                                           \
    [o_combo setNumberOfVisibleItems:10];                                   \
    [o_combo setCompletes:YES];                                             \
}

#define ADD_RIGHT_BUTTON( o_button, superFrame, x_offset, my_y_offset,      \
    tooltip, title )                                                        \
{                                                                           \
    NSRect s_rc = superFrame;                                               \
    o_button = [[[NSButton alloc] initWithFrame: s_rc] retain];             \
    [o_button setButtonType: NSMomentaryPushInButton];                      \
    [o_button setBezelStyle: NSRoundedBezelStyle];                          \
    [o_button setTitle: title];                                             \
    [o_button setFont:[NSFont systemFontOfSize:0]];                         \
    [o_button sizeToFit];                                                   \
    s_rc = [o_button frame];                                                \
    s_rc.origin.x = superFrame.size.width - [o_button frame].size.width - 6;\
    s_rc.origin.y = my_y_offset - 6;                                        \
    s_rc.size.width += 12;                                                  \
    [o_button setFrame: s_rc];                                              \
    [o_button setToolTip: tooltip];                                         \
    [o_button setTarget: self];                                             \
    [o_button setAction: @selector(openFileDialog:)];                       \
}

#define ADD_POPUP( o_popup, superFrame, x_offset, my_y_offset, x2_offset,   \
    tooltip )                                                               \
{                                                                           \
    NSRect s_rc = superFrame;                                               \
    s_rc.origin.x = x_offset - 1;                                           \
    s_rc.origin.y = my_y_offset;                                            \
    s_rc.size.height = 26;                                                  \
    s_rc.size.width = superFrame.size.width + 2 - s_rc.origin.x -           \
        (x2_offset);                                                        \
    o_popup = [[[NSPopUpButton alloc] initWithFrame: s_rc] retain];         \
    [o_popup setFont:[NSFont systemFontOfSize:0]];                          \
    [o_popup setToolTip: tooltip];                                          \
}

#define ADD_STEPPER( o_stepper, superFrame, x_offset, my_y_offset, tooltip, \
    lower, higher )                                                         \
{                                                                           \
    NSRect s_rc = superFrame;                                               \
    s_rc.origin.x = x_offset;                                               \
    s_rc.origin.y = my_y_offset;                                            \
    s_rc.size.height = 23;                                                  \
    s_rc.size.width = 23;                                                   \
    o_stepper = [[[NSStepper alloc] initWithFrame: s_rc] retain];           \
    [o_stepper setFont:[NSFont systemFontOfSize:0]];                        \
    [o_stepper setToolTip: tooltip];                                        \
    [o_stepper setMaxValue: higher];                                        \
    [o_stepper setMinValue: lower];                                         \
    [o_stepper setTarget: self];                                            \
    [o_stepper setAction: @selector(stepperChanged:)];                      \
    [o_stepper sendActionOn:NSLeftMouseUpMask | NSLeftMouseDownMask |       \
        NSLeftMouseDraggedMask];                                            \
}

#define ADD_SLIDER( o_slider, superFrame, x_offset, my_y_offset, my_width,  \
    tooltip, lower, higher )                                                \
{                                                                           \
    NSRect s_rc = superFrame;                                               \
    s_rc.origin.x = x_offset;                                               \
    s_rc.origin.y = my_y_offset;                                            \
    s_rc.size.height = 21;                                                  \
    s_rc.size.width = my_width;                                             \
    o_slider = [[[NSSlider alloc] initWithFrame: s_rc] retain];             \
    [o_slider setFont:[NSFont systemFontOfSize:0]];                         \
    [o_slider setToolTip: tooltip];                                         \
    [o_slider setMaxValue: higher];                                         \
    [o_slider setMinValue: lower];                                          \
}

#define ADD_CHECKBOX( o_checkbox, superFrame, x_offset, my_y_offset, label, \
    tooltip, init_value, position )                                         \
{                                                                           \
    NSRect s_rc = superFrame;                                               \
    s_rc.size.height = 18;                                                  \
    s_rc.origin.x = x_offset - 2;                                           \
    s_rc.origin.y = superFrame.size.height - 18 + my_y_offset;              \
    o_checkbox = [[[NSButton alloc] initWithFrame: s_rc] retain];           \
    [o_checkbox setFont:[NSFont systemFontOfSize:0]];                       \
    [o_checkbox setButtonType: NSSwitchButton];                             \
    [o_checkbox setImagePosition: position];                                \
    [o_checkbox setIntValue: init_value];                                   \
    [o_checkbox setTitle: label];                                           \
    [o_checkbox setToolTip: tooltip];                                       \
    [o_checkbox sizeToFit];                                                 \
}

@implementation VLCConfigControl
- (id)initWithFrame: (NSRect)frame
{
    return [self initWithFrame: frame
                    item: nil];
}

- (id)initWithFrame: (NSRect)frame
        item: (module_config_t *)_p_item
{
    self = [super initWithFrame: frame];

    if( self != nil )
    {
        p_item = _p_item;
        psz_name = strdup( p_item->psz_name );
        o_label = NULL;
        i_type = p_item->i_type;
        i_view_type = 0;
        b_advanced = p_item->b_advanced;
        [self setAutoresizingMask:NSViewWidthSizable | NSViewMinYMargin ];
    }
    return (self);
}

- (void)setYPos:(int)i_yPos
{
    NSRect frame = [self frame];
    frame.origin.y = i_yPos;
    [self setFrame:frame];
}

- (void)dealloc
{
    if( o_label ) [o_label release];
    if( psz_name ) free( psz_name );
    [super dealloc];
}

+ (int)calcVerticalMargin: (int)i_curItem lastItem: (int)i_lastItem
{
    int i_margin;
    switch( i_curItem )
    {
    case CONFIG_ITEM_STRING:
        switch( i_lastItem )
        {
        case CONFIG_ITEM_STRING:
            i_margin = 8;
            break;
        case CONFIG_ITEM_STRING_LIST:
            i_margin = 7;
            break;
        case CONFIG_ITEM_FILE:
            i_margin = 8;
            break;
        case CONFIG_ITEM_MODULE:
            i_margin = 4;
            break;
        case CONFIG_ITEM_INTEGER:
            i_margin = 7;
            break;
        case CONFIG_ITEM_RANGED_INTEGER:
            i_margin = 5;
            break;
        case CONFIG_ITEM_BOOL:
            i_margin = 7;
            break;
        case CONFIG_ITEM_KEY_BEFORE_10_3:
            i_margin = 7;
            break;
        case CONFIG_ITEM_KEY_AFTER_10_3:
            i_margin = 6;
            break;
        case CONFIG_ITEM_MODULE_LIST:
            i_margin = 8;
            break;
        default:
            i_margin = 20;
            break;
        }
        break;
    case CONFIG_ITEM_STRING_LIST:
        switch( i_lastItem )
        {
        case CONFIG_ITEM_STRING:
            i_margin = 8;
            break;
        case CONFIG_ITEM_STRING_LIST:
            i_margin = 7;
            break;
        case CONFIG_ITEM_FILE:
            i_margin = 6;
            break;
        case CONFIG_ITEM_MODULE:
            i_margin = 4;
            break;
        case CONFIG_ITEM_INTEGER:
            i_margin = 7;
            break;
        case CONFIG_ITEM_RANGED_INTEGER:
            i_margin = 5;
            break;
        case CONFIG_ITEM_BOOL:
            i_margin = 7;
            break;
        case CONFIG_ITEM_KEY_BEFORE_10_3:
            i_margin = 7;
            break;
        case CONFIG_ITEM_KEY_AFTER_10_3:
            i_margin = 6;
            break;
        case CONFIG_ITEM_MODULE_LIST:
            i_margin = 8;
            break;
        default:
            i_margin = 20;
            break;
        }
        break;
    case CONFIG_ITEM_FILE:
        switch( i_lastItem )
        {
        case CONFIG_ITEM_STRING:
            i_margin = 13;
            break;
        case CONFIG_ITEM_STRING_LIST:
            i_margin = 10;
            break;
        case CONFIG_ITEM_FILE:
            i_margin = 9;
            break;
        case CONFIG_ITEM_MODULE:
            i_margin = 9;
            break;
        case CONFIG_ITEM_INTEGER:
            i_margin = 10;
            break;
        case CONFIG_ITEM_RANGED_INTEGER:
            i_margin = 8;
            break;
        case CONFIG_ITEM_BOOL:
            i_margin = 10;
            break;
        case CONFIG_ITEM_KEY_BEFORE_10_3:
            i_margin = 10;
            break;
        case CONFIG_ITEM_KEY_AFTER_10_3:
            i_margin = 9;
            break;
        case CONFIG_ITEM_MODULE_LIST:
            i_margin = 11;
            break;
        default:
            i_margin = 23;
            break;
        }
        break;
    case CONFIG_ITEM_MODULE:
        switch( i_lastItem )
        {
        case CONFIG_ITEM_STRING:
            i_margin = 8;
            break;
        case CONFIG_ITEM_STRING_LIST:
            i_margin = 7;
            break;
        case CONFIG_ITEM_FILE:
            i_margin = 6;
            break;
        case CONFIG_ITEM_MODULE:
            i_margin = 5;
            break;
        case CONFIG_ITEM_INTEGER:
            i_margin = 7;
            break;
        case CONFIG_ITEM_RANGED_INTEGER:
            i_margin = 6;
            break;
        case CONFIG_ITEM_BOOL:
            i_margin = 8;
            break;
        case CONFIG_ITEM_KEY_BEFORE_10_3:
            i_margin = 8;
            break;
        case CONFIG_ITEM_KEY_AFTER_10_3:
            i_margin = 7;
            break;
        case CONFIG_ITEM_MODULE_LIST:
            i_margin = 9;
            break;
        default:
            i_margin = 20;
            break;
        }
        break;
    case CONFIG_ITEM_INTEGER:
        switch( i_lastItem )
        {
        case CONFIG_ITEM_STRING:
            i_margin = 8;
            break;
        case CONFIG_ITEM_STRING_LIST:
            i_margin = 7;
            break;
        case CONFIG_ITEM_FILE:
            i_margin = 6;
            break;
        case CONFIG_ITEM_MODULE:
            i_margin = 4;
            break;
        case CONFIG_ITEM_INTEGER:
            i_margin = 7;
            break;
        case CONFIG_ITEM_RANGED_INTEGER:
            i_margin = 5;
            break;
        case CONFIG_ITEM_BOOL:
            i_margin = 7;
            break;
        case CONFIG_ITEM_KEY_BEFORE_10_3:
            i_margin = 7;
            break;
        case CONFIG_ITEM_KEY_AFTER_10_3:
            i_margin = 6;
            break;
        case CONFIG_ITEM_MODULE_LIST:
            i_margin = 8;
            break;
        default:
            i_margin = 20;
            break;
        }
        break;
    case CONFIG_ITEM_RANGED_INTEGER:
        switch( i_lastItem )
        {
        case CONFIG_ITEM_STRING:
            i_margin = 8;
            break;
        case CONFIG_ITEM_STRING_LIST:
            i_margin = 7;
            break;
        case CONFIG_ITEM_FILE:
            i_margin = 8;
            break;
        case CONFIG_ITEM_MODULE:
            i_margin = 4;
            break;
        case CONFIG_ITEM_INTEGER:
            i_margin = 7;
            break;
        case CONFIG_ITEM_RANGED_INTEGER:
            i_margin = 5;
            break;
        case CONFIG_ITEM_BOOL:
            i_margin = 7;
            break;
        case CONFIG_ITEM_KEY_BEFORE_10_3:
            i_margin = 7;
            break;
        case CONFIG_ITEM_KEY_AFTER_10_3:
            i_margin = 6;
            break;
        case CONFIG_ITEM_MODULE_LIST:
            i_margin = 8;
            break;
        default:
            i_margin = 20;
            break;
        }
        break;
    case CONFIG_ITEM_BOOL:
        switch( i_lastItem )
        {
        case CONFIG_ITEM_STRING:
            i_margin = 10;
            break;
        case CONFIG_ITEM_STRING_LIST:
            i_margin = 9;
            break;
        case CONFIG_ITEM_FILE:
            i_margin = 8;
            break;
        case CONFIG_ITEM_MODULE:
            i_margin = 6;
            break;
        case CONFIG_ITEM_INTEGER:
            i_margin = 9;
            break;
        case CONFIG_ITEM_RANGED_INTEGER:
            i_margin = 7;
            break;
        case CONFIG_ITEM_BOOL:
            i_margin = 7;
            break;
        case CONFIG_ITEM_KEY_BEFORE_10_3:
            i_margin = 7;
            break;
        case CONFIG_ITEM_KEY_AFTER_10_3:
            i_margin = 5;
            break;
        case CONFIG_ITEM_MODULE_LIST:
            i_margin = 10;
            break;
        default:
            i_margin = 20;
            break;
        }
        break;
    case CONFIG_ITEM_KEY_BEFORE_10_3:
        switch( i_lastItem )
        {
        case CONFIG_ITEM_STRING:
            i_margin = 6;
            break;
        case CONFIG_ITEM_STRING_LIST:
            i_margin = 5;
            break;
        case CONFIG_ITEM_FILE:
            i_margin = 4;
            break;
        case CONFIG_ITEM_MODULE:
            i_margin = 2;
            break;
        case CONFIG_ITEM_INTEGER:
            i_margin = 5;
            break;
        case CONFIG_ITEM_RANGED_INTEGER:
            i_margin = 3;
            break;
        case CONFIG_ITEM_BOOL:
            i_margin = 3;
            break;
        case CONFIG_ITEM_KEY_BEFORE_10_3:
            i_margin = 10;
            break;
        case CONFIG_ITEM_KEY_AFTER_10_3:
            i_margin = 6;
            break;
        case CONFIG_ITEM_MODULE_LIST:
            i_margin = 6;
            break;
        default:
            i_margin = 18;
            break;
        }
        break;
    case CONFIG_ITEM_KEY_AFTER_10_3:
        switch( i_lastItem )
        {
        case CONFIG_ITEM_STRING:
            i_margin = 8;
            break;
        case CONFIG_ITEM_STRING_LIST:
            i_margin = 7;
            break;
        case CONFIG_ITEM_FILE:
            i_margin = 6;
            break;
        case CONFIG_ITEM_MODULE:
            i_margin = 6;
            break;
        case CONFIG_ITEM_INTEGER:
            i_margin = 7;
            break;
        case CONFIG_ITEM_RANGED_INTEGER:
            i_margin = 5;
            break;
        case CONFIG_ITEM_BOOL:
            i_margin = 7;
            break;
        case CONFIG_ITEM_KEY_BEFORE_10_3:
            i_margin = 7;
            break;
        case CONFIG_ITEM_KEY_AFTER_10_3:
            i_margin = 8;
            break;
        case CONFIG_ITEM_MODULE_LIST:
            i_margin = 10;
            break;
        default:
            i_margin = 20;
            break;
        }
        break;
    case CONFIG_ITEM_MODULE_LIST:
        switch( i_lastItem )
        {
        case CONFIG_ITEM_STRING:
            i_margin = 10;
            break;
        case CONFIG_ITEM_STRING_LIST:
            i_margin = 7;
            break;
        case CONFIG_ITEM_FILE:
            i_margin = 6;
            break;
        case CONFIG_ITEM_MODULE:
            i_margin = 6;
            break;
        case CONFIG_ITEM_INTEGER:
            i_margin = 9;
            break;
        case CONFIG_ITEM_RANGED_INTEGER:
            i_margin = 5;
            break;
        case CONFIG_ITEM_BOOL:
            i_margin = 7;
            break;
        case CONFIG_ITEM_KEY_BEFORE_10_3:
            i_margin = 7;
            break;
        case CONFIG_ITEM_KEY_AFTER_10_3:
            i_margin = 5;
            break;
        case CONFIG_ITEM_MODULE_LIST:
            i_margin = 8;
            break;
        default:
            i_margin = 20;
            break;
        }
        break;
    default:
        i_margin = 20;
        break;
    }
    return i_margin;
}

+ (VLCConfigControl *)newControl: (module_config_t *)_p_item
                      withView: (NSView *)o_parent_view
{
    VLCConfigControl *p_control = NULL;
    switch( _p_item->i_type )
    {
    case CONFIG_ITEM_STRING:
        if( !_p_item->i_list )
        {
            p_control = [[StringConfigControl alloc]
                    initWithItem: _p_item
                    withView: o_parent_view];
        }
        else
        {
            p_control = [[StringListConfigControl alloc]
                    initWithItem: _p_item
                    withView: o_parent_view];
        }
        break;
    case CONFIG_ITEM_FILE:
    case CONFIG_ITEM_DIRECTORY:
        p_control = [[FileConfigControl alloc]
                    initWithItem: _p_item
                    withView: o_parent_view];
        break;
    case CONFIG_ITEM_MODULE:
    case CONFIG_ITEM_MODULE_CAT:
        p_control = [[ModuleConfigControl alloc]
                    initWithItem: _p_item
                    withView: o_parent_view];
        break;
    case CONFIG_ITEM_INTEGER:
        if( _p_item->i_list )
        {
            p_control = [[IntegerListConfigControl alloc]
                        initWithItem: _p_item
                        withView: o_parent_view];
        }
        else if( _p_item->i_min != 0 || _p_item->i_max != 0 )
        {
            p_control = [[RangedIntegerConfigControl alloc]
                        initWithItem: _p_item
                        withView: o_parent_view];
        }
        else
        {
            p_control = [[IntegerConfigControl alloc]
                        initWithItem: _p_item
                        withView: o_parent_view];
        }
        break;
    case CONFIG_ITEM_BOOL:
        p_control = [[BoolConfigControl alloc]
                    initWithItem: _p_item
                    withView: o_parent_view];
        break;
    case CONFIG_ITEM_FLOAT:
        if( _p_item->f_min != 0 || _p_item->f_max != 0 )
        {
            p_control = [[RangedFloatConfigControl alloc]
                        initWithItem: _p_item
                        withView: o_parent_view];
        }
        else
        {
            p_control = [[FloatConfigControl alloc]
                        initWithItem: _p_item
                        withView: o_parent_view];
        }
        break;
    case CONFIG_ITEM_KEY:
        if( MACOS_VERSION < 10.3 )
        {
            p_control = [[KeyConfigControlBefore103 alloc]
                        initWithItem: _p_item
                        withView: o_parent_view];
        }
        else
        {
            p_control = [[KeyConfigControlAfter103 alloc]
                        initWithItem: _p_item
                        withView: o_parent_view];
        }
        break;
    case CONFIG_ITEM_MODULE_LIST:
    case CONFIG_ITEM_MODULE_LIST_CAT:
        p_control = [[ModuleListConfigControl alloc]
                    initWithItem: _p_item
                    withView: o_parent_view];
        break;
    default:
        break;
    }
    return p_control;
}

- (NSString *)getName
{
    return [[VLCMain sharedInstance] localizedString: psz_name];
}

- (int)getType
{
    return i_type;
}

- (int)getViewType
{
    return i_view_type;
}

- (BOOL)isAdvanced
{
    return b_advanced;
}

- (int)intValue
{
    return 0;
}

- (float)floatValue
{
    return 0;
}

- (char *)stringValue
{
    return NULL;
}

- (void)applyChanges
{
    vlc_value_t val;
    switch( p_item->i_type )
    {
    case CONFIG_ITEM_STRING:
    case CONFIG_ITEM_FILE:
    case CONFIG_ITEM_DIRECTORY:
    case CONFIG_ITEM_MODULE:
    case CONFIG_ITEM_MODULE_LIST:
    case CONFIG_ITEM_MODULE_LIST_CAT:
fprintf( stderr, "Applying %s to %s\n" , [self stringValue], psz_name );
        config_PutPsz( VLCIntf, psz_name, [self stringValue] );
        break;
    case CONFIG_ITEM_KEY:
        /* So you don't need to restart to have the changes take effect */
fprintf( stderr, "Applying %d to %s\n" , [self intValue], psz_name );
        val.i_int = [self intValue];
        var_Set( VLCIntf->p_vlc, psz_name, val );
    case CONFIG_ITEM_INTEGER:
    case CONFIG_ITEM_BOOL:
fprintf( stderr, "Applying %d to %s\n" , [self intValue], psz_name );
        config_PutInt( VLCIntf, psz_name, [self intValue] );
        break;
    case CONFIG_ITEM_FLOAT:
fprintf( stderr, "Applying %f to %s\n" , [self floatValue], psz_name );
        config_PutFloat( VLCIntf, psz_name, [self floatValue] );
        break;
    }
}

- (int)getLabelSize
{
    return [o_label frame].size.width;
}
@end

@implementation StringConfigControl
- (id) initWithItem: (module_config_t *)_p_item
           withView: (NSView *)o_parent_view
{
    NSRect mainFrame = [o_parent_view frame];
    NSString *o_labelString, *o_textfieldString, *o_textfieldTooltip;
    mainFrame.size.height = 22;
    mainFrame.size.width = mainFrame.size.width - LEFTMARGIN - RIGHTMARGIN;
    mainFrame.origin.x = LEFTMARGIN;
    mainFrame.origin.y = 0;

    if( [super initWithFrame: mainFrame item: _p_item] != nil )
    {
        i_view_type = CONFIG_ITEM_STRING;
        /* add the label */
        if( p_item->psz_text )
            o_labelString = [[VLCMain sharedInstance]
                                localizedString: p_item->psz_text];
        else
            o_labelString = [NSString stringWithString:@""];
        ADD_LABEL( o_label, mainFrame, 0, -3, o_labelString )
        [o_label setAutoresizingMask:NSViewNotSizable ];
        [self addSubview: o_label];

        /* build the textfield */
        o_textfieldTooltip = [[VLCMain sharedInstance] wrapString:
            [[VLCMain sharedInstance] localizedString: p_item->psz_longtext]
                                         toWidth: PREFS_WRAP];
        if( p_item->psz_value )
            o_textfieldString = [[VLCMain sharedInstance]
                                    localizedString: p_item->psz_value];
        else
            o_textfieldString = [NSString stringWithString: @""];
        ADD_TEXTFIELD( o_textfield, mainFrame, [o_label frame].size.width + 2,
                        0, mainFrame.size.width - [o_label frame].size.width -
                        2, o_textfieldTooltip, o_textfieldString )
        [o_textfield setAutoresizingMask:NSViewWidthSizable ];
        [self addSubview: o_textfield];
    }
    return self;
}

- (void) alignWithXPosition:(int)i_xPos
{
    NSRect frame;
    NSRect superFrame = [self frame];
    frame = [o_label frame];
    frame.origin.x = i_xPos - frame.size.width - 3;
    [o_label setFrame:frame];

    frame = [o_textfield frame];
    frame.origin.x = i_xPos + 2;
    frame.size.width = superFrame.size.width - frame.origin.x - 1;
    [o_textfield setFrame:frame];
}

- (void)dealloc
{
    [o_textfield release];
    [super dealloc];
}

- (char *)stringValue
{
    return strdup( [[VLCMain sharedInstance] delocalizeString:
                        [o_textfield stringValue]] );
}
@end

@implementation StringListConfigControl
- (id) initWithItem: (module_config_t *)_p_item
           withView: (NSView *)o_parent_view
{
    NSRect mainFrame = [o_parent_view frame];
    NSString *o_labelString, *o_textfieldTooltip;
    mainFrame.size.height = 22;
    mainFrame.size.width = mainFrame.size.width - LEFTMARGIN - RIGHTMARGIN + 1;
    mainFrame.origin.x = LEFTMARGIN;
    mainFrame.origin.y = 0;

    if( [super initWithFrame: mainFrame item: _p_item] != nil )
    {
        int i_index;
        i_view_type = CONFIG_ITEM_STRING_LIST;
        /* add the label */
        if( p_item->psz_text )
            o_labelString = [[VLCMain sharedInstance]
                                localizedString: p_item->psz_text];
        else
            o_labelString = [NSString stringWithString:@""];
        ADD_LABEL( o_label, mainFrame, 0, -3, o_labelString )
        [o_label setAutoresizingMask:NSViewNotSizable ];
        [self addSubview: o_label];

        /* build the textfield */
        o_textfieldTooltip = [[VLCMain sharedInstance] wrapString:
            [[VLCMain sharedInstance]
                localizedString: p_item->psz_longtext ] toWidth: PREFS_WRAP];
        ADD_COMBO( o_combo, mainFrame, [o_label frame].size.width,
            -2, 0, o_textfieldTooltip )
        [o_combo setAutoresizingMask:NSViewWidthSizable ];
        for( i_index = 0; i_index < p_item->i_list; i_index++ )
            if( p_item->psz_value &&
                !strcmp( p_item->psz_value, p_item->ppsz_list[i_index] ) )
                [o_combo selectItemAtIndex: i_index];
        [self addSubview: o_combo];
    }
    return self;
}

- (void) alignWithXPosition:(int)i_xPos
{
    NSRect frame;
    NSRect superFrame = [self frame];
    frame = [o_label frame];
    frame.origin.x = i_xPos - frame.size.width - 3;
    [o_label setFrame:frame];

    frame = [o_combo frame];
    frame.origin.x = i_xPos + 2;
    frame.size.width = superFrame.size.width - frame.origin.x + 2;
    [o_combo setFrame:frame];
}

- (void)dealloc
{
    [o_combo release];
    [super dealloc];
}

- (char *)stringValue
{
    if( [o_combo indexOfSelectedItem] >= 0 )
        return strdup( p_item->ppsz_list[[o_combo indexOfSelectedItem]] );
    else
        return strdup( [[VLCMain sharedInstance]
                            delocalizeString: [o_combo stringValue]] );
}
@end

@implementation StringListConfigControl (NSComboBoxDataSource)
- (int)numberOfItemsInComboBox:(NSComboBox *)aComboBox
{
        return p_item->i_list;
}

- (id)comboBox:(NSComboBox *)aComboBox objectValueForItemAtIndex:(int)i_index
{
    if( p_item->ppsz_list_text && p_item->ppsz_list_text[i_index] )
    {
        return [[VLCMain sharedInstance]
                    localizedString: p_item->ppsz_list_text[i_index]];
    } else return [[VLCMain sharedInstance]
                    localizedString: p_item->ppsz_list[i_index]];
}
@end

@implementation FileConfigControl
- (id) initWithItem: (module_config_t *)_p_item
           withView: (NSView *)o_parent_view
{
    NSRect mainFrame = [o_parent_view frame];
    NSString *o_labelString, *o_buttonTooltip, *o_textfieldString;
    NSString *o_textfieldTooltip;
    mainFrame.size.height = 46;
    mainFrame.size.width = mainFrame.size.width - LEFTMARGIN - RIGHTMARGIN;
    mainFrame.origin.x = LEFTMARGIN;
    mainFrame.origin.y = 0;

    if( [super initWithFrame: mainFrame item: _p_item] != nil )
    {
        i_view_type = CONFIG_ITEM_FILE;

        /* is it a directory */
        b_directory = ( [self getType] == CONFIG_ITEM_DIRECTORY ) ? YES : NO;

        /* add the label */
        if( p_item->psz_text )
            o_labelString = [[VLCMain sharedInstance]
                                localizedString: p_item->psz_text];
        else
            o_labelString = [NSString stringWithString:@""];
        ADD_LABEL( o_label, mainFrame, 0, 3, o_labelString )
        [o_label setAutoresizingMask:NSViewNotSizable ];
        [self addSubview: o_label];

        /* build the button */
        o_buttonTooltip = [[VLCMain sharedInstance]
                wrapString: [[VLCMain sharedInstance]
                localizedString: p_item->psz_longtext ] toWidth: PREFS_WRAP];
        ADD_RIGHT_BUTTON( o_button, mainFrame, 0, 0, o_buttonTooltip,
                            _NS("Browse...") )
        [o_button setAutoresizingMask:NSViewMinXMargin ];
        [self addSubview: o_button];

        /* build the textfield */
        o_textfieldTooltip = [[VLCMain sharedInstance] wrapString:
            [[VLCMain sharedInstance]
                localizedString: p_item->psz_longtext ] toWidth: PREFS_WRAP];
        if( p_item->psz_value )
            o_textfieldString = [[VLCMain sharedInstance]
                                    localizedString: p_item->psz_value];
        else
            o_textfieldString = [NSString stringWithString: @""];
        ADD_TEXTFIELD( o_textfield, mainFrame, 12, 2, mainFrame.size.width -
                        8 - [o_button frame].size.width,
                        o_textfieldTooltip, o_textfieldString )
        [o_textfield setAutoresizingMask:NSViewWidthSizable ];
        [self addSubview: o_textfield];
    }
    return self;
}

- (void) alignWithXPosition:(int)i_xPos
{
    ;
}

- (void)dealloc
{
    [o_textfield release];
    [o_button release];
    [super dealloc];
}

- (IBAction)openFileDialog: (id)sender
{
    NSOpenPanel *o_open_panel = [NSOpenPanel openPanel];

    [o_open_panel setTitle: (b_directory)?
        _NS("Select a directory"):_NS("Select a file")];
    [o_open_panel setPrompt: _NS("Select")];
    [o_open_panel setAllowsMultipleSelection: NO];
    [o_open_panel setCanChooseFiles: !b_directory];
    [o_open_panel setCanChooseDirectories: b_directory];
    [o_open_panel beginSheetForDirectory:nil
        file:nil
        types:nil
        modalForWindow:[sender window]
        modalDelegate: self
        didEndSelector: @selector(pathChosenInPanel: 
                        withReturn:
                        contextInfo:)
        contextInfo: nil];
}

- (void)pathChosenInPanel:(NSOpenPanel *)o_sheet
    withReturn:(int)i_return_code contextInfo:(void  *)o_context_info
{
    if( i_return_code == NSOKButton )
    {
        NSString *o_path = [[o_sheet filenames] objectAtIndex: 0];
        [o_textfield setStringValue: o_path];
    }
}

- (char *)stringValue
{
    if( [[o_textfield stringValue] length] != 0)
        return strdup( [[o_textfield stringValue] fileSystemRepresentation] );
    else
        return NULL;
}
@end

@implementation ModuleConfigControl
- (id) initWithItem: (module_config_t *)_p_item
           withView: (NSView *)o_parent_view
{
    NSRect mainFrame = [o_parent_view frame];
    NSString *o_labelString, *o_popupTooltip;
    mainFrame.size.height = 22;
    mainFrame.size.width = mainFrame.size.width - LEFTMARGIN - RIGHTMARGIN + 1;
    mainFrame.origin.x = LEFTMARGIN;
    mainFrame.origin.y = 0;

    if( [super initWithFrame: mainFrame item: _p_item] != nil )
    {
        int i_index;
        vlc_list_t *p_list;
        module_t *p_parser;
        i_view_type = CONFIG_ITEM_MODULE;

        /* add the label */
        if( p_item->psz_text )
            o_labelString = [[VLCMain sharedInstance]
                                localizedString: p_item->psz_text];
        else
            o_labelString = [NSString stringWithString:@""];
        ADD_LABEL( o_label, mainFrame, 0, -1, o_labelString )
        [o_label setAutoresizingMask:NSViewNotSizable ];
        [self addSubview: o_label];

        /* build the popup */
        o_popupTooltip = [[VLCMain sharedInstance] wrapString:
            [[VLCMain sharedInstance]
                localizedString: p_item->psz_longtext ] toWidth: PREFS_WRAP];
        ADD_POPUP( o_popup, mainFrame, [o_label frame].size.width,
            -2, 0, o_popupTooltip )
        [o_popup setAutoresizingMask:NSViewWidthSizable ];
        [o_popup addItemWithTitle: _NS("Default")];
        [[o_popup lastItem] setTag: -1];
        [o_popup selectItem: [o_popup lastItem]];

        /* build a list of available modules */
        p_list = vlc_list_find( VLCIntf, VLC_OBJECT_MODULE, FIND_ANYWHERE );
        for( i_index = 0; i_index < p_list->i_count; i_index++ )
        {
            p_parser = (module_t *)p_list->p_values[i_index].p_object ;
            if( p_item->i_type == CONFIG_ITEM_MODULE )
            {
                if( !strcmp( p_parser->psz_capability,
                            p_item->psz_type ) )
                {
                    NSString *o_description = [[VLCMain sharedInstance]
                        localizedString: p_parser->psz_longname];
                    [o_popup addItemWithTitle: o_description];

                    if( p_item->psz_value &&
                !strcmp( p_item->psz_value, p_parser->psz_object_name ) )
                        [o_popup selectItem:[o_popup lastItem]];
                }
            }
            else
            {
                module_config_t *p_config;
                if( !strcmp( p_parser->psz_object_name, "main" ) )
                      continue;

                p_config = p_parser->p_config;
                if( p_config ) do
                {
                    /* Hack: required subcategory is stored in i_min */
                    if( p_config->i_type == CONFIG_SUBCATEGORY &&
                        p_config->i_value == p_item->i_min )
                    {
                        NSString *o_description = [[VLCMain sharedInstance]
                            localizedString: p_parser->psz_longname];
                        [o_popup addItemWithTitle: o_description];

                        if( p_item->psz_value && !strcmp(p_item->psz_value,
                                                p_parser->psz_object_name) )
                            [o_popup selectItem:[o_popup lastItem]];
                    }
                } while( p_config->i_type != CONFIG_HINT_END && p_config++ );
            }
        }
        vlc_list_release( p_list );
        [self addSubview: o_popup];
    }
    return self;
}

- (void) alignWithXPosition:(int)i_xPos
{
    NSRect frame;
    NSRect superFrame = [self frame];
    frame = [o_label frame];
    frame.origin.x = i_xPos - frame.size.width - 3;
    [o_label setFrame:frame];

    frame = [o_popup frame];
    frame.origin.x = i_xPos - 1;
    frame.size.width = superFrame.size.width - frame.origin.x + 2;
    [o_popup setFrame:frame];
}

- (void)dealloc
{
    [o_popup release];
    [super dealloc];
}

- (char *)stringValue
{
    NSString *newval = [o_popup titleOfSelectedItem];
    char *returnval = NULL;
    int i_index;
    vlc_list_t *p_list;
    module_t *p_parser;

    p_list = vlc_list_find( VLCIntf, VLC_OBJECT_MODULE, FIND_ANYWHERE );
    for( i_index = 0; i_index < p_list->i_count; i_index++ )
    {
        p_parser = (module_t *)p_list->p_values[i_index].p_object ;
        if( p_item->i_type == CONFIG_ITEM_MODULE )
        {
            if( !strcmp( p_parser->psz_capability,
                    p_item->psz_type ) )
            {
                NSString *o_description = [[VLCMain sharedInstance]
                    localizedString: p_parser->psz_longname];
                if( [newval isEqualToString: o_description] )
                {
                    returnval = strdup(p_parser->psz_object_name);
                    break;
                }
            }
        }
        else
        {
            module_config_t *p_config;
            if( !strcmp( p_parser->psz_object_name, "main" ) )
                  continue;

            p_config = p_parser->p_config;
            if( p_config ) do
            {
                /* Hack: required subcategory is stored in i_min */
                if( p_config->i_type == CONFIG_SUBCATEGORY &&
                    p_config->i_value == p_item->i_min )
                {
                    NSString *o_description = [[VLCMain sharedInstance]
                        localizedString: p_parser->psz_longname];
                    if( [newval isEqualToString: o_description] )
                    {
                        returnval = strdup(p_parser->psz_object_name);
                        break;
                    }
                }
            } while( p_config->i_type != CONFIG_HINT_END && p_config++ );
        }
    }
    vlc_list_release( p_list );
    return returnval;
}
@end

@implementation IntegerConfigControl
- (id) initWithItem: (module_config_t *)_p_item
           withView: (NSView *)o_parent_view
{
    NSRect mainFrame = [o_parent_view frame];
    NSString *o_labelString, *o_tooltip, *o_textfieldString;
    mainFrame.size.height = 23;
    mainFrame.size.width = mainFrame.size.width - LEFTMARGIN - RIGHTMARGIN + 1;
    mainFrame.origin.x = LEFTMARGIN;
    mainFrame.origin.y = 0;

    if( [super initWithFrame: mainFrame item: _p_item] != nil )
    {
        i_view_type = CONFIG_ITEM_INTEGER;

        o_tooltip = [[VLCMain sharedInstance] wrapString:
            [[VLCMain sharedInstance]
                localizedString: p_item->psz_longtext ] toWidth: PREFS_WRAP];

        /* add the label */
        if( p_item->psz_text )
            o_labelString = [[VLCMain sharedInstance]
                                localizedString: p_item->psz_text];
        else
            o_labelString = [NSString stringWithString:@""];
        ADD_LABEL( o_label, mainFrame, 0, -3, o_labelString )
        [o_label setAutoresizingMask:NSViewNotSizable ];
        [self addSubview: o_label];

        /* build the stepper */
        ADD_STEPPER( o_stepper, mainFrame, mainFrame.size.width - 19,
            0, o_tooltip, -1600, 1600)
        [o_stepper setIntValue: p_item->i_value];
        [o_stepper setAutoresizingMask:NSViewMaxXMargin ];
        [self addSubview: o_stepper];

        /* build the textfield */
        if( p_item->psz_value )
            o_textfieldString = [[VLCMain sharedInstance]
                                    localizedString: p_item->psz_value];
        else
            o_textfieldString = [NSString stringWithString: @""];
        ADD_TEXTFIELD( o_textfield, mainFrame, mainFrame.size.width - 19 - 52,
            1, 49, o_tooltip, @"" )
        [o_textfield setIntValue: p_item->i_value];
        [o_textfield setDelegate: self];
        [[NSNotificationCenter defaultCenter] addObserver: self
            selector: @selector(textfieldChanged:)
            name: NSControlTextDidChangeNotification
            object: o_textfield];
        [o_textfield setAutoresizingMask:NSViewMaxXMargin ];
        [self addSubview: o_textfield];
    }
    return self;
}

- (void) alignWithXPosition:(int)i_xPos
{
    NSRect frame;
    frame = [o_label frame];
    frame.origin.x = i_xPos - frame.size.width - 3;
    [o_label setFrame:frame];

    frame = [o_textfield frame];
    frame.origin.x = i_xPos + 2;
    [o_textfield setFrame:frame];

    frame = [o_stepper frame];
    frame.origin.x = i_xPos + [o_textfield frame].size.width + 5;
    [o_stepper setFrame:frame];
}

- (void)dealloc
{
    [o_stepper release];
    [o_textfield release];
    [super dealloc];
}

- (IBAction)stepperChanged:(id)sender
{
    [o_textfield setIntValue: [o_stepper intValue]];
}

- (void)textfieldChanged:(NSNotification *)o_notification
{
    [o_stepper setIntValue: [o_textfield intValue]];
}

- (int)intValue
{
    return [o_textfield intValue];
}

@end

@implementation IntegerListConfigControl

- (id) initWithItem: (module_config_t *)_p_item
           withView: (NSView *)o_parent_view
{
    NSRect mainFrame = [o_parent_view frame];
    NSString *o_labelString, *o_textfieldTooltip;
    mainFrame.size.height = 22;
    mainFrame.size.width = mainFrame.size.width - LEFTMARGIN - RIGHTMARGIN + 1;
    mainFrame.origin.x = LEFTMARGIN;
    mainFrame.origin.y = 0;

    if( [super initWithFrame: mainFrame item: _p_item] != nil )
    {
        int i_index;
        i_view_type = CONFIG_ITEM_STRING_LIST;

        /* add the label */
        if( p_item->psz_text )
            o_labelString = [[VLCMain sharedInstance]
                localizedString: p_item->psz_text];
        else
            o_labelString = [NSString stringWithString:@""];
        ADD_LABEL( o_label, mainFrame, 0, -3, o_labelString )
        [o_label setAutoresizingMask:NSViewNotSizable ];
        [self addSubview: o_label];

        /* build the textfield */
        o_textfieldTooltip = [[VLCMain sharedInstance] wrapString:
            [[VLCMain sharedInstance]
                localizedString: p_item->psz_longtext ] toWidth: PREFS_WRAP];
        ADD_COMBO( o_combo, mainFrame, [o_label frame].size.width,
            -2, 0, o_textfieldTooltip )
        [o_combo setAutoresizingMask:NSViewWidthSizable ];
        for( i_index = 0; i_index < p_item->i_list; i_index++ )
        {
            if( p_item->i_value == p_item->pi_list[i_index] )
            {
                [o_combo selectItemAtIndex: i_index];
            }
        }
        [self addSubview: o_combo];
    }
    return self;
}

- (void) alignWithXPosition:(int)i_xPos
{
    NSRect frame;
    NSRect superFrame = [self frame];
    frame = [o_label frame];
    frame.origin.x = i_xPos - frame.size.width - 3;
    [o_label setFrame:frame];

    frame = [o_combo frame];
    frame.origin.x = i_xPos + 2;
    frame.size.width = superFrame.size.width - frame.origin.x + 2;
    [o_combo setFrame:frame];
}

- (void)dealloc
{
    [o_combo release];
    [super dealloc];
}

- (int)intValue
{
    if( [o_combo indexOfSelectedItem] >= 0 )
        return p_item->pi_list[[o_combo indexOfSelectedItem]];
    else
        return [o_combo intValue];
}
@end

@implementation IntegerListConfigControl (NSComboBoxDataSource)
- (int)numberOfItemsInComboBox:(NSComboBox *)aComboBox
{
    return p_item->i_list;
}

- (id)comboBox:(NSComboBox *)aComboBox objectValueForItemAtIndex:(int)i_index
{
    if( p_item->ppsz_list_text && p_item->ppsz_list_text[i_index] )
        return [[VLCMain sharedInstance]
                    localizedString: p_item->ppsz_list_text[i_index]];
    else
        return [NSString stringWithFormat: @"%i", p_item->pi_list[i_index]];
}
@end

@implementation RangedIntegerConfigControl
- (id) initWithItem: (module_config_t *)_p_item
           withView: (NSView *)o_parent_view
{
    NSRect mainFrame = [o_parent_view frame];
    NSString *o_labelString, *o_tooltip;
    mainFrame.size.height = 50;
    mainFrame.size.width = mainFrame.size.width - LEFTMARGIN - RIGHTMARGIN;
    mainFrame.origin.x = LEFTMARGIN;
    mainFrame.origin.y = 0;

    if( [super initWithFrame: mainFrame item: _p_item] != nil )
    {
        i_view_type = CONFIG_ITEM_RANGED_INTEGER;

        /* add the label */
        if( p_item->psz_text )
            o_labelString = [[VLCMain sharedInstance]
                                localizedString: p_item->psz_text];
        else
            o_labelString = [NSString stringWithString:@""];
        ADD_LABEL( o_label, mainFrame, 0, -3, o_labelString )
        [o_label setAutoresizingMask:NSViewNotSizable ];
        [self addSubview: o_label];

        /* build the textfield */
        o_tooltip = [[VLCMain sharedInstance] wrapString:
            [[VLCMain sharedInstance]
                localizedString: p_item->psz_longtext ] toWidth: PREFS_WRAP];
        ADD_TEXTFIELD( o_textfield, mainFrame, [o_label frame].size.width + 2,
            28, 49, o_tooltip, @"" )
        [o_textfield setIntValue: p_item->i_value];
        [o_textfield setAutoresizingMask:NSViewMaxXMargin ];
        [o_textfield setDelegate: self];
        [[NSNotificationCenter defaultCenter] addObserver: self
            selector: @selector(textfieldChanged:)
            name: NSControlTextDidChangeNotification
            object: o_textfield];
        [self addSubview: o_textfield];

        /* build the mintextfield */
        ADD_LABEL( o_textfield_min, mainFrame, 12, -30, @"-8888" )
        [o_textfield_min setIntValue: p_item->i_min];
        [o_textfield_min setAutoresizingMask:NSViewMaxXMargin ];
        [o_textfield_min setAlignment:NSRightTextAlignment];
        [self addSubview: o_textfield_min];

        /* build the maxtextfield */
        ADD_LABEL( o_textfield_max, mainFrame,
                    mainFrame.size.width - 31, -30, @"8888" )
        [o_textfield_max setIntValue: p_item->i_max];
        [o_textfield_max setAutoresizingMask:NSViewMinXMargin ];
        [self addSubview: o_textfield_max];

        /* build the slider */
        ADD_SLIDER( o_slider, mainFrame, [o_textfield_min frame].origin.x +
            [o_textfield_min frame].size.width + 6, -1, mainFrame.size.width -
            [o_textfield_max frame].size.width -
            [o_textfield_max frame].size.width - 14 -
            [o_textfield_min frame].origin.x, o_tooltip,
            p_item->i_min, p_item->i_max )
        [o_slider setIntValue: p_item->i_value];
        [o_slider setAutoresizingMask:NSViewWidthSizable ];
        [o_slider setTarget: self];
        [o_slider setAction: @selector(sliderChanged:)];
        [o_slider sendActionOn:NSLeftMouseUpMask | NSLeftMouseDownMask |
            NSLeftMouseDraggedMask];
        [self addSubview: o_slider];

    }
    return self;
}

- (void) alignWithXPosition:(int)i_xPos
{
    NSRect frame;
    frame = [o_label frame];
    frame.origin.x = i_xPos - frame.size.width - 3;
    [o_label setFrame:frame];

    frame = [o_textfield frame];
    frame.origin.x = i_xPos + 2;
    [o_textfield setFrame:frame];
}

- (void)dealloc
{
    [o_textfield release];
    [o_textfield_min release];
    [o_textfield_max release];
    [o_slider release];
    [super dealloc];
}

- (IBAction)sliderChanged:(id)sender
{
    [o_textfield setIntValue: [o_slider intValue]];
}

- (void)textfieldChanged:(NSNotification *)o_notification
{
    [o_slider setIntValue: [o_textfield intValue]];
}

- (int)intValue
{
    return [o_slider intValue];
}
@end

@implementation FloatConfigControl
- (id) initWithItem: (module_config_t *)_p_item
           withView: (NSView *)o_parent_view
{
    NSRect mainFrame = [o_parent_view frame];
    NSString *o_labelString, *o_tooltip, *o_textfieldString;
    mainFrame.size.height = 23;
    mainFrame.size.width = mainFrame.size.width - LEFTMARGIN - RIGHTMARGIN + 1;
    mainFrame.origin.x = LEFTMARGIN;
    mainFrame.origin.y = 0;

    if( [super initWithFrame: mainFrame item: _p_item] != nil )
    {
        i_view_type = CONFIG_ITEM_INTEGER;

        o_tooltip = [[VLCMain sharedInstance] wrapString:
            [[VLCMain sharedInstance]
                localizedString: p_item->psz_longtext ] toWidth: PREFS_WRAP];

        /* add the label */
        if( p_item->psz_text )
            o_labelString = [[VLCMain sharedInstance]
                                localizedString: p_item->psz_text];
        else
            o_labelString = [NSString stringWithString:@""];
        ADD_LABEL( o_label, mainFrame, 0, -2, o_labelString )
        [o_label setAutoresizingMask:NSViewNotSizable ];
        [self addSubview: o_label];

        /* build the stepper */
        ADD_STEPPER( o_stepper, mainFrame, mainFrame.size.width - 19,
            0, o_tooltip, -1600, 1600)
        [o_stepper setFloatValue: p_item->f_value];
        [o_stepper setAutoresizingMask:NSViewMaxXMargin ];
        [self addSubview: o_stepper];

        /* build the textfield */
        if( p_item->psz_value )
            o_textfieldString = [[VLCMain sharedInstance]
                                    localizedString: p_item->psz_value];
        else
            o_textfieldString = [NSString stringWithString: @""];
        ADD_TEXTFIELD( o_textfield, mainFrame, mainFrame.size.width - 19 - 52,
            1, 49, o_tooltip, @"" )
        [o_textfield setFloatValue: p_item->f_value];
        [o_textfield setDelegate: self];
        [[NSNotificationCenter defaultCenter] addObserver: self
            selector: @selector(textfieldChanged:)
            name: NSControlTextDidChangeNotification
            object: o_textfield];
        [o_textfield setAutoresizingMask:NSViewMaxXMargin ];
        [self addSubview: o_textfield];
    }
    return self;
}

- (void) alignWithXPosition:(int)i_xPos
{
    NSRect frame;
    frame = [o_label frame];
    frame.origin.x = i_xPos - frame.size.width - 3;
    [o_label setFrame:frame];

    frame = [o_textfield frame];
    frame.origin.x = i_xPos + 2;
    [o_textfield setFrame:frame];

    frame = [o_stepper frame];
    frame.origin.x = i_xPos + [o_textfield frame].size.width + 5;
    [o_stepper setFrame:frame];
}

- (void)dealloc
{
    [o_stepper release];
    [o_textfield release];
    [super dealloc];
}

- (IBAction)stepperChanged:(id)sender
{
    [o_textfield setFloatValue: [o_stepper floatValue]];
}

- (void)textfieldChanged:(NSNotification *)o_notification
{
    [o_stepper setFloatValue: [o_textfield floatValue]];
}

- (int)floatValue
{
    return [o_stepper floatValue];
}
@end

@implementation RangedFloatConfigControl
- (id) initWithItem: (module_config_t *)_p_item
           withView: (NSView *)o_parent_view
{
    NSRect mainFrame = [o_parent_view frame];
    NSString *o_labelString, *o_tooltip;
    mainFrame.size.height = 50;
    mainFrame.size.width = mainFrame.size.width - LEFTMARGIN - RIGHTMARGIN;
    mainFrame.origin.x = LEFTMARGIN;
    mainFrame.origin.y = 0;

    if( [super initWithFrame: mainFrame item: _p_item] != nil )
    {
        i_view_type = CONFIG_ITEM_RANGED_INTEGER;

        /* add the label */
        if( p_item->psz_text )
            o_labelString = [[VLCMain sharedInstance]
                                localizedString: p_item->psz_text];
        else
            o_labelString = [NSString stringWithString:@""];
        ADD_LABEL( o_label, mainFrame, 0, -3, o_labelString )
        [o_label setAutoresizingMask:NSViewNotSizable ];
        [self addSubview: o_label];

        /* build the textfield */
        o_tooltip = [[VLCMain sharedInstance] wrapString:
            [[VLCMain sharedInstance]
                localizedString: p_item->psz_longtext ] toWidth: PREFS_WRAP];
        ADD_TEXTFIELD( o_textfield, mainFrame, [o_label frame].size.width + 2,
            28, 49, o_tooltip, @"" )
        [o_textfield setFloatValue: p_item->f_value];
        [o_textfield setAutoresizingMask:NSViewMaxXMargin ];
        [o_textfield setDelegate: self];
        [[NSNotificationCenter defaultCenter] addObserver: self
            selector: @selector(textfieldChanged:)
            name: NSControlTextDidChangeNotification
            object: o_textfield];
        [self addSubview: o_textfield];

        /* build the mintextfield */
        ADD_LABEL( o_textfield_min, mainFrame, 12, -30, @"-8888" )
        [o_textfield_min setFloatValue: p_item->f_min];
        [o_textfield_min setAutoresizingMask:NSViewMaxXMargin ];
        [o_textfield_min setAlignment:NSRightTextAlignment];
        [self addSubview: o_textfield_min];

        /* build the maxtextfield */
        ADD_LABEL( o_textfield_max, mainFrame, mainFrame.size.width - 31,
            -30, @"8888" )
        [o_textfield_max setFloatValue: p_item->f_max];
        [o_textfield_max setAutoresizingMask:NSViewMinXMargin ];
        [self addSubview: o_textfield_max];

        /* build the slider */
        ADD_SLIDER( o_slider, mainFrame, [o_textfield_min frame].origin.x +
            [o_textfield_min frame].size.width + 6, -1, mainFrame.size.width -
            [o_textfield_max frame].size.width -
            [o_textfield_max frame].size.width - 14 -
            [o_textfield_min frame].origin.x, o_tooltip, p_item->f_min,
            p_item->f_max )
        [o_slider setFloatValue: p_item->f_value];
        [o_slider setAutoresizingMask:NSViewWidthSizable ];
        [o_slider setTarget: self];
        [o_slider setAction: @selector(sliderChanged:)];
        [o_slider sendActionOn:NSLeftMouseUpMask | NSLeftMouseDownMask |
            NSLeftMouseDraggedMask];
        [self addSubview: o_slider];

    }
    return self;
}

- (void) alignWithXPosition:(int)i_xPos
{
    NSRect frame;
    frame = [o_label frame];
    frame.origin.x = i_xPos - frame.size.width - 3;
    [o_label setFrame:frame];

    frame = [o_textfield frame];
    frame.origin.x = i_xPos + 2;
    [o_textfield setFrame:frame];
}

- (void)dealloc
{
    [o_textfield release];
    [o_textfield_min release];
    [o_textfield_max release];
    [o_slider release];
    [super dealloc];
}

- (IBAction)sliderChanged:(id)sender
{
    [o_textfield setFloatValue: [o_slider floatValue]];
}

- (void)textfieldChanged:(NSNotification *)o_notification
{
    [o_slider setFloatValue: [o_textfield floatValue]];
}

- (int)floatValue
{
    return [o_slider floatValue];
}

@end

@implementation BoolConfigControl

- (id) initWithItem: (module_config_t *)_p_item
           withView: (NSView *)o_parent_view
{
    NSRect mainFrame = [o_parent_view frame];
    NSString *o_labelString, *o_tooltip;
    mainFrame.size.height = 17;
    mainFrame.size.width = mainFrame.size.width - LEFTMARGIN - RIGHTMARGIN;
    mainFrame.origin.x = LEFTMARGIN;
    mainFrame.origin.y = 0;

    if( [super initWithFrame: mainFrame item: _p_item] != nil )
    {
        i_view_type = CONFIG_ITEM_BOOL;

        /* add the label */
        if( p_item->psz_text )
            o_labelString = [[VLCMain sharedInstance]
                                localizedString: p_item->psz_text];
        else
            o_labelString = [NSString stringWithString:@""];
        ADD_LABEL( o_label, mainFrame, 0, 0, o_labelString )
        [o_label setAutoresizingMask:NSViewNotSizable ];
        [self addSubview: o_label];
        /* add the checkbox */
        o_tooltip = [[VLCMain sharedInstance]
            wrapString: [[VLCMain sharedInstance]
            localizedString: p_item->psz_longtext ] toWidth: PREFS_WRAP];
        ADD_CHECKBOX( o_checkbox, mainFrame, [o_label frame].size.width,
                        0, @"", o_tooltip, p_item->i_value, NSImageLeft)
        [o_checkbox setAutoresizingMask:NSViewNotSizable ];
        [self addSubview: o_checkbox];
    }
    return self;
}

- (void) alignWithXPosition:(int)i_xPos
{
    NSRect frame;
    frame = [o_label frame];
    frame.origin.x = i_xPos - frame.size.width - 3;
    [o_label setFrame:frame];

    frame = [o_checkbox frame];
    frame.origin.x = i_xPos;
    [o_checkbox setFrame:frame];
}

- (void)dealloc
{
    [o_checkbox release];
    [super dealloc];
}

- (int)intValue
{
    return [o_checkbox intValue];
}

@end

@implementation KeyConfigControlBefore103

- (id) initWithItem: (module_config_t *)_p_item
           withView: (NSView *)o_parent_view
{
    NSRect mainFrame = [o_parent_view frame];
    NSString *o_labelString, *o_tooltip;
    mainFrame.size.height = 37;
    mainFrame.size.width = mainFrame.size.width - LEFTMARGIN - RIGHTMARGIN + 1;
    mainFrame.origin.x = LEFTMARGIN;
    mainFrame.origin.y = 0;

    if( [super initWithFrame: mainFrame item: _p_item] != nil )
    {
        i_view_type = CONFIG_ITEM_KEY_BEFORE_10_3;

        /* add the label */
        if( p_item->psz_text )
            o_labelString = [[VLCMain sharedInstance]
                                localizedString: p_item->psz_text];
        else
            o_labelString = [NSString stringWithString:@""];
        ADD_LABEL( o_label, mainFrame, 0, -10, o_labelString )
        [o_label setAutoresizingMask:NSViewNotSizable ];
        [self addSubview: o_label];

        /* add the checkboxes */
        o_tooltip = [[VLCMain sharedInstance] wrapString:
            [[VLCMain sharedInstance]
                localizedString: p_item->psz_longtext ] toWidth: PREFS_WRAP];
        ADD_CHECKBOX( o_cmd_checkbox, mainFrame,
            [o_label frame].size.width + 2, 0,
            [NSString stringWithUTF8String:PLACE_OF_INTEREST_SIGN], o_tooltip,
            ((((unsigned int)p_item->i_value) & KEY_MODIFIER_COMMAND)?YES:NO),
            NSImageLeft )
        [o_cmd_checkbox setState: p_item->i_value & KEY_MODIFIER_COMMAND];
        ADD_CHECKBOX( o_ctrl_checkbox, mainFrame,
            [o_cmd_checkbox frame].size.width +
            [o_cmd_checkbox frame].origin.x + 6, 0,
            [NSString stringWithUTF8String:UP_ARROWHEAD], o_tooltip,
            ((((unsigned int)p_item->i_value) & KEY_MODIFIER_CTRL)?YES:NO),
            NSImageLeft )
        [o_ctrl_checkbox setState: p_item->i_value & KEY_MODIFIER_CTRL];
        ADD_CHECKBOX( o_alt_checkbox, mainFrame, [o_label frame].size.width +
            2, -2 - [o_cmd_checkbox frame].size.height,
            [NSString stringWithUTF8String:OPTION_KEY], o_tooltip,
            ((((unsigned int)p_item->i_value) & KEY_MODIFIER_ALT)?YES:NO),
            NSImageLeft )
        [o_alt_checkbox setState: p_item->i_value & KEY_MODIFIER_ALT];
        ADD_CHECKBOX( o_shift_checkbox, mainFrame,
            [o_cmd_checkbox frame].size.width +
            [o_cmd_checkbox frame].origin.x + 6, -2 -
            [o_cmd_checkbox frame].size.height,
            [NSString stringWithUTF8String:UPWARDS_WHITE_ARROW], o_tooltip,
            ((((unsigned int)p_item->i_value) & KEY_MODIFIER_SHIFT)?YES:NO),
            NSImageLeft )
        [o_shift_checkbox setState: p_item->i_value & KEY_MODIFIER_SHIFT];
        [self addSubview: o_cmd_checkbox];
        [self addSubview: o_ctrl_checkbox];
        [self addSubview: o_alt_checkbox];
        [self addSubview: o_shift_checkbox];

        /* build the popup */
        ADD_POPUP( o_popup, mainFrame, [o_shift_checkbox frame].origin.x +
            [o_shift_checkbox frame].size.width + 4,
            4, 0, o_tooltip )
        [o_popup setAutoresizingMask:NSViewWidthSizable ];

        if( o_keys_menu == nil )
        {
            unsigned int i;
            o_keys_menu = [[NSMenu alloc] initWithTitle: @"Keys Menu"];
            for ( i = 0; i < sizeof(vlc_keys) / sizeof(key_descriptor_t); i++)
                if( vlc_keys[i].psz_key_string && *vlc_keys[i].psz_key_string )
                    POPULATE_A_KEY( o_keys_menu,
                        [NSString stringWithCString:vlc_keys[i].psz_key_string]
                        , vlc_keys[i].i_key_code)
        }
        [o_popup setMenu:[o_keys_menu copyWithZone:nil]];
        [o_popup selectItemWithTitle: [[VLCMain sharedInstance]
            localizedString:KeyToString(
            (((unsigned int)p_item->i_value) & ~KEY_MODIFIER ))]];
        [self addSubview: o_popup];
    }
    return self;
}

- (void) alignWithXPosition:(int)i_xPos
{
    NSRect frame;
    NSRect superFrame = [self frame];
    frame = [o_label frame];
    frame.origin.x = i_xPos - frame.size.width - 3;
    [o_label setFrame:frame];

    frame = [o_cmd_checkbox frame];
    frame.origin.x = i_xPos;
    [o_cmd_checkbox setFrame:frame];

    frame = [o_ctrl_checkbox frame];
    frame.origin.x = [o_cmd_checkbox frame].size.width +
                        [o_cmd_checkbox frame].origin.x + 4;
    [o_ctrl_checkbox setFrame:frame];

    frame = [o_alt_checkbox frame];
    frame.origin.x = i_xPos;
    [o_alt_checkbox setFrame:frame];

    frame = [o_shift_checkbox frame];
    frame.origin.x = [o_cmd_checkbox frame].size.width +
                        [o_cmd_checkbox frame].origin.x + 4;
    [o_shift_checkbox setFrame:frame];

    frame = [o_popup frame];
    frame.origin.x = [o_shift_checkbox frame].origin.x +
                        [o_shift_checkbox frame].size.width + 3;
    frame.size.width = superFrame.size.width - frame.origin.x + 2;
    [o_popup setFrame:frame];
}

- (void)dealloc
{
    [o_cmd_checkbox release];
    [o_ctrl_checkbox release];
    [o_alt_checkbox release];
    [o_shift_checkbox release];
    [o_popup release];
    [super dealloc];
}

- (int)intValue
{
    unsigned int i_new_key = 0;

    i_new_key |= ([o_cmd_checkbox state] == NSOnState) ?
        KEY_MODIFIER_COMMAND : 0;
    i_new_key |= ([o_ctrl_checkbox state] == NSOnState) ?
        KEY_MODIFIER_CTRL : 0;
    i_new_key |= ([o_alt_checkbox state] == NSOnState) ?
        KEY_MODIFIER_ALT : 0;
    i_new_key |= ([o_shift_checkbox state] == NSOnState) ?
        KEY_MODIFIER_SHIFT : 0;

    i_new_key |= StringToKey([[[o_popup selectedItem] title] cString]);
    return i_new_key;
}
@end

@implementation KeyConfigControlAfter103
- (id) initWithItem: (module_config_t *)_p_item
           withView: (NSView *)o_parent_view
{
    NSRect mainFrame = [o_parent_view frame];
    NSString *o_labelString, *o_tooltip;
    mainFrame.size.height = 22;
    mainFrame.size.width = mainFrame.size.width - LEFTMARGIN - RIGHTMARGIN + 1;
    mainFrame.origin.x = LEFTMARGIN;
    mainFrame.origin.y = 0;

    if( [super initWithFrame: mainFrame item: _p_item] != nil )
    {
        i_view_type = CONFIG_ITEM_KEY_AFTER_10_3;

        /* add the label */
        if( p_item->psz_text )
            o_labelString = [[VLCMain sharedInstance]
                localizedString: p_item->psz_text];
        else
            o_labelString = [NSString stringWithString:@""];
        ADD_LABEL( o_label, mainFrame, 0, -1, o_labelString )
        [o_label setAutoresizingMask:NSViewNotSizable ];
        [self addSubview: o_label];

        /* build the popup */
        o_tooltip = [[VLCMain sharedInstance] wrapString:
            [[VLCMain sharedInstance]
                localizedString: p_item->psz_longtext ] toWidth: PREFS_WRAP];
        ADD_POPUP( o_popup, mainFrame, [o_label frame].origin.x +
            [o_label frame].size.width + 3,
            -2, 0, o_tooltip )
        [o_popup setAutoresizingMask:NSViewWidthSizable ];

        if( o_keys_menu == nil )
        {
            unsigned int i;
            o_keys_menu = [[NSMenu alloc] initWithTitle: @"Keys Menu"];
            for ( i = 0; i < sizeof(vlc_keys) / sizeof(key_descriptor_t); i++)
                if( vlc_keys[i].psz_key_string && *vlc_keys[i].psz_key_string )
                    POPULATE_A_KEY( o_keys_menu,
                        [NSString stringWithCString:vlc_keys[i].psz_key_string]
                        , vlc_keys[i].i_key_code)
        }
        [o_popup setMenu:[o_keys_menu copyWithZone:nil]];
        [o_popup selectItem:[[o_popup menu] itemWithTag:p_item->i_value]];
        [self addSubview: o_popup];

    }
    return self;
}

- (void) alignWithXPosition:(int)i_xPos
{
    NSRect frame;
    NSRect superFrame = [self frame];
    frame = [o_label frame];
    frame.origin.x = i_xPos - frame.size.width - 3;
    [o_label setFrame:frame];

    frame = [o_popup frame];
    frame.origin.x = i_xPos - 1;
    frame.size.width = superFrame.size.width - frame.origin.x + 2;
    [o_popup setFrame:frame];
}

- (void)dealloc
{
    [o_popup release];
    [super dealloc];
}

- (int)intValue
{
    return [o_popup selectedTag];
}
@end

@implementation ModuleListConfigControl
- (id) initWithItem: (module_config_t *)_p_item
           withView: (NSView *)o_parent_view
{
if( _p_item->i_type == CONFIG_ITEM_MODULE_LIST )
//TODO....
        return nil;

//Fill our array to know how may items we have...
    vlc_list_t *p_list;
    module_t *p_parser;
    int i_index;
    NSRect mainFrame = [o_parent_view frame];
    NSString *o_labelString, *o_textfieldString, *o_tooltip;

    o_modulearray = [[NSMutableArray alloc] initWithCapacity:10];
    /* build a list of available modules */
    p_list = vlc_list_find( VLCIntf, VLC_OBJECT_MODULE, FIND_ANYWHERE );
    for( i_index = 0; i_index < p_list->i_count; i_index++ )
    {
        p_parser = (module_t *)p_list->p_values[i_index].p_object ;

        if( !strcmp( p_parser->psz_object_name, "main" ) )
            continue;

        module_config_t *p_config = p_parser->p_config;
        if( p_config ) do
        {
            NSString *o_modulelongname, *o_modulename;
            NSNumber *o_moduleenabled = nil;
            /* Hack: required subcategory is stored in i_min */
            if( p_config->i_type == CONFIG_SUBCATEGORY &&
                p_config->i_value == _p_item->i_min )
            {
                o_modulelongname = [NSString stringWithUTF8String:
                                        p_parser->psz_longname];
                o_modulename = [NSString stringWithUTF8String:
                                        p_parser->psz_object_name];

                if( _p_item->psz_value &&
                    strstr( _p_item->psz_value, p_parser->psz_object_name ) )
                    o_moduleenabled = [NSNumber numberWithBool:YES];
                else
                    o_moduleenabled = [NSNumber numberWithBool:NO];

                [o_modulearray addObject:[NSMutableArray
                    arrayWithObjects: o_modulename, o_modulelongname,
                    o_moduleenabled, nil]];
            }
        } while( p_config->i_type != CONFIG_HINT_END && p_config++ );
    }
    vlc_list_release( p_list );

    mainFrame.size.height = 30 + 18 * [o_modulearray count];
    mainFrame.size.width = mainFrame.size.width - LEFTMARGIN - RIGHTMARGIN;
    mainFrame.origin.x = LEFTMARGIN;
    mainFrame.origin.y = 0;
    if( [super initWithFrame: mainFrame item: _p_item] != nil )
    {
        i_view_type = CONFIG_ITEM_MODULE_LIST;

        /* add the label */
        if( p_item->psz_text )
            o_labelString = [[VLCMain sharedInstance]
                                localizedString: p_item->psz_text];
        else
            o_labelString = [NSString stringWithString:@""];
        ADD_LABEL( o_label, mainFrame, 0, -3, o_labelString )
        [o_label setAutoresizingMask:NSViewNotSizable ];
        [self addSubview: o_label];

        /* build the textfield */
        o_tooltip = [[VLCMain sharedInstance] wrapString:
            [[VLCMain sharedInstance]
                localizedString: p_item->psz_longtext ] toWidth: PREFS_WRAP];
        if( p_item->psz_value )
            o_textfieldString = [[VLCMain sharedInstance]
                localizedString: p_item->psz_value];
        else
            o_textfieldString = [NSString stringWithString: @""];
        ADD_TEXTFIELD( o_textfield, mainFrame, [o_label frame].size.width + 2,
            mainFrame.size.height - 22, mainFrame.size.width -
            [o_label frame].size.width - 2, o_tooltip, o_textfieldString )
        [o_textfield setAutoresizingMask:NSViewWidthSizable ];
        [self addSubview: o_textfield];


{
    NSRect s_rc = mainFrame;
    s_rc.size.height = mainFrame.size.height - 30;
    s_rc.size.width = mainFrame.size.width - 12;
    s_rc.origin.x = 12;
    s_rc.origin.y = 0;
    o_scrollview = [[[NSScrollView alloc] initWithFrame: s_rc] retain];
    [o_scrollview setDrawsBackground: NO];
    [o_scrollview setBorderType: NSBezelBorder];
    [o_scrollview setAutohidesScrollers:YES];

    NSTableView *o_tableview;
    o_tableview = [[NSTableView alloc] initWithFrame : s_rc];
    [o_tableview setUsesAlternatingRowBackgroundColors:YES];
    [o_tableview setHeaderView:nil];
/* TODO: find a good way to fix the row height and text size*/
/* FIXME: support for multiple selection... */
//    [o_tableview setAllowsMultipleSelection:YES];

    NSCell *o_headerCell = [[NSCell alloc] initTextCell:@"Enabled"];
    NSCell *o_dataCell = [[NSButtonCell alloc] init];
    [(NSButtonCell*)o_dataCell setButtonType:NSSwitchButton];
    [o_dataCell setTitle:@""];
    [o_dataCell setFont:[NSFont systemFontOfSize:0]];
    NSTableColumn *o_tableColumn = [[NSTableColumn alloc]
        initWithIdentifier:[NSString stringWithCString: "Enabled"]];
    [o_tableColumn setHeaderCell: o_headerCell];
    [o_tableColumn setDataCell: o_dataCell];
    [o_tableColumn setWidth:17];
    [o_tableview addTableColumn: o_tableColumn];

    o_headerCell = [[NSCell alloc] initTextCell:@"Module Name"];
    o_dataCell = [[NSTextFieldCell alloc] init];
    [o_dataCell setFont:[NSFont systemFontOfSize:12]];
    o_tableColumn = [[NSTableColumn alloc]
        initWithIdentifier:[NSString stringWithCString: "Module"]];
    [o_tableColumn setHeaderCell: o_headerCell];
    [o_tableColumn setDataCell: o_dataCell];
    [o_tableColumn setWidth:388 - 17];
    [o_tableview addTableColumn: o_tableColumn];
    [o_tableview registerForDraggedTypes:[NSArray arrayWithObjects:
            @"VLC media player module", nil]];

    [o_tableview setDataSource:self];
    [o_tableview setTarget: self];
    [o_tableview setAction: @selector(tableChanged:)];
    [o_tableview sendActionOn:NSLeftMouseUpMask | NSLeftMouseDownMask |
        NSLeftMouseDraggedMask];
    [o_scrollview setDocumentView: o_tableview];
}
    [o_scrollview setAutoresizingMask:NSViewWidthSizable ];
    [self addSubview: o_scrollview];


    }
    return self;
}

- (void) alignWithXPosition:(int)i_xPos
{
    ;
}

- (IBAction)tableChanged:(id)sender
{
    NSString *o_newstring = @"";
    unsigned int i;
    for( i = 0 ; i < [o_modulearray count] ; i++ )
        if( [[[o_modulearray objectAtIndex:i] objectAtIndex:2]
            boolValue] != NO )
        {
            o_newstring = [o_newstring stringByAppendingString:
                [[o_modulearray objectAtIndex:i] objectAtIndex:0]];
            o_newstring = [o_newstring stringByAppendingString:@","];
        }

    [o_textfield setStringValue: [o_newstring
        substringToIndex: ([o_newstring length])?[o_newstring length] - 1:0]];
}

- (void)dealloc
{
    [o_scrollview release];
    [super dealloc];
}


- (char *)stringValue
{
    return strdup( [[o_textfield stringValue] cString] );
}

@end

@implementation ModuleListConfigControl (NSTableDataSource)

- (BOOL)tableView:(NSTableView*)table writeRows:(NSArray*)rows
    toPasteboard:(NSPasteboard*)pb
{
    // We only want to allow dragging of selected rows.
    NSEnumerator    *iter = [rows objectEnumerator];
    NSNumber        *row;
    while ((row = [iter nextObject]) != nil)
    {
        if (![table isRowSelected:[row intValue]])
            return NO;
    }

    [pb declareTypes:[NSArray
        arrayWithObject:@"VLC media player module"] owner:nil];
    [pb setPropertyList:rows forType:@"VLC media player module"];
    return YES;
}

- (NSDragOperation)tableView:(NSTableView*)table
    validateDrop:(id <NSDraggingInfo>)info proposedRow:(int)row
    proposedDropOperation:(NSTableViewDropOperation)op
{
    // Make drops at the end of the table go to the end.
    if (row == -1)
    {
        row = [table numberOfRows];
        op = NSTableViewDropAbove;
        [table setDropRow:row dropOperation:op];
    }

    // We don't ever want to drop onto a row, only between rows.
    if (op == NSTableViewDropOn)
        [table setDropRow:(row+1) dropOperation:NSTableViewDropAbove];
    return NSTableViewDropAbove;
}

- (BOOL)tableView:(NSTableView*)table acceptDrop:(id <NSDraggingInfo>)info
    row:(int)dropRow dropOperation:(NSTableViewDropOperation)op;
{
    NSPasteboard    *pb = [info draggingPasteboard];
    NSDragOperation srcMask = [info draggingSourceOperationMask];
    BOOL accepted = NO;

    NS_DURING

        NSArray *array;

        // Intra-table drag - data is the array of rows.
        if (!accepted && (array =
            [pb propertyListForType:@"VLC media player module"]) != NULL)
        {
            NSEnumerator *iter = nil;
            id val;
            BOOL isCopy = (srcMask & NSDragOperationMove) ? NO:YES;
            // Move the modules
            iter = [array objectEnumerator];
            while ((val = [iter nextObject]) != NULL)
            {
                NSArray *o_tmp = [[o_modulearray objectAtIndex:
                    [val intValue]] mutableCopyWithZone:nil];
                [o_modulearray removeObject:o_tmp];
                [o_modulearray insertObject:o_tmp
                    atIndex:(dropRow>[val intValue]) ? dropRow - 1 : dropRow];
                dropRow++;
            }

            // Select the newly-dragged items.
            iter = [array objectEnumerator];
//TODO...
            [table deselectAll:self];

            [self tableChanged:self];
            [table setNeedsDisplay:YES];
            // Indicate that we finished the drag.
            accepted = YES;
        }
        [table reloadData];
        [table setNeedsDisplay:YES];

        NS_HANDLER

            // An exception occurred. Uh-oh. Update the track table so that
            // it stays consistent, and re-raise the exception.
            [table reloadData];
            [localException raise];
            [table setNeedsDisplay:YES];
    NS_ENDHANDLER

    return accepted;
}

- (int)numberOfRowsInTableView:(NSTableView *)aTableView
{
    return [o_modulearray count];
}

- (id)tableView:(NSTableView *)aTableView
    objectValueForTableColumn:(NSTableColumn *)aTableColumn row:(int)rowIndex
{
    if( [[aTableColumn identifier] isEqualToString:
        [NSString stringWithCString:"Enabled"]] )
        return [[o_modulearray objectAtIndex:rowIndex] objectAtIndex:2];
    if( [[aTableColumn identifier] isEqualToString:
        [NSString stringWithCString:"Module"]] )
        return [[o_modulearray objectAtIndex:rowIndex] objectAtIndex:1];

    return nil;
}

- (void)tableView:(NSTableView *)aTableView setObjectValue:(id)anObject
    forTableColumn:(NSTableColumn *)aTableColumn row:(int)rowIndex
{
    [[o_modulearray objectAtIndex:rowIndex] replaceObjectAtIndex:2
        withObject: anObject];
}
@end
