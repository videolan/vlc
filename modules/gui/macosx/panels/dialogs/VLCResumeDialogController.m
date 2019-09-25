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


#import "VLCResumeDialogController.h"

#import "main/VLCMain.h"
#import "extensions/NSString+Helpers.h"
#import "windows/video/VLCVideoOutputProvider.h"
#import "library/VLCInputItem.h"
#import "views/VLCWrappableTextField.h"

@interface VLCResumeDialogController()
{
    int _currentResumeTimeout;
    CompletionBlock _completionBlock;
    NSTimer *_countdownTimer;
    VLCInputItem *_inputItem;
    NSInteger _lastPosition;
}
@end

@implementation VLCResumeDialogController

- (instancetype)init
{
    self = [super initWithWindowNibName:@"ResumeDialog"];
    if (self) {
        [[NSNotificationCenter defaultCenter] addObserver:self
                                                 selector:@selector(updateCocoaWindowLevel:)
                                                     name:VLCWindowShouldUpdateLevel
                                                   object:nil];
    }
    return self;
}

- (void)dealloc
{
    [[NSNotificationCenter defaultCenter] removeObserver:self];
}

- (void)windowDidLoad
{
    [_titleLabel setStringValue:_NS("Continue playback")];
    [_resumeButton setTitle:_NS("Continue playback")];

    [_alwaysResumeCheckbox setTitle:_NS("Always continue media playback")];

    [self updateTextLabels];
}

- (void)showWindowWithItem:(VLCInputItem *)inputItem withLastPosition:(NSInteger)pos completionBlock:(CompletionBlock)block
{
    _inputItem = inputItem;
    _lastPosition = pos;
    _currentResumeTimeout = 10;
    _completionBlock = [block copy];

    [self updateTextLabels];

    _countdownTimer = [NSTimer scheduledTimerWithTimeInterval:1
                                                       target:self
                                                     selector:@selector(updateAlertWindow:)
                                                     userInfo:nil
                                                      repeats:YES];

    NSWindow *window = [self window];
    [window setLevel:[[[VLCMain sharedInstance] voutProvider] currentStatusWindowLevel]];
    [window center];
    [window makeKeyAndOrderFront:nil];
}

- (void)updateTextLabels
{
    NSString *restartButtonLabel = _NS("Restart playback");
    restartButtonLabel = [restartButtonLabel stringByAppendingFormat:@" (%d)", _currentResumeTimeout];
    [_restartButton setTitle:restartButtonLabel];

    if (!_inputItem) {
        return;
    }

    NSString *labelString = [NSString stringWithFormat:_NS("Playback of \"%@\" will continue at %@"), _inputItem.title, [NSString stringWithTime:_lastPosition]];
    [_descriptionLabel setStringValue:labelString];
    [_alwaysResumeCheckbox setState:NSOffState];
}

- (void)updateAlertWindow:(NSTimer *)timer
{
    --_currentResumeTimeout;
    if (_currentResumeTimeout <= 0) {
        [self buttonClicked:_restartButton];
        [timer invalidate];
        timer = nil;
    }

    NSString *buttonLabel = _NS("Restart playback");
    buttonLabel = [buttonLabel stringByAppendingFormat:@" (%d)", _currentResumeTimeout];

    [_restartButton setTitle:buttonLabel];
}

- (IBAction)buttonClicked:(id)sender
{
    enum ResumeResult resumeResult = RESUME_FAIL;

    if (sender == _restartButton)
        resumeResult = RESUME_RESTART;
    else if (sender == _resumeButton)
        resumeResult = RESUME_NOW;

    [[self window] close];

    if (_completionBlock) {
        _completionBlock(resumeResult);
        _completionBlock = nil;
    }
}

- (IBAction)resumeSettingChanged:(id)sender
{
    int newState = [sender state] == NSOnState ? 1 : 0;
    msg_Dbg(getIntf(), "Changing resume setting to %i", newState);
    config_PutInt("macosx-continue-playback", newState);
}

- (void)updateCocoaWindowLevel:(NSNotification *)aNotification
{
    NSInteger i_level = [aNotification.userInfo[VLCWindowLevelKey] integerValue];
    if (self.isWindowLoaded && [self.window isVisible] && [self.window level] != i_level)
        [self.window setLevel: i_level];
}

- (void)cancel
{
    if (![self isWindowLoaded])
        return;

    if (_countdownTimer != nil) {
        [_countdownTimer invalidate];
        _countdownTimer = nil;
    }

    [self.window close];
}

@end
