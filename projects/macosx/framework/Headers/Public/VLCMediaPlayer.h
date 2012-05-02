/*****************************************************************************
 * VLCMediaPlayer.h: VLCKit.framework VLCMediaPlayer header
 *****************************************************************************
 * Copyright (C) 2007-2009 Pierre d'Herbemont
 * Copyright (C) 2007-2009 VLC authors and VideoLAN
 * Partial Copyright (C) 2009 Felix Paul Kühne
 * $Id$
 *
 * Authors: Pierre d'Herbemont <pdherbemont # videolan.org>
 *          Felix Paul Kühne <fkuehne # videolan.org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#import <Foundation/Foundation.h>
#if TARGET_OS_IPHONE
# import <CoreGraphics/CoreGraphics.h>
#endif
#import "VLCMedia.h"
#import "VLCTime.h"
#import "VLCAudio.h"

#if !TARGET_OS_IPHONE
@class VLCVideoView;
@class VLCVideoLayer;
#endif

/* Notification Messages */
extern NSString * VLCMediaPlayerTimeChanged;
extern NSString * VLCMediaPlayerStateChanged;

/**
 * VLCMediaPlayerState describes the state of the media player.
 */
typedef enum VLCMediaPlayerState
{
    VLCMediaPlayerStateStopped,        //< Player has stopped
    VLCMediaPlayerStateOpening,        //< Stream is opening
    VLCMediaPlayerStateBuffering,      //< Stream is buffering
    VLCMediaPlayerStateEnded,          //< Stream has ended
    VLCMediaPlayerStateError,          //< Player has generated an error
    VLCMediaPlayerStatePlaying,        //< Stream is playing
    VLCMediaPlayerStatePaused          //< Stream is paused
} VLCMediaPlayerState;

/**
 * Returns the name of the player state as a string.
 * \param state The player state.
 * \return A string containing the name of state. If state is not a valid state, returns nil.
 */
extern NSString * VLCMediaPlayerStateToString(VLCMediaPlayerState state);

/**
 * Formal protocol declaration for playback delegates.  Allows playback messages
 * to be trapped by delegated objects.
 */
@protocol VLCMediaPlayerDelegate
/**
 * Sent by the default notification center whenever the player's time has changed.
 * \details Discussion The value of aNotification is always an VLCMediaPlayerTimeChanged notification. You can retrieve
 * the VLCMediaPlayer object in question by sending object to aNotification.
 */
- (void)mediaPlayerTimeChanged:(NSNotification *)aNotification;

/**
 * Sent by the default notification center whenever the player's state has changed.
 * \details Discussion The value of aNotification is always an VLCMediaPlayerStateChanged notification. You can retrieve
 * the VLCMediaPlayer object in question by sending object to aNotification.
 */
- (void)mediaPlayerStateChanged:(NSNotification *)aNotification;
@end


// TODO: Should we use medialist_player or our own flavor of media player?
@interface VLCMediaPlayer : NSObject
{
    id delegate;                        //< Object delegate
    void * instance;                    //  Internal
    VLCMedia * media;                   //< Current media being played
    VLCTime * cachedTime;               //< Cached time of the media being played
    VLCTime * cachedRemainingTime;      //< Cached remaining time of the media being played
    VLCMediaPlayerState cachedState;    //< Cached state of the media being played
    float position;                     //< The position of the media being played
    id drawable;                        //< The drawable associated to this media player
    VLCAudio *audio;
}

#if !TARGET_OS_IPHONE
/* Initializers */
- (id)initWithVideoView:(VLCVideoView *)aVideoView;
- (id)initWithVideoLayer:(VLCVideoLayer *)aVideoLayer;
#endif

/* Properties */
- (void)setDelegate:(id)value;
- (id)delegate;

/* Video View Options */
// TODO: Should be it's own object?

#if !TARGET_OS_IPHONE
- (void)setVideoView:(VLCVideoView *)aVideoView;
- (void)setVideoLayer:(VLCVideoLayer *)aVideoLayer;
#endif

@property (retain) id drawable; /* The videoView or videoLayer */

- (void)setVideoAspectRatio:(char *)value;
- (char *)videoAspectRatio;

- (void)setVideoCropGeometry:(char *)value;
- (char *)videoCropGeometry;

/**
 * Take a snapshot of the current video.
 *
 * If width AND height is 0, original size is used.
 * If width OR height is 0, original aspect-ratio is preserved.
 *
 * \param path the path where to save the screenshot to
 * \param width the snapshot's width
 * \param height the snapshot's height
 */
- (void)saveVideoSnapshotAt: (NSString *)path withWidth:(NSUInteger)width andHeight:(NSUInteger)height;

/**
 * Enable or disable deinterlace filter
 *
 * \param name of deinterlace filter to use (availability depends on underlying VLC version), NULL to disable.
 */
- (void)setDeinterlaceFilter: (NSString *)name;

@property float rate;

@property (readonly) VLCAudio * audio;

/* Video Information */
- (CGSize)videoSize;
- (BOOL)hasVideoOut;
- (float)framesPerSecond;

/**
 * Sets the current position (or time) of the feed.
 * \param value New time to set the current position to.  If time is [VLCTime nullTime], 0 is assumed.
 */
- (void)setTime:(VLCTime *)value;

/**
 * Returns the current position (or time) of the feed.
 * \return VLCTIme object with current time.
 */
- (VLCTime *)time;

@property (readonly) VLCTime *remainingTime;
@property (readonly) NSUInteger fps;

/**
 * Return the current video subtitle index
 * \return 0 if none is set.
 *
 * Pass 0 to disable.
 */
@property (readwrite) NSUInteger currentVideoSubTitleIndex;

/**
 * Return the video subtitle tracks
 *
 * It includes the disabled fake track at index 0.
 */
- (NSArray *)videoSubTitles;

/**
 * Load and set a specific video subtitle, from a file.
 * \param path to a file
 * \return if the call succeed..
 */
- (BOOL)openVideoSubTitlesFromFile:(NSString *)path;

/**
 * Chapter selection and enumeration, it is bound
 * to a title option.
 */

/**
 * Return the current video subtitle index, or
 * \return NSNotFound if none is set.
 *
 * To disable subtitle pass NSNotFound.
 */
@property (readwrite) NSUInteger currentChapterIndex;
- (void)previousChapter;
- (void)nextChapter;
- (NSArray *)chaptersForTitleIndex:(NSUInteger)titleIndex;

/**
 * Title selection and enumeration
 * \return NSNotFound if none is set.
 */
@property (readwrite) NSUInteger currentTitleIndex;
- (NSArray *)titles;

/* Audio Options */

/**
 * Return the current audio track index
 * \return 0 if none is set.
 *
 * Pass 0 to disable.
 */
@property (readwrite) NSUInteger currentAudioTrackIndex;

/**
 * Return the audio tracks
 *
 * It includes the "Disable" fake track at index 0.
 */
- (NSArray *)audioTracks;

- (void)setAudioChannel:(NSInteger)value;
- (NSInteger)audioChannel;

/* Media Options */
- (void)setMedia:(VLCMedia *)value;
- (VLCMedia *)media;

/* Playback Operations */
/**
 * Plays a media resource using the currently selected media controller (or
 * default controller.  If feed was paused then the feed resumes at the position
 * it was paused in.
 * \return A Boolean determining whether the stream was played or not.
 */
- (BOOL)play;

/**
 * Toggle's the pause state of the feed.
 */
- (void)pause;

/**
 * Stop the playing.
 */
- (void)stop;

/**
 * Advance one frame.
 */
- (void)gotoNextFrame;

/**
 * Fast forwards through the feed at the standard 1x rate.
 */
- (void)fastForward;

/**
 * Fast forwards through the feed at the rate specified.
 * \param rate Rate at which the feed should be fast forwarded.
 */
- (void)fastForwardAtRate:(float)rate;

/**
 * Rewinds through the feed at the standard 1x rate.
 */
- (void)rewind;

/**
 * Rewinds through the feed at the rate specified.
 * \param rate Rate at which the feed should be fast rewound.
 */
- (void)rewindAtRate:(float)rate;

/**
 * Jumps shortly backward in current stream if seeking is supported.
 * \param interval to skip, in sec.
 */
- (void)jumpBackward:(NSInteger)interval;

/**
 * Jumps shortly forward in current stream if seeking is supported.
 * \param interval to skip, in sec.
 */
- (void)jumpForward:(NSInteger)interval;

/**
 * Jumps shortly backward in current stream if seeking is supported.
 */
- (void)extraShortJumpBackward;

/**
 * Jumps shortly forward in current stream if seeking is supported.
 */
- (void)extraShortJumpForward;

/**
 * Jumps shortly backward in current stream if seeking is supported.
 */
- (void)shortJumpBackward;

/**
 * Jumps shortly forward in current stream if seeking is supported.
 */
- (void)shortJumpForward;

/**
 * Jumps shortly backward in current stream if seeking is supported.
 */
- (void)mediumJumpBackward;

/**
 * Jumps shortly forward in current stream if seeking is supported.
 */
- (void)mediumJumpForward;

/**
 * Jumps shortly backward in current stream if seeking is supported.
 */
- (void)longJumpBackward;

/**
 * Jumps shortly forward in current stream if seeking is supported.
 */
- (void)longJumpForward;

/* Playback Information */
/**
 * Playback state flag identifying that the stream is currently playing.
 * \return TRUE if the feed is playing, FALSE if otherwise.
 */
- (BOOL)isPlaying;

/**
 * Playback state flag identifying wheather the stream will play.
 * \return TRUE if the feed is ready for playback, FALSE if otherwise.
 */
- (BOOL)willPlay;

/**
 * Playback's current state.
 * \see VLCMediaState
 */
- (VLCMediaPlayerState)state;

/**
 * Returns the receiver's position in the reading.
 * \return A number between 0 and 1. indicating the position
 */
- (float)position;
- (void)setPosition:(float)newPosition;

- (BOOL)isSeekable;

- (BOOL)canPause;

@end
