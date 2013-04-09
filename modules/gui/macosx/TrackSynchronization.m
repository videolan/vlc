/*****************************************************************************
 * TrackSynchronization.m: MacOS X interface module
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

#import "CompatibilityFixes.h"
#import "intf.h"
#import "VideoEffects.h"
#import <vlc_common.h>
#import "TrackSynchronization.h"

#define SUBSDELAY_CFG_MODE                     "subsdelay-mode"
#define SUBSDELAY_CFG_FACTOR                   "subsdelay-factor"
#define SUBSDELAY_MODE_ABSOLUTE                0
#define SUBSDELAY_MODE_RELATIVE_SOURCE_DELAY   1
#define SUBSDELAY_MODE_RELATIVE_SOURCE_CONTENT 2

@implementation VLCTrackSynchronization
static VLCTrackSynchronization *_o_sharedInstance = nil;

+ (VLCTrackSynchronization *)sharedInstance
{
    return _o_sharedInstance ? _o_sharedInstance : [[self alloc] init];
}

- (id)init
{
    if (_o_sharedInstance)
        [self dealloc];
    else {
        p_intf = VLCIntf;
        _o_sharedInstance = [super init];
    }

    return _o_sharedInstance;
}

- (void)awakeFromNib
{
    [o_window setTitle:_NS("Track Synchronization")];
    [o_reset_btn setTitle:_NS("Reset")];
    [o_av_lbl setStringValue:_NS("Audio/Video")];
    [o_av_advance_lbl setStringValue: _NS("Audio track synchronization:")];
    [[o_av_value_fld formatter] setFormat:[NSString stringWithFormat:@"#,##0.000 %@", _NS("s")]];
    [o_av_value_fld setToolTip: _NS("A positive value means that the audio is ahead of the video")];
    [o_sv_lbl setStringValue: _NS("Subtitles/Video")];
    [o_sv_advance_lbl setStringValue: _NS("Subtitle track synchronization:")];
    [[o_sv_advance_value_fld formatter] setFormat:[NSString stringWithFormat:@"#,##0.000 %@", _NS("s")]];
    [o_sv_advance_value_fld setToolTip: _NS("A positive value means that the subtitles are ahead of the video")];
    [o_sv_speed_lbl setStringValue: _NS("Subtitle speed:")];
    [[o_sv_speed_value_fld formatter] setFormat:[NSString stringWithFormat:@"#,##0.000 %@", _NS("fps")]];
    [o_sv_dur_lbl setStringValue: _NS("Subtitle duration factor:")];

    int i_mode = var_InheritInteger(p_intf, SUBSDELAY_CFG_MODE);
    NSString * o_toolTip, * o_suffix;

    switch (i_mode) {
        default:
        case SUBSDELAY_MODE_ABSOLUTE:
            o_toolTip = _NS("Extend subtitle duration by this value.\nSet 0 to disable.");
            o_suffix = @" s";
            break;
        case SUBSDELAY_MODE_RELATIVE_SOURCE_DELAY:
            o_toolTip = _NS("Multiply subtitle duration by this value.\nSet 0 to disable.");
            o_suffix = @"";
            break;
        case SUBSDELAY_MODE_RELATIVE_SOURCE_CONTENT:
            o_toolTip = _NS("Recalculate subtitle duration according\nto their content and this value.\nSet 0 to disable.");
            o_suffix = @"";
            break;
    }

    [[o_sv_dur_value_fld formatter] setFormat:[NSString stringWithFormat:@"#,##0.000%@", o_suffix]];
    [o_sv_dur_value_fld setToolTip: o_toolTip];

    if (!OSX_SNOW_LEOPARD)
        [o_window setCollectionBehavior: NSWindowCollectionBehaviorFullScreenAuxiliary];

    [self resetValues:self];
}

- (void)updateCocoaWindowLevel:(NSInteger)i_level
{
    if (o_window && [o_window isVisible] && [o_window level] != i_level)
        [o_window setLevel: i_level];
}

- (IBAction)toggleWindow:(id)sender
{
    if ([o_window isVisible])
        [o_window orderOut:sender];
    else {
        [o_window setLevel: [[[VLCMain sharedInstance] voutController] currentWindowLevel]];
        [o_window makeKeyAndOrderFront:sender];
    }
}

- (IBAction)resetValues:(id)sender
{
    [o_av_value_fld setFloatValue:0.0];
    [o_sv_advance_value_fld setFloatValue:0.0];
    [o_sv_speed_value_fld setFloatValue:1.0];
    [o_sv_dur_value_fld setFloatValue:0.0];
    [o_av_stp setFloatValue:0.0];
    [o_sv_advance_stp setFloatValue:0.0];
    [o_sv_speed_stp setFloatValue:1.0];
    [o_sv_dur_stp setFloatValue:0.0];

    input_thread_t * p_input = pl_CurrentInput(p_intf);

    if (p_input) {
        var_SetTime(p_input, "audio-delay", 0.0);
        var_SetTime(p_input, "spu-delay", 0.0);
        var_SetFloat(p_input, "sub-fps", 1.0);
        [self svDurationValueChanged:nil];
        vlc_object_release(p_input);
    }
}

- (void)updateValues
{
    input_thread_t * p_input = pl_CurrentInput(p_intf);

    if (p_input) {
        [o_av_value_fld setDoubleValue: var_GetTime(p_input, "audio-delay") / 1000000.];
        [o_sv_advance_value_fld setDoubleValue: var_GetTime(p_input, "spu-delay") / 1000000.];
        [o_sv_speed_value_fld setFloatValue: var_GetFloat(p_input, "sub-fps")];
        vlc_object_release(p_input);
    }
    [o_av_stp setDoubleValue: [o_av_value_fld doubleValue]];
    [o_sv_advance_stp setDoubleValue: [o_sv_advance_value_fld doubleValue]];
    [o_sv_speed_stp setDoubleValue: [o_sv_speed_value_fld doubleValue]];
}

- (IBAction)avValueChanged:(id)sender
{
    if (sender == o_av_stp)
        [o_av_value_fld setDoubleValue: [o_av_stp doubleValue]];
    else
        [o_av_stp setDoubleValue: [o_av_value_fld doubleValue]];

    input_thread_t * p_input = pl_CurrentInput(p_intf);

    if (p_input) {
        var_SetTime(p_input, "audio-delay", [o_av_value_fld doubleValue] * 1000000.);

        vlc_object_release(p_input);
    }
}

- (IBAction)svAdvanceValueChanged:(id)sender
{
    if (sender == o_sv_advance_stp)
        [o_sv_advance_value_fld setDoubleValue: [o_sv_advance_stp doubleValue]];
    else
        [o_sv_advance_stp setDoubleValue: [o_sv_advance_value_fld doubleValue]];

    input_thread_t * p_input = pl_CurrentInput(p_intf);

    if (p_input) {
        var_SetTime(p_input, "spu-delay", [o_sv_advance_value_fld doubleValue] * 1000000.);

        vlc_object_release(p_input);
    }
}

- (IBAction)svSpeedValueChanged:(id)sender
{
    if (sender == o_sv_speed_stp)
        [o_sv_speed_value_fld setFloatValue: [o_sv_speed_stp floatValue]];
    else
        [o_sv_speed_stp setFloatValue: [o_sv_speed_value_fld floatValue]];

    input_thread_t * p_input = pl_CurrentInput(p_intf);

    if (p_input) {
        var_SetFloat(p_input, "sub-fps", [o_sv_speed_value_fld floatValue]);

        vlc_object_release(p_input);
    }
}

- (IBAction)svDurationValueChanged:(id)sender
{
    if (sender == o_sv_dur_stp)
        [o_sv_dur_value_fld setFloatValue: [o_sv_dur_stp floatValue]];
    else
        [o_sv_dur_stp setFloatValue: [o_sv_dur_value_fld floatValue]];

    input_thread_t * p_input = pl_CurrentInput(p_intf);

    if (p_input) {
        float f_factor = [o_sv_dur_value_fld floatValue];
        config_PutFloat(p_intf, SUBSDELAY_CFG_FACTOR, f_factor);

        /* Try to find an instance of subsdelay, and set its factor */
        vlc_object_t *p_obj = (vlc_object_t *) vlc_object_find_name(p_intf->p_libvlc, "subsdelay");
        if (p_obj) {
            var_SetFloat(p_obj, SUBSDELAY_CFG_FACTOR, f_factor);
            vlc_object_release(p_obj);
        }
        [[VLCVideoEffects sharedInstance] setVideoFilter: "subsdelay" on: f_factor > 0];

        vlc_object_release(p_input);
    }
}

@end
