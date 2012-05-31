/*****************************************************************************
 * ConvertAndSave.h: MacOS X interface module
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

@interface VLCConvertAndSave : NSObject
{
    IBOutlet id _window;
    IBOutlet id _cancel_btn;
    IBOutlet id _ok_btn;

    IBOutlet id _drop_lbl;
    IBOutlet id _drop_image_view;
    IBOutlet id _drop_btn;
    IBOutlet id _drop_box;

    IBOutlet id _profile_lbl;
    IBOutlet id _profile_pop;
    IBOutlet id _profile_btn;

    IBOutlet id _destination_lbl;
    IBOutlet id _destination_btn;
    IBOutlet id _destination_icon_view;
    IBOutlet id _destination_filename_lbl;
    IBOutlet id _destination_filename_stub_lbl;

    IBOutlet id _dropin_view;
    IBOutlet id _dropin_icon_view;
    IBOutlet id _dropin_media_lbl;

    NSString * _MRL;
}
@property (readwrite, nonatomic, retain) NSString * MRL;

+ (VLCConvertAndSave *)sharedInstance;

- (IBAction)toggleWindow;

- (IBAction)windowButtonAction:(id)sender;
- (IBAction)openMedia:(id)sender;
- (IBAction)profileSelection:(id)sender;
- (IBAction)customizeProfile:(id)sender;
- (IBAction)chooseDestination:(id)sender;

- (void)updateDropView;

@end

@interface VLCDropEnabledBox : NSBox

@end

@interface VLCDropEnabledImageView : NSImageView

@end

@interface VLCDropEnabledButton : NSButton

@end
