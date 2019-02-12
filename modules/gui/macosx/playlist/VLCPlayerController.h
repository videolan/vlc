/*****************************************************************************
 * VLCPlayerController.h: MacOS X interface module
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

#import <Foundation/Foundation.h>
#import <vlc_player.h>

NS_ASSUME_NONNULL_BEGIN

extern NSString *VLCPlayerCurrentMediaItem;
/**
 * Listen to VLCPlayerCurrentMediaItemChanged to notified if the current media item changes for the player
 * @note the affected player object will be the object of the notification
 * @note the userInfo dictionary will have the pointer to the new input_item_t for key VLCPlayerCurrentMediaItem
 */
extern NSString *VLCPlayerCurrentMediaItemChanged;

/**
 * Listen to VLCPlayerStateChanged to be notified if the player's state changes
 * @note the affected player object will be the object of the notification
 */
extern NSString *VLCPlayerStateChanged;

/**
 * Listen to VLCPlayerErrorChanged to be notified if the player's error expression changes
 * @note the affected player object will be the object of the notification
 */
extern NSString *VLCPlayerErrorChanged;

extern NSString *VLCPlayerBufferFill;
/**
 * Listen to VLCPlayerBufferChanged to be notified if the player's buffer value changes
 * @note the affected player object will be the object of the notification
 * @note the userInfo dictionary will have the float value indicating the fill state as percentage from 0.0 to 1.0
 * for key VLCPlayerBufferFill */
extern NSString *VLCPlayerBufferChanged;

/**
 * Listen to VLCPlayerRateChanged to be notified if the player's playback rate changes
 * @note the affected player object will be the object of the notification
 */
extern NSString *VLCPlayerRateChanged;

/**
 * Listen to VLCPlayerCapabilitiesChanged to be notified if the player's capabilities change
 * Those are: seekable, rewindable, pausable, recordable, rateChangable
 * @note the affected player object will be the object of the notification
 */
extern NSString *VLCPlayerCapabilitiesChanged;

/**
 * Listen to VLCPlayerTimeAndPositionChanged to be notified if playback position and/or time change
 * @note the affected player object will be the object of the notification
 */
extern NSString *VLCPlayerTimeAndPositionChanged;

/**
 * Listen to VLCPlayerLengthChanged to be notified if the length of the current media changes
 * @note the affected player object will be the object of the notification
 */
extern NSString *VLCPlayerLengthChanged;

/**
 * Listen to VLCPlayerTeletextMenuAvailable to be notified if a teletext menu becomes (un-)available
 * @note the affected player object will be the object of the notification
 */
extern NSString *VLCPlayerTeletextMenuAvailable;

/**
 * Listen to VLCPlayerTeletextEnabled to be notified if teletext becomes enabled or disabled
 * @note the affected player object will be the object of the notification
 */
extern NSString *VLCPlayerTeletextEnabled;

/**
 * Listen to VLCPlayerTeletextPageChanged to be notified if the teletext page changes
 * @note the affected player object will be the object of the notification
 */
extern NSString *VLCPlayerTeletextPageChanged;

/**
 * Listen to VLCPlayerTeletextTransparencyChanged to be notified if the teletext transparency changes
 * @note the affected player object will be the object of the notification
 */
extern NSString *VLCPlayerTeletextTransparencyChanged;

/**
 * Listen to VLCPlayerAudioDelayChanged to be notified if the audio delay of the current media changes
 * @note the affected player object will be the object of the notification
 */
extern NSString *VLCPlayerAudioDelayChanged;

/**
 * Listen to VLCPlayerSubtitlesDelayChanged to be notified if the subtitles delay of the current media changes
 * @note the affected player object will be the object of the notification
 */
extern NSString *VLCPlayerSubtitlesDelayChanged;

/**
 * Listen to VLCPlayerRecordingChanged to be notified if the recording state of the current media changes
 * @note the affected player object will be the object of the notification
 */
extern NSString *VLCPlayerRecordingChanged;

/**
 * Listen to VLCPlayerFullscreenChanged to be notified whether the fullscreen state of the video output changes
 * @note the affected player object will be the object of the notification
 */
extern NSString *VLCPlayerFullscreenChanged;

/**
 * Listen to VLCPlayerWallpaperModeChanged to be notified whether the fullscreen state of the video output changes
 * @note the affected player object will be the object of the notification
 */
extern NSString *VLCPlayerWallpaperModeChanged;

/**
 * Listen to VLCPlayerVolumeChanged to be notified whether the audio output volume changes
 * @note the affected player object will be the object of the notification
 */
extern NSString *VLCPlayerVolumeChanged;

/**
 * Listen to VLCPlayerMuteChanged to be notified whether the audio output is muted or not
 * @note the affected player object will be the object of the notification
 */
extern NSString *VLCPlayerMuteChanged;

@interface VLCPlayerController : NSObject

- (instancetype)initWithPlayer:(vlc_player_t *)player;
- (void)deinitialize;

/**
 * Start playback of the current media
 * @return VLC_SUCCESS on success, otherwise a VLC error
 */
- (int)start;

/**
 * Request to start playback in paused state
 * @param yes if you want to start in paused state, no if you want to cancel your previous request
 */
- (void)startInPausedState:(BOOL)startPaused;

/**
 * Pause the current playback
 */
- (void)pause;

/**
 * Resume the current playback
 */
- (void)resume;

/**
 * Stop the current playback
 */
- (void)stop;

/**
 * Define the action to perform after playback of the current media stopped (for any reason)
 * Options are: continue with next time, pause on last frame, stop even if there is a next item and quit VLC
 * @see the vlc_player_media_stopped_action enum for details
 */
@property (readwrite, nonatomic) enum vlc_player_media_stopped_action actionAfterStop;

/**
 * Move on to the next video frame and pause
 * @warning this relies on a gross hack in the core and will work for 20 consecutive frames maximum only
 */
- (void)nextVideoFrame;

/**
 * get the current media item
 * @return the current media item, NULL if none
 * @note it is the receiver's obligation to release the input item
 */
@property (readonly, nullable) input_item_t * currentMedia;
/**
 * set the current media item
 * @note this is typically done by the associated playlist so you should not need to do it
 * @return VLC_SUCCESS on success, another VLC error on failure
 */
- (int)setCurrentMedia:(input_item_t *)currentMedia;

/**
 * the current player state
 * @return a value according to the vlc_player_state enum
 * @note listen to VLCPlayerStateChanged to be notified about changes to this property
 */
@property (readonly) enum vlc_player_state playerState;

/**
 * the current error value
 * @return a value according to the vlc_player_error enum
 * @note listen to VLCPlayerErrorChanged to be notified about changes to this property
 */
@property (readonly) enum vlc_player_error error;

/**
 * the current buffer state
 * @return a float ranging from 0.0 to 1.0 indicating the buffer fill state as percentage
 * @note listen to VLCPlayerBufferChanged to be notified about changes to this property
 */
@property (readonly) float bufferFill;

/**
 * the current playback rate
 * @return a float larger than 0.0 indicating the playback rate in multiples
 * @note 1.0 defines playback at the default rate, <1.0 will slow things, >1.0 will be faster
 */
@property (readwrite, nonatomic) float playbackRate;

/**
 * helper function to increment the playback rate
 */
- (void)incrementPlaybackRate;
/**
 * helper function to decrement the playback rate
 */
- (void)decrementPlaybackRate;

/**
 * is the currently playing input seekable?
 * @return a BOOL value indicating whether the current input is seekable
 * @note listen to VLCPlayerCapabilitiesChanged to be notified about changes to this property
 */
@property (readonly) BOOL seekable;

/**
 * is the currently playing input rewindable?
 * @return a BOOL value indicating whether the current input is rewindable
 * @note listen to VLCPlayerCapabilitiesChanged to be notified about changes to this property
 */
@property (readonly) BOOL rewindable;

/**
 * is the currently playing input pausable?
 * @return a BOOL value indicating whether the current input can be paused
 * @note listen to VLCPlayerCapabilitiesChanged to be notified about changes to this property
 */
@property (readonly) BOOL pausable;

/**
 * is the currently playing input recordable?
 * @return a BOOL value indicating whether the current input can be recorded
 * @note listen to VLCPlayerCapabilitiesChanged to be notified about changes to this property
 */
@property (readonly) BOOL recordable;

/**
 * is the currently playing input rateChangable?
 * @return a BOOL value indicating whether the playback rate can be changed for the current input
 * @note listen to VLCPlayerCapabilitiesChanged to be notified about changes to this property
 */
@property (readonly) BOOL rateChangable;

/**
 * the time of the currently playing media
 * @return a valid time or VLC_TICK_INVALID (if no media is set, the media
 * doesn't have any time, if playback is not yet started or in case of error)
 * @note A started and playing media doesn't have necessarily a valid time.
 * @note listen to VLCPlayerTimeAndPositionChanged to be notified about changes to this property
 */
@property (readonly) vlc_tick_t time;

/**
 * set the playback position in time for the currently playing media
 * @note this methods prefers speed in seeking over precision
 * @note This method can be called before starting to set a starting position.
 */
- (void)setTimeFast:(vlc_tick_t)time;

/**
 * set the playback position in time for the currently playing media
 * @note this methods prefers precision in seeking over speed
 * @note This method can be called before starting to set a starting position.
 */
- (void)setTimePrecise:(vlc_tick_t)time;

/**
 * the time of the currently playing media
 * @return a valid time or VLC_TICK_INVALID (if no media is set, the media
 * doesn't have any time, if playback is not yet started or in case of error)
 * @note A started and playing media doesn't have necessarily a valid time.
 * @note listen to VLCPlayerTimeAndPositionChanged to be notified about changes to this property
 */
@property (readonly) float position;

/**
 * set the playback position as a percentage (range 0.0 to 1.0) for the currently playing media
 * @note this methods prefers speed in seeking over precision
 * @note This method can be called before starting to set a starting position.
 */
- (void)setPositionFast:(float)position;

/**
 * set the playback position as a percentage (range 0.0 to 1.0) for the currently playing media
 * @note this methods prefers precision in seeking over speed
 * @note This method can be called before starting to set a starting position.
 */
- (void)setPositionPrecise:(float)position;

/**
 * helper function to jump forward with the extra short interval (user configurable in preferences)
 */
- (void)jumpForwardExtraShort;

/**
 * helper function to jump backward with the extra short interval (user configurable in preferences)
 */
- (void)jumpBackwardExtraShort;

/**
 * helper function to jump forward with the short interval (user configurable in preferences)
 */
- (void)jumpForwardShort;

/**
 * helper function to jump backward with the extra short interval (user configurable in preferences)
 */
- (void)jumpBackwardShort;

/**
 * helper function to jump forward with the medium interval (user configurable in preferences)
 */
- (void)jumpForwardMedium;

/**
 * helper function to jump backward with the medium interval (user configurable in preferences)
 */
- (void)jumpBackwardMedium;

/**
 * helper function to jump forward with the long interval (user configurable in preferences)
 */
- (void)jumpForwardLong;

/**
 * helper function to jump forward with the long interval (user configurable in preferences)
 */
- (void)jumpBackwardLong;

/**
 * the length of the currently playing media in ticks
 * @return a valid time or VLC_TICK_INVALID (if no media is set, the media
 * doesn't have any length, if playback is not yet started or in case of error)
 * @note A started and playing media doesn't have necessarily a valid length.
 * @note listen to VLCPlayerLengthChanged to be notified about changes to this property
 */
@property (readonly) vlc_tick_t length;

/**
 * exposes whether a teletext menu is available or not
 * @note listen to VLCPlayerTeletextMenuAvailable to be notified about changes to this property
 */
@property (readonly) BOOL teletextMenuAvailable;

/**
 * enable/disable teletext display
 * @note listen to VLCPlayerTeletextEnabled to be notified about changes to this property
 */
@property (readwrite, nonatomic) BOOL teletextEnabled;

/**
 * set/get the currently displayed (or looked for) teletext page
 *
 * @note Page keys can be the following: @ref VLC_PLAYER_TELETEXT_KEY_RED,
 * @ref VLC_PLAYER_TELETEXT_KEY_GREEN, @ref VLC_PLAYER_TELETEXT_KEY_YELLOW,
 * @ref VLC_PLAYER_TELETEXT_KEY_BLUE or @ref VLC_PLAYER_TELETEXT_KEY_INDEX.
 *
 * @param page a page in the range 0 to 888 or a valid key
 * @note listen to VLCPlayerTeletextPageChanged to be notified about changes to this property
 */
@property (readwrite, nonatomic) unsigned int teletextPage;

/**
 * is the teletext background transparent or not?
 * @return a BOOL value indicating the current state
 * @note listen to VLCPlayerTeletextTransparencyChanged to be notified about changes to this property
 */
@property (readwrite, nonatomic) BOOL teletextTransparent;

/**
 * the audio delay for the current media
 * @warning this property expects you to provide an absolute delay time
 * @return the audio delay in vlc ticks
 * @note listed to VLCPlayerAudioDelayChanged to be notified about changes to this property
 */
@property (readwrite, nonatomic) vlc_tick_t audioDelay;

/**
 * the subtitles delay for the current media
 * @warning this property expects you to provide an absolute delay time
 * @return the subtitles delay in vlc ticks
 * @note listen to VLCPlayerSubtitlesDelayChanged to be notified about changes to this property
 */
@property (readwrite, nonatomic) vlc_tick_t subtitlesDelay;

/**
 * a scale factor for text based subtitles, range 10 - 500, default 100
 * @warning this does not have any effect on bitmapped subtitles
 */
@property (readwrite, nonatomic) unsigned int subtitleTextScalingFactor;

/**
 * enable recording of the current media or check if it is being done
 * @note listen to VLCPlayerRecordingChanged to be notified about changes to this property
 */
@property (readwrite, nonatomic) BOOL enableRecording;

/**
 * helper function to inverse the current recording state
 */
- (void)toggleRecord;

#pragma mark - video output properties

/**
 * indicates whether video is displayed in fullscreen or shall to
 * @note listen to VLCPlayerFullscreenChanged to be notified about changes to this property
 */
@property (readwrite, nonatomic) BOOL fullscreen;

/**
 * helper function to inverse the current fullscreen state
 */
- (void)toggleFullscreen;

/**
 * indicates whether video is displaed in wallpaper mode or shall to
 * @note listen to VLCPlayerWallpaperModeChanged to be notified about changes to this property
 */
@property (readwrite, nonatomic) BOOL wallpaperMode;

/**
 * Take a snapshot of all vouts
 */
- (void)takeSnapshot;

#pragma mark - audio output properties

/**
 * reveals or sets the audio output volume
 * range is 0.0 to 2.0 in percentage, whereas 100% is the unamplified maximum
 * @note listen to VLCPlayerVolumeChanged to be notified about changes to this property
 */
@property (readwrite, nonatomic) float volume;

/**
 * helper function to increase volume by 5%
 */
- (void)incrementVolume;

/**
 * helper function to decrease volume by 5%
 */
- (void)decrementVolume;

/**
 * reveals or sets whether audio output is set to mute or not
 * @note listen to VLCPlayerMuteChanged to be notified about changes to this property
 */
@property (readwrite, nonatomic) BOOL mute;

/**
 * helper function to inverse the current mute state
 */
- (void)toggleMute;

@end

NS_ASSUME_NONNULL_END
