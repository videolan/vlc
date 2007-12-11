/*****************************************************************************
 * VLCMediaDiscoverer.m: VLC.framework VLCMediaDiscoverer implementation
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

#import <Cocoa/Cocoa.h>
#import "VLCMediaDiscoverer.h"
#import "VLCLibrary.h"
#import "VLCLibVLCBridging.h"

#include <vlc/libvlc.h>

static NSArray * availableMediaDiscoverer = nil;

@implementation VLCMediaDiscoverer
+ (NSArray *)availableMediaDiscoverer
{
        if( !availableMediaDiscoverer )
        {
            availableMediaDiscoverer = [[NSArray arrayWithObjects:
                                    [[[VLCMediaDiscoverer alloc] initWithName:@"sap"] autorelease],
                                    [[[VLCMediaDiscoverer alloc] initWithName:@"shoutcast"] autorelease],
                                    [[[VLCMediaDiscoverer alloc] initWithName:@"shoutcasttv"] autorelease], nil] retain];
        }
        return availableMediaDiscoverer;
}

- (id)initWithName:(NSString *)aServiceName
{
    if (self = [super init])
    {
        libvlc_exception_t ex;
        libvlc_exception_init( &ex );
        localizedName = nil;
        discoveredMedia = nil;
        mdis = libvlc_media_discoverer_new_from_name( [VLCLibrary sharedInstance],
                                                      [aServiceName UTF8String],
                                                      &ex );
        quit_on_exception( &ex );       
    }
    return self;
}

- (void)dealloc
{
    if( localizedName )
        [localizedName release];
    if( discoveredMedia )
        [discoveredMedia release];
    libvlc_media_discoverer_release( mdis );
    [super dealloc];
}

- (VLCMediaList *) discoveredMedia
{
    if( discoveredMedia )
        return discoveredMedia;

    libvlc_media_list_t * p_mlist = libvlc_media_discoverer_media_list( mdis );
    VLCMediaList * ret = [VLCMediaList mediaListWithLibVLCMediaList: p_mlist];
    libvlc_media_list_release( p_mlist );

    if( ret )
    {
        discoveredMedia = [ret retain];
    }
    return discoveredMedia;
}

- (NSString *)localizedName
{
    NSString * aString = nil;
    char * name = libvlc_media_discoverer_localized_name( mdis );

    if( localizedName )
        return localizedName;

    if (name)
    {
        aString = [NSString stringWithUTF8String:name];
        free( name );
    }
    if( aString )
    {
        localizedName = [aString retain];
    }
    return localizedName;
}
@end
