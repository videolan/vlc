/*****************************************************************************
 * prefs_widgets.m: Preferences controls
 *****************************************************************************
 * Copyright (C) 2002-2012 VLC authors and VideoLAN
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <stdlib.h>                                      /* malloc(), free() */
#include <string.h>

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_modules.h>
#include <vlc_plugin.h>
#include <vlc_keys.h>

#include "intf.h"
#include "prefs_widgets.h"

#define PREFS_WRAP 300
#define OFFSET_RIGHT 20
#define OFFSET_BETWEEN 2

#define UPWARDS_WHITE_ARROW                 "\xE2\x87\xA7"
#define OPTION_KEY                          "\xE2\x8C\xA5"
#define UP_ARROWHEAD                        "\xE2\x8C\x83"
#define PLACE_OF_INTEREST_SIGN              "\xE2\x8C\x98"

#define POPULATE_A_KEY(o_menu, string, value)                               \
{                                                                           \
    NSMenuItem *o_mi;                                                       \
/*  Normal */                                                               \
    o_mi = [[NSMenuItem alloc] initWithTitle:string                         \
        action:nil keyEquivalent:@""];                                      \
    [o_mi setKeyEquivalentModifierMask:                                     \
        0];                                                                 \
    [o_mi setAlternate: NO];                                                \
    [o_mi setTag:                                                           \
        (value)];                                                           \
    [o_menu addItem: o_mi];                                                 \
/*  Ctrl */                                                                 \
    o_mi = [[NSMenuItem alloc] initWithTitle:                               \
        [[NSString stringWithUTF8String:UP_ARROWHEAD]                       \
          stringByAppendingString: string]                                  \
        action:nil keyEquivalent:@""];                                      \
    [o_mi setKeyEquivalentModifierMask:                                     \
        NSControlKeyMask];                                                  \
    [o_mi setAlternate: YES];                                               \
    [o_mi setTag:                                                           \
        KEY_MODIFIER_CTRL | (value)];                                       \
    [o_menu addItem: o_mi];                                                 \
/* Ctrl+Alt */                                                              \
    o_mi = [[NSMenuItem alloc] initWithTitle:                               \
        [[NSString stringWithUTF8String:UP_ARROWHEAD OPTION_KEY]            \
          stringByAppendingString: string]                                  \
        action:nil keyEquivalent:@""];                                      \
    [o_mi setKeyEquivalentModifierMask:                                     \
        NSControlKeyMask | NSAlternateKeyMask];                             \
    [o_mi setAlternate: YES];                                               \
    [o_mi setTag:                                                           \
        (KEY_MODIFIER_CTRL | KEY_MODIFIER_ALT) | (value)];                  \
    [o_menu addItem: o_mi];                                                 \
/* Ctrl+Shift */                                                            \
    o_mi = [[NSMenuItem alloc] initWithTitle:                               \
        [[NSString stringWithUTF8String:UP_ARROWHEAD UPWARDS_WHITE_ARROW]   \
          stringByAppendingString: string]                                  \
        action:nil keyEquivalent:@""];                                      \
    [o_mi setKeyEquivalentModifierMask:                                     \
       NSControlKeyMask | NSShiftKeyMask];                                  \
    [o_mi setAlternate: YES];                                               \
    [o_mi setTag:                                                           \
        (KEY_MODIFIER_CTRL | KEY_MODIFIER_SHIFT) | (value)];                \
    [o_menu addItem: o_mi];                                                 \
/* Ctrl+Apple */                                                            \
    o_mi = [[NSMenuItem alloc] initWithTitle:                               \
        [[NSString stringWithUTF8String:UP_ARROWHEAD PLACE_OF_INTEREST_SIGN]\
          stringByAppendingString: string]                                  \
        action:nil keyEquivalent:@""];                                      \
    [o_mi setKeyEquivalentModifierMask:                                     \
        NSControlKeyMask | NSCommandKeyMask];                               \
    [o_mi setAlternate: YES];                                               \
    [o_mi setTag:                                                           \
        (KEY_MODIFIER_CTRL | KEY_MODIFIER_COMMAND) | (value)];              \
    [o_menu addItem: o_mi];                                                 \
/* Ctrl+Alt+Shift */                                                        \
    o_mi = [[NSMenuItem alloc] initWithTitle:                               \
        [[NSString stringWithUTF8String:UP_ARROWHEAD OPTION_KEY UPWARDS_WHITE_ARROW] \
          stringByAppendingString: string]                                  \
        action:nil keyEquivalent:@""];                                      \
    [o_mi setKeyEquivalentModifierMask:                                     \
        NSControlKeyMask | NSAlternateKeyMask | NSShiftKeyMask];            \
    [o_mi setAlternate: YES];                                               \
    [o_mi setTag:                                                           \
        (KEY_MODIFIER_CTRL | KEY_MODIFIER_ALT | KEY_MODIFIER_SHIFT) |       \
             (value)];                                                      \
    [o_menu addItem: o_mi];                                                 \
/* Ctrl+Alt+Apple */                                                        \
    o_mi = [[NSMenuItem alloc] initWithTitle:                               \
        [[NSString stringWithUTF8String:UP_ARROWHEAD OPTION_KEY PLACE_OF_INTEREST_SIGN] \
          stringByAppendingString: string]                                  \
        action:nil keyEquivalent:@""];                                      \
    [o_mi setKeyEquivalentModifierMask:                                     \
        NSControlKeyMask | NSAlternateKeyMask | NSCommandKeyMask];          \
    [o_mi setAlternate: YES];                                               \
    [o_mi setTag:                                                           \
        (KEY_MODIFIER_CTRL | KEY_MODIFIER_ALT | KEY_MODIFIER_COMMAND) |     \
            (value)];                                                       \
    [o_menu addItem: o_mi];                                                 \
/* Ctrl+Shift+Apple */                                                      \
    o_mi = [[NSMenuItem alloc] initWithTitle:                               \
        [[NSString stringWithUTF8String:UP_ARROWHEAD UPWARDS_WHITE_ARROW PLACE_OF_INTEREST_SIGN] \
          stringByAppendingString: string]                                  \
        action:nil keyEquivalent:@""];                                      \
    [o_mi setKeyEquivalentModifierMask:                                     \
        NSControlKeyMask | NSShiftKeyMask | NSCommandKeyMask];              \
    [o_mi setAlternate: YES];                                               \
    [o_mi setTag:                                                           \
        (KEY_MODIFIER_CTRL | KEY_MODIFIER_SHIFT | KEY_MODIFIER_COMMAND) |   \
            (value)];                                                       \
    [o_menu addItem: o_mi];                                                 \
/* Ctrl+Alt+Shift+Apple */                                                  \
    o_mi = [[NSMenuItem alloc] initWithTitle:                               \
        [[NSString stringWithUTF8String:UP_ARROWHEAD OPTION_KEY UPWARDS_WHITE_ARROW PLACE_OF_INTEREST_SIGN] \
         stringByAppendingString: string]                                  \
        action:nil keyEquivalent:@""];                                      \
    [o_mi setKeyEquivalentModifierMask:                                     \
        NSControlKeyMask | NSAlternateKeyMask | NSShiftKeyMask |            \
            NSCommandKeyMask];                                              \
    [o_mi setAlternate: YES];                                               \
    [o_mi setTag:                                                           \
        (KEY_MODIFIER_CTRL | KEY_MODIFIER_ALT | KEY_MODIFIER_SHIFT |        \
            KEY_MODIFIER_COMMAND) | (value)];                               \
    [o_menu addItem: o_mi];                                                 \
/* Alt */                                                                   \
    o_mi = [[NSMenuItem alloc] initWithTitle:                               \
        [[NSString stringWithUTF8String:OPTION_KEY] stringByAppendingString: string] \
        action:nil keyEquivalent:@""];                                      \
    [o_mi setKeyEquivalentModifierMask:                                     \
        NSAlternateKeyMask];                                                \
    [o_mi setAlternate: YES];                                               \
    [o_mi setTag:                                                           \
        KEY_MODIFIER_ALT | (value)];                                        \
    [o_menu addItem: o_mi];                                                 \
/* Alt+Shift */                                                             \
    o_mi = [[NSMenuItem alloc] initWithTitle:                               \
        [[NSString stringWithUTF8String:OPTION_KEY UPWARDS_WHITE_ARROW] stringByAppendingString: string] \
        action:nil keyEquivalent:@""];                                      \
    [o_mi setKeyEquivalentModifierMask:                                     \
        NSAlternateKeyMask | NSShiftKeyMask];                               \
    [o_mi setAlternate: YES];                                               \
    [o_mi setTag:                                                           \
        (KEY_MODIFIER_ALT | KEY_MODIFIER_SHIFT) | (value)];                 \
    [o_menu addItem: o_mi];                                                 \
/* Alt+Apple */                                                             \
    o_mi = [[NSMenuItem alloc] initWithTitle:                               \
        [[NSString stringWithUTF8String:OPTION_KEY PLACE_OF_INTEREST_SIGN]  \
         stringByAppendingString: string] action:nil keyEquivalent:@""];    \
    [o_mi setKeyEquivalentModifierMask:                                     \
        NSAlternateKeyMask | NSCommandKeyMask];                             \
    [o_mi setAlternate: YES];                                               \
    [o_mi setTag:                                                           \
        (KEY_MODIFIER_ALT | KEY_MODIFIER_COMMAND) | (value)];               \
    [o_menu addItem: o_mi];                                                 \
/* Alt+Shift+Apple */                                                       \
    o_mi = [[NSMenuItem alloc] initWithTitle:                               \
        [[NSString stringWithUTF8String:OPTION_KEY UPWARDS_WHITE_ARROW PLACE_OF_INTEREST_SIGN] \
          stringByAppendingString: string]                                  \
        action:nil keyEquivalent:@""];                                      \
    [o_mi setKeyEquivalentModifierMask:                                     \
        NSAlternateKeyMask | NSShiftKeyMask | NSCommandKeyMask];            \
    [o_mi setAlternate: YES];                                               \
    [o_mi setTag:                                                           \
        (KEY_MODIFIER_ALT | KEY_MODIFIER_SHIFT | KEY_MODIFIER_COMMAND) |    \
            (value)];                                                       \
    [o_menu addItem: o_mi];                                                 \
/* Shift */                                                                 \
    o_mi = [[NSMenuItem alloc] initWithTitle:                               \
        [[NSString stringWithUTF8String:UPWARDS_WHITE_ARROW]                \
          stringByAppendingString: string]                                  \
        action:nil keyEquivalent:@""];                                      \
    [o_mi setKeyEquivalentModifierMask:                                     \
        NSShiftKeyMask];                                                    \
    [o_mi setAlternate: YES];                                               \
    [o_mi setTag:                                                           \
        KEY_MODIFIER_SHIFT | (value)];                                      \
    [o_menu addItem: o_mi];                                                 \
/* Shift+Apple */                                                           \
    o_mi = [[NSMenuItem alloc] initWithTitle:                               \
        [[NSString stringWithUTF8String:UPWARDS_WHITE_ARROW PLACE_OF_INTEREST_SIGN] \
         stringByAppendingString: string]                                   \
        action:nil keyEquivalent:@""];                                      \
    [o_mi setKeyEquivalentModifierMask:                                     \
        NSShiftKeyMask | NSCommandKeyMask];                                 \
    [o_mi setAlternate: YES];                                               \
    [o_mi setTag:                                                           \
        (KEY_MODIFIER_SHIFT | KEY_MODIFIER_COMMAND) | (value)];             \
    [o_menu addItem: o_mi];                                                 \
/* Apple */                                                                 \
    o_mi = [[NSMenuItem alloc] initWithTitle:                               \
        [[NSString stringWithUTF8String:PLACE_OF_INTEREST_SIGN]             \
          stringByAppendingString: string]                                  \
        action:nil keyEquivalent:@""];                                      \
    [o_mi setKeyEquivalentModifierMask:                                     \
        NSCommandKeyMask];                                                  \
    [o_mi setAlternate: YES];                                               \
    [o_mi setTag:                                                           \
        KEY_MODIFIER_COMMAND | (value)];                                    \
    [o_menu addItem: o_mi];                                                 \
}

#define ADD_LABEL(o_label, superFrame, x_offset, my_y_offset, label,        \
    tooltip)                                                                \
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
    [o_label setToolTip: tooltip];                                          \
    [o_label setFont:[NSFont systemFontOfSize:0]];                          \
    [o_label sizeToFit];                                                    \
}

#define ADD_TEXTFIELD(o_textfield, superFrame, x_offset, my_y_offset,       \
    my_width, tooltip, init_value)                                          \
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

#define ADD_SECURETEXTFIELD(o_textfield, superFrame, x_offset, my_y_offset, \
my_width, tooltip, init_value)                                              \
{                                                                           \
NSRect s_rc = superFrame;                                                   \
s_rc.origin.x = x_offset;                                                   \
s_rc.origin.y = my_y_offset;                                                \
s_rc.size.height = 22;                                                      \
s_rc.size.width = my_width;                                                 \
o_textfield = [[[NSSecureTextField alloc] initWithFrame: s_rc] retain];     \
[o_textfield setFont:[NSFont systemFontOfSize:0]];                          \
[o_textfield setToolTip: tooltip];                                          \
[o_textfield setStringValue: init_value];                                   \
}

#define ADD_COMBO(o_combo, superFrame, x_offset, my_y_offset, x2_offset,    \
    tooltip)                                                                \
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

#define ADD_RIGHT_BUTTON(o_button, superFrame, x_offset, my_y_offset,       \
    tooltip, title)                                                         \
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

#define ADD_POPUP(o_popup, superFrame, x_offset, my_y_offset, x2_offset,    \
    tooltip)                                                                \
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

#define ADD_STEPPER(o_stepper, superFrame, x_offset, my_y_offset, tooltip,  \
    lower, higher)                                                          \
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

#define ADD_SLIDER(o_slider, superFrame, x_offset, my_y_offset, my_width,   \
    tooltip, lower, higher)                                                 \
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

#define ADD_CHECKBOX(o_checkbox, superFrame, x_offset, my_y_offset, label,  \
    tooltip, init_value, position)                                          \
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
@synthesize type = i_type, viewType = i_view_type, advanced = b_advanced;

- (id)initWithFrame: (NSRect)frame
{
    return [self initWithFrame: frame
                    item: nil];
}

- (id)initWithFrame: (NSRect)frame
        item: (module_config_t *)_p_item
{
    self = [super initWithFrame: frame];

    if (self != nil) {
        p_item = _p_item;
        psz_name = p_item->psz_name;
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
    if (o_label) [o_label release];
    free(psz_name);
    [super dealloc];
}

+ (int)calcVerticalMargin: (int)i_curItem lastItem: (int)i_lastItem
{
    int i_margin;
    switch(i_curItem) {
    case CONFIG_ITEM_STRING:
    case CONFIG_ITEM_PASSWORD:
        switch(i_lastItem) {
        case CONFIG_ITEM_STRING:
        case CONFIG_ITEM_PASSWORD:
            i_margin = 8;
            break;
        case CONFIG_ITEM_STRING_LIST:
            i_margin = 7;
            break;
        case CONFIG_ITEM_LOADFILE:
        case CONFIG_ITEM_SAVEFILE:
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
        case CONFIG_ITEM_KEY:
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
        switch(i_lastItem) {
        case CONFIG_ITEM_STRING:
        case CONFIG_ITEM_PASSWORD:
            i_margin = 8;
            break;
        case CONFIG_ITEM_STRING_LIST:
            i_margin = 7;
            break;
        case CONFIG_ITEM_LOADFILE:
        case CONFIG_ITEM_SAVEFILE:
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
        case CONFIG_ITEM_KEY:
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
    case CONFIG_ITEM_LOADFILE:
    case CONFIG_ITEM_SAVEFILE:
        switch(i_lastItem) {
        case CONFIG_ITEM_STRING:
        case CONFIG_ITEM_PASSWORD:
            i_margin = 13;
            break;
        case CONFIG_ITEM_STRING_LIST:
            i_margin = 10;
            break;
        case CONFIG_ITEM_LOADFILE:
        case CONFIG_ITEM_SAVEFILE:
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
        case CONFIG_ITEM_KEY:
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
        switch(i_lastItem) {
        case CONFIG_ITEM_STRING:
        case CONFIG_ITEM_PASSWORD:
            i_margin = 8;
            break;
        case CONFIG_ITEM_STRING_LIST:
            i_margin = 7;
            break;
        case CONFIG_ITEM_LOADFILE:
        case CONFIG_ITEM_SAVEFILE:
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
        case CONFIG_ITEM_KEY:
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
        switch(i_lastItem) {
        case CONFIG_ITEM_STRING:
        case CONFIG_ITEM_PASSWORD:
            i_margin = 8;
            break;
        case CONFIG_ITEM_STRING_LIST:
            i_margin = 7;
            break;
        case CONFIG_ITEM_LOADFILE:
        case CONFIG_ITEM_SAVEFILE:
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
        case CONFIG_ITEM_KEY:
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
        switch(i_lastItem) {
        case CONFIG_ITEM_STRING:
        case CONFIG_ITEM_PASSWORD:
            i_margin = 8;
            break;
        case CONFIG_ITEM_STRING_LIST:
            i_margin = 7;
            break;
        case CONFIG_ITEM_LOADFILE:
        case CONFIG_ITEM_SAVEFILE:
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
        case CONFIG_ITEM_KEY:
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
        switch(i_lastItem) {
        case CONFIG_ITEM_STRING:
        case CONFIG_ITEM_PASSWORD:
            i_margin = 10;
            break;
        case CONFIG_ITEM_STRING_LIST:
            i_margin = 9;
            break;
        case CONFIG_ITEM_LOADFILE:
        case CONFIG_ITEM_SAVEFILE:
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
        case CONFIG_ITEM_KEY:
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
    case CONFIG_ITEM_KEY:
        switch(i_lastItem) {
        case CONFIG_ITEM_STRING:
        case CONFIG_ITEM_PASSWORD:
            i_margin = 8;
            break;
        case CONFIG_ITEM_STRING_LIST:
            i_margin = 7;
            break;
        case CONFIG_ITEM_LOADFILE:
        case CONFIG_ITEM_SAVEFILE:
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
        case CONFIG_ITEM_KEY:
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
        switch(i_lastItem) {
        case CONFIG_ITEM_STRING:
        case CONFIG_ITEM_PASSWORD:
            i_margin = 10;
            break;
        case CONFIG_ITEM_STRING_LIST:
            i_margin = 7;
            break;
        case CONFIG_ITEM_LOADFILE:
        case CONFIG_ITEM_SAVEFILE:
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
        case CONFIG_ITEM_KEY:
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

    switch(_p_item->i_type) {
    case CONFIG_ITEM_STRING:
    case CONFIG_ITEM_PASSWORD:
        if (!_p_item->list_count) {
            p_control = [[StringConfigControl alloc]
                    initWithItem: _p_item
                    withView: o_parent_view];
        } else {
            p_control = [[StringListConfigControl alloc]
                    initWithItem: _p_item
                    withView: o_parent_view];
        }
        break;
    case CONFIG_ITEM_LOADFILE:
    case CONFIG_ITEM_SAVEFILE:
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
        if (_p_item->list_count)
            p_control = [[IntegerListConfigControl alloc] initWithItem: _p_item withView: o_parent_view];
        else if ((_p_item->min.i != 0 || _p_item->max.i != 0) && (_p_item->min.i != INT_MIN || _p_item->max.i != INT_MAX))
            p_control = [[RangedIntegerConfigControl alloc] initWithItem: _p_item withView: o_parent_view];
        else
            p_control = [[IntegerConfigControl alloc] initWithItem: _p_item withView: o_parent_view];
        break;
    case CONFIG_ITEM_BOOL:
        p_control = [[BoolConfigControl alloc]
                    initWithItem: _p_item
                    withView: o_parent_view];
        break;
    case CONFIG_ITEM_FLOAT:
        if ((_p_item->min.i != 0 || _p_item->max.i != 0) && (_p_item->min.i != INT_MIN || _p_item->max.i != INT_MAX))
            p_control = [[RangedFloatConfigControl alloc] initWithItem: _p_item withView: o_parent_view];
        else
            p_control = [[FloatConfigControl alloc] initWithItem: _p_item withView: o_parent_view];
        break;
    /* don't display keys in the advanced settings, since the current controls
    are broken by design. The user is required to change hotkeys in the sprefs
    and can only change really advanced stuff here..
    case CONFIG_ITEM_KEY:
        p_control = [[KeyConfigControl alloc]
                        initWithItem: _p_item
                        withView: o_parent_view];
        break; */
    case CONFIG_ITEM_MODULE_LIST:
    case CONFIG_ITEM_MODULE_LIST_CAT:
        p_control = [[ModuleListConfigControl alloc] initWithItem: _p_item withView: o_parent_view];
        break;
    case CONFIG_SECTION:
        p_control = [[SectionControl alloc] initWithItem: _p_item withView: o_parent_view];
        break;
    default:
        break;
    }
    return p_control;
}

- (NSString *)name
{
    return _NS(psz_name);
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
    switch(p_item->i_type) {
    case CONFIG_ITEM_STRING:
    case CONFIG_ITEM_PASSWORD:
    case CONFIG_ITEM_LOADFILE:
    case CONFIG_ITEM_SAVEFILE:
    case CONFIG_ITEM_DIRECTORY:
    case CONFIG_ITEM_MODULE:
    case CONFIG_ITEM_MODULE_LIST:
    case CONFIG_ITEM_MODULE_LIST_CAT: {
        char *psz_val = [self stringValue];
        config_PutPsz(VLCIntf, psz_name, psz_val);
        free(psz_val);
        break;
    }
    case CONFIG_ITEM_KEY:
        /* So you don't need to restart to have the changes take effect */
        val.i_int = [self intValue];
        var_Set(VLCIntf->p_libvlc, psz_name, val);
    case CONFIG_ITEM_INTEGER:
    case CONFIG_ITEM_BOOL:
        config_PutInt(VLCIntf, psz_name, [self intValue]);
        break;
    case CONFIG_ITEM_FLOAT:
        config_PutFloat(VLCIntf, psz_name, [self floatValue]);
        break;
    }
}

- (void)resetValues
{
}

- (int)labelSize
{
    return [o_label frame].size.width;
}

- (void) alignWithXPosition:(int)i_xPos;
{
    /* FIXME: not implemented atm, but created to shut up the warning
     * about "method definition not found" -- FK @ 7/24/05 */
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

    if ([super initWithFrame: mainFrame item: _p_item] != nil) {
        if (p_item->i_type == CONFIG_ITEM_PASSWORD)
            i_view_type = CONFIG_ITEM_PASSWORD;
        else
            i_view_type = CONFIG_ITEM_STRING;

        o_textfieldTooltip = [[VLCStringUtility sharedInstance] wrapString: _NS((char *)p_item->psz_longtext) toWidth: PREFS_WRAP];

        /* add the label */
        if (p_item->psz_text)
            o_labelString = _NS((char *)p_item->psz_text);
        else
            o_labelString = @"";
        ADD_LABEL(o_label, mainFrame, 0, -3, o_labelString, o_textfieldTooltip)
        [o_label setAutoresizingMask:NSViewNotSizable ];
        [self addSubview: o_label];

        /* build the textfield */
        if (p_item->value.psz)
            o_textfieldString = [NSString stringWithCString:p_item->value.psz encoding:NSUTF8StringEncoding];
        else
            o_textfieldString = @"";
        if (p_item->i_type == CONFIG_ITEM_PASSWORD) {
            ADD_SECURETEXTFIELD(o_textfield, mainFrame, [o_label frame].size.width + 2,
                          0, mainFrame.size.width - [o_label frame].size.width -
                          2, o_textfieldTooltip, o_textfieldString)
        } else {
            ADD_TEXTFIELD(o_textfield, mainFrame, [o_label frame].size.width + 2,
                            0, mainFrame.size.width - [o_label frame].size.width -
                            2, o_textfieldTooltip, o_textfieldString)
        }
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
    return strdup([[o_textfield stringValue] UTF8String]);
}

- (void)resetValues
{
    NSString *o_textfieldString;
    char *psz_value = config_GetPsz(VLCIntf, p_item->psz_name);
    if (psz_value)
        o_textfieldString = _NS(psz_value);
    else
        o_textfieldString = @"";
    free(psz_value);
    [super resetValues];
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

    if ([super initWithFrame: mainFrame item: _p_item] != nil) {
        i_view_type = CONFIG_ITEM_STRING_LIST;

        o_textfieldTooltip = [[VLCStringUtility sharedInstance] wrapString: _NS(p_item->psz_longtext) toWidth: PREFS_WRAP];

        /* add the label */
        if (p_item->psz_text)
            o_labelString = _NS((char *)p_item->psz_text);
        else
            o_labelString = @"";
        ADD_LABEL(o_label, mainFrame, 0, -3, o_labelString, o_textfieldTooltip)
        [o_label setAutoresizingMask:NSViewNotSizable ];
        [self addSubview: o_label];

        /* build the textfield */
        ADD_POPUP(o_popup, mainFrame, [o_label frame].size.width,
            -2, 0, o_textfieldTooltip)
        [o_popup setAutoresizingMask:NSViewWidthSizable];

        /* add items */
        for (int i_index = 0; i_index < p_item->list_count; i_index++) {
            NSString *o_text;
            if (p_item->list_text && p_item->list_text[i_index])
                o_text = _NS((char *)p_item->list_text[i_index]);
            else
                o_text = _NS((char *)p_item->list.psz[i_index]);
            [o_popup addItemWithTitle: o_text];

            /* select default item */
            if (!p_item->value.psz && !p_item->list.psz[i_index])
                [o_popup selectItemAtIndex: i_index];
            else if (p_item->value.psz && p_item->list.psz[i_index] &&
                     !strcmp(p_item->value.psz, p_item->list.psz[i_index]))
                [o_popup selectItemAtIndex: i_index];
        }

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
    frame.origin.x = i_xPos + 2;
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
    if ([o_popup indexOfSelectedItem] >= 0) {
        if (p_item->list.psz[[o_popup indexOfSelectedItem]] != NULL)
            return strdup(p_item->list.psz[[o_popup indexOfSelectedItem]]);
    }

    return NULL;
}

- (void)resetValues
{
    char *psz_value = config_GetPsz(VLCIntf, p_item->psz_name);

    for (int i_index = 0; i_index < p_item->list_count; i_index++) {
        if (!psz_value && !p_item->list.psz[i_index])
            [o_popup selectItemAtIndex: i_index];
        else if (psz_value && p_item->list.psz[i_index] &&
            !strcmp(psz_value, p_item->list.psz[i_index]))
            [o_popup selectItemAtIndex: i_index];
    }

    free(psz_value);
    [super resetValues];
}
@end

@implementation FileConfigControl
- (id) initWithItem: (module_config_t *)_p_item
           withView: (NSView *)o_parent_view
{
    NSRect mainFrame = [o_parent_view frame];
    NSString *o_labelString, *o_itemTooltip, *o_textfieldString;
    mainFrame.size.height = 46;
    mainFrame.size.width = mainFrame.size.width - LEFTMARGIN - RIGHTMARGIN;
    mainFrame.origin.x = LEFTMARGIN;
    mainFrame.origin.y = 0;

    if ([super initWithFrame: mainFrame item: _p_item] != nil) {
        i_view_type = CONFIG_ITEM_LOADFILE;

        o_itemTooltip = [[VLCStringUtility sharedInstance] wrapString: _NS((char *)p_item->psz_longtext) toWidth: PREFS_WRAP];

        /* is it a directory */
        b_directory = ([self type] == CONFIG_ITEM_DIRECTORY) ? YES : NO;

        /* add the label */
        if (p_item->psz_text)
            o_labelString = _NS((char *)p_item->psz_text);
        else
            o_labelString = @"";
        ADD_LABEL(o_label, mainFrame, 0, 3, o_labelString, o_itemTooltip)
        [o_label setAutoresizingMask:NSViewNotSizable ];
        [self addSubview: o_label];

        /* build the button */
        ADD_RIGHT_BUTTON(o_button, mainFrame, 0, 0, o_itemTooltip,
                            _NS("Browse..."))
        [o_button setAutoresizingMask:NSViewMinXMargin ];
        [self addSubview: o_button];

        /* build the textfield */
        if (p_item->value.psz)
            o_textfieldString = [NSString stringWithFormat: @"%s", (char *)p_item->value.psz];
        else
            o_textfieldString = @"";
        ADD_TEXTFIELD(o_textfield, mainFrame, 12, 2, mainFrame.size.width -
                        8 - [o_button frame].size.width,
                        o_itemTooltip, o_textfieldString)
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
    [o_open_panel beginSheetModalForWindow:[sender window] completionHandler:^(NSInteger returnCode) {
        if (returnCode == NSOKButton) {
            NSString *o_path = [[[o_open_panel URLs] objectAtIndex:0] path];
            [o_textfield setStringValue: o_path];
        }        
    }];
}

- (char *)stringValue
{
    if ([[o_textfield stringValue] length] != 0)
        return strdup([[o_textfield stringValue] fileSystemRepresentation]);
    else
        return NULL;
}

-(void)resetValues
{
    NSString *o_textfieldString;
    char *psz_value = config_GetPsz(VLCIntf, p_item->psz_name);
    if (psz_value)
        o_textfieldString = [NSString stringWithFormat: @"%s", psz_value];
    else
        o_textfieldString = @"";

    free(psz_value);
    [super resetValues];
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

    if ([super initWithFrame: mainFrame item: _p_item] != nil) {
        i_view_type = CONFIG_ITEM_MODULE;

        o_popupTooltip = [[VLCStringUtility sharedInstance] wrapString: _NS((char *)p_item->psz_longtext) toWidth: PREFS_WRAP];

        /* add the label */
        if (p_item->psz_text)
            o_labelString = _NS((char *)p_item->psz_text);
        else
            o_labelString = @"";

        ADD_LABEL(o_label, mainFrame, 0, -1, o_labelString, o_popupTooltip)
        [o_label setAutoresizingMask:NSViewNotSizable ];
        [self addSubview: o_label];

        /* build the popup */
        ADD_POPUP(o_popup, mainFrame, [o_label frame].size.width,
            -2, 0, o_popupTooltip)
        [o_popup setAutoresizingMask:NSViewWidthSizable ];
        [o_popup addItemWithTitle: _NS("Default")];
        [[o_popup lastItem] setTag: -1];
        [o_popup selectItem: [o_popup lastItem]];

        [self resetValues];
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
    size_t i_module_index;
    module_t *p_parser, **p_list;

    size_t count;
    p_list = module_list_get(&count);
    for (i_module_index = 0; i_module_index < count; i_module_index++) {
        p_parser = p_list[i_module_index];

        if (p_item->i_type == CONFIG_ITEM_MODULE) {
            if (module_provides(p_parser, p_item->psz_type)) {
                NSString *o_description = _NS(module_get_name(p_parser, TRUE));
                if ([newval isEqualToString: o_description]) {
                    returnval = strdup(module_get_object(p_parser));
                    break;
                }
            }
        } else {
            if (module_is_main(p_parser))
                continue;

            unsigned int confsize;
            module_config_t *p_config = module_config_get(p_parser, &confsize);
            for (size_t i = 0; i < confsize; i++) {
                module_config_t *p_cfg = p_config + i;
                /* Hack: required subcategory is stored in i_min */
                if (p_cfg->i_type == CONFIG_SUBCATEGORY &&
                    p_cfg->value.i == p_cfg->min.i) {
                    NSString *o_description = _NS(module_get_name(p_parser, TRUE));
                    if ([newval isEqualToString: o_description]) {
                        returnval = strdup(module_get_object(p_parser));
                        break;
                    }
                }
            }
            module_config_free(p_config);
        }
    }
    module_list_free(p_list);

    if(returnval == NULL && [newval isEqualToString: _NS("Default")] && p_item->orig.psz != NULL) {
        returnval = strdup(p_item->orig.psz);
    }
    return returnval;
}

-(void)resetValues
{
    /* build a list of available modules */
    module_t *p_parser, **p_list;

    size_t count;
    p_list = module_list_get(&count);
    for (size_t i_index = 0; i_index < count; i_index++) {
        p_parser = p_list[i_index];

        if (p_item->i_type == CONFIG_ITEM_MODULE) {
            if (module_provides(p_parser, p_item->psz_type)) {
                NSString *o_description = _NS(module_get_name(p_parser, TRUE));
                [o_popup addItemWithTitle: o_description];
                char *psz_value = config_GetPsz(VLCIntf, p_item->psz_name);

                if (psz_value &&
                    !strcmp(psz_value, module_get_object(p_parser)))
                    [o_popup selectItem:[o_popup lastItem]];

                free(psz_value);
            }
        } else {
            if (module_is_main(p_parser))
                continue;
            unsigned int confsize;

            module_config_t *p_configlist = module_config_get(p_parser, &confsize);
            for (size_t i = 0; i < confsize; i++) {
                module_config_t *p_config = &p_configlist[i];
                /* Hack: required subcategory is stored in i_min */
                if (p_config->i_type == CONFIG_SUBCATEGORY &&
                    p_config->value.i == p_item->min.i) {
                    NSString *o_description = _NS(module_get_name(p_parser, TRUE));
                    [o_popup addItemWithTitle: o_description];

                    if (p_item->value.psz && !strcmp(p_item->value.psz,
                                            module_get_object(p_parser)))
                        [o_popup selectItem:[o_popup lastItem]];
                }
            }
            module_config_free(p_configlist);
        }
    }
    module_list_free(p_list);
    [super resetValues];
}
@end

@implementation IntegerConfigControl
- (id) initWithItem: (module_config_t *)_p_item
           withView: (NSView *)o_parent_view
{
    NSRect mainFrame = [o_parent_view frame];
    NSString *o_labelString, *o_tooltip;
    mainFrame.size.height = 23;
    mainFrame.size.width = mainFrame.size.width - LEFTMARGIN - RIGHTMARGIN + 1;
    mainFrame.origin.x = LEFTMARGIN;
    mainFrame.origin.y = 0;

    if ([super initWithFrame: mainFrame item: _p_item] != nil) {
        i_view_type = CONFIG_ITEM_INTEGER;

        o_tooltip = [[VLCStringUtility sharedInstance] wrapString: _NS((char *)p_item->psz_longtext) toWidth: PREFS_WRAP];

        /* add the label */
        if (p_item->psz_text)
            o_labelString = _NS((char *)p_item->psz_text);
        else
            o_labelString = @"";
        ADD_LABEL(o_label, mainFrame, 0, -3, o_labelString, o_tooltip)
        [o_label setAutoresizingMask:NSViewNotSizable ];
        [self addSubview: o_label];

        /* build the stepper */
        ADD_STEPPER(o_stepper, mainFrame, mainFrame.size.width - 19,
            0, o_tooltip, -100000, 100000)
        [o_stepper setIntValue: p_item->value.i];
        [o_stepper setAutoresizingMask:NSViewMaxXMargin ];
        [self addSubview: o_stepper];

        ADD_TEXTFIELD(o_textfield, mainFrame, mainFrame.size.width - 19 - 52,
            1, 49, o_tooltip, @"")
        [o_textfield setIntValue: p_item->value.i];
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

-(void)resetValues
{
    [o_textfield setIntValue: config_GetInt(VLCIntf, p_item->psz_name)];
    [super resetValues];
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

    if ([super initWithFrame: mainFrame item: _p_item] != nil) {
        i_view_type = CONFIG_ITEM_STRING_LIST;

        o_textfieldTooltip = [[VLCStringUtility sharedInstance] wrapString: _NS((char *)p_item->psz_longtext) toWidth: PREFS_WRAP];

        /* add the label */
        if (p_item->psz_text)
            o_labelString = _NS((char *)p_item->psz_text);
        else
            o_labelString = @"";
        ADD_LABEL(o_label, mainFrame, 0, -3, o_labelString, o_textfieldTooltip)
        [o_label setAutoresizingMask:NSViewNotSizable ];
        [self addSubview: o_label];

        /* build the textfield */
        ADD_POPUP(o_popup, mainFrame, [o_label frame].size.width,
            -2, 0, o_textfieldTooltip)
        [o_popup setAutoresizingMask:NSViewWidthSizable ];

        /* add items */
        for (int i_index = 0; i_index < p_item->list_count; i_index++) {
            NSString *o_text;
            if (p_item->list_text && p_item->list_text[i_index])
                o_text = _NS((char *)p_item->list_text[i_index]);
            else
                o_text = [NSString stringWithFormat: @"%i", p_item->list.i[i_index]];
            [o_popup addItemWithTitle: o_text];

            if (p_item->value.i == p_item->list.i[i_index])
                [o_popup selectItemAtIndex: i_index];
        }
        
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
    frame.origin.x = i_xPos + 2;
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
    if ([o_popup indexOfSelectedItem] >= 0)
        return p_item->list.i[[o_popup indexOfSelectedItem]];
    else
        return [o_popup intValue];
}

-(void)resetValues
{
    for (int i_index = 0; i_index < p_item->list_count; i_index++) {
        if (config_GetInt(VLCIntf, p_item->psz_name) == p_item->list.i[i_index])
            [o_popup selectItemAtIndex: i_index];
    }

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

    if ([super initWithFrame: mainFrame item: _p_item] != nil) {
        i_view_type = CONFIG_ITEM_RANGED_INTEGER;

        o_tooltip = [[VLCStringUtility sharedInstance] wrapString: _NS((char *)p_item->psz_longtext) toWidth: PREFS_WRAP];

        /* add the label */
        if (p_item->psz_text)
            o_labelString = _NS((char *)p_item->psz_text);
        else
            o_labelString = @"";
        ADD_LABEL(o_label, mainFrame, 0, -3, o_labelString, o_tooltip)
        [o_label setAutoresizingMask:NSViewNotSizable ];
        [self addSubview: o_label];

        /* build the textfield */
        ADD_TEXTFIELD(o_textfield, mainFrame, [o_label frame].size.width + 2,
            28, 49, o_tooltip, @"")
        [o_textfield setIntValue: p_item->value.i];
        [o_textfield setAutoresizingMask:NSViewMaxXMargin ];
        [o_textfield setDelegate: self];
        [[NSNotificationCenter defaultCenter] addObserver: self
            selector: @selector(textfieldChanged:)
            name: NSControlTextDidChangeNotification
            object: o_textfield];
        [self addSubview: o_textfield];

        /* build the mintextfield */
        ADD_LABEL(o_textfield_min, mainFrame, 12, -30, @"-8888", @"")
        [o_textfield_min setIntValue: p_item->min.i];
        [o_textfield_min setAutoresizingMask:NSViewMaxXMargin ];
        [o_textfield_min setAlignment:NSRightTextAlignment];
        [self addSubview: o_textfield_min];

        /* build the maxtextfield */
        ADD_LABEL(o_textfield_max, mainFrame,
                    mainFrame.size.width - 31, -30, @"8888", @"")
        [o_textfield_max setIntValue: p_item->max.i];
        [o_textfield_max setAutoresizingMask:NSViewMinXMargin ];
        [self addSubview: o_textfield_max];

        /* build the slider */
        ADD_SLIDER(o_slider, mainFrame, [o_textfield_min frame].origin.x +
            [o_textfield_min frame].size.width + 6, -1, mainFrame.size.width -
            [o_textfield_max frame].size.width -
            [o_textfield_max frame].size.width - 14 -
            [o_textfield_min frame].origin.x, o_tooltip,
            p_item->min.i, p_item->max.i)
        [o_slider setIntValue: p_item->value.i];
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

- (void)resetValues
{
    int value = config_GetInt(VLCIntf, p_item->psz_name);
    [o_textfield setIntValue:value];
    [o_slider setIntValue:value];
    [super resetValues];
}
@end

@implementation FloatConfigControl
- (id) initWithItem: (module_config_t *)_p_item
           withView: (NSView *)o_parent_view
{
    NSRect mainFrame = [o_parent_view frame];
    NSString *o_labelString, *o_tooltip;
    mainFrame.size.height = 23;
    mainFrame.size.width = mainFrame.size.width - LEFTMARGIN - RIGHTMARGIN + 1;
    mainFrame.origin.x = LEFTMARGIN;
    mainFrame.origin.y = 0;

    if ([super initWithFrame: mainFrame item: _p_item] != nil) {
        i_view_type = CONFIG_ITEM_INTEGER;

        o_tooltip = [[VLCStringUtility sharedInstance] wrapString: _NS((char *)p_item->psz_longtext) toWidth: PREFS_WRAP];

        /* add the label */
        if (p_item->psz_text)
            o_labelString = _NS((char *)p_item->psz_text);
        else
            o_labelString = @"";
        ADD_LABEL(o_label, mainFrame, 0, -2, o_labelString, o_tooltip)
        [o_label setAutoresizingMask:NSViewNotSizable ];
        [self addSubview: o_label];

        /* build the stepper */
        ADD_STEPPER(o_stepper, mainFrame, mainFrame.size.width - 19,
            0, o_tooltip, -100000, 100000)
        [o_stepper setFloatValue: p_item->value.f];
        [o_stepper setAutoresizingMask:NSViewMaxXMargin ];
        [self addSubview: o_stepper];

        /* build the textfield */
        ADD_TEXTFIELD(o_textfield, mainFrame, mainFrame.size.width - 19 - 52,
            1, 49, o_tooltip, @"")
        [o_textfield setFloatValue: p_item->value.f];
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

- (float)floatValue
{
    return [o_stepper floatValue];
}

- (void)resetValues
{
    [o_textfield setFloatValue: config_GetFloat(VLCIntf, p_item->psz_name)];
    [super resetValues];
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

    if ([super initWithFrame: mainFrame item: _p_item] != nil) {
        i_view_type = CONFIG_ITEM_RANGED_INTEGER;

        o_tooltip = [[VLCStringUtility sharedInstance] wrapString: _NS((char *)p_item->psz_longtext) toWidth: PREFS_WRAP];

        /* add the label */
        if (p_item->psz_text)
            o_labelString = _NS((char *)p_item->psz_text);
        else
            o_labelString = @"";
        ADD_LABEL(o_label, mainFrame, 0, -3, o_labelString, o_tooltip)
        [o_label setAutoresizingMask:NSViewNotSizable ];
        [self addSubview: o_label];

        /* build the textfield */
        ADD_TEXTFIELD(o_textfield, mainFrame, [o_label frame].size.width + 2,
            28, 49, o_tooltip, @"")
        [o_textfield setFloatValue: p_item->value.f];
        [o_textfield setAutoresizingMask:NSViewMaxXMargin ];
        [o_textfield setDelegate: self];
        [[NSNotificationCenter defaultCenter] addObserver: self
            selector: @selector(textfieldChanged:)
            name: NSControlTextDidChangeNotification
            object: o_textfield];
        [self addSubview: o_textfield];

        /* build the mintextfield */
        ADD_LABEL(o_textfield_min, mainFrame, 12, -30, @"-8888", @"")
        [o_textfield_min setFloatValue: p_item->min.f];
        [o_textfield_min setAutoresizingMask:NSViewMaxXMargin ];
        [o_textfield_min setAlignment:NSRightTextAlignment];
        [self addSubview: o_textfield_min];

        /* build the maxtextfield */
        ADD_LABEL(o_textfield_max, mainFrame, mainFrame.size.width - 31,
            -30, @"8888", @"")
        [o_textfield_max setFloatValue: p_item->max.f];
        [o_textfield_max setAutoresizingMask:NSViewMinXMargin ];
        [self addSubview: o_textfield_max];

        /* build the slider */
        ADD_SLIDER(o_slider, mainFrame, [o_textfield_min frame].origin.x +
            [o_textfield_min frame].size.width + 6, -1, mainFrame.size.width -
            [o_textfield_max frame].size.width -
            [o_textfield_max frame].size.width - 14 -
            [o_textfield_min frame].origin.x, o_tooltip, p_item->min.f,
            p_item->max.f)
        [o_slider setFloatValue: p_item->value.f];
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

- (float)floatValue
{
    return [o_slider floatValue];
}

- (void)resetValues
{
    [o_textfield setFloatValue: config_GetFloat(VLCIntf, p_item->psz_name)];
    [o_slider setFloatValue: config_GetFloat(VLCIntf, p_item->psz_name)];
    [super resetValues];
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

    if ([super initWithFrame: mainFrame item: _p_item] != nil) {
        i_view_type = CONFIG_ITEM_BOOL;

        if (p_item->psz_text)
            o_labelString = _NS((char *)p_item->psz_text);
        else
            o_labelString = @"";

        o_tooltip = [[VLCStringUtility sharedInstance] wrapString: _NS((char *)p_item->psz_longtext) toWidth: PREFS_WRAP];

        /* add the checkbox */
        ADD_CHECKBOX(o_checkbox, mainFrame, 0,
                        0, o_labelString, o_tooltip, p_item->value.i, NSImageLeft)
        [o_checkbox setAutoresizingMask:NSViewNotSizable ];
        [self addSubview: o_checkbox];
    }
    return self;
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

- (void)resetValues
{
    [o_checkbox setState: config_GetInt(VLCIntf, p_item->psz_name)];
    [super resetValues];
}
@end

@implementation KeyConfigControl
- (id) initWithItem: (module_config_t *)_p_item
           withView: (NSView *)o_parent_view
{
    NSRect mainFrame = [o_parent_view frame];
    NSString *o_labelString, *o_tooltip;
    mainFrame.size.height = 22;
    mainFrame.size.width = mainFrame.size.width - LEFTMARGIN - RIGHTMARGIN + 1;
    mainFrame.origin.x = LEFTMARGIN;
    mainFrame.origin.y = 0;

    if ([super initWithFrame: mainFrame item: _p_item] != nil) {
        i_view_type = CONFIG_ITEM_KEY;

        o_tooltip = [[VLCStringUtility sharedInstance] wrapString: _NS((char *)p_item->psz_longtext) toWidth: PREFS_WRAP];

        /* add the label */
        if (p_item->psz_text)
            o_labelString = _NS((char *)p_item->psz_text);
        else
            o_labelString = @"";
        ADD_LABEL(o_label, mainFrame, 0, -1, o_labelString, o_tooltip)
        [o_label setAutoresizingMask:NSViewNotSizable ];
        [self addSubview: o_label];

        /* build the popup */
        ADD_POPUP(o_popup, mainFrame, [o_label frame].origin.x +
            [o_label frame].size.width + 3,
            -2, 0, o_tooltip)
        [o_popup setAutoresizingMask:NSViewWidthSizable ];

        if (o_keys_menu == nil) {
            unsigned int i;
            o_keys_menu = [[NSMenu alloc] initWithTitle: @"Keys Menu"];
#warning This does not work anymore. FIXME.
#if 0
            for (i = 0; i < sizeof(vlc_key) / sizeof(key_descriptor_t); i++)
                if (vlc_key[i].psz_key_string)
                    POPULATE_A_KEY(o_keys_menu,
                        [NSString stringWithUTF8String:vlc_key[i].psz_key_string],
                                   vlc_key[i].i_key_code)
#endif
        }
        [o_popup setMenu:[o_keys_menu copyWithZone:nil]];
        [o_popup selectItem:[[o_popup menu] itemWithTag:p_item->value.i]];
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

- (void)resetValues
{
    [o_popup selectItem:[[o_popup menu] itemWithTag:config_GetInt(VLCIntf, p_item->psz_name)]];
    [super resetValues];
}
@end

@implementation ModuleListConfigControl
- (id) initWithItem: (module_config_t *)_p_item
           withView: (NSView *)o_parent_view
{
    if (_p_item->i_type == CONFIG_ITEM_MODULE_LIST)
        //TODO....
        return nil;

    //Fill our array to know how may items we have...
    module_t *p_parser, **p_list;
    size_t i_module_index;
    NSRect mainFrame = [o_parent_view frame];
    NSString *o_labelString, *o_textfieldString, *o_tooltip;

    o_modulearray = [[NSMutableArray alloc] initWithCapacity:10];
    /* build a list of available modules */
    size_t count;
    p_list = module_list_get(&count);
    for (i_module_index = 0; i_module_index < count; i_module_index++) {
        int i;
        p_parser = p_list[i_module_index];

        if (module_is_main(p_parser))
            continue;

        unsigned int confsize;
        module_config_t *p_configlist = module_config_get(p_parser, &confsize);

        for (i = 0; i < confsize; i++) {
            unsigned int unused;
            module_config_t *p_config = &p_configlist[i];
            NSString *o_modulelongname, *o_modulename;
            NSNumber *o_moduleenabled = nil;

            /* Hack: required subcategory is stored in i_min */
            if (p_config->i_type == CONFIG_SUBCATEGORY &&
               p_config->value.i == _p_item->min.i) {
                o_modulelongname = [NSString stringWithUTF8String:module_get_name(p_parser, TRUE)];
                o_modulename = [NSString stringWithUTF8String:module_get_object(p_parser)];

                if (_p_item->value.psz &&
                   strstr(_p_item->value.psz, module_get_object(p_parser)))
                    o_moduleenabled = [NSNumber numberWithBool:YES];
                else
                    o_moduleenabled = [NSNumber numberWithBool:NO];

                [o_modulearray addObject:[NSMutableArray
                                          arrayWithObjects: o_modulename, o_modulelongname,
                                          o_moduleenabled, nil]];
            }

            /* Parental Advisory HACK:
             * Selecting HTTP, RC and Telnet interfaces is difficult now
             * since they are just the lua interface module */
            if (p_config->i_type == CONFIG_SUBCATEGORY &&
               !strcmp(module_get_object(p_parser), "lua") &&
               !strcmp(_p_item->psz_name, "extraintf") &&
               p_config->value.i == _p_item->min.i) {

#define addLuaIntf(shortname, longname) \
                if (_p_item->value.psz && strstr(_p_item->value.psz, shortname))\
                    o_moduleenabled = [NSNumber numberWithBool:YES];\
                else\
                    o_moduleenabled = [NSNumber numberWithBool:NO];\
                [o_modulearray addObject:[NSMutableArray arrayWithObjects: @shortname, _NS(longname), o_moduleenabled, nil]]

                addLuaIntf("http", "Web");
                addLuaIntf("telnet", "Telnet");
                addLuaIntf("cli", "Console");

#undef addLuaIntf
            }
        }
        module_config_free(p_configlist);
    }
    module_list_free(p_list);

    mainFrame.size.height = 30 + 20 * [o_modulearray count];
    mainFrame.size.width = mainFrame.size.width - LEFTMARGIN - RIGHTMARGIN;
    mainFrame.origin.x = LEFTMARGIN;
    mainFrame.origin.y = 0;
    if ([super initWithFrame: mainFrame item: _p_item] != nil) {
        i_view_type = CONFIG_ITEM_MODULE_LIST;

        o_tooltip = [[VLCStringUtility sharedInstance] wrapString: _NS((char *)p_item->psz_longtext) toWidth: PREFS_WRAP];

        /* add the label */
        if (p_item->psz_text)
            o_labelString = _NS((char *)p_item->psz_text);
        else
            o_labelString = @"";
        ADD_LABEL(o_label, mainFrame, 0, -3, o_labelString, o_tooltip)
        [o_label setAutoresizingMask:NSViewNotSizable ];
        [self addSubview: o_label];

        /* build the textfield */
        if (p_item->value.psz)
            o_textfieldString = _NS((char *)p_item->value.psz);
        else
            o_textfieldString = @"";
        ADD_TEXTFIELD(o_textfield, mainFrame, [o_label frame].size.width + 2,
                      mainFrame.size.height - 22, mainFrame.size.width -
                      [o_label frame].size.width - 2, o_tooltip, o_textfieldString)
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
                                            initWithIdentifier:@"Enabled"];
            [o_tableColumn setHeaderCell: o_headerCell];
            [o_tableColumn setDataCell: o_dataCell];
            [o_tableColumn setWidth:17];
            [o_tableview addTableColumn: o_tableColumn];

            o_headerCell = [[NSCell alloc] initTextCell:@"Module Name"];
            o_dataCell = [[NSTextFieldCell alloc] init];
            [o_dataCell setFont:[NSFont systemFontOfSize:12]];
            o_tableColumn = [[NSTableColumn alloc]
                             initWithIdentifier:@"Module"];
            [o_tableColumn setHeaderCell: o_headerCell];
            [o_tableColumn setDataCell: o_dataCell];
            [o_tableColumn setWidth:s_rc.size.width - 34];
            [o_tableview addTableColumn: o_tableColumn];
            [o_tableview registerForDraggedTypes:[NSArray arrayWithObject:@"VLC media player module"]];

            [o_tableview setDataSource:self];
            [o_tableview setTarget: self];
            [o_tableview setAction: @selector(tableChanged:)];
            [o_tableview sendActionOn:NSLeftMouseUpMask | NSLeftMouseDownMask |
             NSLeftMouseDraggedMask];
            [o_scrollview setDocumentView: o_tableview];
        }
        [o_scrollview setAutoresizingMask:NSViewWidthSizable ];
        [o_scrollview setAutohidesScrollers:YES];
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
    NSUInteger count = [o_modulearray count];
    for (NSUInteger i = 0 ; i < count ; i++)
        if ([[[o_modulearray objectAtIndex:i] objectAtIndex:2]
            boolValue] != NO) {
            o_newstring = [o_newstring stringByAppendingString:
                [[o_modulearray objectAtIndex:i] objectAtIndex:0]];
            o_newstring = [o_newstring stringByAppendingString:@":"];
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
    return strdup([[o_textfield stringValue] UTF8String]);
}

-(void)resetValues
{
#warning Reset prefs of the module selector is broken atm.
    msg_Err(VLCIntf, "don't forget about modulelistconfig");
    [super resetValues];
}

@end

@implementation ModuleListConfigControl (NSTableDataSource)

- (BOOL)tableView:(NSTableView*)table writeRows:(NSArray*)rows
    toPasteboard:(NSPasteboard*)pb
{
    // We only want to allow dragging of selected rows.
    NSEnumerator    *iter = [rows objectEnumerator];
    NSNumber        *row;
    while ((row = [iter nextObject]) != nil) {
        if (![table isRowSelected:[row intValue]])
            return NO;
    }

    [pb declareTypes:[NSArray arrayWithObject:@"VLC media player module"] owner:nil];
    [pb setPropertyList:rows forType:@"VLC media player module"];
    return YES;
}

- (NSDragOperation)tableView:(NSTableView*)table
    validateDrop:(id <NSDraggingInfo>)info proposedRow:(NSInteger)row
    proposedDropOperation:(NSTableViewDropOperation)op
{
    // Make drops at the end of the table go to the end.
    if (row == -1) {
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
    row:(NSInteger)dropRow dropOperation:(NSTableViewDropOperation)op;
{
    NSPasteboard    *pb = [info draggingPasteboard];
    NSDragOperation srcMask = [info draggingSourceOperationMask];
    BOOL accepted = NO;

    NS_DURING

        NSArray *array;

        // Intra-table drag - data is the array of rows.
        if (!accepted && (array =
            [pb propertyListForType:@"VLC media player module"]) != NULL) {
            NSEnumerator *iter = nil;
            id val;
            BOOL isCopy = (srcMask & NSDragOperationMove) ? NO:YES;
            // Move the modules
            iter = [array objectEnumerator];
            while ((val = [iter nextObject]) != NULL) {
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

- (NSInteger)numberOfRowsInTableView:(NSTableView *)aTableView
{
    return [o_modulearray count];
}

- (id)tableView:(NSTableView *)aTableView
    objectValueForTableColumn:(NSTableColumn *)aTableColumn row:(NSInteger)rowIndex
{
    if ([[aTableColumn identifier] isEqualToString: @"Enabled"])
        return [[o_modulearray objectAtIndex:rowIndex] objectAtIndex:2];
    if ([[aTableColumn identifier] isEqualToString: @"Module"])
        return [[o_modulearray objectAtIndex:rowIndex] objectAtIndex:1];

    return nil;
}

- (void)tableView:(NSTableView *)aTableView setObjectValue:(id)anObject
    forTableColumn:(NSTableColumn *)aTableColumn row:(NSInteger)rowIndex
{
    [[o_modulearray objectAtIndex:rowIndex] replaceObjectAtIndex:2
        withObject: anObject];
}
@end

@implementation SectionControl

- (id) initWithItem: (module_config_t *)_p_item
           withView: (NSView *)o_parent_view
{
    NSRect mainFrame = [o_parent_view frame];
    NSString *o_labelString, *o_tooltip;
    mainFrame.size.height = 17;
    mainFrame.size.width = mainFrame.size.width - LEFTMARGIN - RIGHTMARGIN;
    mainFrame.origin.x = LEFTMARGIN;
    mainFrame.origin.y = 0;

    if ([super initWithFrame: mainFrame item: _p_item] != nil) {
        
        /* add the label */
        if (p_item->psz_text)
            o_labelString = _NS((char *)p_item->psz_text);
        else
            o_labelString = @"";

        NSDictionary *boldAttributes = [NSDictionary dictionaryWithObjectsAndKeys:
                        [NSFont boldSystemFontOfSize:[NSFont systemFontSize]],
                        NSFontAttributeName,
                        nil];
        NSAttributedString *o_bold_string = [[NSAttributedString alloc] initWithString: o_labelString attributes: boldAttributes];

        ADD_LABEL(o_label, mainFrame, 1, 0, @"", @"")
        [o_label setAttributedStringValue: o_bold_string];
        [o_label sizeToFit];

        [o_bold_string release];
        
        [o_label setAutoresizingMask:NSViewNotSizable];
        [self addSubview: o_label];
    }
    return self;
}

@end

