/*****************************************************************************
 * VLCLibraryHomeViewActionsViewController.h: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2024 VLC authors and VideoLAN
 *
 * Authors: Claudio Cambra <developer@claudiocambra.com>
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

#import "views/VLCFileDragRecognisingView.h"

NS_ASSUME_NONNULL_BEGIN

@interface VLCLibraryHomeViewActionsView : VLCFileDragRecognisingView <VLCDragDropTarget>

@property (readwrite, weak) IBOutlet NSImageView *logoImageView;
@property (readwrite, weak) IBOutlet NSTextField *dropTitleTextField;
@property (readwrite, weak) IBOutlet NSTextField *dropDetailTextField;
@property (readwrite, weak) IBOutlet NSTextField *dropHintTextField;

@property (readwrite, weak) IBOutlet NSButton *openFileButton;
@property (readwrite, weak) IBOutlet NSButton *openDiscButton;
@property (readwrite, weak) IBOutlet NSButton *openUrlButton;
@property (readwrite, weak) IBOutlet NSButton *openCaptureDeviceButton;

- (IBAction)openFileAction:(id)sender;
- (IBAction)openDiscAction:(id)sender;
- (IBAction)openUrlAction:(id)sender;
- (IBAction)openCaptureDeviceAction:(id)sender;

@end

NS_ASSUME_NONNULL_END
