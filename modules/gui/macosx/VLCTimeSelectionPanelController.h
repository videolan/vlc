/*****************************************************************************
 * TimeSelectionPanelController.h: Controller for time selection panel
 *****************************************************************************
 * Copyright (C) 2015 VideoLAN and authors
 * Author:       David Fuhrmann <david dot fuhrmann at googlemail dot com>
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

@interface VLCTimeSelectionPanelController : NSWindowController

@property (readwrite, weak) IBOutlet NSButton *cancelButton;
@property (readwrite, weak) IBOutlet NSTextField *textField;
@property (readwrite, weak) IBOutlet NSTextField *goToLabel;
@property (readwrite, weak) IBOutlet NSButton *okButton;
@property (readwrite, weak) IBOutlet NSTextField *secsLabel;
@property (readwrite, weak) IBOutlet NSStepper *stepper;

@property (nonatomic) int jumpTimeValue;
@property (nonatomic) int maxValue;

- (IBAction)buttonPressed:(id)sender;

/**
 * Completion handler for textfield panel
 * \param returnCode Result from panel. Can be NSOKButton or NSCancelButton.
 * \param returnTime Resulting time in seconds entered in panel.
 */
typedef void(^TimeSelectionCompletionHandler)(NSInteger returnCode, int64_t returnTime);

/**
 * Shows the panel as a modal dialog with window as its owner.
 * \param window Parent window for the dialog.
 * \param handler Completion block.
 */
- (void)runModalForWindow:(NSWindow *)window completionHandler:(TimeSelectionCompletionHandler)handler;


@end