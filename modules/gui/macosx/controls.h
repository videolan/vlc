/*****************************************************************************
 * controls.h: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2002-2012 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Jon Lech Johansen <jon-vl@nanocrew.net>
 *          Derk-Jan Hartman <thedj@users.sourceforge.net>
 *          Felix Paul Kühne <fkuehne at videolan org>
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

/*****************************************************************************
 * VLCControls interface
 *****************************************************************************/
@interface VLCControls : NSObject
{
    IBOutlet id o_main;

    IBOutlet id o_specificTime_cancel_btn;
    IBOutlet id o_specificTime_enter_fld;
    IBOutlet id o_specificTime_goTo_lbl;
    IBOutlet id o_specificTime_ok_btn;
    IBOutlet id o_specificTime_win;
    IBOutlet id o_specificTime_sec_lbl;
    IBOutlet id o_specificTime_stepper;
    IBOutlet id o_specificTime_mi;
}

@property (nonatomic) int jumpTimeValue;

- (IBAction)play:(id)sender;
- (IBAction)stop:(id)sender;

- (IBAction)prev:(id)sender;
- (IBAction)next:(id)sender;
- (IBAction)random:(id)sender;
- (IBAction)repeat:(id)sender;
- (IBAction)loop:(id)sender;
- (IBAction)quitAfterPlayback:(id)sender;

- (IBAction)forward:(id)sender;
- (IBAction)backward:(id)sender;

- (IBAction)volumeUp:(id)sender;
- (IBAction)volumeDown:(id)sender;
- (IBAction)mute:(id)sender;
- (IBAction)volumeSliderUpdated:(id)sender;

- (IBAction)showPosition: (id)sender;

- (IBAction)lockVideosAspectRatio:(id)sender;

- (BOOL)keyEvent:(NSEvent *)o_event;

- (IBAction)goToSpecificTime:(id)sender;
@end

