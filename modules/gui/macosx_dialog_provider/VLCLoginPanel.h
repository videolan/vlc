/*****************************************************************************
 * VLCLoginPanel.h: A Generic Login Panel created for VLC
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

#import <Cocoa/Cocoa.h>


@interface VLCLoginPanel : NSPanel {
    IBOutlet NSButton * _cancelButton;
    IBOutlet NSButton * _okayButton;
    IBOutlet NSTextField * _userNameLabel;
    IBOutlet NSTextField * _userNameField;
    IBOutlet NSTextField * _passwordLabel;
    IBOutlet NSSecureTextField * _passwordField;
    IBOutlet NSTextField * _titleField;
    IBOutlet NSTextField * _informativeTextField;
    IBOutlet NSImageView * _iconView;
}

- (IBAction)buttonAction:(id)sender;
- (void)createContentView;

- (void)setDialogTitle:(NSString *)title;
- (void)setDialogMessage:(NSString *)message;

- (NSString *)userName;
- (NSString *)password;

@end
