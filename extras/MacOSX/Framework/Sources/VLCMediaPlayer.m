/*****************************************************************************
 * VLCMediaPlayer.m: VLC.framework VLCMediaPlayer implementation
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
                                       withArgumentAsObject:[NSNumber numberWithLongLong:event->u.media_instance_time_changed.new_time]];

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
                                       withArgumentAsObject:[NSNumber numberWithFloat:event->u.media_instance_position_changed.new_position]];
    [pool release];
}

static void HandleMediaInstanceStateChanged(const libvlc_event_t * event, void * self)
{
    VLCMediaPlayerState newState;
    
    if( event->type == libvlc_MediaInstancePlayed )
        newState = VLCMediaPlayerStatePlaying;
    else if( event->type == libvlc_MediaInstancePaused )
        newState = VLCMediaPlayerStatePaused;
    else if( event->type == libvlc_MediaInstanceReachedEnd )
        newState = VLCMediaPlayerStateStopped;
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
- (void)setDrawable:(id)aDrawable;

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
    if( !dict )
    {
        dict = [[NSDictionary dictionaryWithObjectsAndKeys:
            [NSSet setWithObject:@"state"], @"playing",
            [NSSet setWithObjects:@"state", @"media", nil], @"seekable",
            nil] retain];
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

    libvlc_media_instance_release((libvlc_media_instance_t *)instance);
    
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

- (void)setFullscreen:(BOOL)value
{
    libvlc_set_fullscreen(instance, value, NULL);
}

- (BOOL)fullscreen
{
    libvlc_exception_t ex;
    libvlc_exception_init( &ex );
    int result = libvlc_get_fullscreen( instance, &ex );
    catch_exception( &ex );
    return result;
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

- (void)setRate:(int)value
{
    libvlc_media_instance_set_rate( instance, value, NULL );
}

- (int)rate
{
    libvlc_exception_t ex;
    libvlc_exception_init( &ex );
    float result = libvlc_media_instance_get_rate( instance, &ex );
    catch_exception( &ex );
    return result;
}

- (NSSize)videoSize
{
    libvlc_exception_t ex;
    libvlc_exception_init( &ex );
    NSSize result = NSMakeSize(libvlc_video_get_height((libvlc_media_instance_t *)instance, &ex),
                               libvlc_video_get_width((libvlc_media_instance_t *)instance, &ex));
    catch_exception( &ex );
    return result;    
}

- (BOOL)hasVideoOut
{
    libvlc_exception_t ex;
    libvlc_exception_init( &ex );
    BOOL result = libvlc_media_instance_has_vout((libvlc_media_instance_t *)instance, &ex);
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
    float result = libvlc_media_instance_get_fps( (libvlc_media_instance_t *)instance, &ex );
    catch_exception( &ex );
    return result;
}

- (void)setTime:(VLCTime *)value
{
    libvlc_exception_t ex;
    libvlc_exception_init( &ex );
    // Time is managed in seconds, while duration is managed in microseconds
    // TODO: Redo VLCTime to provide value numberAsMilliseconds, numberAsMicroseconds, numberAsSeconds, numberAsMinutes, numberAsHours
    libvlc_media_instance_set_time( (libvlc_media_instance_t *)instance, 
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
    libvlc_media_instance_set_chapter( instance, value, NULL );
}

- (int)chapter
{
    libvlc_exception_t ex;
    libvlc_exception_init( &ex );
    int result = libvlc_media_instance_get_chapter( instance, &ex );
    catch_exception( &ex );
    return result;
}

- (int)countOfChapters
{
    libvlc_exception_t ex;
    libvlc_exception_init( &ex );
    int result = libvlc_media_instance_get_chapter_count( instance, &ex );
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
    // We only know how to play media files...not media resources with subitems
    if (media != value && [media subitems] == nil)
    {
        if (media && [media compare:value] == NSOrderedSame)
            return;
        
        BOOL wasPlaying;
        if (wasPlaying = [self isPlaying])
        {
            [self pause];
//            // TODO: Should we wait until it stops playing?
//            while ([self isPlaying])
//                usleep(1000);
        }
        
        [media release];
        media = [value retain];

        libvlc_exception_t ex;
        libvlc_exception_init( &ex );
        libvlc_media_instance_set_media_descriptor( instance, [media libVLCMediaDescriptor], &ex );
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
    libvlc_media_instance_play( (libvlc_media_instance_t *)instance, &ex );
    catch_exception( &ex );
    return YES;
}

- (void)pause
{
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
    libvlc_media_instance_pause( (libvlc_media_instance_t *)instance, &ex );
    catch_exception( &ex );
    
    // TODO: Should we record the time in case the media instance is destroyed
    // then rebuilt?
}

- (void)stop
{
    // Return if there is no media available or if the system is not in play status 
    // or pause status.
    if (!media || (![self isPlaying] && [self state] != VLCMediaPlayerStatePaused))
        return;
    
    // The following is not implemented in the core, should I fix it or just
    // compensate?
    //    libvlc_exception_t ex;
    //    libvlc_exception_init( &ex );
    //    libvlc_media_instance_stop((libvlc_media_instance_t *)instance, &ex);
    //    catch_exception( &ex );
    
    // Pause and reposition to the begining of the stream.
    [self pause];
    [self setTime:0];
    // TODO: Should we pause this or destroy the media instance so that it appears as being "stopped"?
}

//- (void)fastForward;
//- (void)fastForwardAtRate:(int)rate;
//- (void)rewind;
//- (void)rewindAtRate:(int)rate;

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
    BOOL ret = libvlc_media_instance_will_play( (libvlc_media_instance_t *)instance, &ex );
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
    libvlc_media_instance_set_position( instance, newPosition, &ex );
    catch_exception( &ex );
}

- (BOOL)isSeekable
{
    libvlc_exception_t ex;
    libvlc_exception_init( &ex );
    BOOL ret = libvlc_media_instance_is_seekable( instance, &ex );
    catch_exception( &ex );
    return ret;
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
        instance = (void *)libvlc_media_instance_new([VLCLibrary sharedInstance], &ex);
        catch_exception( &ex );
        
        [self registerObservers];
        
        [self setDrawable:aDrawable];
    }
    return self;
}

- (void)setDrawable:(id)aDrawable
{
    // Make sure that this instance has been associated with the drawing canvas.
    libvlc_exception_t ex;
    libvlc_exception_init( &ex );
    libvlc_media_instance_set_drawable ((libvlc_media_instance_t *)instance, 
                                        (libvlc_drawable_t)aDrawable, 
                                        &ex);
    catch_exception( &ex );
}

- (void)registerObservers
{
    libvlc_exception_t ex;
    libvlc_exception_init( &ex );

    // Attach event observers into the media instance
    libvlc_event_manager_t * p_em = libvlc_media_instance_event_manager( instance, &ex );
    libvlc_event_attach( p_em, libvlc_MediaInstancePlayed,          HandleMediaInstanceStateChanged, self, &ex );
    libvlc_event_attach( p_em, libvlc_MediaInstancePaused,          HandleMediaInstanceStateChanged, self, &ex );
    libvlc_event_attach( p_em, libvlc_MediaInstanceReachedEnd,      HandleMediaInstanceStateChanged, self, &ex );
    /* FIXME: We may want to turn that off when none is interested by that */
    libvlc_event_attach( p_em, libvlc_MediaInstancePositionChanged, HandleMediaPositionChanged,      self, &ex );
    libvlc_event_attach( p_em, libvlc_MediaInstanceTimeChanged,     HandleMediaTimeChanged,          self, &ex );
    catch_exception( &ex );
}

- (void)unregisterObservers
{
    libvlc_event_manager_t * p_em = libvlc_media_instance_event_manager( instance, NULL );
    libvlc_event_detach( p_em, libvlc_MediaInstancePlayed,          HandleMediaInstanceStateChanged, self, NULL );
    libvlc_event_detach( p_em, libvlc_MediaInstancePaused,          HandleMediaInstanceStateChanged, self, NULL );
    libvlc_event_detach( p_em, libvlc_MediaInstanceReachedEnd,      HandleMediaInstanceStateChanged, self, NULL );
    //libvlc_event_detach( p_em, libvlc_MediaInstancePositionChanged, HandleMediaTimeChanged,            self, NULL );
    libvlc_event_detach( p_em, libvlc_MediaInstanceTimeChanged,     HandleMediaTimeChanged,            self, NULL );
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
    if( [newPosition floatValue] - position < 0.005 && position - [newPosition floatValue] < 0.005 )
        return; /* Forget that, this is too much precision for our uses */
    [self willChangeValueForKey:@"position"];
    position = ((float)((int)([newPosition floatValue]*1000)))/1000.;
    [self didChangeValueForKey:@"position"];
}

- (void)mediaPlayerStateChanged:(NSNumber *)newState
{
    [self willChangeValueForKey:@"state"];
    cachedState = [newState intValue];
    [self didChangeValueForKey:@"state"];
}
@end
