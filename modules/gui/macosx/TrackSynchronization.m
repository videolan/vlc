/*****************************************************************************
 * TrackSynchronization.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2011 VLC authors and VideoLAN
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

#import "CompatibilityFixes.h"
#import "intf.h"
#import <vlc_common.h>
#import "TrackSynchronization.h"

@implementation VLCTrackSynchronization
static VLCTrackSynchronization *_o_sharedInstance = nil;

+ (VLCTrackSynchronization *)sharedInstance
{
    return _o_sharedInstance ? _o_sharedInstance : [[self alloc] init];
}

- (id)init
{
    if (_o_sharedInstance) {
        [self dealloc];
    } else {
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
    [o_av_advance_lbl setStringValue: _NS("Advance of audio over video:")];
    [[o_av_value_fld formatter] setFormat:[NSString stringWithFormat:@"#,##0.000 %@", _NS("s")]];
    [o_av_value_fld setToolTip: _NS("A positive value means that the audio is ahead of the video")];
    [o_sv_lbl setStringValue: _NS("Subtitles/Video")];
    [o_sv_advance_lbl setStringValue: _NS("Advance of subtitles over video:")];
    [[o_sv_advance_value_fld formatter] setFormat:[NSString stringWithFormat:@"#,##0.000 %@", _NS("s")]];
    [o_sv_advance_value_fld setToolTip: _NS("A positive value means that the subtitles are ahead of the video" )];
    [o_sv_speed_lbl setStringValue: _NS("Speed of the subtitles:")];
    [[o_sv_speed_value_fld formatter] setFormat:[NSString stringWithFormat:@"#,##0.000 %@", _NS("fps")]];

    if (OSX_LION)
        [o_window setCollectionBehavior: NSWindowCollectionBehaviorFullScreenAuxiliary];

    [self resetValues:self];
}

- (IBAction)toggleWindow:(id)sender
{
    if( [o_window isVisible] )
        [o_window orderOut:sender];
    else
        [o_window makeKeyAndOrderFront:sender];
}

- (IBAction)resetValues:(id)sender
{
    [o_av_value_fld setFloatValue:0.0];
    [o_sv_advance_value_fld setFloatValue:0.0];
    [o_sv_speed_value_fld setFloatValue:1.0];

    input_thread_t * p_input = pl_CurrentInput( p_intf );

    if( p_input )
    {
        var_SetTime( p_input, "audio-delay", 0.0 );
        var_SetTime( p_input, "spu-delay", 0.0 );
        var_SetFloat( p_input, "sub-fps", 1.0 );
        vlc_object_release( p_input );
    }
}

- (void)updateValues
{
    input_thread_t * p_input = pl_CurrentInput( p_intf );

    if( p_input )
    {
        [o_av_value_fld setFloatValue: var_GetTime( p_input, "audio-delay" ) / 1000000];
        [o_sv_advance_value_fld setFloatValue: var_GetTime( p_input, "spu-delay" ) / 1000000];
        [o_sv_speed_value_fld setFloatValue: var_GetFloat( p_input, "sub-fps" )];
        vlc_object_release( p_input );
    }
}

- (IBAction)avValueChanged:(id)sender
{
    if( sender == o_av_minus_btn )
        [o_av_value_fld setFloatValue: [o_av_value_fld floatValue] - 0.5];

    if( sender == o_av_plus_btn )
        [o_av_value_fld setFloatValue: [o_av_value_fld floatValue] + 0.5];

    input_thread_t * p_input = pl_CurrentInput( p_intf );

    if( p_input )
    {
        int64_t i_delay = [o_av_value_fld floatValue] * 1000000;
        var_SetTime( p_input, "audio-delay", i_delay );

        vlc_object_release( p_input );
    }
}

- (IBAction)svAdvanceValueChanged:(id)sender
{
    if( sender == o_sv_advance_minus_btn )
        [o_sv_advance_value_fld setFloatValue: [o_sv_advance_value_fld floatValue] - 0.5];

    if( sender == o_sv_advance_plus_btn )
        [o_sv_advance_value_fld setFloatValue: [o_sv_advance_value_fld floatValue] + 0.5];

    input_thread_t * p_input = pl_CurrentInput( p_intf );

    if( p_input )
    {
        int64_t i_delay = [o_sv_advance_value_fld floatValue] * 1000000;
        var_SetTime( p_input, "spu-delay", i_delay );

        vlc_object_release( p_input );
    }
}

- (IBAction)svSpeedValueChanged:(id)sender
{
    if( sender == o_sv_speed_minus_btn )
        [o_sv_speed_value_fld setFloatValue: [o_sv_speed_value_fld floatValue] - 0.5];

    if( sender == o_sv_speed_plus_btn )
        [o_sv_speed_value_fld setFloatValue: [o_sv_speed_value_fld floatValue] + 0.5];

    input_thread_t * p_input = pl_CurrentInput( p_intf );

    if( p_input )
    {
        var_SetFloat( p_input, "sub-fps", [o_av_value_fld floatValue] );

        vlc_object_release( p_input );
    }
}

- (void)controlTextDidChange:(NSNotification *)aNotification
{
    if( [aNotification object] == o_av_value_fld )
        [self avValueChanged:self];
    else if( [aNotification object] == o_sv_advance_value_fld )
        [self svAdvanceValueChanged:self];
    else if( [aNotification object] == o_sv_speed_value_fld )
        [self svSpeedValueChanged:self];
}

@end
