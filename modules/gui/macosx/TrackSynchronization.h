/*****************************************************************************
 * TrackSynchronization.h: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2011-2012 VLC authors and VideoLAN
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


@interface VLCTrackSynchronization : NSObject {
    /* generic */
    IBOutlet id o_window;
    intf_thread_t *p_intf;
    IBOutlet id o_reset_btn;

    /* Audio / Video */
    IBOutlet id o_av_lbl;
    IBOutlet id o_av_advance_lbl;
    IBOutlet id o_av_value_fld;
    IBOutlet id o_av_stp;

    /* Subtitles / Video */
    IBOutlet id o_sv_lbl;
    IBOutlet id o_sv_advance_lbl;
    IBOutlet id o_sv_advance_value_fld;
    IBOutlet id o_sv_advance_stp;
    IBOutlet id o_sv_speed_lbl;
    IBOutlet id o_sv_speed_value_fld;
    IBOutlet id o_sv_speed_stp;
    IBOutlet id o_sv_dur_lbl;
    IBOutlet id o_sv_dur_value_fld;
    IBOutlet id o_sv_dur_stp;
}

/* generic */
+ (VLCTrackSynchronization *)sharedInstance;

- (void)updateCocoaWindowLevel:(NSInteger)i_level;
- (IBAction)toggleWindow:(id)sender;
- (IBAction)resetValues:(id)sender;
- (void)updateValues;

/* Audio / Video */
- (IBAction)avValueChanged:(id)sender;

/* Subtitles / Video */
- (IBAction)svAdvanceValueChanged:(id)sender;
- (IBAction)svSpeedValueChanged:(id)sender;
- (IBAction)svDurationValueChanged:(id)sender;
@end
