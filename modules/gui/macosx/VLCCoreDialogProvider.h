/*****************************************************************************
 * VLCCoreDialogProvider.h: Mac OS X Core Dialogs
 *****************************************************************************
 * Copyright (C) 2005-2016 VLC authors and VideoLAN
 * $Id$
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

#import <vlc_common.h>
#import <vlc_dialog.h>
#import <Cocoa/Cocoa.h>

/*****************************************************************************
 * VLCCoreDialogProvider interface
 *****************************************************************************/

@class VLCErrorWindowController;

@interface VLCCoreDialogProvider : NSObject
{
    /* authentication dialog */
    IBOutlet NSButton *authenticationCancelButton;
    IBOutlet NSTextField *authenticationDescriptionLabel;
    IBOutlet NSTextField *authenticationLoginTextField;
    IBOutlet NSTextField *authenticationLoginLabel;
    IBOutlet NSButton *authenticationOkButton;
    IBOutlet NSTextField *authenticationPasswordTextField;
    IBOutlet NSTextField *authenticationPasswordLabel;
    IBOutlet NSTextField *authenticationTitleLabel;
    IBOutlet NSButton *authenticationStorePasswordCheckbox;
    IBOutlet NSWindow *authenticationWindow;

    /* progress dialog */
    IBOutlet NSProgressIndicator *progressIndicator;
    IBOutlet NSButton *progressCancelButton;
    IBOutlet NSTextField *progressDescriptionLabel;
    IBOutlet NSTextField *progressTitleLabel;
    IBOutlet NSWindow *progressWindow;
}

@property (readonly) VLCErrorWindowController* errorPanel;

@property (atomic,readwrite) BOOL progressCancelled;

- (IBAction)authenticationDialogAction:(id)sender;

- (IBAction)progressDialogAction:(id)sender;

@end
