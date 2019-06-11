/*****************************************************************************
 * VLCTrackSynchronizationWindowController.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2011-2014 VLC authors and VideoLAN
 * Copyright (C) 2011-2019 Felix Paul Kühne
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

#import "VLCTrackSynchronizationWindowController.h"

#import "extensions/NSString+Helpers.h"
#import "coreinteraction/VLCVideoFilterHelper.h"
#import "main/VLCMain.h"
#import "playlist/VLCPlaylistController.h"
#import "playlist/VLCPlayerController.h"
#import "windows/video/VLCVideoOutputProvider.h"

#define SUBSDELAY_CFG_MODE                     "subsdelay-mode"
#define SUBSDELAY_CFG_FACTOR                   "subsdelay-factor"
#define SUBSDELAY_MODE_ABSOLUTE                0
#define SUBSDELAY_MODE_RELATIVE_SOURCE_DELAY   1
#define SUBSDELAY_MODE_RELATIVE_SOURCE_CONTENT 2

@interface VLCTrackSynchronizationWindowController()
{
    VLCPlayerController *_playerController;
}
@end

@implementation VLCTrackSynchronizationWindowController

- (id)init
{
    self = [super initWithWindowNibName:@"SyncTracks"];
    return self;
}

- (void)dealloc
{
    [[NSNotificationCenter defaultCenter] removeObserver:self];
}

- (void)windowDidLoad
{
    _playerController = [[[VLCMain sharedInstance] playlistController] playerController];
    NSNotificationCenter *notificationCenter = [NSNotificationCenter defaultCenter];
    [notificationCenter addObserver:self
                           selector:@selector(updateValues:)
                               name:VLCPlayerCurrentMediaItemChanged
                             object:nil];
    [notificationCenter addObserver:self
                           selector:@selector(updateValues:)
                               name:VLCPlayerAudioDelayChanged
                             object:nil];
    [notificationCenter addObserver:self
                           selector:@selector(updateValues:)
                               name:VLCPlayerSubtitlesDelayChanged
                             object:nil];
    [notificationCenter addObserver:self
                           selector:@selector(updateValues:)
                               name:VLCPlayerSubtitlesFPSChanged
                             object:nil];
    [notificationCenter addObserver:self
                           selector:@selector(updateCocoaWindowLevel:)
                               name:VLCWindowShouldUpdateLevel
                             object:nil];

    [self.window setTitle:_NS("Track Synchronization")];
    [_resetButton setTitle:_NS("Reset")];
    [_avLabel setStringValue:_NS("Audio/Video")];
    [_av_advanceLabel setStringValue: _NS("Audio track synchronization:")];
    [[_av_advanceTextField formatter] setFormat:[NSString stringWithFormat:@"#,##0.000 %@", _NS("s")]];
    [_av_advanceTextField setToolTip: _NS("A positive value means that the audio is ahead of the video")];
    [_svLabel setStringValue: _NS("Subtitles/Video")];
    [_sv_advanceLabel setStringValue: _NS("Subtitle track synchronization:")];
    [[_sv_advanceTextField formatter] setFormat:[NSString stringWithFormat:@"#,##0.000 %@", _NS("s")]];
    [_sv_advanceTextField setToolTip: _NS("A positive value means that the subtitles are ahead of the video")];
    [_sv_speedLabel setStringValue: _NS("Subtitle speed:")];
    [[_sv_speedTextField formatter] setFormat:[NSString stringWithFormat:@"#,##0.000 %@", _NS("fps")]];
    [_sv_durLabel setStringValue: _NS("Subtitle duration factor:")];

    int i_mode = (int)var_InheritInteger(getIntf(), SUBSDELAY_CFG_MODE);
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

    [[_sv_durTextField formatter] setFormat:[NSString stringWithFormat:@"#,##0.000%@", o_suffix]];
    [_sv_durTextField setToolTip: o_toolTip];

    [self.window setCollectionBehavior: NSWindowCollectionBehaviorFullScreenAuxiliary];

    [self resetValues:self];
}

- (void)updateCocoaWindowLevel:(NSNotification *)aNotification
{
    NSInteger i_level = [aNotification.userInfo[VLCWindowLevelKey] integerValue];
    if (self.isWindowLoaded && [self.window isVisible] && [self.window level] != i_level)
        [self.window setLevel: i_level];
}

- (IBAction)toggleWindow:(id)sender
{
    if ([self.window isVisible])
        [self.window orderOut:sender];
    else {
        [self.window setLevel: [[[VLCMain sharedInstance] voutProvider] currentStatusWindowLevel]];
        [self.window makeKeyAndOrderFront:sender];

        [self updateValues:nil];
    }
}

- (IBAction)resetValues:(id)sender
{
    [_av_advanceTextField setFloatValue:0.0];
    [_sv_advanceTextField setFloatValue:0.0];
    [_sv_speedTextField setFloatValue:1.0];
    [_sv_durTextField setFloatValue:0.0];
    [_avStepper setFloatValue:0.0];
    [_sv_advanceStepper setFloatValue:0.0];
    [_sv_speedStepper setFloatValue:1.0];
    [_sv_durStepper setFloatValue:0.0];

    _playerController.audioDelay = 0;
    _playerController.subtitlesDelay = 0;
    _playerController.subtitlesFPS = 1.;

    [self svDurationValueChanged:nil];
}

- (void)updateValues:(NSNotification *)aNotification
{
    [_av_advanceTextField setDoubleValue: secf_from_vlc_tick(_playerController.audioDelay)];
    [_sv_advanceTextField setDoubleValue: secf_from_vlc_tick(_playerController.subtitlesDelay)];
    [_sv_speedTextField setFloatValue: _playerController.subtitlesFPS];

    [_avStepper setDoubleValue: [_av_advanceTextField doubleValue]];
    [_sv_advanceStepper setDoubleValue: [_sv_advanceTextField doubleValue]];
    [_sv_speedStepper setDoubleValue: [_sv_speedTextField doubleValue]];
}

- (IBAction)avValueChanged:(id)sender
{
    if (sender == _avStepper)
        [_av_advanceTextField setDoubleValue: [_avStepper doubleValue]];
    else
        [_avStepper setDoubleValue: [_av_advanceTextField doubleValue]];

    _playerController.audioDelay = vlc_tick_from_sec([_av_advanceTextField doubleValue]);
}

- (IBAction)svAdvanceValueChanged:(id)sender
{
    if (sender == _sv_advanceStepper)
        [_sv_advanceTextField setDoubleValue: [_sv_advanceStepper doubleValue]];
    else
        [_sv_advanceStepper setDoubleValue: [_sv_advanceTextField doubleValue]];

    _playerController.subtitlesDelay = vlc_tick_from_sec([_sv_advanceTextField doubleValue]);
}

- (IBAction)svSpeedValueChanged:(id)sender
{
    if (sender == _sv_speedStepper)
        [_sv_speedTextField setFloatValue: [_sv_speedStepper floatValue]];
    else
        [_sv_speedStepper setFloatValue: [_sv_speedTextField floatValue]];

    _playerController.subtitlesFPS = [_sv_speedTextField floatValue];
}

- (IBAction)svDurationValueChanged:(id)sender
{
    if (sender == _sv_durStepper)
        [_sv_durTextField setFloatValue: [_sv_durStepper floatValue]];
    else
        [_sv_durStepper setFloatValue: [_sv_durTextField floatValue]];

    float f_factor = [_sv_durTextField floatValue];
    NSArray<NSValue *> *vouts = [_playerController allVideoOutputThreads];

    if (vouts) {
        for (NSValue *ptr in vouts) {
            vout_thread_t *p_vout = [ptr pointerValue];

            var_SetFloat(p_vout, SUBSDELAY_CFG_FACTOR, f_factor);
            vout_Release(p_vout);
        }
    }
    [VLCVideoFilterHelper setVideoFilter: "subsdelay" on: f_factor > 0];
}

@end
