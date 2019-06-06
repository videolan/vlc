/*****************************************************************************
 * VLCPopupPanelController.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2012 Felix Paul Kühne
 *
 * Authors: Felix Paul Kühne <fkuehne -at- videolan -dot- org>
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

#import "VLCPopupPanelController.h"

@interface VLCPopupPanelController()
{
    PopupPanelCompletionBlock _completionBlock;
}
@end

@implementation VLCPopupPanelController

- (id)init
{
    self = [super initWithWindowNibName:@"PopupPanel"];

    return self;
}

- (IBAction)windowElementAction:(id)sender
{
    [self.window orderOut:sender];
    [NSApp endSheet: self.window];

    if (_completionBlock)
        _completionBlock(sender == _okButton ? NSModalResponseOK : NSModalResponseCancel, [_popupButton indexOfSelectedItem]);
}

- (void)runModalForWindow:(NSWindow *)window completionHandler:(PopupPanelCompletionBlock)handler;
{
    [self window];

    [_titleLabel setStringValue:self.titleString];
    [_subtitleLabel setStringValue:self.subTitleString];
    [_cancelButton setTitle:self.cancelButtonString];
    [_okButton setTitle:self.okButtonString];
    [_popupButton removeAllItems];
    for (NSString *value in self.popupButtonContent)
        [[_popupButton menu] addItemWithTitle:value action:nil keyEquivalent:@""];

    _completionBlock = [handler copy];

    [window beginSheet:self.window completionHandler:nil];
}

@end
