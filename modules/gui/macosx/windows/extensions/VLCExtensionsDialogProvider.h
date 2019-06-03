/*****************************************************************************
 * VLCExtensionsDialogProvider.h: Mac OS X Extensions Dialogs
 *****************************************************************************
 * Copyright (C) 2005-2012 VLC authors and VideoLAN
 *
 * Authors: Brendon Justin <brendonjustin@gmail.com>,
 *          Derk-Jan Hartman <hartman@videolan dot org>,
 *          Felix Paul Kühne <fkuehne@videolan dot org>
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

#import <vlc_common.h>
#import <vlc_dialog.h>
#import <vlc_extensions.h>

@class VLCDialogWindow;

/*****************************************************************************
 * ExtensionsDialogProvider interface
 *****************************************************************************/
@interface VLCExtensionsDialogProvider : NSObject <NSWindowDelegate>

- (void)performEventWithObject: (NSValue *)o_value ofType:(const char*)type;

- (void)triggerClick:(id)sender;
- (void)syncTextField:(NSNotification *)notifcation;
- (void)tableViewSelectionDidChange:(NSNotification *)notifcation;
- (void)popUpSelectionChanged:(id)sender;
- (NSSize)windowWillResize:(NSWindow *)sender toSize:(NSSize)frameSize;
- (BOOL)windowShouldClose:(id)sender;
- (void)updateWidgets:(extension_dialog_t *)dialog;

- (VLCDialogWindow *)createExtensionDialog:(extension_dialog_t *)p_dialog;
- (int)destroyExtensionDialog:(extension_dialog_t *)o_value;
- (VLCDialogWindow *)updateExtensionDialog:(NSValue *)o_value;
- (void)manageDialog:(extension_dialog_t *)p_dialog;

@end
