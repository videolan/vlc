/*****************************************************************************
 * MainWindowTitle.h: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2011-2012 Felix Paul Kühne
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
#import "misc.h"

/*****************************************************************************
 * VLCMainWindowTitleView
 *****************************************************************************/

@interface VLCMainWindowTitleView : VLCThreePartImageView
{
    NSImage * o_red_img;
    NSImage * o_red_over_img;
    NSImage * o_red_on_img;
    NSImage * o_yellow_img;
    NSImage * o_yellow_over_img;
    NSImage * o_yellow_on_img;
    NSImage * o_green_img;
    NSImage * o_green_over_img;
    NSImage * o_green_on_img;
    NSShadow * o_window_title_shadow;
    NSDictionary * o_window_title_attributes_dict;

    IBOutlet id o_red_btn;
    IBOutlet id o_yellow_btn;
    IBOutlet id o_green_btn;
    IBOutlet id o_fullscreen_btn;
    IBOutlet id o_title_lbl;
}
@property (readonly) NSButton * closeButton;
@property (readonly) NSButton * minimizeButton;
@property (readonly) NSButton * zoomButton;

- (void)loadButtonIcons;
- (IBAction)buttonAction:(id)sender;
- (void)setWindowTitle:(NSString *)title;
- (void)setFullscreenButtonHidden:(BOOL)b_value;
- (void)setWindowButtonOver:(BOOL)b_value;
- (void)setWindowFullscreenButtonOver:(BOOL)b_value;

@end

@interface VLCWindowButtonCell : NSButtonCell

@end

@interface VLCResizeControl : NSImageView

@end

@interface VLCColorView : NSView

@end

@interface VLCCustomWindowButtonPrototype: NSButton
- (NSArray*)extendedAccessibilityAttributeNames: (NSArray*)theAttributeNames;
- (id)extendedAccessibilityAttributeValue: (NSString*)theAttributeName;
- (NSNumber*)extendedAccessibilityIsAttributeSettable: (NSString*)theAttributeName;

@end

@interface VLCCustomWindowCloseButton: VLCCustomWindowButtonPrototype

@end


@interface VLCCustomWindowMinimizeButton: VLCCustomWindowButtonPrototype

@end


@interface VLCCustomWindowZoomButton: VLCCustomWindowButtonPrototype

@end

@interface VLCCustomWindowFullscreenButton : VLCCustomWindowButtonPrototype

@end

@interface VLCWindowTitleTextField : NSTextField
{
    NSMenu * contextMenu;
}

@end
