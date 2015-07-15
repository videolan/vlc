/*****************************************************************************
 * ResumeDialogController.m: MacOS X interface module
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


#import "ResumeDialogController.h"

#import "intf.h"
#import "StringUtility.h"

@interface ResumeDialogController()
{
    int currentResumeTimeout;
    CompletionBlock completionBlock;
}
@end

@implementation ResumeDialogController

- (id)init
{
    self = [super initWithWindowNibName:@"ResumeDialog"];

    return self;
}

- (void)windowDidLoad
{
    [o_title_lbl setStringValue:_NS("Continue playback?")];
    [o_resume_btn setTitle:_NS("Continue")];

    [o_always_resume_btn setTitle:_NS("Always continue")];
}

- (void)showWindowWithItem:(input_item_t *)p_item withLastPosition:(NSInteger)pos completionBlock:(CompletionBlock)block
{
    NSWindow *w = [self window];

    if ([w isVisible]) {
        msg_Err(VLCIntf, "Resume dialog in use already");
        return;
    }

    currentResumeTimeout = 10;
    completionBlock = [block copy];


    NSString *o_restartButtonLabel = _NS("Restart playback");
    o_restartButtonLabel = [o_restartButtonLabel stringByAppendingFormat:@" (%d)", currentResumeTimeout];
    [o_restart_btn setTitle:o_restartButtonLabel];

    char *psz_title_name = input_item_GetTitleFbName(p_item);
    NSString *o_title = toNSStr(psz_title_name);
    free(psz_title_name);
    NSString *labelString = [NSString stringWithFormat:_NS("Playback of \"%@\" will continue at %@"), o_title, [[VLCStringUtility sharedInstance] stringForTime:pos]];
    [o_text_lbl setStringValue:labelString];

    NSTimer *timer = [NSTimer scheduledTimerWithTimeInterval:1
                                             target:self
                                           selector:@selector(updateAlertWindow:)
                                           userInfo:nil
                                            repeats:YES];

    [w setLevel:[[[VLCMain sharedInstance] voutController] currentStatusWindowLevel]];
    [w center];

    [w makeKeyAndOrderFront:nil];
}

- (void)updateAlertWindow:(NSTimer *)timer
{
    --currentResumeTimeout;
    if (currentResumeTimeout <= 0) {
        [self buttonClicked:o_restart_btn];
        [timer invalidate];
        timer = nil;
    }

    NSString *buttonLabel = _NS("Restart playback");
    buttonLabel = [buttonLabel stringByAppendingFormat:@" (%d)", currentResumeTimeout];

    [o_restart_btn setTitle:buttonLabel];
}

- (IBAction)buttonClicked:(id)sender
{
    enum ResumeResult resumeResult;

    if (sender == o_restart_btn)
        resumeResult = RESUME_RESTART;
    else if (sender == o_resume_btn)
        resumeResult = RESUME_NOW;
    else
        resumeResult = RESUME_ALWAYS;

    [[self window] close];

    if (completionBlock) {
        completionBlock(resumeResult);
        completionBlock = nil;
    }
}

- (void)updateCocoaWindowLevel:(NSInteger)i_level
{
    if (self.window && [self.window isVisible] && [self.window level] != i_level)
        [self.window setLevel: i_level];
}

@end
