/*****************************************************************************
 * VLCTimeField.m: NSTextField subclass for playback time fields
 *****************************************************************************
 * Copyright (C) 2003-2017 VLC authors and VideoLAN
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

#import "main/VLCMain.h"
#import "menus/VLCMainMenu.h"

NSString *VLCTimeFieldDisplayTimeAsRemaining = @"DisplayTimeAsTimeRemaining";

@interface VLCTimeField ()
{
    NSString *o_remaining_identifier;
    BOOL b_time_remaining;
}
@end

@implementation VLCTimeField
+ (void)initialize
{
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    NSDictionary *appDefaults = [NSDictionary dictionaryWithObjectsAndKeys:
                                 @"NO", VLCTimeFieldDisplayTimeAsRemaining,
                                 nil];

    [defaults registerDefaults:appDefaults];
}


- (void)setRemainingIdentifier:(NSString *)o_string
{
    o_remaining_identifier = o_string;
    b_time_remaining = [[NSUserDefaults standardUserDefaults] boolForKey:o_remaining_identifier];
}

- (void)mouseDown: (NSEvent *)ourEvent
{
    if ( [ourEvent clickCount] > 1 )
        [[[VLCMain sharedInstance] mainMenu] goToSpecificTime: nil];
    else
    {
        if (o_remaining_identifier) {
            b_time_remaining = [[NSUserDefaults standardUserDefaults] boolForKey:o_remaining_identifier];
            b_time_remaining = !b_time_remaining;
            [[NSUserDefaults standardUserDefaults] setObject:(b_time_remaining ? @"YES" : @"NO") forKey:o_remaining_identifier];
        } else {
            b_time_remaining = !b_time_remaining;
        }
    }

    [[self nextResponder] mouseDown:ourEvent];
}

- (BOOL)timeRemaining
{
    if (o_remaining_identifier)
        return [[NSUserDefaults standardUserDefaults] boolForKey:o_remaining_identifier];
    else
        return b_time_remaining;
}

@end
