/*****************************************************************************
 * VLCVideoView.h: VLC.framework VLCVideoView implementation
 *****************************************************************************
 * Copyright (C) 2007 Pierre d'Herbemont
 * Copyright (C) 2007 the VideoLAN team
 * $Id$
 *
 * Authors: Pierre d'Herbemont <pdherbemont # videolan.org>
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

#import <VLC/VLCVideoView.h>
#import "VLCLibrary.h"
#import "VLCEventManager.h"

/* Libvlc */
#include <vlc/vlc.h>
#include <vlc/libvlc.h>

/* Notification */
NSString * VLCVideoDidChangeVolume                  = @"VLCVideoDidChangeVolume";
NSString * VLCVideoDidChangeTime                    = @"VLCVideoDidChangeTime";
NSString * VLCVideoDidChangeCurrentlyPlayingItem    = @"VLCVideoDidChangeCurrentlyPlayingItem";
NSString * VLCVideoDidPause                         = @"VLCVideoDidPause";
NSString * VLCVideoDidPlay                          = @"VLCVideoDidPlay";
NSString * VLCVideoDidStop                          = @"VLCVideoDidStop";

static void HandleMediaListPlayerCurrentPlayingItemChanged( const libvlc_event_t * event, void * self )
{
    [[VLCEventManager sharedManager] callOnMainThreadDelegateOfObject: self
                                     withDelegateMethod: @selector(volumeDidChangeCurrentPlayingItem:)
                                     withNotificationName: VLCVideoDidChangeCurrentlyPlayingItem];
}

static void HandleMediaListPlayerPaused( const libvlc_event_t * event, void * self )
{
    [[VLCEventManager sharedManager] callOnMainThreadDelegateOfObject: self
                                     withDelegateMethod: @selector(videoDidPause:)
                                     withNotificationName: VLCVideoDidPause];
}

static void HandleMediaListPlayerPlayed( const libvlc_event_t * event, void * self )
{
    [[VLCEventManager sharedManager] callOnMainThreadDelegateOfObject: self
                                     withDelegateMethod: @selector(videoDidPlay:)
                                     withNotificationName: VLCVideoDidPlay];
}

static void HandleMediaListPlayerStopped( const libvlc_event_t * event, void * self )
{
    [[VLCEventManager sharedManager] callOnMainThreadDelegateOfObject: self
                                     withDelegateMethod: @selector(videoDidStop:)
                                     withNotificationName: VLCVideoDidStop];
}

@implementation VLCVideoView
- (id)initWithFrame:(NSRect)frameRect
{
    if (self = [super initWithFrame: frameRect])
    {
        libvlc_exception_t p_e;
        libvlc_exception_init( &p_e );
        p_mi = libvlc_media_instance_new( [VLCLibrary sharedInstance], &p_e );
        quit_on_exception( &p_e );
        p_mlp = libvlc_media_list_player_new( [VLCLibrary sharedInstance], &p_e );
        quit_on_exception( &p_e );
        libvlc_media_list_player_set_media_instance( p_mlp, p_mi, &p_e );
        quit_on_exception( &p_e );
        libvlc_media_instance_set_drawable( p_mi, (libvlc_drawable_t)self, &p_e );
        quit_on_exception( &p_e );

        libvlc_event_manager_t * p_em = libvlc_media_list_event_manager( p_mlp, &p_e );
        //libvlc_event_attach( p_em, libvlc_MediaListPlayerCurrentPlayingItemChanged, HandleMediaListPlayerCurrentPlayingItemChanged, self, &p_e );
        //libvlc_event_attach( p_em, libvlc_MediaListPlayerPaused,                    HandleMediaListPlayerPaused,  self, &p_e );
        //libvlc_event_attach( p_em, libvlc_MediaListPlayerPlayed,                    HandleMediaListPlayerPlayed,  self, &p_e );
        //libvlc_event_attach( p_em, libvlc_MediaListPlayerStopped,                   HandleMediaListPlayerStopped, self, &p_e );
        quit_on_exception( &p_e );

        delegate = nil;
        playlist = nil;

        [self setStretchesVideo: NO];
        [self setAutoresizesSubviews: YES];
    }
    return self;
}

- (void)dealloc
{
    libvlc_media_instance_release( p_mi );

    if (delegate)
    {
        [delegate release];
        delegate = nil;
    }
    [super dealloc];
}

- (void)setPlaylist: (VLCPlaylist *)newPlaylist
{
    libvlc_exception_t p_e;
    libvlc_exception_init( &p_e );

    if (playlist)
        [playlist release];

    playlist = [newPlaylist retain];

    libvlc_media_list_player_set_media_list( p_mlp, [playlist libVLCMediaList], &p_e );
    quit_on_exception( &p_e );
}

- (VLCPlaylist *)playlist
{
    return playlist;
}

/* Play */
- (void)play
{
    libvlc_exception_t p_e;
    libvlc_exception_init( &p_e );
    libvlc_media_list_player_play( p_mlp, &p_e );
    quit_on_exception( &p_e );
}

- (void)playItemAtIndex:(int) i
{
    libvlc_exception_t p_e;
    libvlc_exception_init( &p_e );
    libvlc_media_list_player_play_item_at_index( p_mlp, i, &p_e );
    quit_on_exception( &p_e );
}

- (void)playMedia:(VLCMedia *) media
{
    libvlc_exception_t p_e;
    libvlc_exception_init( &p_e );
    libvlc_media_list_player_play_item( p_mlp, [media libVLCMediaDescriptor], &p_e );
    quit_on_exception( &p_e );
}

- (void)pause
{
    libvlc_exception_t p_e;
    libvlc_exception_init( &p_e );
    libvlc_media_list_player_pause( p_mlp, &p_e );
    quit_on_exception( &p_e );
}

- (void)setCurrentTime:(VLCTime *)timeObj
{
    
}

/* State */
- (BOOL)isPlaying
{
    libvlc_exception_t p_e;
    BOOL ret = libvlc_media_list_player_is_playing( p_mlp, &p_e );
    quit_on_exception( &p_e );
    return ret;
}

- (BOOL)isPaused
{
    libvlc_exception_t p_e;
    libvlc_state_t state = libvlc_media_list_player_get_state( p_mlp, &p_e );
    quit_on_exception( &p_e );
    return (state == libvlc_Paused);
}

- (VLCTime *)currentTime
{
    return NULL;
}

- (id)currentPlaylistItem
{
    return NULL;
}

/* Video output property */
- (void)setStretchesVideo:(BOOL)flag
{
    stretchVideo = flag;
}

- (BOOL)stretchesVideo
{
    return stretchVideo;
}

/* Fullscreen */
- (void)enterFullscreen
{

}

- (void)leaveFullscreen
{

}

/* Delegate */
- (void)setDelegate: (id)newDelegate
{
    if (delegate)
        [delegate release];
    delegate = [newDelegate retain];
}

- (id)delegate
{
    return delegate;
}
@end

@implementation VLCVideoView (NSViewSubclass)
- (void)drawRect:(NSRect)aRect
{
    [self lockFocus];
    [[NSColor blackColor] set];
    NSRectFill(aRect);
    [self unlockFocus];
}
- (BOOL)isOpaque
{
    return YES;
}
@end

@interface VLCOpenGLVoutView : NSView
 /* This is part of the hack to avoid a warning in -[VLCVideoView addVoutSubview:] */
- (void)detachFromVout;
@end

@implementation VLCVideoView (VLCOpenGLVoutEmbedding)
/* This is called by the libvlc module 'macosx' as soon as there is one vout
 * available */
- (void)addVoutSubview:(id)aView
{
    if( [[self subviews] count] )
    {
        /* XXX: This is a hack until core gets fixed */
        int i;
        for( i = 0; i < [[self subviews] count]; i++ )
        {
            [[[self subviews] objectAtIndex: i] detachFromVout];
            [[[self subviews] objectAtIndex: i] retain];
            [[[self subviews] objectAtIndex: i] removeFromSuperview];
        }
    }
    [self addSubview: aView];
    [aView setFrame: [self bounds]];
    [aView setAutoresizingMask:NSViewHeightSizable|NSViewWidthSizable];
}
@end
