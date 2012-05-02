/*****************************************************************************
 * VLCMediaPlayer.m: VLCKit.framework VLCMediaPlayer implementation
 *****************************************************************************
 * Copyright (C) 2007-2009 Pierre d'Herbemont
 * Copyright (C) 2007-2009 VLC authors and VideoLAN
 * Partial Copyright (C) 2009 Felix Paul Kühne
 * $Id$
 *
 * Authors: Pierre d'Herbemont <pdherbemont # videolan.org>
 *          Faustion Osuna <enrique.osuna # gmail.com>
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

#import "VLCLibrary.h"
#import "VLCMediaPlayer.h"
#import "VLCEventManager.h"
#import "VLCLibVLCBridging.h"
#if !TARGET_OS_IPHONE
# import "VLCVideoView.h"
#endif
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#if !TARGET_OS_IPHONE
/* prevent system sleep */
# import <CoreServices/CoreServices.h>
/* FIXME: Ugly hack! */
# ifdef __x86_64__
#  import <CoreServices/../Frameworks/OSServices.framework/Headers/Power.h>
# endif
#endif

#include <vlc/vlc.h>

/* Notification Messages */
NSString * VLCMediaPlayerTimeChanged    = @"VLCMediaPlayerTimeChanged";
NSString * VLCMediaPlayerStateChanged   = @"VLCMediaPlayerStateChanged";

NSString * VLCMediaPlayerStateToString(VLCMediaPlayerState state)
{
    static NSString * stateToStrings[] = {
        [VLCMediaPlayerStateStopped]      = @"VLCMediaPlayerStateStopped",
        [VLCMediaPlayerStateOpening]      = @"VLCMediaPlayerStateOpening",
        [VLCMediaPlayerStateBuffering]    = @"VLCMediaPlayerStateBuffering",
        [VLCMediaPlayerStateEnded]        = @"VLCMediaPlayerStateEnded",
        [VLCMediaPlayerStateError]        = @"VLCMediaPlayerStateError",
        [VLCMediaPlayerStatePlaying]      = @"VLCMediaPlayerStatePlaying",
        [VLCMediaPlayerStatePaused]       = @"VLCMediaPlayerStatePaused"
    };
    return stateToStrings[state];
}

/* libvlc event callback */
static void HandleMediaInstanceVolumeChanged(const libvlc_event_t * event, void * self)
{
    [[VLCEventManager sharedManager] callOnMainThreadDelegateOfObject:self
                                                   withDelegateMethod:@selector(mediaPlayerVolumeChanged:)
                                                 withNotificationName:VLCMediaPlayerVolumeChanged];
}

static void HandleMediaTimeChanged(const libvlc_event_t * event, void * self)
{
    NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];
    [[VLCEventManager sharedManager] callOnMainThreadObject:self
                                                 withMethod:@selector(mediaPlayerTimeChanged:)
                                       withArgumentAsObject:[NSNumber numberWithLongLong:event->u.media_player_time_changed.new_time]];

    [[VLCEventManager sharedManager] callOnMainThreadDelegateOfObject:self
                                                   withDelegateMethod:@selector(mediaPlayerTimeChanged:)
                                                 withNotificationName:VLCMediaPlayerTimeChanged];
    [pool drain];
}

static void HandleMediaPositionChanged(const libvlc_event_t * event, void * self)
{
    NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];

    [[VLCEventManager sharedManager] callOnMainThreadObject:self
                                                 withMethod:@selector(mediaPlayerPositionChanged:)
                                       withArgumentAsObject:[NSNumber numberWithFloat:event->u.media_player_position_changed.new_position]];
    [pool drain];
}

static void HandleMediaInstanceStateChanged(const libvlc_event_t * event, void * self)
{
    VLCMediaPlayerState newState;

    if( event->type == libvlc_MediaPlayerPlaying )
        newState = VLCMediaPlayerStatePlaying;
    else if( event->type == libvlc_MediaPlayerPaused )
        newState = VLCMediaPlayerStatePaused;
    else if( event->type == libvlc_MediaPlayerEndReached )
        newState = VLCMediaPlayerStateStopped;
    else if( event->type == libvlc_MediaPlayerEncounteredError )
        newState = VLCMediaPlayerStateError;
    else if( event->type == libvlc_MediaPlayerBuffering )
        newState = VLCMediaPlayerStateBuffering;
    else if( event->type == libvlc_MediaPlayerOpening )
        newState = VLCMediaPlayerStateOpening;
    else
    {
        NSLog(@"%s: Unknown event", __FUNCTION__);
        return;
    }

    NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];

    [[VLCEventManager sharedManager] callOnMainThreadObject:self
                                                 withMethod:@selector(mediaPlayerStateChanged:)
                                       withArgumentAsObject:[NSNumber numberWithInt:newState]];

    [[VLCEventManager sharedManager] callOnMainThreadDelegateOfObject:self
                                                   withDelegateMethod:@selector(mediaPlayerStateChanged:)
                                                 withNotificationName:VLCMediaPlayerStateChanged];

    [pool drain];

}

static void HandleMediaPlayerMediaChanged(const libvlc_event_t * event, void * self)
{
    NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];

    [[VLCEventManager sharedManager] callOnMainThreadObject:self
                                                 withMethod:@selector(mediaPlayerMediaChanged:)
                                       withArgumentAsObject:[VLCMedia mediaWithLibVLCMediaDescriptor:event->u.media_player_media_changed.new_media]];

    [pool drain];

}


// TODO: Documentation
@interface VLCMediaPlayer (Private)
- (id)initWithDrawable:(id)aDrawable;

- (void)registerObservers;
- (void)unregisterObservers;
- (void)mediaPlayerTimeChanged:(NSNumber *)newTime;
- (void)mediaPlayerPositionChanged:(NSNumber *)newTime;
- (void)mediaPlayerStateChanged:(NSNumber *)newState;
- (void)mediaPlayerMediaChanged:(VLCMedia *)media;
@end

@implementation VLCMediaPlayer

/* Bindings */
+ (NSSet *)keyPathsForValuesAffectingValueForKey:(NSString *)key
{
    static NSDictionary * dict = nil;
    NSSet * superKeyPaths;
    if( !dict )
    {
        dict = [[NSDictionary dictionaryWithObjectsAndKeys:
            [NSSet setWithObject:@"state"], @"playing",
            [NSSet setWithObjects:@"state", @"media", nil], @"seekable",
            [NSSet setWithObjects:@"state", @"media", nil], @"canPause",
            [NSSet setWithObjects:@"state", @"media", nil], @"description",
            nil] retain];
    }
    if( (superKeyPaths = [super keyPathsForValuesAffectingValueForKey: key]) )
    {
        NSMutableSet * ret = [NSMutableSet setWithSet:[dict objectForKey: key]];
        [ret unionSet:superKeyPaths];
        return ret;
    }
    return [dict objectForKey: key];
}

/* Contructor */
- (id)init
{
    return [self initWithDrawable:nil];
}

#if !TARGET_OS_IPHONE
- (id)initWithVideoView:(VLCVideoView *)aVideoView
{
    return [self initWithDrawable: aVideoView];
}

- (id)initWithVideoLayer:(VLCVideoLayer *)aVideoLayer
{
    return [self initWithDrawable: aVideoLayer];
}
#endif

- (void)dealloc
{
    NSAssert(libvlc_media_player_get_state(instance) == libvlc_Stopped, @"You released the media player before ensuring that it is stopped");

    [self unregisterObservers];
    [[VLCEventManager sharedManager] cancelCallToObject:self];

    // Always get rid of the delegate first so we can stop sending messages to it
    // TODO: Should we tell the delegate that we're shutting down?
    delegate = nil;

    // Clear our drawable as we are going to release it, we don't
    // want the core to use it from this point. This won't happen as
    // the media player must be stopped.
    libvlc_media_player_set_nsobject(instance, nil);

    libvlc_media_player_release(instance);

    // Get rid of everything else
    [media release];
    [cachedTime release];
    [cachedRemainingTime release];
    [drawable release];
    [audio release];

    [super dealloc];
}

- (void)setDelegate:(id)value
{
    delegate = value;
}

- (id)delegate
{
    return delegate;
}

#if !TARGET_OS_IPHONE
- (void)setVideoView:(VLCVideoView *)aVideoView
{
    [self setDrawable: aVideoView];
}

- (void)setVideoLayer:(VLCVideoLayer *)aVideoLayer
{
    [self setDrawable: aVideoLayer];
}
#endif

- (void)setDrawable:(id)aDrawable
{
    // Make sure that this instance has been associated with the drawing canvas.
    libvlc_media_player_set_nsobject(instance, aDrawable);
}

- (id)drawable
{
    return libvlc_media_player_get_nsobject(instance);
}

- (VLCAudio *)audio
{
    if (!audio)
        audio = [[VLCAudio alloc] initWithMediaPlayer:self];
    return audio;
}

#pragma mark -
#pragma mark Subtitles

- (void)setCurrentVideoSubTitleIndex:(NSUInteger)index
{
    libvlc_video_set_spu(instance, (int)index);
}

- (NSUInteger)currentVideoSubTitleIndex
{
    NSInteger count = libvlc_video_get_spu_count(instance);

    if (count <= 0)
        return NSNotFound;

    return libvlc_video_get_spu(instance);
}

- (BOOL)openVideoSubTitlesFromFile:(NSString *)path
{
    return libvlc_video_set_subtitle_file(instance, [path UTF8String]);
}

- (NSArray *)videoSubTitles
{
    libvlc_track_description_t *currentTrack = libvlc_video_get_spu_description(instance);

    NSMutableArray *tempArray = [NSMutableArray array];
    while (currentTrack) {
        [tempArray addObject:[NSString stringWithUTF8String:currentTrack->psz_name]];
        currentTrack = currentTrack->p_next;
    }
    libvlc_track_description_release(currentTrack);
    return [NSArray arrayWithArray: tempArray];
}


#pragma mark -
#pragma mark Video Crop geometry

- (void)setVideoCropGeometry:(char *)value
{
    libvlc_video_set_crop_geometry(instance, value);
}

- (char *)videoCropGeometry
{
    char * result = libvlc_video_get_crop_geometry(instance);
    return result;
}

- (void)setVideoAspectRatio:(char *)value
{
    libvlc_video_set_aspect_ratio( instance, value );
}

- (char *)videoAspectRatio
{
    char * result = libvlc_video_get_aspect_ratio( instance );
    return result;
}

- (void)saveVideoSnapshotAt:(NSString *)path withWidth:(NSUInteger)width andHeight:(NSUInteger)height
{
    int failure = libvlc_video_take_snapshot(instance, 0, [path UTF8String], width, height);
    if (failure)
        [[NSException exceptionWithName:@"Can't take a video snapshot" reason:@"No video output" userInfo:nil] raise];
}

- (void)setDeinterlaceFilter:(NSString *)name
{
    libvlc_video_set_deinterlace(instance, [name UTF8String]);
}

- (void)setRate:(float)value
{
    libvlc_media_player_set_rate(instance, value);
}

- (float)rate
{
    return libvlc_media_player_get_rate(instance);
}

- (CGSize)videoSize
{
    unsigned height = 0, width = 0;
    int failure = libvlc_video_get_size(instance, 0, &width, &height);
    if (failure)
        [[NSException exceptionWithName:@"Can't get video size" reason:@"No video output" userInfo:nil] raise];
    return CGSizeMake(width, height);
}

- (BOOL)hasVideoOut
{
    return libvlc_media_player_has_vout(instance);
}

- (float)framesPerSecond
{
    return libvlc_media_player_get_fps(instance);
}

- (void)setTime:(VLCTime *)value
{
    // Time is managed in seconds, while duration is managed in microseconds
    // TODO: Redo VLCTime to provide value numberAsMilliseconds, numberAsMicroseconds, numberAsSeconds, numberAsMinutes, numberAsHours
    libvlc_media_player_set_time(instance, value ? [[value numberValue] longLongValue] : 0);
}

- (VLCTime *)time
{
    return cachedTime;
}

- (VLCTime *)remainingTime
{
    return cachedRemainingTime;
}

- (NSUInteger)fps
{
    return libvlc_media_player_get_fps(instance);
}

#pragma mark -
#pragma mark Chapters
- (void)setCurrentChapterIndex:(NSUInteger)value;
{
    libvlc_media_player_set_chapter(instance, value);
}

- (NSUInteger)currentChapterIndex
{
    NSInteger count = libvlc_media_player_get_chapter_count(instance);
    if (count <= 0)
        return NSNotFound;
    NSUInteger result = libvlc_media_player_get_chapter(instance);
    return result;
}

- (void)nextChapter
{
    libvlc_media_player_next_chapter(instance);
}

- (void)previousChapter
{
    libvlc_media_player_previous_chapter(instance);
}

- (NSArray *)chaptersForTitleIndex:(NSUInteger)title
{
    NSInteger count = libvlc_media_player_get_chapter_count(instance);
    if (count <= 0)
        return [NSArray array];

    libvlc_track_description_t *tracks = libvlc_video_get_chapter_description(instance, title);
    NSMutableArray *tempArray = [NSMutableArray array];
    NSInteger i;
    for (i = 0; i < count ; i++)
    {
        [tempArray addObject:[NSString stringWithUTF8String:tracks->psz_name]];
        tracks = tracks->p_next;
    }
    libvlc_track_description_release(tracks);
    return [NSArray arrayWithArray:tempArray];
}

#pragma mark -
#pragma mark Titles

- (void)setCurrentTitleIndex:(NSUInteger)value
{
    libvlc_media_player_set_title(instance, value);
}

- (NSUInteger)currentTitleIndex
{
    NSInteger count = libvlc_media_player_get_title_count(instance);
    if (count <= 0)
        return NSNotFound;

    return libvlc_media_player_get_title(instance);
}

- (NSUInteger)countOfTitles
{
    NSUInteger result = libvlc_media_player_get_title_count(instance);
    return result;
}

- (NSArray *)titles
{
    libvlc_track_description_t *tracks = libvlc_video_get_title_description(instance);
    NSMutableArray *tempArray = [NSMutableArray array];
    NSInteger i;
    for (i = 0; i < [self countOfTitles] ; i++)
    {
        [tempArray addObject:[NSString stringWithUTF8String: tracks->psz_name]];
        tracks = tracks->p_next;
    }
    libvlc_track_description_release(tracks);
    return [NSArray arrayWithArray: tempArray];
}

#pragma mark -
#pragma mark Audio tracks
- (void)setCurrentAudioTrackIndex:(NSUInteger)value
{
    libvlc_audio_set_track( instance, (int)value);
}

- (NSUInteger)currentAudioTrackIndex
{
    NSInteger count = libvlc_audio_get_track_count(instance);
    if (count <= 0)
        return NSNotFound;

    NSUInteger result = libvlc_audio_get_track(instance);
    return result;
}

- (NSArray *)audioTracks
{
    NSInteger count = libvlc_audio_get_track_count(instance);
    if (count <= 0)
        return [NSArray array];

    libvlc_track_description_t *tracks = libvlc_audio_get_track_description(instance);
    NSMutableArray *tempArray = [NSMutableArray array];
    NSUInteger i;
    for (i = 0; i < count ; i++)
    {
        [tempArray addObject:[NSString stringWithUTF8String: tracks->psz_name]];
        tracks = tracks->p_next;
    }
    libvlc_track_description_release(tracks);

    return [NSArray arrayWithArray: tempArray];
}

- (void)setAudioChannel:(NSInteger)value
{
    libvlc_audio_set_channel(instance, value);
}

- (NSInteger)audioChannel
{
    return libvlc_audio_get_channel(instance);
}

- (void)setMedia:(VLCMedia *)value
{
    if (media != value)
    {
        if (media && [media compare:value] == NSOrderedSame)
            return;

        [media release];
        media = [value retain];

        libvlc_media_player_set_media(instance, [media libVLCMediaDescriptor]);
    }
}

- (VLCMedia *)media
{
    return media;
}

- (BOOL)play
{
    libvlc_media_player_play(instance);
    return YES;
}

- (void)pause
{
    if( [NSThread isMainThread] )
    {
        /* Hack because we create a dead lock here, when the vout is stopped
         * and tries to recontact us on the main thread */
        /* FIXME: to do this properly we need to do some locking. We may want
         * to move that to libvlc */
        [self performSelectorInBackground:@selector(pause) withObject:nil];
        return;
    }

    // Pause the stream
    libvlc_media_player_pause(instance);
}

- (void)stop
{
    libvlc_media_player_stop(instance);
}

- (void)gotoNextFrame
{
    libvlc_media_player_next_frame(instance);

}

- (void)fastForward
{
    [self fastForwardAtRate: 2.0];
}

- (void)fastForwardAtRate:(float)rate
{
    [self setRate:rate];
}

- (void)rewind
{
    [self rewindAtRate: 2.0];
}

- (void)rewindAtRate:(float)rate
{
    [self setRate: -rate];
}

- (void)jumpBackward:(NSInteger)interval
{
    if( [self isSeekable] )
    {
        interval = interval * 1000;
        [self setTime: [VLCTime timeWithInt: ([[self time] intValue] - interval)]];
    }
}

- (void)jumpForward:(NSInteger)interval
{
    if( [self isSeekable] )
    {
        interval = interval * 1000;
        [self setTime: [VLCTime timeWithInt: ([[self time] intValue] + interval)]];
    }
}

- (void)extraShortJumpBackward
{
    [self jumpBackward:3];
}

- (void)extraShortJumpForward
{
    [self jumpForward:3];
}

- (void)shortJumpBackward
{
    [self jumpBackward:10];
}

- (void)shortJumpForward
{
    [self jumpForward:10];
}

- (void)mediumJumpBackward
{
    [self jumpBackward:60];
}

- (void)mediumJumpForward
{
    [self jumpForward:60];
}

- (void)longJumpBackward
{
    [self jumpBackward:300];
}

- (void)longJumpForward
{
    [self jumpForward:300];
}

+ (NSSet *)keyPathsForValuesAffectingIsPlaying
{
    return [NSSet setWithObjects:@"state", nil];
}

- (BOOL)isPlaying
{
    VLCMediaPlayerState state = [self state];
    return ((state == VLCMediaPlayerStateOpening) || (state == VLCMediaPlayerStateBuffering) ||
            (state == VLCMediaPlayerStatePlaying));
}

- (BOOL)willPlay
{
    return libvlc_media_player_will_play(instance);
}

static const VLCMediaPlayerState libvlc_to_local_state[] =
{
    [libvlc_Stopped]    = VLCMediaPlayerStateStopped,
    [libvlc_Opening]    = VLCMediaPlayerStateOpening,
    [libvlc_Buffering]  = VLCMediaPlayerStateBuffering,
    [libvlc_Playing]    = VLCMediaPlayerStatePlaying,
    [libvlc_Paused]     = VLCMediaPlayerStatePaused,
    [libvlc_Ended]      = VLCMediaPlayerStateEnded,
    [libvlc_Error]      = VLCMediaPlayerStateError
};

- (VLCMediaPlayerState)state
{
    return cachedState;
}

- (float)position
{
    return position;
}

- (void)setPosition:(float)newPosition
{
    libvlc_media_player_set_position(instance, newPosition);
}

- (BOOL)isSeekable
{
    return libvlc_media_player_is_seekable(instance);
}

- (BOOL)canPause
{
    return libvlc_media_player_can_pause(instance);
}

- (void *)libVLCMediaPlayer
{
    return instance;
}
@end

@implementation VLCMediaPlayer (Private)
- (id)initWithDrawable:(id)aDrawable
{
    if (self = [super init])
    {
        delegate = nil;
        media = nil;
        cachedTime = [[VLCTime nullTime] retain];
        cachedRemainingTime = [[VLCTime nullTime] retain];
        position = 0.0f;
        cachedState = VLCMediaPlayerStateStopped;

        // Create a media instance, it doesn't matter what library we start off with
        // it will change depending on the media descriptor provided to the media
        // instance
        instance = libvlc_media_player_new([VLCLibrary sharedInstance]);

        [self registerObservers];

        [self setDrawable:aDrawable];
    }
    return self;
}

- (void)registerObservers
{
    // Attach event observers into the media instance
    libvlc_event_manager_t * p_em = libvlc_media_player_event_manager(instance);
    libvlc_event_attach(p_em, libvlc_MediaPlayerPlaying,          HandleMediaInstanceStateChanged, self);
    libvlc_event_attach(p_em, libvlc_MediaPlayerPaused,           HandleMediaInstanceStateChanged, self);
    libvlc_event_attach(p_em, libvlc_MediaPlayerEncounteredError, HandleMediaInstanceStateChanged, self);
    libvlc_event_attach(p_em, libvlc_MediaPlayerEndReached,       HandleMediaInstanceStateChanged, self);
    libvlc_event_attach(p_em, libvlc_MediaPlayerOpening,          HandleMediaInstanceStateChanged, self);
    libvlc_event_attach(p_em, libvlc_MediaPlayerBuffering,        HandleMediaInstanceStateChanged, self);

    /* FIXME: We may want to turn that off when none is interested by that */
    libvlc_event_attach(p_em, libvlc_MediaPlayerPositionChanged,  HandleMediaPositionChanged,      self);
    libvlc_event_attach(p_em, libvlc_MediaPlayerTimeChanged,      HandleMediaTimeChanged,          self);
    libvlc_event_attach(p_em, libvlc_MediaPlayerMediaChanged,     HandleMediaPlayerMediaChanged,   self);
}

- (void)unregisterObservers
{
    libvlc_event_manager_t * p_em = libvlc_media_player_event_manager(instance);
    libvlc_event_detach(p_em, libvlc_MediaPlayerPlaying,          HandleMediaInstanceStateChanged, self);
    libvlc_event_detach(p_em, libvlc_MediaPlayerPaused,           HandleMediaInstanceStateChanged, self);
    libvlc_event_detach(p_em, libvlc_MediaPlayerEncounteredError, HandleMediaInstanceStateChanged, self);
    libvlc_event_detach(p_em, libvlc_MediaPlayerEndReached,       HandleMediaInstanceStateChanged, self);
    libvlc_event_detach(p_em, libvlc_MediaPlayerOpening,          HandleMediaInstanceStateChanged, self);
    libvlc_event_detach(p_em, libvlc_MediaPlayerBuffering,        HandleMediaInstanceStateChanged, self);

    libvlc_event_detach(p_em, libvlc_MediaPlayerPositionChanged,  HandleMediaPositionChanged,      self);
    libvlc_event_detach(p_em, libvlc_MediaPlayerTimeChanged,      HandleMediaTimeChanged,          self);
    libvlc_event_detach(p_em, libvlc_MediaPlayerMediaChanged,     HandleMediaPlayerMediaChanged,   self);
}

- (void)mediaPlayerTimeChanged:(NSNumber *)newTime
{
    [self willChangeValueForKey:@"time"];
    [self willChangeValueForKey:@"remainingTime"];
    [cachedTime release];
    cachedTime = [[VLCTime timeWithNumber:newTime] retain];
    [cachedRemainingTime release];
    double currentTime = [[cachedTime numberValue] doubleValue];
    double remaining = currentTime / position * (1 - position);
    cachedRemainingTime = [[VLCTime timeWithNumber:[NSNumber numberWithDouble:-remaining]] retain];
    [self didChangeValueForKey:@"remainingTime"];
    [self didChangeValueForKey:@"time"];
}

#if !TARGET_OS_IPHONE
- (void)delaySleep
{
    UpdateSystemActivity(UsrActivity);
}
#endif

- (void)mediaPlayerPositionChanged:(NSNumber *)newPosition
{
#if !TARGET_OS_IPHONE
    // This seems to be the most relevant place to delay sleeping and screen saver.
    [self delaySleep];
#endif

    [self willChangeValueForKey:@"position"];
    position = [newPosition floatValue];
    [self didChangeValueForKey:@"position"];
}

- (void)mediaPlayerStateChanged:(NSNumber *)newState
{
    [self willChangeValueForKey:@"state"];
    cachedState = [newState intValue];
    [self didChangeValueForKey:@"state"];
}

- (void)mediaPlayerMediaChanged:(VLCMedia *)newMedia
{
    [self willChangeValueForKey:@"media"];
    if (media != newMedia)
    {
        [media release];
        media = [newMedia retain];
    }
    [self didChangeValueForKey:@"media"];
}

@end
