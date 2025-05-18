/*****************************************************************************
 * VLCPlaybackEndViewController.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2025 VLC authors and VideoLAN
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

#import "VLCPlaybackEndViewController.h"

#import "main/VLCMain.h"
#import "extensions/NSColor+VLCAdditions.h"
#import "extensions/NSString+Helpers.h"
#import "library/VLCLibraryUIUnits.h"
#import "playqueue/VLCPlayQueueController.h"

static const NSTimeInterval kVLCPlaybackEndTimeout = 10;
static const NSTimeInterval kVLCPlaybackEndUpdateInterval = 0.1;

NSString * const VLCPlaybackEndViewHideNotificationName = @"VLCPlaybackEndViewRequestHide";
NSString * const VLCPlaybackEndViewReturnToLibraryNotificationName = @"VLCPlaybackEndViewReturnToLibrary";

@interface VLCPlaybackEndViewController ()

@property NSDate *timeoutDate;

@end

@implementation VLCPlaybackEndViewController

- (instancetype)init
{
    return [super initWithNibName:@"VLCPlaybackEndView" bundle:nil];
}

- (void)viewDidLoad
{
    [super viewDidLoad];
    self.view.wantsLayer = YES;
    self.view.layer.cornerRadius = VLCLibraryUIUnits.cornerRadius;
    self.view.layer.borderColor = NSColor.VLCSubtleBorderColor.CGColor;
    self.view.layer.borderWidth = VLCLibraryUIUnits.borderThickness;
    self.largeTitleLabel.stringValue = _NS("Reached the end of the play queue");
    self.returnToLibraryButton.stringValue = _NS("Return to library");
    self.returnToLibraryButton.target = self;
    self.returnToLibraryButton.action = @selector(returnToLibrary:);
    self.restartPlayQueueButton.stringValue = _NS("Restart play queue");
    self.restartPlayQueueButton.target = self;
    self.restartPlayQueueButton.action = @selector(restartPlayQueue:);
}

- (void)startCountdown
{
    [self.countdownTimer invalidate];
    self.timeoutDate = [NSDate dateWithTimeIntervalSinceNow:kVLCPlaybackEndTimeout];
    _countdownTimer = [NSTimer scheduledTimerWithTimeInterval:kVLCPlaybackEndUpdateInterval
                                                       target:self
                                                     selector:@selector(handleUpdateInterval:)
                                                     userInfo:nil
                                                      repeats:YES];
    [self handleUpdateInterval:nil];
}

- (void)handleUpdateInterval:(nullable NSTimer *)timer
{
    NSDate * const now = NSDate.date;
    NSDate * const timeout = self.timeoutDate;
    const NSTimeInterval timeRemaining = [timeout timeIntervalSinceDate:now];
    if (timeRemaining <= 0) {
        [self returnToLibrary:self];
        return;
    }

    NSString *remainingTimeString = @"";
    if (@available(macOS 10.15, *)) {
        NSRelativeDateTimeFormatter * const formatter = [[NSRelativeDateTimeFormatter alloc] init];
        remainingTimeString = [formatter localizedStringForDate:timeout relativeToDate:now];
    } else {
        NSDateComponentsFormatter * const formatter = [[NSDateComponentsFormatter alloc] init];
        NSString * const timeString = [formatter stringFromTimeInterval:timeRemaining];
        remainingTimeString = [NSString stringWithFormat:_NS("in %@"), timeString];
    }
    self.countdownLabel.stringValue =
        [NSString stringWithFormat:_NS("Returning to library %@"), remainingTimeString];
}

- (void)setHideLibraryControls:(BOOL)hideLibraryControls
{
    if (self.hideLibraryControls == hideLibraryControls)
        return;
    _hideLibraryControls = hideLibraryControls;
    self.countdownLabel.hidden = hideLibraryControls;
    self.returnToLibraryButton.hidden = hideLibraryControls;
}

- (void)returnToLibrary:(id)sender
{
    [self.countdownTimer invalidate];
    [NSNotificationCenter.defaultCenter postNotificationName:VLCPlaybackEndViewReturnToLibraryNotificationName
                                                      object:self];
    [NSNotificationCenter.defaultCenter postNotificationName:VLCPlaybackEndViewHideNotificationName
                                                      object:self];
}

- (void)restartPlayQueue:(id)sender
{
    [self.countdownTimer invalidate];
    [VLCMain.sharedInstance.playQueueController playItemAtIndex:0];
    [NSNotificationCenter.defaultCenter postNotificationName:VLCPlaybackEndViewHideNotificationName
                                                      object:self];
}

@end
