/*****************************************************************************
 * VLCCustomCropArWindowController.m: Controller for custom crop / AR panel
 *****************************************************************************
 * Copyright (C) 2019 VideoLAN and authors
 * Author:       Felix Paul Kühne <fkuehne # videolan.org>
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

#import "VLCCustomCropArWindowController.h"
#import "extensions/NSString+Helpers.h"

@interface VLCCustomCropArWindowController ()
{
    CustomCropArCompletionHandler _completionHandler;
}
@end

@implementation VLCCustomCropArWindowController

- (instancetype)init
{
    self = [super initWithWindowNibName:@"VLCCustomCropARPanel"];
    return self;
}

- (void)windowDidLoad {
    [super windowDidLoad];

    [_applyButton setTitle:_NS("Apply")];
    [_cancelButton setTitle:_NS("Cancel")];
    [_titleLabel setStringValue:self.title];
}

- (void)buttonPressed:(id)sender
{
    [self.window orderOut:sender];
    [NSApp endSheet: self.window];
    NSString *geometry = [NSString stringWithFormat:@"%@:%@", _numeratorTextField.stringValue, _denominatorTextField.stringValue];

    if (_completionHandler)
        _completionHandler(sender == _applyButton ? NSModalResponseOK : NSModalResponseCancel, geometry);
}

- (void)runModalForWindow:(NSWindow *)window completionHandler:(CustomCropArCompletionHandler)handler
{
    _completionHandler = handler;
    [window beginSheet:self.window completionHandler:nil];
}

@end
