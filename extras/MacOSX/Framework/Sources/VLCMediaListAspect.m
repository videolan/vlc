/*****************************************************************************
 * VLCMediaList.m: VLC.framework VLCMediaList implementation
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

#import "VLCMediaListAspect.h"
#import "VLCLibrary.h"
#import "VLCEventManager.h"
#import "VLCLibVLCBridging.h"
#include <vlc/vlc.h>
#include <vlc/libvlc.h>

// TODO: Documentation
@interface VLCMediaListAspect (Private)
/* Initializers */
- (void)initInternalMediaListView;
@end

@implementation VLCMediaListAspect (KeyValueCodingCompliance)
/* For the @"Media" key */
- (int) countOfMedia
{
    return [self count];
}

- (id) objectInMediaAtIndex:(int)i
{
    return [self mediaAtIndex:i];
}
@end

@implementation VLCMediaListAspect
- (void)dealloc
{
    // Release allocated memory
    libvlc_media_list_release(p_mlv);
    
    [super dealloc];
}
- (VLCMedia *)mediaAtIndex:(int)index
{
    libvlc_exception_t p_e;
    libvlc_exception_init( &p_e );
    libvlc_media_descriptor_t *p_md = libvlc_media_list_view_item_at_index( p_mlv, index, &p_e );
    quit_on_exception( &p_e );
    
    // Returns local object for media descriptor, searchs for user data first.  If not found it creates a 
    // new cocoa object representation of the media descriptor.
    return [VLCMedia mediaWithLibVLCMediaDescriptor:p_md];
}

- (int)count
{
    libvlc_exception_t p_e;
    libvlc_exception_init( &p_e );
    int result = libvlc_media_list_view_count( p_mlv, &p_e );
    quit_on_exception( &p_e );

    return result;
}
@end

@implementation VLCMediaListAspect (LibVLCBridging)
+ (id)mediaListAspectWithLibVLCMediaListView:(void *)p_new_mlv;
{
    return [[[VLCMediaList alloc] initWithLibVLCMediaList:p_new_mlv] autorelease];
}

- (id)initWithLibVLCMediaListView:(void *)p_new_mlv;
{
    if( self = [super init] )
    {
        p_mlv = p_new_mlv;
        libvlc_media_list_view_retain(p_mlv);
        [self initInternalMediaListView];
    }
    return self;
}

- (void *)libVLCMediaListView
{
    return p_mlv;
}
@end

@implementation VLCMediaListAspect (Private)
- (void)initInternalMediaListView
{
    /* Add internal callback */
}
@end

