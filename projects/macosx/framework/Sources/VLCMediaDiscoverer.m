/*****************************************************************************
 * VLCMediaDiscoverer.m: VLCKit.framework VLCMediaDiscoverer implementation
 *****************************************************************************
 * Copyright (C) 2007 Pierre d'Herbemont
 * Copyright (C) 2007 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Pierre d'Herbemont <pdherbemont # videolan.org>
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

#import "VLCMediaDiscoverer.h"
#import "VLCLibrary.h"
#import "VLCLibVLCBridging.h"
#import "VLCEventManager.h"

#include <vlc/libvlc.h>

static NSMutableArray * availableMediaDiscoverer = nil;     // Global list of media discoverers

/**
 * Declares call back functions to be used with libvlc event callbacks.
 */
@interface VLCMediaDiscoverer (Private)
/**
 * TODO: Documention
 */
- (void)mediaDiscovererStarted;

/**
 * TODO: Documention
 */
- (void)mediaDiscovererEnded;
@end

/* libvlc event callback */
static void HandleMediaDiscovererStarted(const libvlc_event_t * event, void * user_data)
{
    NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];
    id self = user_data;
    [[VLCEventManager sharedManager] callOnMainThreadObject:self
                                                 withMethod:@selector(mediaDiscovererStarted)
                                       withArgumentAsObject:nil];
    [pool drain];
}

static void HandleMediaDiscovererEnded( const libvlc_event_t * event, void * user_data)
{
    NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];
    id self = user_data;
    [[VLCEventManager sharedManager] callOnMainThreadObject:self
                                                 withMethod:@selector(mediaDiscovererEnded)
                                       withArgumentAsObject:nil];
    [pool drain];
}


@implementation VLCMediaDiscoverer
+ (NSArray *)availableMediaDiscoverer
{
    if( !availableMediaDiscoverer )
    {
        availableMediaDiscoverer = [[NSArray arrayWithObjects:
                                [[[VLCMediaDiscoverer alloc] initWithName:@"sap"] autorelease],
                                [[[VLCMediaDiscoverer alloc] initWithName:@"upnp"] autorelease],
                                [[[VLCMediaDiscoverer alloc] initWithName:@"freebox"] autorelease],
                                [[[VLCMediaDiscoverer alloc] initWithName:@"video_dir"] autorelease], nil] retain];
    }
    return availableMediaDiscoverer;
}

- (id)initWithName:(NSString *)aServiceName
{
    if (self = [super init])
    {
        localizedName = nil;
        discoveredMedia = nil;
        mdis = libvlc_media_discoverer_new_from_name([VLCLibrary sharedInstance],
                                                     [aServiceName UTF8String]);
        NSAssert(mdis, @"No such media discoverer");
        libvlc_event_manager_t * p_em = libvlc_media_discoverer_event_manager(mdis);
        libvlc_event_attach(p_em, libvlc_MediaDiscovererStarted, HandleMediaDiscovererStarted, self);
        libvlc_event_attach(p_em, libvlc_MediaDiscovererEnded,   HandleMediaDiscovererEnded,   self);

        running = libvlc_media_discoverer_is_running(mdis);
    }
    return self;
}

- (void)dealloc
{
    libvlc_event_manager_t *em = libvlc_media_list_event_manager(mdis);
    libvlc_event_detach(em, libvlc_MediaDiscovererStarted, HandleMediaDiscovererStarted, self);
    libvlc_event_detach(em, libvlc_MediaDiscovererEnded,   HandleMediaDiscovererEnded,   self);
    [[VLCEventManager sharedManager] cancelCallToObject:self];

    [localizedName release];
    [discoveredMedia release];
    libvlc_media_discoverer_release( mdis );
    [super dealloc];
}

- (VLCMediaList *) discoveredMedia
{
    if( discoveredMedia )
        return discoveredMedia;

    libvlc_media_list_t * p_mlist = libvlc_media_discoverer_media_list( mdis );
    VLCMediaList * ret = [VLCMediaList mediaListWithLibVLCMediaList:p_mlist];
    libvlc_media_list_release( p_mlist );

    discoveredMedia = [ret retain];
    return discoveredMedia;
}

- (NSString *)localizedName
{
    if ( localizedName )
        return localizedName;

    char * name = libvlc_media_discoverer_localized_name( mdis );
    if (name)
    {
        localizedName = [[NSString stringWithUTF8String:name] retain];
        free( name );
    }
    return localizedName;
}

- (BOOL)isRunning
{
    return running;
}
@end

@implementation VLCMediaDiscoverer (Private)
- (void)mediaDiscovererStarted
{
    [self willChangeValueForKey:@"running"];
    running = YES;
    [self didChangeValueForKey:@"running"];
}

- (void)mediaDiscovererEnded
{
    [self willChangeValueForKey:@"running"];
    running = NO;
    [self didChangeValueForKey:@"running"];
}
@end
