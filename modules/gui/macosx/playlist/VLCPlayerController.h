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

@class VLCInputStats;
@class VLCTrackMetaData;
@class VLCProgramMetaData;
@class VLCInputItem;

extern NSString *VLCPlayerElementaryStreamID;
extern NSString *VLCTick;

/**
 * Listen to VLCPlayerCurrentMediaItemChanged to notified if the current media item changes for the player
 * @note the affected player object will be the object of the notification
 */
extern NSString *VLCPlayerCurrentMediaItemChanged;

/**
 * Listen to VLCPlayerMetadataChangedForCurrentMedia to be notified if metadata such as title, artwork, etc change
 * for the media item currently played
 * @note the affected player object will be the object of the notification
 */
extern NSString *VLCPlayerMetadataChangedForCurrentMedia;

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
 * Listen to VLCPlayerTitleSelectionChanged to be notified if the selected title of the current media changes
 * @note the affected player object will be the object of the notification
 */
extern NSString *VLCPlayerTitleSelectionChanged;

/**
 * Listen to VLCPlayerTitleListChanged to be notified if the list of titles of the current media changes
 * @note the affected player object will be the object of the notification
 */
extern NSString *VLCPlayerTitleListChanged;

/**
 * Listen to VLCPlayerChapterSelectionChanged to be notified if the selected chapter of the current title changes
 * @note the affected player object will be the object of the notification
 */
extern NSString *VLCPlayerChapterSelectionChanged;

/**
 * Listen to VLCPlayerProgramSelectionChanged to be notified if the selected program of the current media changes
 * @note the affected player object will be the object of the notification
 */
extern NSString *VLCPlayerProgramSelectionChanged;

/**
 * Listen to VLCPlayerProgramListChanged to be notified if the list of available programs of the current media changes
 * @note the affected player object will be the object of the notification
 */
extern NSString *VLCPlayerProgramListChanged;

/**
 * Listen to VLCPlayerABLoopStateChanged to be notified if the A→B loop state changes
 * @note the affected player object will be the object of the notification
 */
extern NSString *VLCPlayerABLoopStateChanged;

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
 * Listen to VLCPlayerDelayChangedForSpecificElementaryStream to be notified if the delay of a specific elementary stream changes
 * @note the affected player object will be the object of the notification
 * @return the notification's userInfo dictionary will hold key/value pairs for VLCPlayerElementaryStreamID and VLCTick to describe the changes
 */
extern NSString *VLCPlayerDelayChangedForSpecificElementaryStream;

/**
 * Listen to VLCPlayerSubtitlesFPSChanged to be notified if the subtitles FPS of the current media changes
 * @note the affected player object will be the object of the notification
 */
extern NSString *VLCPlayerSubtitlesFPSChanged;

/**
 * Listen to VLCPlayerRecordingChanged to be notified if the recording state of the current media changes
 * @note the affected player object will be the object of the notification
 */
extern NSString *VLCPlayerRecordingChanged;

/**
 * Listen to VLCPlayerRendererChanged to be notified if the renderer (such as a Chromecast device) changes
 * @note the affected playser object will be the obejct of the notification
 */
extern NSString *VLCPlayerRendererChanged;

extern NSString *VLCPlayerInputStats;
/**
 * Listen to VLCPlayerStatisticsUpdated to be notified if the playback statistics state of the current media update
 * @note the affected player object will be the object of the notification
 * @note the userInfo dictionary will have an instance of VLCInputStats for key VLCPlayerInputStats representating the new state
 */
extern NSString *VLCPlayerStatisticsUpdated;

/**
 * Listen to VLCPlayerTrackListChanged to be notified of the list of audio/video/SPU tracks changes for the current media
 * @note the affected player object will be the object of the notification
 */
extern NSString *VLCPlayerTrackListChanged;

/**
 * Listen to VLCPlayerTrackSelectionChanged to be notified if a selected audio/video/SPU track changes for the current media
 * @note the affected player object will be the object of the notification
 */
extern NSString *VLCPlayerTrackSelectionChanged;

/**
 * Listen to VLCPlayerFullscreenChanged to be notified whether the fullscreen state of the video output changes
 * @note the affected player object will be the object of the notification
 */
extern NSString *VLCPlayerFullscreenChanged;

/**
 * Listen to VLCPlayerListOfVideoOutputThreadsChanged to be notified when a video output thread was added or removed
 * @note the affected player object will be the object of the notification
 */
extern NSString *VLCPlayerListOfVideoOutputThreadsChanged;

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

extern const CGFloat VLCVolumeMaximum;
extern const CGFloat VLCVolumeDefault;

@interface VLCPlayerController : NSObject

- (instancetype)initWithPlayer:(vlc_player_t *)player;

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
 * Convinience method to either start or pause playback
 */
- (void)togglePlayPause;

/**
 * Stop the current playback
 */
- (void)stop;

/**
 * the current status of the A→B loop
 * It will be A if A is set, B if A _and_ B are set or none if there is none
 * @note listen to VLCPlayerABLoopStateChanged for changes to this property
 */
@property (readonly) enum vlc_player_abloop abLoopState;

/**
 * set the A→B loop
 * this function will need to be called twice to set the A and the B point
 * @note VLC core will automatically pick the current time stamp, so there is no parameter to this method
 * @return VLC_SUCCESS or a VLC error code
 */
- (int)setABLoop;

/**
 * disable the A→B loop
 * @return VLC_SUCCESS or a VLC error code
 */
- (int)disableABLoop;

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
@property (readonly, nullable) VLCInputItem * currentMedia;
/**
 * set the current media item
 * @note this is typically done by the associated playlist so you should not need to do it
 * @return VLC_SUCCESS on success, another VLC error on failure
 */
- (int)setCurrentMedia:(VLCInputItem *)currentMedia;

/**
 * returns the duration of the current media in vlc ticks
 */
@property (readonly) vlc_tick_t durationOfCurrentMediaItem;

/**
 * returns the URL of the current media or NULL if there is none
 */
@property (readonly, copy, nullable) NSURL *URLOfCurrentMediaItem;

/**
 * returns the name of the current media or NULL if there is none
 */
@property (readonly, copy, nullable) NSString *nameOfCurrentMediaItem;

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
 * shows the current position as OSD within the video
 * does not do anything if you do not have a vout
 */
- (void)displayPosition;

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
 * set/get the currently selected title
 * @note listen to VLCPlayerTitleSelectionChanged to be notified about changes to this property
 */
@property (readwrite, nonatomic) size_t selectedTitleIndex;

/**
 * convinience method to get the current title
 * @note this may return NULL if there is no title
 */
- (const struct vlc_player_title * _Nullable)selectedTitle;

/**
 * get the number of titles available for the currently playing media item
 * @note listen to VLCPlayerTitleListChanged to be notified about changes to this property
 */
@property (readonly) size_t numberOfTitlesOfCurrentMedia;

/**
 * get a vlc_player_title by the index
 * @note listen to VLCPlayerTitleListChanged in case the list changes so previous indexes will no longer be valid anymore
 */
- (const struct vlc_player_title *)titleAtIndexForCurrentMedia:(size_t)index;

/**
 * the index of the currently selected chapter within the current title
 * @note listen to VLCPlayerChapterSelectionChanged to be notified about changes to this property
 */
@property (readwrite, nonatomic) size_t selectedChapterIndex;

/**
 * select the next chapter of the current title
 */
- (void)selectNextChapter;

/**
 * select the previous chapter of the current title
 */
- (void)selectPreviousChapter;

/**
 * returns the number of chapters for the current title
 */
@property (readonly) size_t numberOfChaptersForCurrentTitle;

/**
 * returns the chapter for the index within the current title
 */
- (nullable const struct vlc_player_chapter *)chapterAtIndexForCurrentTitle:(size_t)index;

/**
 * returns the selected program ID, typically in the range 0 to 32,000
 * @warning the counter does not necessarily start at 0 nor are programs numbered consecutively
 * @note listen to VLCPlayerProgramSelectionChanged to be notified about changes to this property
 */
@property (readonly) int selectedProgramID;

/**
 * select the program defined by the provided VLCProgramMetaData instance
 * @note listen to VLCPlayerProgramSelectionChanged to be notified once the change takes effect
 */
- (void)selectProgram:(VLCProgramMetaData *)program;

/**
 * returns the number of programs available for the current media
 * @note listen to VLCPlayerProgramListChanged to be notified about changes to this property
 */
@property (readonly) size_t numberOfPrograms;

/**
 * returns an instance of VLCProgramMetaData with details about the program at the specified index
 */
- (nullable VLCProgramMetaData *)programAtIndex:(size_t)index;

/**
 * returns an instance of VLCProgramMetaData with details about the program with the specified ID
 */
- (nullable VLCProgramMetaData *)programForID:(int)programID;

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
 * fetch the delay for a specific track identified through its elementary stream ID
 * @return the delay for the track or INT64_MAX if none is set
 */
- (vlc_tick_t)delayForElementaryStreamID:(vlc_es_id_t *)esID;

/**
 * set the delay of a specific track identified through its elementary stream ID
 * @warning Setting the delay of one specific track will override previous and future changes of delay made through generic calls
 * @param delay the delay as a valid time or INT64_MAX to reset to the default for the ES category
 * @param esID the ID for the elementary stream
 * @param relative use an absolute or relative whence to describe the time
 * @return VLC_SUCCESS on success
 */
- (int)setDelay:(vlc_tick_t)delay forElementaryStreamID:(vlc_es_id_t *)esID relativeWhence:(BOOL)relative;

/**
 * the subtitles fps to correct mismatch between video and text
 * the default value shall be 1.0
 * @note listen to VLCPlayerSubtitlesFPSChanged to be notified about changes to this property
 */
@property (readwrite, nonatomic) float subtitlesFPS;

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

/**
 * allows to add associated media to the currently playing one (previously known as input slaves)
 * @param the URL for the media
 * @param the media category (SPU or audio)
 * @param defines whether the added track shall be immediately selected
 * @param defines whether a OSD notification shall be displayed after adding the track
 * @param defines whether the core shall verify the file extension before trying the addition
 * @return VLC_SUCCESS or an error
 */
- (int)addAssociatedMediaToCurrentFromURL:(NSURL *)URL
                               ofCategory:(enum es_format_category_e)category
                         shallSelectTrack:(BOOL)selectTrack
                          shallDisplayOSD:(BOOL)showOSD
                     shallVerifyExtension:(BOOL)verifyExtension;

/**
 * set / get the renderer for the current player
 * @warning the returned vlc_renderer_item_t * must be released with vlc_renderer_item_release().
 * @note listen to VLCPlayerRendererChanged to be notified about changes
 */
@property (readwrite, nonatomic, nullable) vlc_renderer_item_t *rendererItem;

/**
 * navigate in interactive content such as DVD or BR menus
 */
- (void)navigateInInteractiveContent:(enum vlc_player_nav)navigationAction;

/**
 * the latest available playback statistics
 * @return an instance of VLCInputStats holding the data
 * @note listen to VLCPlayerStatisticsUpdated to be notified about changes to this property
 */
@property (readonly) VLCInputStats *statistics;

#pragma mark - track selection

/**
 * select a track
 * @param the track to select
 * @param indicate whether multiple tracks may be played (video and SPU only)
 * @note since tracks are unique, you do not need to specify the type
 * @note listen to VLCTrackSelectionChanged to be notified once the change occured
 */
- (void)selectTrack:(VLCTrackMetaData *)track exclusively:(BOOL)exclusiveSelection;

/**
 * unselect a track
 * @note since tracks are unique, you do not need to specify the type
 * @note listen to VLCTrackSelectionChanged to be notified once the change occured
 */
- (void)unselectTrack:(VLCTrackMetaData *)track;

/**
 * unselect any track of a certain category
 * @param the es_format_category_e category to unselect
 * @note listen to VLCTrackSelectionChanged to be notified once the change occured
 */
- (void)unselectTracksFromCategory:(enum es_format_category_e)category;

/**
 * cycle to the previous track of a certain category
 * @param the category, @see es_format_category_e
 * @note listen to VLCTrackSelectionChanged to be notified once the change occured
 */
- (void)selectPreviousTrackForCategory:(enum es_format_category_e)category;

/**
 * cycle to the next track of a certain category
 * @param the category, @see es_format_category_e
 * @note listen to VLCTrackSelectionChanged to be notified once the change occured
 */
- (void)selectNextTrackForCategory:(enum es_format_category_e)category;

/**
 * an array holding instances of VLCTrackMetaData describing the available audio tracks for the current media
 * @note listen to VLCPlayerTrackListChanged to be notified of changes to the list
 */
@property (readonly, nullable) NSArray<VLCTrackMetaData *>* audioTracks;

/**
 * an array holding instances of VLCTrackMetaData describing the available video tracks for the current media
 * @note listen to VLCPlayerTrackListChanged to be notified of changes to the list
 */
@property (readonly, nullable) NSArray<VLCTrackMetaData *>* videoTracks;

/**
 * an array holding instances of VLCTrackMetaData describing the available subtitle tracks for the current media
 * @note listen to VLCPlayerTrackListChanged to be notified of changes to the list
 */
@property (readonly, nullable) NSArray<VLCTrackMetaData *>* subtitleTracks;

#pragma mark - video output properties

/**
 * the main video output thread
 * @warning the returned vout_thread_t * must be released with vout_Release().
 * @note listen to VLCPlayerListOfVideoOutputThreadsChanged to be notified about changes
 * @return the current video output thread or NULL if there is none
 */
@property (readonly, nullable) vout_thread_t *mainVideoOutputThread;

/**
 * the video output embedded in the current key window
 * @warning the returned vout_thread_t * must be released with vout_Release().
 * @return the current video output thread for the key window or the main video output thread or NULL if there is none
 */
@property (readonly, nullable) vout_thread_t *videoOutputThreadForKeyWindow;

/**
 * an array holding all current video output threads
 * @warning the returned vout_thread_t * instances must be individually released with vout_Release().
 * @note listen to VLCPlayerListOfVideoOutputThreadsChanged to be notified about changes
 */
@property (readonly, nullable, copy) NSArray<NSValue *> *allVideoOutputThreads;

/**
 * indicates whether video is displayed in fullscreen or shall to
 * @note listen to VLCPlayerFullscreenChanged to be notified about changes to this property
 */
@property (readwrite, nonatomic) BOOL fullscreen;

/**
 * indicates whether we currently have an active video playback session
 */
@property (readwrite) BOOL activeVideoPlayback;

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

/**
 * displays a OSD message format string
 */
- (void)displayOSDMessage:(NSString *)message;

/**
 * defines whether the vout windows lock on the video's AR or can be resized arbitrarily
 */
@property (nonatomic, readwrite) BOOL aspectRatioIsLocked;

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

/**
 * the main audio output thread
 * @warning the returned vout_thread_t * must be released with aout_Release().
 * @return the current audio output instance or NULL if there is none
 */
@property (readonly, nullable) audio_output_t *mainAudioOutput;

- (int)enableAudioFilterWithName:(NSString *)name state:(BOOL)state;

@end

@interface VLCInputStats : NSObject

- (instancetype)initWithStatsStructure:(const struct input_stats_t *)stats;

/* Input */
@property (readonly) int64_t inputReadPackets;
@property (readonly) int64_t inputReadBytes;
@property (readonly) float inputBitrate;

/* Demux */
@property (readonly) int64_t demuxReadPackets;
@property (readonly) int64_t demuxReadBytes;
@property (readonly) float demuxBitrate;
@property (readonly) int64_t demuxCorrupted;
@property (readonly) int64_t demuxDiscontinuity;

/* Decoders */
@property (readonly) int64_t decodedAudio;
@property (readonly) int64_t decodedVideo;

/* Vout */
@property (readonly) int64_t displayedPictures;
@property (readonly) int64_t lostPictures;

/* Aout */
@property (readonly) int64_t playedAudioBuffers;
@property (readonly) int64_t lostAudioBuffers;

@end

@interface VLCTrackMetaData : NSObject

- (instancetype)initWithTrackStructure:(const struct vlc_player_track *)track;

@property (readonly) vlc_es_id_t *esID;
@property (readonly) NSString *name;
@property (readonly) BOOL selected;

@end

@interface VLCProgramMetaData : NSObject

- (instancetype)initWithProgramStructure:(const struct vlc_player_program *)structure;

@property (readonly) int group_id;
@property (readonly) NSString *name;
@property (readonly) BOOL selected;
@property (readonly) BOOL scrambled;

@end

NS_ASSUME_NONNULL_END
