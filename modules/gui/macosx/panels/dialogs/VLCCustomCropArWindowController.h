/*****************************************************************************
 * VLCCustomCropArWindowController.h: Controller for custom crop / AR panel
 *****************************************************************************
 * Copyright (C) 2019 VideoLAN and authors
 * Author:       Felix Paul Kühne <fkuehne # videolan.org>
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

NS_ASSUME_NONNULL_BEGIN

@interface VLCCustomCropArWindowController : NSWindowController

@property (readwrite, weak) IBOutlet NSButton *cancelButton;
@property (readwrite, weak) IBOutlet NSButton *applyButton;
@property (readwrite, weak) IBOutlet NSTextField *titleLabel;
@property (readwrite, weak) IBOutlet NSTextField *numeratorTextField;
@property (readwrite, weak) IBOutlet NSTextField *denominatorTextField;
@property (readwrite, retain) NSString *title;

- (IBAction)buttonPressed:(id)sender;

/**
 * \param returnCode Result from panel. Can be NSModalResponseOK or NSModalResponseCancel.
 * \param geometry Geometry based on numbers entered in panel
 */
typedef void(^CustomCropArCompletionHandler)(NSInteger returnCode, NSString *geometry);

/**
 * Shows the panel as a modal dialog with window as its owner.
 * \param window Parent window for the dialog.
 * \param handler Completion block.
 */
- (void)runModalForWindow:(NSWindow *)window completionHandler:(CustomCropArCompletionHandler)handler;

@end

NS_ASSUME_NONNULL_END
