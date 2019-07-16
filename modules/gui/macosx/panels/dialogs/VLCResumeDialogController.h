/*****************************************************************************
 * VLCResumeDialogController.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2015 VLC authors and VideoLAN
 *
 * Authors: Felix Paul Kühne <fkuehne -at- videolan -dot- org>
 *          David Fuhrmann <david dot fuhrmann at googlemail dot com>
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

@class VLCInputItem;
@class VLCWrappableTextField;

enum ResumeResult {
    RESUME_RESTART,
    RESUME_NOW,
    RESUME_FAIL
};

typedef void(^CompletionBlock)(enum ResumeResult);

@interface VLCResumeDialogController : NSWindowController

@property (readwrite, strong) IBOutlet NSTextField *titleLabel;
@property (readwrite, strong) IBOutlet VLCWrappableTextField *descriptionLabel;
@property (readwrite, strong) IBOutlet NSButton *restartButton;
@property (readwrite, strong) IBOutlet NSButton *resumeButton;
@property (readwrite, strong) IBOutlet NSButton *alwaysResumeCheckbox;

- (IBAction)buttonClicked:(id)sender;
- (IBAction)resumeSettingChanged:(id)sender;

- (void)showWindowWithItem:(VLCInputItem *)inputItem withLastPosition:(NSInteger)pos completionBlock:(CompletionBlock)block;

- (void)cancel;

@end
