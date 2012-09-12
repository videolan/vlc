/*****************************************************************************
 * SharedDialogs.h: MacOS X interface module
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

#import <Cocoa/Cocoa.h>

@interface VLCEnterTextPanel : NSObject
{
    IBOutlet id _panel;
    IBOutlet id _title_lbl;
    IBOutlet id _subtitle_lbl;
    IBOutlet id _text_fld;
    IBOutlet id _cancel_btn;
    IBOutlet id _ok_btn;

    NSString * _title;
    NSString * _subtitle;
    NSString * _okTitle;
    NSString * _cancelTitle;
    id _target;
}
+ (VLCEnterTextPanel *)sharedInstance;

@property (readwrite, assign) NSString *title;
@property (readwrite, assign) NSString *subTitle;
@property (readwrite, assign) NSString *OKButtonLabel;
@property (readwrite, assign) NSString *CancelButtonLabel;
@property (readwrite, assign) id target;
@property (readonly) NSString *enteredText;

- (void)runModalForWindow:(NSWindow *)window;

- (IBAction)windowElementAction:(id)sender;

@end

@protocol VLCEnterTextPanel <NSObject>
@optional
- (void)panel:(VLCEnterTextPanel *)view returnValue:(NSUInteger)value text:(NSString *)text;
@end

@interface VLCSelectItemInPopupPanel : NSObject
{
    IBOutlet id _panel;
    IBOutlet id _title_lbl;
    IBOutlet id _subtitle_lbl;
    IBOutlet id _pop;
    IBOutlet id _cancel_btn;
    IBOutlet id _ok_btn;

    NSString * _title;
    NSString * _subtitle;
    NSString * _okTitle;
    NSString * _cancelTitle;
    NSArray * _popData;

    id _target;
}
+ (VLCSelectItemInPopupPanel *)sharedInstance;

@property (readwrite, assign) NSString *title;
@property (readwrite, assign) NSString *subTitle;
@property (readwrite, assign) NSString *OKButtonLabel;
@property (readwrite, assign) NSString *CancelButtonLabel;
@property (readwrite, assign) NSArray *popupButtonContent;
@property (readwrite, assign) id target;
@property (readonly) NSUInteger currentItem;

- (void)runModalForWindow:(NSWindow *)window;

- (IBAction)windowElementAction:(id)sender;

@end

@protocol VLCSelectItemInPopupPanel <NSObject>
@optional
- (void)panel:(VLCSelectItemInPopupPanel *)panel returnValue:(NSUInteger)value item:(NSUInteger)item;
@end
