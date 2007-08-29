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
#import <VLC/VLCMediaDiscoverer.h>
#import "VLCLibrary.h"

#include <vlc/libvlc.h>


@implementation VLCMediaDiscoverer
- (id)initWithName:(NSString *)aServiceName
{
    if (self = [super init])
    {
        libvlc_exception_t ex;
        libvlc_exception_init( &ex );
        localizedName = nil;
        playlist = nil;
        mdis = libvlc_media_discoverer_new_from_name( [VLCLibrary sharedInstance],
                                                      [aServiceName cString],
                                                      &ex );
        quit_on_exception( &ex );       
    }
    return self;
}

- (void)dealloc
{
    if( localizedName )
        [localizedName release];
    if( playlist )
        [playlist release];
    libvlc_media_discoverer_release( mdis );
    [super dealloc];
}

- (VLCPlaylist *) playlist
{
    if( playlist )
        return playlist;

    libvlc_media_list_t * p_mlist = libvlc_media_discoverer_media_list( mdis );
    VLCPlaylist * ret = [VLCPlaylist playlistWithLibVLCMediaList: p_mlist];
    libvlc_media_list_release( p_mlist );

    /* Hack until this gets done properly upstream */
    char * name = libvlc_media_discoverer_localized_name( mdis );
    if( !name )
    {
        VLCMedia * media = [ret mediaAtIndex: 0];
        ret = media ? (VLCPlaylist *)[media subitems] : nil;
    }
    free(name);

    if( ret )
    {
        playlist = [ret retain];
    }
    return ret;
}

- (NSString *)localizedName
{
    NSString * ret = nil;
    char * name = libvlc_media_discoverer_localized_name( mdis );

    if( localizedName )
        return localizedName;

    if (name)
    {
        ret = [NSString stringWithCString:name encoding:NSUTF8StringEncoding];
        free( name );
    }
    /* XXX: Hack until this gets done properly upstream. This is really slow. */
    if (!ret)
    {
        libvlc_media_list_t * p_mlist = libvlc_media_discoverer_media_list( mdis );
        ret = [[[[VLCPlaylist playlistWithLibVLCMediaList: p_mlist] mediaAtIndex:0] metaInformation] objectForKey: VLCMetaInformationTitle];
        libvlc_media_list_release( p_mlist );
    }
    if( ret )
    {
        localizedName = [ret retain];
    }
    return localizedName;
}
@end
