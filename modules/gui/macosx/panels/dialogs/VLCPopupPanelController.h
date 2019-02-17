/*****************************************************************************
 * VLCPopupPanelController.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2012 Felix Paul Kühne
 *
 * Authors: Felix Paul Kühne <fkuehne -at- videolan -dot- org>
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

@interface VLCPopupPanelController : NSWindowController

@property (weak) IBOutlet NSTextField *titleLabel;
@property (weak) IBOutlet NSTextField *subtitleLabel;
@property (weak) IBOutlet NSPopUpButton *popupButton;
@property (weak) IBOutlet NSButton *cancelButton;
@property (weak) IBOutlet NSButton *okButton;

@property (readwrite) NSString *titleString;
@property (readwrite) NSString *subTitleString;
@property (readwrite) NSString *okButtonString;
@property (readwrite) NSString *cancelButtonString;
@property (readwrite) NSArray *popupButtonContent;

/**
 * Completion handler for popup panel
 * \param returnCode Result from panel. Can be NSModalResponseOK or NSModalResponseCancel.
 * \param selectedIndex Selected index of the popup in panel.
 */
typedef void(^PopupPanelCompletionBlock)(NSInteger returnCode, NSInteger selectedIndex);

/**
 * Shows the panel as a modal dialog with window as its owner.
 * \param window Parent window for the dialog.
 * \param handler Completion block.
 */
- (void)runModalForWindow:(NSWindow *)window completionHandler:(PopupPanelCompletionBlock)handler;

- (IBAction)windowElementAction:(id)sender;

@end
