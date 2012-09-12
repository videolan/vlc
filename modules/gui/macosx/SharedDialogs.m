/*****************************************************************************
 * SharedDialogs.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2012 Felix Paul Kühne
 * $Id$
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

#import "SharedDialogs.h"

static VLCEnterTextPanel *_textPanelInstance = nil;
static VLCSelectItemInPopupPanel *_selectItemPanelInstance = nil;

@implementation VLCEnterTextPanel
+ (VLCEnterTextPanel *)sharedInstance
{
    return _textPanelInstance ? _textPanelInstance : [[self alloc] init];
}

- (id)init
{
    if (_textPanelInstance)
        [self dealloc];
    else
        _textPanelInstance = [super init];

    return _textPanelInstance;
}

@synthesize title=_title, subTitle=_subtitle, OKButtonLabel=_okTitle, CancelButtonLabel=_cancelTitle, target=_target;

- (IBAction)windowElementAction:(id)sender
{
    [_panel orderOut:sender];
    [NSApp endSheet: _panel];

    if (self.target) {
        if ([self.target respondsToSelector:@selector(panel:returnValue:text:)]) {
            if (sender == _cancel_btn)
                [self.target panel:self returnValue:NSCancelButton text:NULL];
            else
                [self.target panel:self returnValue:NSOKButton text:self.enteredText];
        }
    }
}

- (void)runModalForWindow:(NSWindow *)window
{
    [_title_lbl setStringValue:self.title];
    [_subtitle_lbl setStringValue:self.subTitle];
    [_cancel_btn setTitle:self.CancelButtonLabel];
    [_ok_btn setTitle:self.OKButtonLabel];
    [_text_fld setStringValue:@""];

    [NSApp beginSheet:_panel modalForWindow:window modalDelegate:self didEndSelector:NULL contextInfo:nil];
}

- (NSString *)enteredText
{
    return [_text_fld stringValue];
}

@end

@implementation VLCSelectItemInPopupPanel
@synthesize title=_title, subTitle=_subtitle, OKButtonLabel=_okTitle, CancelButtonLabel=_cancelTitle, popupButtonContent=_popData, target=_target;

+ (VLCSelectItemInPopupPanel *)sharedInstance
{
    return _selectItemPanelInstance ? _selectItemPanelInstance : [[self alloc] init];
}

- (id)init
{
    if (_selectItemPanelInstance)
        [self dealloc];
    else
        _selectItemPanelInstance = [super init];

    return _selectItemPanelInstance;
}

- (IBAction)windowElementAction:(id)sender
{
    [_panel orderOut:sender];
    [NSApp endSheet: _panel];

    if (self.target) {
        if ([self.target respondsToSelector:@selector(panel:returnValue:item:)]) {
            if (sender == _cancel_btn)
                [self.target panel:self returnValue:NSCancelButton item:0];
            else
                [self.target panel:self returnValue:NSOKButton item:self.currentItem];
        }
    }
}

- (void)runModalForWindow:(NSWindow *)window
{
    [_title_lbl setStringValue:self.title];
    [_subtitle_lbl setStringValue:self.subTitle];
    [_cancel_btn setTitle:self.CancelButtonLabel];
    [_ok_btn setTitle:self.OKButtonLabel];
    [_pop removeAllItems];
    [_pop addItemsWithTitles:self.popupButtonContent];
    [NSApp beginSheet:_panel modalForWindow:window modalDelegate:self didEndSelector:NULL contextInfo:nil];
}

- (NSUInteger)currentItem
{
    return [_pop indexOfSelectedItem];
}

@end
