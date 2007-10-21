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
#include <vlc/vlc.h>

/* Notification Messages */
NSString *VLCMediaPlayerTimeChanged   = @"VLCMediaPlayerTimeChanged";
NSString *VLCMediaPlayerStateChanged  = @"VLCMediaPlayerStateChanged";

/* libvlc event callback */
static void HandleMediaInstanceVolumeChanged(const libvlc_event_t *event, void *self)
{
    [[VLCEventManager sharedManager] callOnMainThreadDelegateOfObject:self
                                                   withDelegateMethod:@selector(mediaPlayerVolumeChanged:)
                                                 withNotificationName:VLCMediaPlayerVolumeChanged];
}

static void HandleMediaTimeChanged(const libvlc_event_t * event, void * self)
{
    [[VLCEventManager sharedManager] callOnMainThreadDelegateOfObject:self
                                                   withDelegateMethod:@selector(mediaPlayerTimeChanged:)
                                                 withNotificationName:VLCMediaPlayerTimeChanged];
        
}

static void HandleMediaInstanceStateChanged(const libvlc_event_t *event, void *self)
{
    [[VLCEventManager sharedManager] callOnMainThreadDelegateOfObject:self
                                                   withDelegateMethod:@selector(mediaPlayerStateChanged:)
                                                 withNotificationName:VLCMediaPlayerStateChanged];
}

NSString *VLCMediaPlayerStateToString(VLCMediaPlayerState state)
{
    static NSString *stateToStrings[] = {
        [VLCMediaPlayerStateStopped]    = @"VLCMediaPlayerStateStopped",
        [VLCMediaPlayerStateOpening]    = @"VLCMediaPlayerStateOpening",
        [VLCMediaPlayerStateBuffering]    = @"VLCMediaPlayerStateBuffering",
        [VLCMediaPlayerStateEnded]        = @"VLCMediaPlayerStateEnded",
        [VLCMediaPlayerStateError]        = @"VLCMediaPlayerStateError",
        [VLCMediaPlayerStatePlaying]    = @"VLCMediaPlayerStatePlaying",
        [VLCMediaPlayerStatePaused]        = @"VLCMediaPlayerStatePaused"
    };
    return stateToStrings[state];
}

// TODO: Documentation
@interface VLCMediaPlayer (Private)
- (void)registerObservers;
- (void)unregisterObservers;
@end

@implementation VLCMediaPlayer
- (id)init
{
    self = [self initWithVideoView:nil];
    return self;
}

- (id)initWithVideoView:(VLCVideoView *)aVideoView
{
    if (self = [super init])
    {
        delegate = nil;
        media = nil;
        
        // Create a media instance, it doesn't matter what library we start off with
        // it will change depending on the media descriptor provided to the media
        // instance
        libvlc_exception_t ex;
        libvlc_exception_init( &ex );
        instance = (void *)libvlc_media_instance_new([VLCLibrary sharedInstance], &ex);
        quit_on_exception( &ex );
        
        [self registerObservers];
        
        [self setVideoView:aVideoView];
    }
    return self;
}

- (void)dealloc
{
    // Always get rid of the delegate first so we can stop sending messages to it
    // TODO: Should we tell the delegate that we're shutting down?
    delegate = nil;

    // Next get rid of the event managers so we can stop trapping events
    [self unregisterObservers];
    libvlc_media_instance_release((libvlc_media_instance_t *)instance);
    
    // Get rid of everything else
    instance = nil;
    videoView = nil;
    [media release];
    
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

- (void)setVideoView:(VLCVideoView *)value
{
    videoView = value;
    
    // Make sure that this instance has been associated with the drawing canvas.
    libvlc_exception_t ex;
    libvlc_exception_init( &ex );
    libvlc_media_instance_set_drawable ((libvlc_media_instance_t *)instance, 
                                        (libvlc_drawable_t)videoView, 
                                        &ex);
    quit_on_exception( &ex );
}

- (VLCVideoView *)videoView
{
    return videoView;
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
    quit_on_exception( &ex );
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
    char *result = libvlc_video_get_aspect_ratio( instance, &ex );
    quit_on_exception( &ex );
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
    quit_on_exception( &ex );
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
    char *result = libvlc_video_get_crop_geometry( instance, &ex );
    quit_on_exception( &ex );
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
    quit_on_exception( &ex );
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
    quit_on_exception( &ex );
    return result;
}

- (NSSize)videoSize
{
    libvlc_exception_t ex;
    libvlc_exception_init( &ex );
    NSSize result = NSMakeSize(libvlc_video_get_height((libvlc_media_instance_t *)instance, &ex),
                               libvlc_video_get_width((libvlc_media_instance_t *)instance, &ex));
    quit_on_exception( &ex );
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
    quit_on_exception( &ex );
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
    quit_on_exception( &ex );
}

- (VLCTime *)time
{
    libvlc_exception_t ex;
    libvlc_exception_init( &ex );
    
    // Results are returned in seconds...duration is returned in milliseconds
    long long time = libvlc_media_instance_get_time( (libvlc_media_instance_t *)instance, &ex ) * 1000;
    if (libvlc_exception_raised( &ex ))
    {
        libvlc_exception_clear( &ex );
        return [VLCTime nullTime];        // Error in obtaining the time, return a null time defintition (--:--:--)
    }
    else
        return [VLCTime timeWithNumber:[NSNumber numberWithLongLong:time]];
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
    quit_on_exception( &ex );
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
    quit_on_exception( &ex );
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
        
        [self willChangeValueForKey:@"media"];
        [media release];
        media = [value retain];
        [self didChangeValueForKey:@"media"];

        libvlc_exception_t ex;
        libvlc_exception_init( &ex );
        libvlc_media_instance_set_media_descriptor( instance, [media libVLCMediaDescriptor], &ex );
        quit_on_exception( &ex );
        
        if (media) {
            if (wasPlaying)
                [self play];
        }
    }
}

- (VLCMedia *)media
{
    return media;
}

- (BOOL)play
{
    // Return if there is no media available or if the stream is already playing something
    if (!media || [self isPlaying])
        return [self isPlaying];
    
    libvlc_exception_t ex;
    libvlc_exception_init( &ex );

    libvlc_media_instance_play( (libvlc_media_instance_t *)instance, &ex );
    quit_on_exception( &ex );

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
    quit_on_exception( &ex );
    
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
    //    quit_on_exception( &ex );
    
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
    // If there is no instance, assume that we're in a stopped state
    if (!instance)
        return VLCMediaPlayerStateStopped;
    
    libvlc_exception_t ex;
    libvlc_exception_init( &ex );
    libvlc_state_t libvlc_state = libvlc_media_instance_get_state( (libvlc_media_instance_t *)instance, &ex );
    if (libvlc_exception_raised( &ex ))
    {
        libvlc_exception_clear( &ex );
        return VLCMediaPlayerStateError;
    }
    else
        return libvlc_to_local_state[libvlc_state];
}
@end

@implementation VLCMediaPlayer (Private)
- (void)registerObservers
{
    libvlc_exception_t ex;
    libvlc_exception_init( &ex );

    // Attach event observers into the media instance
    libvlc_event_manager_t *p_em = libvlc_media_instance_event_manager( instance, &ex );
    libvlc_event_attach( p_em, libvlc_MediaInstancePlayed,          HandleMediaInstanceStateChanged, self, &ex );
    libvlc_event_attach( p_em, libvlc_MediaInstancePaused,          HandleMediaInstanceStateChanged, self, &ex );
    libvlc_event_attach( p_em, libvlc_MediaInstanceReachedEnd,      HandleMediaInstanceStateChanged, self, &ex );
    libvlc_event_attach( p_em, libvlc_MediaInstancePositionChanged, HandleMediaTimeChanged,            self, &ex );
    quit_on_exception( &ex );
}

- (void)unregisterObservers
{
    libvlc_event_manager_t *p_em = libvlc_media_instance_event_manager( instance, NULL );
    libvlc_event_detach( p_em, libvlc_MediaInstancePlayed,          HandleMediaInstanceStateChanged, self, NULL );
    libvlc_event_detach( p_em, libvlc_MediaInstancePaused,          HandleMediaInstanceStateChanged, self, NULL );
    libvlc_event_detach( p_em, libvlc_MediaInstanceReachedEnd,      HandleMediaInstanceStateChanged, self, NULL );
    libvlc_event_detach( p_em, libvlc_MediaInstancePositionChanged, HandleMediaTimeChanged,            self, NULL );
}
@end