/*****************************************************************************
 * VLCTimeField.m: NSTextField subclass for playback time fields
 *****************************************************************************
 * Copyright (C) 2003-2017 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Jon Lech Johansen <jon-vl@nanocrew.net>
 *          Felix Paul Kühne <fkuehne at videolan dot org>
 *          Marvin Scholz <epirat07 at gmail dot com>
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

#import "VLCMain.h"
#import "VLCMainMenu.h"

@interface VLCTimeField ()
{
    NSString *_identifier;
    BOOL _isTimeRemaining;

    NSString *_cachedTime;
    NSString *_remainingTime;
}
@end

@implementation VLCTimeField
+ (void)initialize
{
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    NSDictionary *appDefaults = [NSDictionary dictionaryWithObjectsAndKeys:
                                 @"NO", @"DisplayTimeAsTimeRemaining",
                                 @"YES", @"DisplayFullscreenTimeAsTimeRemaining",
                                 nil];

    [defaults registerDefaults:appDefaults];
}


- (void)setRemainingIdentifier:(NSString *)identifier
{
    _identifier = identifier;
    _isTimeRemaining = [[NSUserDefaults standardUserDefaults] boolForKey:_identifier];
}

- (void)mouseDown: (NSEvent *)ourEvent
{
    if ( [ourEvent clickCount] > 1 )
        [[[VLCMain sharedInstance] mainMenu] goToSpecificTime: nil];
    else
    {
        if (_identifier) {
            _isTimeRemaining = [[NSUserDefaults standardUserDefaults] boolForKey:_identifier];
            _isTimeRemaining = !_isTimeRemaining;
            [[NSUserDefaults standardUserDefaults] setObject:(_isTimeRemaining ? @"YES" : @"NO") forKey:_identifier];
        } else {
            _isTimeRemaining = !_isTimeRemaining;
        }

        [self updateTimeValue];
    }

    [[self nextResponder] mouseDown:ourEvent];
}

- (void)setTime:(NSString *)time withRemainingTime:(NSString *)remainingTime
{
    _cachedTime = time;
    _remainingTime = remainingTime;

    [self updateTimeValue];
}

- (void)updateTimeValue
{
    if (!_cachedTime || !_remainingTime)
        return;

    if ([self timeRemaining]) {
        [super setStringValue:_remainingTime];
    } else {
        [super setStringValue:_cachedTime];
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
    if (_identifier)
        return [[NSUserDefaults standardUserDefaults] boolForKey:_identifier];
    else
        return _isTimeRemaining;
}

@end
