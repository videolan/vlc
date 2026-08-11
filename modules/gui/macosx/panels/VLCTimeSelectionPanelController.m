/*****************************************************************************
 * TimeSelectionPanelController.m: Controller for time selection panel
 *****************************************************************************
 * Copyright (C) 2015-2018 VideoLAN and authors
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

#import "extensions/NSString+Helpers.h"

@interface VLCTimeSelectionPanelController ()
{
    TimeSelectionCompletionHandler _completionHandler;
    id _keyMonitor;
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
    [_cancelButton setTitle:_NS("Cancel")];
    [_okButton setTitle:_NS("OK")];
    [_secsLabel setStringValue:_NS("ss")];
    [_minsLabel setStringValue:_NS("mm")];
    [_hoursLabel setStringValue:_NS("hh")];
    [_goToLabel setStringValue:_NS("Jump to Time")];
}

- (void)controlTextDidChange:(NSNotification *)notification
{
    NSTextField *sender = notification.object;
    NSString *stringValue = sender.stringValue;

    if ([stringValue containsString:@":"]) {
        [self applyColonSeparatedDimensions:stringValue startingAtField:sender];
        [self setPosition:[self getTimeInSecs]];
        return;
    }

    [self setPosition:[self getTimeInSecs]];
}

- (void)setMaxTime:(NSInteger)secsMax
{
    [self setTimeMax:secsMax];

    [self setHoursMax:(int)secsMax / 3600];

    if (secsMax >= 3600) {
        [self setMinsMax:59];
        [self setSecsMax:59];
    } else if (secsMax >= 60) {
        [self setMinsMax:(int)secsMax / 60];
        [self setSecsMax:59];
    } else {
        [self setSecsMax:secsMax];
        [self setMinsMax:0];
    }
}

- (void)setPosition:(NSInteger)secsPos
{
    const NSInteger maxTime = [self timeMax];

    secsPos = MAX(MIN(secsPos, maxTime), 0);

    NSInteger minsPos = secsPos / 60;
    secsPos = secsPos % 60;
    const NSInteger hoursPos = minsPos / 60;
    minsPos = minsPos % 60;

    [self setJumpSecsValue:secsPos];
    [self setJumpMinsValue:minsPos];
    [self setJumpHoursValue:hoursPos];
}

- (NSInteger)getTimeInSecs
{
    NSInteger timeInSec = self.jumpSecsValue;
    timeInSec += self.jumpMinsValue * 60;
    timeInSec += self.jumpHoursValue * 3600;
    return timeInSec;
}

- (IBAction)buttonPressed:(id)sender
{
    [self.window orderOut:sender];
    [NSApp endSheet:self.window];
    int64_t timeInSec = [self getTimeInSecs];

    if (_completionHandler)
        _completionHandler(sender == _okButton ? NSModalResponseOK : NSModalResponseCancel,
                           timeInSec);
}

- (void)runModalForWindow:(NSWindow *)window
        completionHandler:(TimeSelectionCompletionHandler)handler
{
    __weak typeof(self) weakSelf = self;

    _keyMonitor =
        [NSEvent addLocalMonitorForEventsMatchingMask:NSEventMaskKeyDown
                                              handler:^NSEvent *(NSEvent *event) {
                                                  return [weakSelf handleKeyEvent:event];
                                              }];

    _completionHandler = handler;

    [window beginSheet:self.window
        completionHandler:^(NSModalResponse returnCode) {
        if (self->_keyMonitor) {
            [NSEvent removeMonitor:self->_keyMonitor];
            }
        }];
}

- (NSEvent *)handleKeyEvent:(NSEvent *)event
{
    NSString *const chars = event.characters;
    if (chars.length == 0) {
        return event;
    }

    if ([chars isEqualToString:@":"]) {
        if ([self moveToNextField]) {
            return nil;
        }
        return event;
    }

    unichar key = [chars characterAtIndex:0];

    if (key != NSUpArrowFunctionKey && key != NSDownArrowFunctionKey) {
        return event;
    }

    id responder = self.window.firstResponder;

    if (![responder isKindOfClass:[NSTextView class]]) {
        return event;
    }

    const NSInteger timeDifference = [self getTimeDifference:[responder delegate]];

    if (!timeDifference) {
        return event;
    }

    if (key == NSUpArrowFunctionKey) {
        [self setPosition:[self getTimeInSecs] + timeDifference];
    } else if (key == NSDownArrowFunctionKey) {
        [self setPosition:[self getTimeInSecs] - timeDifference];
    }
    return nil;
}

- (NSInteger)getTimeDifference:(NSTextField *)textField
{
    if (textField == self.hoursValueField) {
        return 3600;
    } else if (textField == self.minsValueField) {
        return 60;
    } else if (textField == self.secsValueField) {
        return 1;
    }
    return 0;
}

- (BOOL)moveToNextField
{
    NSWindow *window = self.window;
    id responder = window.firstResponder;

    if (![responder isKindOfClass:[NSTextView class]]) {
        return NO;
    }

    NSTextView *textView = responder;
    id delegate = textView.delegate;

    if (delegate == self.hoursValueField) {
        [window makeFirstResponder:self.minsValueField];
        return YES;
    } else if (delegate == self.minsValueField) {
        [window makeFirstResponder:self.secsValueField];
        return YES;
    } else if (delegate == self.secsValueField) {
        return YES;
    }
    return NO;
}

- (void)applyColonSeparatedDimensions:(NSString *)text startingAtField:(NSTextField *)field
{
    NSArray<NSString *> *parts = [text componentsSeparatedByString:@":"];

    NSInteger startingField = 0;
    NSInteger timeDifference = [self getTimeDifference:field];

    if (timeDifference == 3600) {
        startingField = 0;
    } else if (timeDifference == 60) {
        startingField = 1;
    } else if (timeDifference == 1) {
        startingField = 2;
    } else {
        return;
    }

    NSInteger count = MIN(parts.count, 3 - startingField);

    NSInteger newPosition = 0;
    for (NSInteger i = 0; i < count; i++) {
        NSString *part = [parts[i]
            stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
        newPosition += timeDifference * part.integerValue;
        timeDifference /= 60;
    }

    [self setPosition:newPosition];
}

@end
