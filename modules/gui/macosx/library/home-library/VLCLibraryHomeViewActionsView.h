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

#import <Cocoa/Cocoa.h>

NS_ASSUME_NONNULL_BEGIN

@interface VLCLibraryHomeViewActionsView : NSStackView

@property (readwrite, weak) IBOutlet NSButton *openFileButton;
@property (readwrite, weak) IBOutlet NSButton *openDiscButton;
@property (readwrite, weak) IBOutlet NSButton *openUrlButton;
@property (readwrite, weak) IBOutlet NSButton *openCaptureDeviceButton;
@property (readwrite, weak) IBOutlet NSButton *openBrowseButton;

+ (instancetype)fromNibWithOwner:(id)owner;

- (IBAction)openFileAction:(id)sender;
- (IBAction)openDiscAction:(id)sender;
- (IBAction)openUrlAction:(id)sender;
- (IBAction)openCaptureDeviceAction:(id)sender;
- (IBAction)openBrowseAction:(id)sender;

@end

NS_ASSUME_NONNULL_END
