/*****************************************************************************
 * VLCLoginPanel.m: A Generic Login Panel created for VLC
 *****************************************************************************
 * Copyright (C) 2009-2010 the VideoLAN team
 * $Id$
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

#import "VLCLoginPanel.h"


@implementation VLCLoginPanel

- (id)init
{
    NSRect windowRect;
    windowRect.size.height = 278;
    windowRect.size.width = 505;
    windowRect.origin.x = windowRect.origin.y = 0;

    return [super initWithContentRect:windowRect
                            styleMask:NSTitledWindowMask
                              backing:NSBackingStoreBuffered
                                defer:YES];
}

- (void)createContentView
{
    NSRect s_rc = [self frame];
    id ourContentView = [self contentView];

    s_rc.origin.x = 275;
    s_rc.origin.y = 44;
    s_rc.size.height = 32;
    s_rc.size.width = 108;
    _cancelButton = [[NSButton alloc] initWithFrame:s_rc];
    [_cancelButton setButtonType:NSMomentaryLightButton];
    [_cancelButton setTitle:@"Cancel"];
    [_cancelButton setBezelStyle:NSRoundedBezelStyle];
    [_cancelButton setBordered:YES];
    [_cancelButton setTarget:self];
    [_cancelButton setAction:@selector(buttonAction:)];
    [_cancelButton setKeyEquivalent:@"\e"] ; // escape key
    [ourContentView addSubview:_cancelButton];

    s_rc.origin.x = 383;
    s_rc.origin.y = 44;
    s_rc.size.height = 32;
    s_rc.size.width = 108;
    _okayButton = [[NSButton alloc] initWithFrame:s_rc];
    [_okayButton setButtonType:NSMomentaryLightButton];
    [_okayButton setTitle:@"OK"];
    [_okayButton setBezelStyle:NSRoundedBezelStyle];
    [_okayButton setBordered:YES];
    [_okayButton setTarget:self];
    [_okayButton setAction:@selector(buttonAction:)];
    [_okayButton setKeyEquivalent:@"\r"] ; // enter key
    [ourContentView addSubview:_okayButton];

    s_rc.origin.x = 94;
    s_rc.origin.y = 170;
    s_rc.size.height = 14;
    s_rc.size.width = 129;
    _userNameLabel = [[NSTextField alloc] initWithFrame:s_rc];
    [_userNameLabel setFont:[NSFont systemFontOfSize:[NSFont smallSystemFontSize]]];
    [_userNameLabel setStringValue:@"User Name"];
    [_userNameLabel setBezeled:NO];
    [_userNameLabel setEditable:NO];
    [_userNameLabel setSelectable:NO];
    [_userNameLabel setDrawsBackground:NO];
    [ourContentView addSubview:_userNameLabel];

    s_rc.origin.x = 97;
    s_rc.origin.y = 148;
    s_rc.size.height = 22;
    s_rc.size.width = 310;
    _userNameField = [[NSTextField alloc] initWithFrame:s_rc];
    [_userNameField setBezeled:YES];
    [_userNameField setEditable:YES];
    [_userNameField setImportsGraphics:NO];
    [ourContentView addSubview:_userNameField];

    s_rc.origin.x = 94;
    s_rc.origin.y = 116;
    s_rc.size.height = 14;
    s_rc.size.width = 129;
    _passwordLabel = [[NSTextField alloc] initWithFrame:s_rc];
    [_passwordLabel setFont:[NSFont systemFontOfSize:[NSFont smallSystemFontSize]]];
    [_passwordLabel setStringValue:@"Password"];
    [_passwordLabel setBezeled:NO];
    [_passwordLabel setEditable:NO];
    [_passwordLabel setSelectable:NO];
    [_passwordLabel setDrawsBackground:NO];
    [ourContentView addSubview:_passwordLabel];

    s_rc.origin.x = 97;
    s_rc.origin.y = 94;
    s_rc.size.height = 22;
    s_rc.size.width = 310;
    _passwordField = [[NSSecureTextField alloc] initWithFrame:s_rc];
    [_passwordField setBezeled:YES];
    [_passwordField setEditable:YES];
    [_passwordField setImportsGraphics:NO];
    [ourContentView addSubview:_passwordField];

    s_rc.origin.x = 94;
    s_rc.origin.y = 238;
    s_rc.size.height = 17;
    s_rc.size.width = 316;
    _titleField = [[NSTextField alloc] initWithFrame:s_rc];
    [_titleField setFont:[NSFont boldSystemFontOfSize:0]];
    [_titleField setBezeled:NO];
    [_titleField setEditable:NO];
    [_titleField setSelectable:YES];
    [_titleField setDrawsBackground:NO];
    [ourContentView addSubview:_titleField];

    s_rc.origin.x = 94;
    s_rc.origin.y = 183;
    s_rc.size.height = 44;
    s_rc.size.width = 394;
    _informativeTextField = [[NSTextField alloc] initWithFrame:s_rc];
    [_informativeTextField setFont:[NSFont systemFontOfSize:[NSFont smallSystemFontSize]]];
    [_informativeTextField setBezeled:NO];
    [_informativeTextField setEditable:NO];
    [_informativeTextField setSelectable:YES];
    [_informativeTextField setDrawsBackground:NO];
    [ourContentView addSubview:_informativeTextField];

    s_rc.origin.x = 20;
    s_rc.origin.y = 188;
    s_rc.size.height = s_rc.size.width = 64;
    _iconView = [[NSImageView alloc] initWithFrame:s_rc];
    [_iconView setImage:[NSImage imageNamed:@"NSApplicationIcon"]];
    [_iconView setEditable:NO];
    [_iconView setAllowsCutCopyPaste:NO];
    [ourContentView addSubview:_iconView];
}

- (IBAction)buttonAction:(id)sender
{
    if (sender == _okayButton)
        [NSApp stopModalWithCode: 1];
    else
        [NSApp stopModalWithCode: 0];
}

- (void)setDialogTitle:(NSString *)title
{
    [_titleField setStringValue:title];
    [self setTitle:title];
}
- (void)setDialogMessage:(NSString *)message
{
    [_informativeTextField setStringValue:message];
}

- (NSString *)userName
{
    return [_userNameField stringValue];
}

- (NSString *)password
{
    return [_passwordField stringValue];
}

@end
