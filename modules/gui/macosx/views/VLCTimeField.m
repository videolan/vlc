/*****************************************************************************
 * VLCTimeField.m: NSTextField subclass for playback time fields
 *****************************************************************************
 * Copyright (C) 2003-2017 VLC authors and VideoLAN
 *
 * Authors: Jon Lech Johansen <jon-vl@nanocrew.net>
 *          Felix Paul Kühne <fkuehne at videolan dot org>
 *          Marvin Scholz <epirat07 at gmail dot com>
 *          Claudio Cambra <developer at claudiocambra dot com>
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

#import "VLCTimeField.h"

#import "main/VLCMain.h"
#import "menus/VLCMainMenu.h"

NSString * const VLCTimeFieldDisplayTimeAsElapsed = @"DisplayTimeAsTimeElapsed";
NSString * const VLCTimeFieldDisplayTimeAsRemaining = @"DisplayTimeAsTimeRemaining";

@interface VLCTimeField ()
{
    NSString *_cachedTime;
    NSString *_remainingTime;
}
@end

@implementation VLCTimeField

+ (void)initialize
{
    NSDictionary * const appDefaults = @{VLCTimeFieldDisplayTimeAsRemaining : @NO};
    [NSUserDefaults.standardUserDefaults registerDefaults:appDefaults];
}

- (void)setPreferencesIdentifier:(NSString *)preferencesIdentifier
{
    _preferencesIdentifier = preferencesIdentifier;
    self.isTimeRemaining = [NSUserDefaults.standardUserDefaults
                            boolForKey:self.preferencesIdentifier];
}

- (void)setIsTimeRemaining:(BOOL)isTimeRemaining
{
    _isTimeRemaining = isTimeRemaining;

    if (self.identifier) {
        [NSUserDefaults.standardUserDefaults setBool:_isTimeRemaining
                                              forKey:self.preferencesIdentifier];
    }

    [self updateTimeValue];
}

- (void)mouseDown:(NSEvent *)ourEvent
{
    if (ourEvent.clickCount > 1) {
        [VLCMain.sharedInstance.mainMenu goToSpecificTime:nil];
    } else {
        self.isTimeRemaining = !self.isTimeRemaining;
    }

    [self updateTimeValue];
    [self.nextResponder mouseDown:ourEvent];
}

- (void)setTime:(NSString *)time withRemainingTime:(NSString *)remainingTime
{
    _cachedTime = time;
    _remainingTime = remainingTime;

    [self updateTimeValue];
}

- (void)updateTimeValue
{
    if (!_cachedTime || !_remainingTime) {
        return;
    }

    if (self.timeRemaining) {
        self.stringValue = _remainingTime;
    } else {
        self.stringValue = _cachedTime;
    }
}

- (void)setStringValue:(NSString *)stringValue
{
    [super setStringValue:stringValue];

    _cachedTime = nil;
    _remainingTime = nil;
}

- (BOOL)timeRemaining
{
    if (self.preferencesIdentifier) {
        return [NSUserDefaults.standardUserDefaults boolForKey:self.preferencesIdentifier];
    } else {
        return _isTimeRemaining;
    }
}

@end
