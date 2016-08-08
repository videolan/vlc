/*****************************************************************************
 * TimeSelectionPanelController.m: Controller for time selection panel
 *****************************************************************************
 * Copyright (C) 2015 VideoLAN and authors
 * Author:       David Fuhrmann <david dot fuhrmann at googlemail dot com>
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

#import "VLCTimeSelectionPanelController.h"

#import "misc.h"
#import "VLCStringUtility.h"

@interface VLCTimeSelectionPanelController()
{
    TimeSelectionCompletionHandler _completionHandler;
}

@end

@implementation VLCTimeSelectionPanelController

#pragma mark - object handling

- (id)init
{
    self = [super initWithWindowNibName:@"TimeSelectionPanel"];

    return self;
}


#pragma mark - UI handling

- (void)windowDidLoad
{
    [_cancelButton setTitle: _NS("Cancel")];
    [_okButton setTitle: _NS("OK")];
    [_secsLabel setStringValue: _NS("sec.")];
    [_goToLabel setStringValue: _NS("Jump to Time")];
    [_textField setFormatter:[[PositionFormatter alloc] init]];
}

- (IBAction)buttonPressed:(id)sender
{
    [self.window orderOut:sender];
    [NSApp endSheet: self.window];

    // calculate resulting time in secs:
    int64_t timeInSec = 0;
    NSString *string = [_textField stringValue];
    if ([[string componentsSeparatedByString: @":"] count] > 1 &&
        [[string componentsSeparatedByString: @":"] count] <= 3) {
        NSArray *ourTempArray = \
        [string componentsSeparatedByString: @":"];

        if ([[string componentsSeparatedByString: @":"] count] == 3) {
            timeInSec += ([[ourTempArray firstObject] intValue] *3600); //h
            timeInSec += ([[ourTempArray objectAtIndex:1] intValue] *60); //m
            timeInSec += [[ourTempArray objectAtIndex:2] intValue];        //s
        } else {
            timeInSec += ([[ourTempArray firstObject] intValue] *60); //m
            timeInSec += [[ourTempArray objectAtIndex:1] intValue]; //s
        }
    }
    else
        timeInSec = [string intValue];

    if (_completionHandler)
        _completionHandler(sender == _okButton ? NSOKButton : NSCancelButton, timeInSec);
}

- (void)runModalForWindow:(NSWindow *)window completionHandler:(TimeSelectionCompletionHandler)handler
{
    [self window];
    [_stepper setMaxValue:self.maxValue];

    _completionHandler = [handler copy];
    [NSApp beginSheet:self.window
       modalForWindow:window modalDelegate:self
       didEndSelector:nil
          contextInfo:nil];
}

@end
