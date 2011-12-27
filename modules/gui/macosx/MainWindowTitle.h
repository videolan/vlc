/*****************************************************************************
 * MainWindowTitle.h: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2011 Felix Paul Kühne
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

/*****************************************************************************
 * VLCMainWindowTitleView
 *****************************************************************************/

@interface VLCMainWindowTitleView : NSImageView
{
    IBOutlet id o_red_btn;
    IBOutlet id o_yellow_btn;
    IBOutlet id o_green_btn;
    IBOutlet id o_fullscreen_btn;
    IBOutlet id o_title_lbl;
}

- (IBAction)buttonAction:(id)sender;
- (void)setWindowTitle:(NSString *)title;
- (void)setFullscreenButtonHidden:(BOOL)b_value;
- (void)setWindowButtonOver:(BOOL)b_value;
- (void)setWindowFullscreenButtonOver:(BOOL)b_value;

@end

@interface VLCWindowButtonCell : NSButtonCell
{
}
@end

@interface VLCResizeControl : NSImageView
{
}

@end
