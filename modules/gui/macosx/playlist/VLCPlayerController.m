/*****************************************************************************
 * VLCPlayerController.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2019 VLC authors and VideoLAN
 *
 * Authors: Felix Paul Kühne <fkuehne # videolan -dot- org>
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

#import "VLCPlayerController.h"
#import "main/VLCMain.h"

NSString *VLCPlayerCurrentMediaItem = @"VLCPlayerCurrentMediaItem";
NSString *VLCPlayerCurrentMediaItemChanged = @"VLCPlayerCurrentMediaItemChanged";
NSString *VLCPlayerStateChanged = @"VLCPlayerStateChanged";
NSString *VLCPlayerErrorChanged = @"VLCPlayerErrorChanged";
NSString *VLCPlayerBufferFill = @"VLCPlayerBufferFill";
NSString *VLCPlayerBufferChanged = @"VLCPlayerBufferChanged";
NSString *VLCPlayerRateChanged = @"VLCPlayerRateChanged";
NSString *VLCPlayerCapabilitiesChanged = @"VLCPlayerCapabilitiesChanged";
NSString *VLCPlayerTimeAndPositionChanged = @"VLCPlayerTimeAndPositionChanged";
NSString *VLCPlayerLengthChanged = @"VLCPlayerLengthChanged";
NSString *VLCPlayerTeletextMenuAvailable = @"VLCPlayerTeletextMenuAvailable";
NSString *VLCPlayerTeletextEnabled = @"VLCPlayerTeletextEnabled";
NSString *VLCPlayerTeletextPageChanged = @"VLCPlayerTeletextPageChanged";
NSString *VLCPlayerTeletextTransparencyChanged = @"VLCPlayerTeletextTransparencyChanged";
NSString *VLCPlayerAudioDelayChanged = @"VLCPlayerAudioDelayChanged";
NSString *VLCPlayerSubtitlesDelayChanged = @"VLCPlayerSubtitlesDelayChanged";
NSString *VLCPlayerSubtitleTextScalingFactorChanged = @"VLCPlayerSubtitleTextScalingFactorChanged";
NSString *VLCPlayerRecordingChanged = @"VLCPlayerRecordingChanged";
NSString *VLCPlayerFullscreenChanged = @"VLCPlayerFullscreenChanged";
NSString *VLCPlayerWallpaperModeChanged = @"VLCPlayerWallpaperModeChanged";
NSString *VLCPlayerVolumeChanged = @"VLCPlayerVolumeChanged";
NSString *VLCPlayerMuteChanged = @"VLCPlayerMuteChanged";

@interface VLCPlayerController ()
{
    vlc_player_t *_p_player;
    vlc_player_listener_id *_playerListenerID;
    vlc_player_aout_listener_id *_playerAoutListenerID;
    vlc_player_vout_listener_id *_playerVoutListenerID;
    NSNotificationCenter *_defaultNotificationCenter;
}

- (void)currentMediaItemChanged:(input_item_t *)newMediaItem;
- (void)stateChanged:(enum vlc_player_state)state;
- (void)errorChanged:(enum vlc_player_error)error;
- (void)newBufferingValue:(float)bufferValue;
- (void)newRateValue:(float)rateValue;
- (void)capabilitiesChanged:(int)newCapabilities;
- (void)position:(float)position andTimeChanged:(vlc_tick_t)time;
- (void)lengthChanged:(vlc_tick_t)length;
- (void)teletextAvailibilityChanged:(BOOL)hasTeletextMenu;
- (void)teletextEnabledChanged:(BOOL)teletextOn;
- (void)teletextPageChanged:(unsigned int)page;
- (void)teletextTransparencyChanged:(BOOL)isTransparent;
- (void)audioDelayChanged:(vlc_tick_t)audioDelay;
- (void)subtitlesDelayChanged:(vlc_tick_t)subtitlesDelay;
- (void)recordingChanged:(BOOL)recording;
- (void)stopActionChanged:(enum vlc_player_media_stopped_action)stoppedAction;

/* video */
- (void)fullscreenChanged:(BOOL)isFullscreen;
- (void)wallpaperModeChanged:(BOOL)wallpaperModeValue;

/* audio */
- (void)volumeChanged:(float)volume;
- (void)muteChanged:(BOOL)mute;
@end

#pragma mark - player callback implementations

static void cb_player_current_media_changed(vlc_player_t *p_player, input_item_t *p_newMediaItem, void *p_data)
{
    VLC_UNUSED(p_player);
    dispatch_async(dispatch_get_main_queue(), ^{
        VLCPlayerController *playerController = (__bridge VLCPlayerController *)p_data;
        [playerController currentMediaItemChanged:p_newMediaItem];
    });
}

static void cb_player_state_changed(vlc_player_t *p_player, enum vlc_player_state state, void *p_data)
{
    VLC_UNUSED(p_player);
    dispatch_async(dispatch_get_main_queue(), ^{
        VLCPlayerController *playerController = (__bridge VLCPlayerController *)p_data;
        [playerController stateChanged:state];
    });
}

static void cb_player_error_changed(vlc_player_t *p_player, enum vlc_player_error error, void *p_data)
{
    VLC_UNUSED(p_player);
    dispatch_async(dispatch_get_main_queue(), ^{
        VLCPlayerController *playerController = (__bridge VLCPlayerController *)p_data;
        [playerController errorChanged:error];
    });
}

static void cb_player_buffering(vlc_player_t *p_player, float newBufferValue, void *p_data)
{
    VLC_UNUSED(p_player);
    dispatch_async(dispatch_get_main_queue(), ^{
        VLCPlayerController *playerController = (__bridge VLCPlayerController *)p_data;
        [playerController newBufferingValue:newBufferValue];
    });
}

static void cb_player_rate_changed(vlc_player_t *p_player, float newRateValue, void *p_data)
{
    VLC_UNUSED(p_player);
    dispatch_async(dispatch_get_main_queue(), ^{
        VLCPlayerController *playerController = (__bridge VLCPlayerController *)p_data;
        [playerController newRateValue:newRateValue];
    });
}

static void cb_player_capabilities_changed(vlc_player_t *p_player, int newCapabilities, void *p_data)
{
    VLC_UNUSED(p_player);
    dispatch_async(dispatch_get_main_queue(), ^{
        VLCPlayerController *playerController = (__bridge VLCPlayerController *)p_data;
        [playerController capabilitiesChanged:newCapabilities];
    });
}

static void cb_player_position_changed(vlc_player_t *p_player, vlc_tick_t time, float position, void *p_data)
{
    VLC_UNUSED(p_player);
    dispatch_async(dispatch_get_main_queue(), ^{
        VLCPlayerController *playerController = (__bridge VLCPlayerController *)p_data;
        [playerController position:position andTimeChanged:time];
    });
}

static void cb_player_length_changed(vlc_player_t *p_player, vlc_tick_t newLength, void *p_data)
{
    VLC_UNUSED(p_player);
    dispatch_async(dispatch_get_main_queue(), ^{
        VLCPlayerController *playerController = (__bridge VLCPlayerController *)p_data;
        [playerController lengthChanged:newLength];
    });
}

static void cb_player_teletext_menu_availability_changed(vlc_player_t *p_player, bool hasTeletextMenu, void *p_data)
{
    VLC_UNUSED(p_player);
    dispatch_async(dispatch_get_main_queue(), ^{
        VLCPlayerController *playerController = (__bridge VLCPlayerController *)p_data;
        [playerController teletextAvailibilityChanged:hasTeletextMenu];
    });
}

static void cb_player_teletext_enabled_changed(vlc_player_t *p_player, bool teletextEnabled, void *p_data)
{
    VLC_UNUSED(p_player);
    dispatch_async(dispatch_get_main_queue(), ^{
        VLCPlayerController *playerController = (__bridge VLCPlayerController *)p_data;
        [playerController teletextEnabledChanged:teletextEnabled];
    });
}

static void cb_player_teletext_page_changed(vlc_player_t *p_player, unsigned page, void *p_data)
{
    VLC_UNUSED(p_player);
    dispatch_async(dispatch_get_main_queue(), ^{
        VLCPlayerController *playerController = (__bridge VLCPlayerController *)p_data;
        [playerController teletextPageChanged:page];
    });
}

static void cb_player_teletext_transparency_changed(vlc_player_t *p_player, bool isTransparent, void *p_data)
{
    VLC_UNUSED(p_player);
    dispatch_async(dispatch_get_main_queue(), ^{
        VLCPlayerController *playerController = (__bridge VLCPlayerController *)p_data;
        [playerController teletextTransparencyChanged:isTransparent];
    });
}

static void cb_player_audio_delay_changed(vlc_player_t *p_player, vlc_tick_t newDelay, void *p_data)
{
    VLC_UNUSED(p_player);
    dispatch_async(dispatch_get_main_queue(), ^{
        VLCPlayerController *playerController = (__bridge VLCPlayerController *)p_data;
        [playerController audioDelayChanged:newDelay];
    });
}

static void cb_player_subtitle_delay_changed(vlc_player_t *p_player, vlc_tick_t newDelay, void *p_data)
{
    VLC_UNUSED(p_player);
    dispatch_async(dispatch_get_main_queue(), ^{
        VLCPlayerController *playerController = (__bridge VLCPlayerController *)p_data;
        [playerController audioDelayChanged:newDelay];
    });
}

static void cb_player_record_changed(vlc_player_t *p_player, bool recording, void *p_data)
{
    VLC_UNUSED(p_player);
    dispatch_async(dispatch_get_main_queue(), ^{
        VLCPlayerController *playerController = (__bridge VLCPlayerController *)p_data;
        [playerController recordingChanged:recording];
    });
}

static void cb_player_media_stopped_action_changed(vlc_player_t *p_player,
                                                   enum vlc_player_media_stopped_action newAction,
                                                   void *p_data)
{

}

static const struct vlc_player_cbs player_callbacks = {
    cb_player_current_media_changed,
    cb_player_state_changed,
    cb_player_error_changed,
    cb_player_buffering,
    cb_player_rate_changed,
    cb_player_capabilities_changed,
    cb_player_position_changed,
    cb_player_length_changed,
    NULL, //cb_player_track_list_changed,
    NULL, //cb_player_track_selection_changed,
    NULL, //cb_player_program_list_changed,
    NULL, //cb_player_program_selection_changed,
    NULL, //cb_player_titles_changed,
    NULL, //cb_player_title_selection_changed,
    NULL, //cb_player_chapter_selection_changed,
    cb_player_teletext_menu_availability_changed,
    cb_player_teletext_enabled_changed,
    cb_player_teletext_page_changed,
    cb_player_teletext_transparency_changed,
    cb_player_audio_delay_changed,
    cb_player_subtitle_delay_changed,
    NULL, //cb_player_associated_subs_fps_changed,
    NULL, //cb_player_renderer_changed,
    cb_player_record_changed,
    NULL, //cb_player_signal_changed,
    NULL, //cb_player_stats_changed,
    NULL, //cb_player_atobloop_changed,
    cb_player_media_stopped_action_changed,
    NULL, //cb_player_item_meta_changed,
    NULL, //cb_player_item_epg_changed,
    NULL, //cb_player_subitems_changed,
    NULL, //cb_player_vout_list_changed,
};

#pragma mark - video specific callback implementations

static void cb_player_vout_fullscreen_changed(vlc_player_t *p_player, vout_thread_t *p_vout, bool isFullscreen, void *p_data)
{
    VLC_UNUSED(p_player); VLC_UNUSED(p_vout);
    dispatch_async(dispatch_get_main_queue(), ^{
        VLCPlayerController *playerController = (__bridge VLCPlayerController *)p_data;
        [playerController fullscreenChanged:isFullscreen];
    });
}

static void cb_player_vout_wallpaper_mode_changed(vlc_player_t *p_player, vout_thread_t *p_vout,  bool wallpaperModeEnabled, void *p_data)
{
    VLC_UNUSED(p_player); VLC_UNUSED(p_vout);
    dispatch_async(dispatch_get_main_queue(), ^{
        VLCPlayerController *playerController = (__bridge VLCPlayerController *)p_data;
        [playerController wallpaperModeChanged:wallpaperModeEnabled];
    });
}

static const struct vlc_player_vout_cbs player_vout_callbacks = {
    cb_player_vout_fullscreen_changed,
    cb_player_vout_wallpaper_mode_changed,
};

#pragma mark - video specific callback implementations

static void cb_player_aout_volume_changed(vlc_player_t *p_player, float volume, void *p_data)
{
    VLC_UNUSED(p_player);
    dispatch_async(dispatch_get_main_queue(), ^{
        VLCPlayerController *playerController = (__bridge VLCPlayerController *)p_data;
        [playerController volumeChanged:volume];
    });
}

static void cb_player_aout_mute_changed(vlc_player_t *p_player, bool muted, void *p_data)
{
    VLC_UNUSED(p_player);
    dispatch_async(dispatch_get_main_queue(), ^{
        VLCPlayerController *playerController = (__bridge VLCPlayerController *)p_data;
        [playerController muteChanged:muted];
    });
}

static const struct vlc_player_aout_cbs player_aout_callbacks = {
    cb_player_aout_volume_changed,
    cb_player_aout_mute_changed,
};

#pragma mark - controller initialization

@implementation VLCPlayerController

- (instancetype)initWithPlayer:(vlc_player_t *)player
{
    self = [super init];
    if (self) {
        _defaultNotificationCenter = [NSNotificationCenter defaultCenter];
        _position = -1.f;
        _time = VLC_TICK_INVALID;
        _p_player = player;
        vlc_player_Lock(_p_player);
        // FIXME: initialize state machine here
        _playerListenerID = vlc_player_AddListener(_p_player,
                               &player_callbacks,
                               (__bridge void *)self);
        vlc_player_Unlock(_p_player);
        _playerAoutListenerID = vlc_player_aout_AddListener(_p_player,
                                                            &player_aout_callbacks,
                                                            (__bridge void *)self);
        _playerVoutListenerID = vlc_player_vout_AddListener(_p_player,
                                                            &player_vout_callbacks,
                                                            (__bridge void *)self);
    }

    return self;
}

- (void)dealloc
{
    if (_p_player) {
        if (_playerListenerID) {
            vlc_player_Lock(_p_player);
            vlc_player_RemoveListener(_p_player, _playerListenerID);
            vlc_player_Unlock(_p_player);
        }
        if (_playerAoutListenerID) {
            vlc_player_aout_RemoveListener(_p_player, _playerAoutListenerID);
        }
        if (_playerVoutListenerID) {
            vlc_player_vout_RemoveListener(_p_player, _playerVoutListenerID);
        }
    }
}

#pragma mark - playback control methods

- (int)start
{
    vlc_player_Lock(_p_player);
    int ret = vlc_player_Start(_p_player);
    vlc_player_Unlock(_p_player);
    return ret;
}

- (void)startInPausedState:(BOOL)startPaused
{
    vlc_player_Lock(_p_player);
    vlc_player_SetStartPaused(_p_player, startPaused);
    vlc_player_Unlock(_p_player);
}

- (void)pause
{
    vlc_player_Lock(_p_player);
    vlc_player_Pause(_p_player);
    vlc_player_Unlock(_p_player);
}

- (void)resume
{
    vlc_player_Lock(_p_player);
    vlc_player_Resume(_p_player);
    vlc_player_Unlock(_p_player);
}

- (void)stop
{
    vlc_player_Lock(_p_player);
    vlc_player_Stop(_p_player);
    vlc_player_Unlock(_p_player);
}

- (void)stopActionChanged:(enum vlc_player_media_stopped_action)stoppedAction
{
    _actionAfterStop = stoppedAction;
}

- (void)setActionAfterStop:(enum vlc_player_media_stopped_action)actionAfterStop
{
    vlc_player_Lock(_p_player);
    vlc_player_SetMediaStoppedAction(_p_player, actionAfterStop);
    vlc_player_Unlock(_p_player);
}

- (void)nextVideoFrame
{
    vlc_player_Lock(_p_player);
    vlc_player_NextVideoFrame(_p_player);
    vlc_player_Unlock(_p_player);
}

#pragma mark - player callback delegations

- (input_item_t *)currentMedia
{
    input_item_t *inputItem;
    vlc_player_Lock(_p_player);
    inputItem = vlc_player_GetCurrentMedia(_p_player);
    vlc_player_Unlock(_p_player);
    return inputItem;
}

- (int)setCurrentMedia:(input_item_t *)currentMedia
{
    if (currentMedia == NULL) {
        return VLC_ENOITEM;
    }
    vlc_player_Lock(_p_player);
    int ret = vlc_player_SetCurrentMedia(_p_player, currentMedia);
    vlc_player_Unlock(_p_player);
    return ret;
}

- (void)currentMediaItemChanged:(input_item_t *)newMediaItem
{
    [_defaultNotificationCenter postNotificationName:VLCPlayerCurrentMediaItemChanged
                                              object:self
                                            userInfo:@{VLCPlayerCurrentMediaItem:[NSValue valueWithPointer:newMediaItem]}];
}

- (void)stateChanged:(enum vlc_player_state)state
{
    /* instead of using vlc_player_GetState, we cache the state and provide it through a synthesized getter
     * as the direct call might not reflect the actual state due the asynchronous API nature */
    _playerState = state;
    [_defaultNotificationCenter postNotificationName:VLCPlayerStateChanged
                                              object:self];
}

- (void)errorChanged:(enum vlc_player_error)error
{
    /* instead of using vlc_player_GetError, we cache the error and provide it through a synthesized getter
     * as the direct call might not reflect the actual error due the asynchronous API nature */
    _error = error;
    [_defaultNotificationCenter postNotificationName:VLCPlayerErrorChanged
                                              object:self];
}

- (void)newBufferingValue:(float)bufferValue
{
    _bufferFill = bufferValue;
    [_defaultNotificationCenter postNotificationName:VLCPlayerBufferChanged
                                              object:self
                                            userInfo:@{VLCPlayerBufferFill : @(bufferValue)}];
}

- (void)newRateValue:(float)rateValue
{
    _playbackRate = rateValue;
    [_defaultNotificationCenter postNotificationName:VLCPlayerRateChanged
                                              object:self];
}

- (void)setPlaybackRate:(float)playbackRate
{
    vlc_player_Lock(_p_player);
    vlc_player_ChangeRate(_p_player, playbackRate);
    vlc_player_Unlock(_p_player);
}

- (void)incrementPlaybackRate
{
    vlc_player_Lock(_p_player);
    vlc_player_IncrementRate(_p_player);
    vlc_player_Unlock(_p_player);
}

- (void)decrementPlaybackRate
{
    vlc_player_Lock(_p_player);
    vlc_player_DecrementRate(_p_player);
    vlc_player_Unlock(_p_player);
}

- (void)capabilitiesChanged:(int)newCapabilities
{
    _seekable = newCapabilities & VLC_INPUT_CAPABILITIES_SEEKABLE;
    _rewindable = newCapabilities & VLC_INPUT_CAPABILITIES_REWINDABLE;
    _pausable = newCapabilities & VLC_INPUT_CAPABILITIES_PAUSEABLE;
    _recordable = newCapabilities & VLC_INPUT_CAPABILITIES_RECORDABLE;
    _rateChangable = newCapabilities & VLC_INPUT_CAPABILITIES_CHANGE_RATE;
    [_defaultNotificationCenter postNotificationName:VLCPlayerCapabilitiesChanged
                                              object:self];
}

- (void)position:(float)position andTimeChanged:(vlc_tick_t)time
{
    _position = position;
    _time = time;
    [_defaultNotificationCenter postNotificationName:VLCPlayerTimeAndPositionChanged
                                              object:self];
}

- (void)setTimeFast:(vlc_tick_t)time
{
    vlc_player_Lock(_p_player);
    vlc_player_SeekByTime(_p_player, time, VLC_PLAYER_SEEK_FAST, VLC_PLAYER_WHENCE_ABSOLUTE);
    vlc_player_Unlock(_p_player);
}

- (void)setTimePrecise:(vlc_tick_t)time
{
    vlc_player_Lock(_p_player);
    vlc_player_SeekByTime(_p_player, time, VLC_PLAYER_SEEK_PRECISE, VLC_PLAYER_WHENCE_ABSOLUTE);
    vlc_player_Unlock(_p_player);
}

- (void)setPositionFast:(float)position
{
    vlc_player_Lock(_p_player);
    vlc_player_SeekByPos(_p_player, position, VLC_PLAYER_SEEK_FAST, VLC_PLAYER_WHENCE_ABSOLUTE);
    vlc_player_Unlock(_p_player);
}

- (void)setPositionPrecise:(float)position
{
    vlc_player_Lock(_p_player);
    vlc_player_SeekByPos(_p_player, position, VLC_PLAYER_SEEK_PRECISE, VLC_PLAYER_WHENCE_ABSOLUTE);
    vlc_player_Unlock(_p_player);
}

- (void)jumpWithValue:(char *)p_userDefinedJumpSize forward:(BOOL)shallJumpForward
{
    int64_t interval = var_InheritInteger(getIntf(), p_userDefinedJumpSize);
    if (interval > 0) {
        vlc_tick_t jumptime = vlc_tick_from_sec( interval );
        if (!shallJumpForward)
            jumptime = jumptime * -1;

        /* No fask seek for jumps. Indeed, jumps can seek to the current position
         * if not precise enough or if the jump value is too small. */
        vlc_player_SeekByTime(_p_player,
                              jumptime,
                              VLC_PLAYER_SEEK_PRECISE,
                              VLC_PLAYER_WHENCE_RELATIVE);
    }
}

- (void)jumpForwardExtraShort
{
    [self jumpWithValue:"extrashort-jump-size" forward:YES];
}

- (void)jumpBackwardExtraShort
{
    [self jumpWithValue:"extrashort-jump-size" forward:NO];
}

- (void)jumpForwardShort
{
    [self jumpWithValue:"short-jump-size" forward:YES];
}

- (void)jumpBackwardShort
{
    [self jumpWithValue:"short-jump-size" forward:NO];
}

- (void)jumpForwardMedium
{
    [self jumpWithValue:"medium-jump-size" forward:YES];
}

- (void)jumpBackwardMedium
{
    [self jumpWithValue:"medium-jump-size" forward:NO];
}

- (void)jumpForwardLong
{
    [self jumpWithValue:"long-jump-size" forward:YES];
}

- (void)jumpBackwardLong
{
    [self jumpWithValue:"long-jump-size" forward:NO];
}

- (void)lengthChanged:(vlc_tick_t)length
{
    _length = length;
    [_defaultNotificationCenter postNotificationName:VLCPlayerLengthChanged
                                              object:self];
}

- (void)teletextAvailibilityChanged:(BOOL)hasTeletextMenu
{
    _teletextMenuAvailable = hasTeletextMenu;
    [_defaultNotificationCenter postNotificationName:VLCPlayerTeletextTransparencyChanged
                                              object:self];
}

- (void)teletextEnabledChanged:(BOOL)teletextOn
{
    _teletextEnabled = teletextOn;
    [_defaultNotificationCenter postNotificationName:VLCPlayerTeletextEnabled
                                              object:self];
}

- (void)setTeletextEnabled:(BOOL)teletextEnabled
{
    vlc_player_Lock(_p_player);
    vlc_player_SetTeletextEnabled(_p_player, teletextEnabled);
    vlc_player_Unlock(_p_player);
}

- (void)teletextPageChanged:(unsigned int)page
{
    _teletextPage = page;
    [_defaultNotificationCenter postNotificationName:VLCPlayerTeletextPageChanged
                                              object:self];
}

- (void)setTeletextPage:(unsigned int)teletextPage
{
    vlc_player_Lock(_p_player);
    vlc_player_SelectTeletextPage(_p_player, teletextPage);
    vlc_player_Unlock(_p_player);
}

- (void)teletextTransparencyChanged:(BOOL)isTransparent
{
    _teletextTransparent = isTransparent;
    [_defaultNotificationCenter postNotificationName:VLCPlayerTeletextTransparencyChanged
                                              object:self];
}

- (void)setTeletextTransparent:(BOOL)teletextTransparent
{
    vlc_player_Lock(_p_player);
    vlc_player_SetTeletextTransparency(_p_player, teletextTransparent);
    vlc_player_Unlock(_p_player);
}

- (void)audioDelayChanged:(vlc_tick_t)audioDelay
{
    _audioDelay = audioDelay;
    [_defaultNotificationCenter postNotificationName:VLCPlayerAudioDelayChanged
                                              object:self];
}

- (void)setAudioDelay:(vlc_tick_t)audioDelay
{
    vlc_player_Lock(_p_player);
    vlc_player_SetAudioDelay(_p_player, audioDelay, VLC_PLAYER_WHENCE_ABSOLUTE);
    vlc_player_Unlock(_p_player);
}

- (void)subtitlesDelayChanged:(vlc_tick_t)subtitlesDelay
{
    _subtitlesDelay = subtitlesDelay;
    [_defaultNotificationCenter postNotificationName:VLCPlayerSubtitlesDelayChanged
                                              object:self];
}

- (void)setSubtitlesDelay:(vlc_tick_t)subtitlesDelay
{
    vlc_player_Lock(_p_player);
    vlc_player_SetSubtitleDelay(_p_player, subtitlesDelay, VLC_PLAYER_WHENCE_ABSOLUTE);
    vlc_player_Unlock(_p_player);
}

- (unsigned int)subtitleTextScalingFactor
{
    unsigned int ret = 100;
    vlc_player_Lock(_p_player);
    ret = vlc_player_GetSubtitleTextScale(_p_player);
    vlc_player_Unlock(_p_player);
    return ret;
}

- (void)setSubtitleTextScalingFactor:(unsigned int)subtitleTextScalingFactor
{
    if (subtitleTextScalingFactor < 10)
        subtitleTextScalingFactor = 10;
    if (subtitleTextScalingFactor > 500)
        subtitleTextScalingFactor = 500;

    vlc_player_Lock(_p_player);
    vlc_player_SetSubtitleTextScale(_p_player, subtitleTextScalingFactor);
    vlc_player_Unlock(_p_player);
}

- (void)recordingChanged:(BOOL)recording
{
    _enableRecording = recording;
    [_defaultNotificationCenter postNotificationName:VLCPlayerRecordingChanged
                                              object:self];
}

- (void)setEnableRecording:(BOOL)enableRecording
{
    vlc_player_Lock(_p_player);
    vlc_player_SetRecordingEnabled(_p_player, enableRecording);
    vlc_player_Unlock(_p_player);
}

- (void)toggleRecord
{
    vlc_player_Lock(_p_player);
    vlc_player_SetRecordingEnabled(_p_player, !_enableRecording);
    vlc_player_Unlock(_p_player);
}

#pragma mark - video specific delegation

- (void)fullscreenChanged:(BOOL)isFullscreen
{
    _fullscreen = isFullscreen;
    [_defaultNotificationCenter postNotificationName:VLCPlayerFullscreenChanged
                                              object:self];
}

- (void)setFullscreen:(BOOL)fullscreen
{
    vlc_player_vout_SetFullscreen(_p_player, fullscreen);
}

- (void)toggleFullscreen
{
    vlc_player_vout_SetFullscreen(_p_player, !_fullscreen);
}

- (void)wallpaperModeChanged:(BOOL)wallpaperModeValue
{
    _wallpaperMode = wallpaperModeValue;
    [_defaultNotificationCenter postNotificationName:VLCPlayerWallpaperModeChanged
                                              object:self];
}

- (void)setWallpaperMode:(BOOL)wallpaperMode
{
    vlc_player_vout_SetWallpaperModeEnabled(_p_player, wallpaperMode);
}

- (void)takeSnapshot
{
    vlc_player_vout_Snapshot(_p_player);
}

#pragma mark - audio specific delegation

- (void)volumeChanged:(float)volume
{
    _volume = volume;
    [_defaultNotificationCenter postNotificationName:VLCPlayerVolumeChanged
                                              object:self];
}

- (void)setVolume:(float)volume
{
    vlc_player_aout_SetVolume(_p_player, volume);
}

- (void)incrementVolume
{
    vlc_player_aout_SetVolume(_p_player, _volume + 0.05);
}

- (void)decrementVolume
{
    vlc_player_aout_SetVolume(_p_player, _volume - 0.05);
}

- (void)muteChanged:(BOOL)mute
{
    _mute = mute;
    [_defaultNotificationCenter postNotificationName:VLCPlayerMuteChanged
                                              object:self];
}

- (void)setMute:(BOOL)mute
{
    vlc_player_aout_Mute(_p_player, mute);
}

- (void)toggleMute
{
    vlc_player_aout_Mute(_p_player, !_mute);
}

@end
