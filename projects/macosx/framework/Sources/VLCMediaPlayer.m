/*****************************************************************************
 * VLCMediaPlayer.m: VLCKit.framework VLCMediaPlayer implementation
 *****************************************************************************
 * Copyright (C) 2007 Pierre d'Herbemont
 * Copyright (C) 2007 the VideoLAN team
 * $Id$
 *
 * Authors: Pierre d'Herbemont <pdherbemont # videolan.org>
 *          Faustion Osuna <enrique.osuna # gmail.com>
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

#import "VLCLibrary.h"
#import "VLCMediaPlayer.h"
#import "VLCEventManager.h"
#import "VLCLibVLCBridging.h"
#import "VLCVideoView.h"
#ifdef HAVE_CONFIG_H
# include "config.h"
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
    [pool release];
}

static void HandleMediaPositionChanged(const libvlc_event_t * event, void * self)
{
    NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];

    [[VLCEventManager sharedManager] callOnMainThreadObject:self 
                                                 withMethod:@selector(mediaPlayerPositionChanged:) 
                                       withArgumentAsObject:[NSNumber numberWithFloat:event->u.media_player_position_changed.new_position]];
    [pool release];
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

    [pool release];

}


// TODO: Documentation
@interface VLCMediaPlayer (Private)
- (id)initWithDrawable:(id)aDrawable;

- (void)registerObservers;
- (void)unregisterObservers;
- (void)mediaPlayerTimeChanged:(NSNumber *)newTime;
- (void)mediaPlayerPositionChanged:(NSNumber *)newTime;
- (void)mediaPlayerStateChanged:(NSNumber *)newState;
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

- (id)initWithVideoView:(VLCVideoView *)aVideoView
{
    return [self initWithDrawable: aVideoView];
}

- (id)initWithVideoLayer:(VLCVideoLayer *)aVideoLayer
{
    return [self initWithDrawable: aVideoLayer];
}

- (void)release
{
    @synchronized(self)
    {
        if([self retainCount] <= 1)
        {
            /* We must make sure we won't receive new event after an upcoming dealloc
             * We also may receive a -retain in some event callback that may occcur
             * Before libvlc_event_detach. So this can't happen in dealloc */
            [self unregisterObservers];
        }
        [super release];
    }
}

- (void)dealloc
{
    // Always get rid of the delegate first so we can stop sending messages to it
    // TODO: Should we tell the delegate that we're shutting down?
    delegate = nil;

    libvlc_media_player_release((libvlc_media_player_t *)instance);
    
    // Get rid of everything else
    [media release];
    [cachedTime release];

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

- (void)setVideoView:(VLCVideoView *)aVideoView
{    
    [self setDrawable: aVideoView];
}

- (void)setVideoLayer:(VLCVideoLayer *)aVideoLayer
{
    [self setDrawable: aVideoLayer];
}

- (void)setDrawable:(id)aDrawable
{
    // Make sure that this instance has been associated with the drawing canvas.
    libvlc_exception_t ex;
    libvlc_exception_init( &ex );
    libvlc_media_player_set_nsobject(instance, aDrawable, &ex);
    catch_exception( &ex );
}

- (id)drawable
{
    libvlc_exception_t ex;
    libvlc_exception_init( &ex );
    id ret = libvlc_media_player_get_nsobject(instance);
    catch_exception( &ex );
    return ret;
}

- (VLCAudio *)audio
{
    return [[VLCLibrary sharedLibrary] audio];
}

- (void)setVideoAspectRatio:(char *)value
{
    libvlc_video_set_aspect_ratio( instance, value, NULL );
}

- (char *)videoAspectRatio
{
    libvlc_exception_t ex;
    libvlc_exception_init( &ex );
    char * result = libvlc_video_get_aspect_ratio( instance, &ex );
    catch_exception( &ex );
    return result;
}

- (void)setVideoSubTitles:(int)value
{
    libvlc_video_set_spu( instance, value, NULL );
}

- (int)videoSubTitles
{
    libvlc_exception_t ex;
    libvlc_exception_init( &ex );
    int result = libvlc_video_get_spu( instance, &ex );
    catch_exception( &ex );
    return result;
}

- (void)setVideoCropGeometry:(char *)value
{
    libvlc_video_set_crop_geometry( instance, value, NULL );
}

- (char *)videoCropGeometry
{
    libvlc_exception_t ex;
    libvlc_exception_init( &ex );
    char * result = libvlc_video_get_crop_geometry( instance, &ex );
    catch_exception( &ex );
    return result;
}

- (void)setVideoTeleText:(int)value
{
    libvlc_video_set_teletext( instance, value, NULL );
}

- (int)videoTeleText
{
    libvlc_exception_t ex;
    libvlc_exception_init( &ex );
    int result = libvlc_video_get_teletext( instance, &ex );
    catch_exception( &ex );
    return result;
}

- (void)setRate:(float)value
{
    libvlc_media_player_set_rate( instance, value, NULL );
}

- (float)rate
{
    libvlc_exception_t ex;
    libvlc_exception_init( &ex );
    float result = libvlc_media_player_get_rate( instance, &ex );
    catch_exception( &ex );
    return result;
}

- (NSSize)videoSize
{
    libvlc_exception_t ex;
    libvlc_exception_init( &ex );
    NSSize result = NSMakeSize(libvlc_video_get_height((libvlc_media_player_t *)instance, &ex),
                               libvlc_video_get_width((libvlc_media_player_t *)instance, &ex));
    catch_exception( &ex );
    return result;    
}

- (BOOL)hasVideoOut
{
    libvlc_exception_t ex;
    libvlc_exception_init( &ex );
    BOOL result = libvlc_media_player_has_vout((libvlc_media_player_t *)instance, &ex);
    if (libvlc_exception_raised( &ex ))
    {
        libvlc_exception_clear( &ex );
        return NO;
    }
    else
        return result;
}

- (float)framesPerSecond
{
    libvlc_exception_t ex;
    libvlc_exception_init( &ex );
    float result = libvlc_media_player_get_fps( (libvlc_media_player_t *)instance, &ex );
    catch_exception( &ex );
    return result;
}

- (void)setTime:(VLCTime *)value
{
    libvlc_exception_t ex;
    libvlc_exception_init( &ex );
    // Time is managed in seconds, while duration is managed in microseconds
    // TODO: Redo VLCTime to provide value numberAsMilliseconds, numberAsMicroseconds, numberAsSeconds, numberAsMinutes, numberAsHours
    libvlc_media_player_set_time( (libvlc_media_player_t *)instance, 
                                    (value ? [[value numberValue] longLongValue] / 1000 : 0),
                                    &ex );
    catch_exception( &ex );
}

- (VLCTime *)time
{
    return cachedTime;
}

- (void)setChapter:(int)value;
{
    libvlc_media_player_set_chapter( instance, value, NULL );
}

- (int)chapter
{
    libvlc_exception_t ex;
    libvlc_exception_init( &ex );
    int result = libvlc_media_player_get_chapter( instance, &ex );
    catch_exception( &ex );
    return result;
}

- (int)countOfChapters
{
    libvlc_exception_t ex;
    libvlc_exception_init( &ex );
    int result = libvlc_media_player_get_chapter_count( instance, &ex );
    catch_exception( &ex );
    return result;
}

- (void)setAudioTrack:(int)value
{
    libvlc_audio_set_track( instance, value, NULL );
}

- (int)audioTrack
{
    libvlc_exception_t ex;
    libvlc_exception_init( &ex );
    int result = libvlc_audio_get_track( instance, &ex );
    catch_exception( &ex );
    return result;
}

- (int)countOfAudioTracks
{
    libvlc_exception_t ex;
    libvlc_exception_init( &ex );
    int result = libvlc_audio_get_track_count( instance, &ex );
    catch_exception( &ex );
    return result;
}

- (void)setAudioChannel:(int)value
{
    libvlc_audio_set_channel( instance, value, NULL );
}

- (int)audioChannel
{
    libvlc_exception_t ex;
    libvlc_exception_init( &ex );
    int result = libvlc_audio_get_channel( instance, &ex );
    catch_exception( &ex );
    return result;
}

- (void)setMedia:(VLCMedia *)value
{
    if (media != value)
    {
        if (media && [media compare:value] == NSOrderedSame)
            return;
        
        [media release];
        media = [value retain];

        libvlc_exception_t ex;
        libvlc_exception_init( &ex );
        libvlc_media_player_set_media( instance, [media libVLCMediaDescriptor], &ex );
        catch_exception( &ex );
    }
}

- (VLCMedia *)media
{
    return media;
}

- (BOOL)play
{    
    libvlc_exception_t ex;
    libvlc_exception_init( &ex );
    libvlc_media_player_play( (libvlc_media_player_t *)instance, &ex );
    catch_exception( &ex );
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

    // Return if there is no media available or if the stream is not paused or 
    // playing something else
    if (!media || (![self isPlaying] && [self state] != VLCMediaPlayerStatePaused))
        return;

    // Should never get here.
    if (!instance)
        return;


    // Pause the stream
    libvlc_exception_t ex;
    libvlc_exception_init( &ex );
    libvlc_media_player_pause( (libvlc_media_player_t *)instance, &ex );
    catch_exception( &ex );
    
    // TODO: Should we record the time in case the media instance is destroyed
    // then rebuilt?
}

- (void)stop
{
    if( 0 && [NSThread isMainThread] )
    {
        /* Hack because we create a dead lock here, when the vout is stopped
         * and tries to recontact us on the main thread */
        /* FIXME: to do this properly we need to do some locking. We may want 
         * to move that to libvlc */
        [self performSelectorInBackground:@selector(stop) withObject:nil];
        return;
    }

    // Return if there is no media available or if the system is not in play status 
    // or pause status.
    if (!media)
        return;
    
    libvlc_exception_t ex;
    libvlc_exception_init( &ex );
    libvlc_media_player_stop((libvlc_media_player_t *)instance, &ex);
    catch_exception( &ex );
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
    libvlc_exception_t ex;
    libvlc_exception_init( &ex );
    BOOL ret = libvlc_media_player_will_play( (libvlc_media_player_t *)instance, &ex );
    if (libvlc_exception_raised(&ex))
    {
        libvlc_exception_clear(&ex);
        return NO;
    }
    else
        return ret;
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
    libvlc_exception_t ex;
    libvlc_exception_init( &ex );
    libvlc_media_player_set_position( instance, newPosition, &ex );
    catch_exception( &ex );
}

- (BOOL)isSeekable
{
    libvlc_exception_t ex;
    libvlc_exception_init( &ex );
    BOOL ret = libvlc_media_player_is_seekable( instance, &ex );
    catch_exception( &ex );
    return ret;
}

- (BOOL)canPause
{
    libvlc_exception_t ex;
    libvlc_exception_init( &ex );
    BOOL ret = libvlc_media_player_can_pause( instance, &ex );
    catch_exception( &ex );
    return ret;
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
        position = 0.0f;
        cachedState = VLCMediaPlayerStateStopped;

        // Create a media instance, it doesn't matter what library we start off with
        // it will change depending on the media descriptor provided to the media
        // instance
        libvlc_exception_t ex;
        libvlc_exception_init( &ex );
        instance = (void *)libvlc_media_player_new([VLCLibrary sharedInstance], &ex);
        catch_exception( &ex );
        
        [self registerObservers];
        
        [self setDrawable:aDrawable];
    }
    return self;
}

- (void)registerObservers
{
    libvlc_exception_t ex;
    libvlc_exception_init( &ex );

    // Attach event observers into the media instance
    libvlc_event_manager_t * p_em = libvlc_media_player_event_manager( instance, &ex );
    libvlc_event_attach( p_em, libvlc_MediaPlayerPlaying,          HandleMediaInstanceStateChanged, self, &ex );
    libvlc_event_attach( p_em, libvlc_MediaPlayerPaused,           HandleMediaInstanceStateChanged, self, &ex );
    libvlc_event_attach( p_em, libvlc_MediaPlayerEncounteredError, HandleMediaInstanceStateChanged, self, &ex );
    libvlc_event_attach( p_em, libvlc_MediaPlayerEndReached,       HandleMediaInstanceStateChanged, self, &ex );
    /* FIXME: We may want to turn that off when none is interested by that */
    libvlc_event_attach( p_em, libvlc_MediaPlayerPositionChanged, HandleMediaPositionChanged,      self, &ex );
    libvlc_event_attach( p_em, libvlc_MediaPlayerTimeChanged,     HandleMediaTimeChanged,          self, &ex );
    catch_exception( &ex );
}

- (void)unregisterObservers
{
    libvlc_event_manager_t * p_em = libvlc_media_player_event_manager( instance, NULL );
    libvlc_event_detach( p_em, libvlc_MediaPlayerPlaying,          HandleMediaInstanceStateChanged, self, NULL );
    libvlc_event_detach( p_em, libvlc_MediaPlayerPaused,           HandleMediaInstanceStateChanged, self, NULL );
    libvlc_event_detach( p_em, libvlc_MediaPlayerEncounteredError, HandleMediaInstanceStateChanged, self, NULL );
    libvlc_event_detach( p_em, libvlc_MediaPlayerEndReached,       HandleMediaInstanceStateChanged, self, NULL );
    libvlc_event_detach( p_em, libvlc_MediaPlayerPositionChanged,  HandleMediaPositionChanged,      self, NULL );
    libvlc_event_detach( p_em, libvlc_MediaPlayerTimeChanged,      HandleMediaTimeChanged,          self, NULL );
}

- (void)mediaPlayerTimeChanged:(NSNumber *)newTime
{
    [self willChangeValueForKey:@"time"];
    [cachedTime release];
    cachedTime = [[VLCTime timeWithNumber:newTime] retain];

    [self didChangeValueForKey:@"time"];
}

- (void)mediaPlayerPositionChanged:(NSNumber *)newPosition
{
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
@end
