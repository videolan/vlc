/*****************************************************************************
 * VLCCoreDialogProvider.h: Mac OS X Core Dialogs
 *****************************************************************************
 * Copyright (C) 2005-2016 VLC authors and VideoLAN
 *
 * Authors: Derk-Jan Hartman <hartman at videolan dot org>
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

/*****************************************************************************
 * VLCCoreDialogProvider interface
 *****************************************************************************/

@class VLCErrorWindowController;

@interface VLCCoreDialogProvider : NSObject

/* authentication dialog */
@property (readwrite, strong) IBOutlet NSWindow *authenticationWindow;

@property (readwrite, weak) IBOutlet NSButton *authenticationCancelButton;
@property (readwrite, weak) IBOutlet NSTextField *authenticationDescriptionLabel;
@property (readwrite, weak) IBOutlet NSTextField *authenticationLoginTextField;
@property (readwrite, weak) IBOutlet NSTextField *authenticationLoginLabel;
@property (readwrite, weak) IBOutlet NSButton *authenticationOkButton;
@property (readwrite, weak) IBOutlet NSTextField *authenticationPasswordTextField;
@property (readwrite, weak) IBOutlet NSTextField *authenticationPasswordLabel;
@property (readwrite, weak) IBOutlet NSTextField *authenticationTitleLabel;
@property (readwrite, weak) IBOutlet NSButton *authenticationStorePasswordCheckbox;

/* progress dialog */
@property (readwrite, strong) IBOutlet NSWindow *progressWindow;

@property (readwrite, weak) IBOutlet NSProgressIndicator *progressIndicator;
@property (readwrite, weak) IBOutlet NSButton *progressCancelButton;
@property (readwrite, weak) IBOutlet NSTextField *progressDescriptionLabel;
@property (readwrite, weak) IBOutlet NSTextField *progressTitleLabel;

@property (readonly) VLCErrorWindowController* errorPanel;

- (IBAction)authenticationDialogAction:(id)sender;

- (IBAction)progressDialogAction:(id)sender;

@end
