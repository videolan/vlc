/*****************************************************************************
 * VLCMedia.m: VLC.framework VLCMedia implementation
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
#import <VLC/VLCMedia.h>
#import <VLC/VLCPlaylist.h>
#import "VLCEventManager.h"
#import "VLCLibrary.h"

#include <vlc/libvlc.h>

NSString * VLCMetaInformationTitle = @"title";
NSString * VLCMetaInformationAuthor = @"author";
NSString * VLCMetaInformationArtwork = @"artwork";

/* Our notification */
NSString * VLCMediaSubItemAdded = @"VLCMediaSubItemAdded";
NSString * VLCMediaMetaChanged = @"VLCMediaMetaChanged";

/* libvlc event callback */
static void HandleMediaSubItemAdded( const libvlc_event_t * event, void * self)
{
    [[VLCEventManager sharedManager] callOnMainThreadDelegateOfObject: self
                                     withDelegateMethod: @selector(subItemAdded:)
                                     withNotificationName: VLCMediaSubItemAdded];
}

/* libvlc event callback */
static void HandleMediaMetaChanged( const libvlc_event_t * event, void * self)
{
    [[VLCEventManager sharedManager] callOnMainThreadDelegateOfObject: self
                                     withDelegateMethod: @selector(metaChanged:)
                                     withNotificationName: VLCMediaMetaChanged];

}

@interface VLCMedia (Private)
- (void)initializeInternalMediaDescriptor;
- (void)subItemAdded:(NSNotification *)notification;
- (void)metaChanged:(NSNotification *)notification;
- (void)fetchMetaInformation;
@end

@implementation VLCMedia (Private)
- (void)initializeInternalMediaDescriptor
{
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(subItemAdded:) name:VLCMediaSubItemAdded object:self];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(metaChanged:) name:VLCMediaMetaChanged object:self];
    libvlc_exception_t ex;
    libvlc_exception_init( &ex );
    libvlc_event_manager_t * p_em = libvlc_media_descriptor_event_manager( md, &ex );
    libvlc_event_attach( p_em, libvlc_MediaDescriptorSubItemAdded, HandleMediaSubItemAdded, self, &ex );
    libvlc_event_attach( p_em, libvlc_MediaDescriptorMetaChanged, HandleMediaMetaChanged, self, &ex );
    quit_on_exception( &ex );
    libvlc_media_list_t * p_mlist = libvlc_media_descriptor_subitems( md, NULL );
    if (!p_mlist)
        subitems = nil;
    else
    {
        subitems= [[VLCPlaylist playlistWithLibVLCMediaList: p_mlist] retain];
        libvlc_media_list_release( p_mlist );
    }
    [self fetchMetaInformation];
}

- (void)subItemAdded:(NSNotification *)notification
{
    if(!subitems)
    {
        libvlc_media_list_t * p_mlist = libvlc_media_descriptor_subitems( md, NULL );
        [self willChangeValueForKey:@"subitems"];
        subitems = [[VLCPlaylist playlistWithLibVLCMediaList: p_mlist] retain];
        [self didChangeValueForKey:@"subitems"];
        libvlc_media_list_release( p_mlist );
    }
}
- (void)metaChanged:(NSNotification *)notification
{
    [self fetchMetaInformation];
}

- (void)fetchMetaInformation
{
    NSMutableDictionary * temp;
    libvlc_exception_t ex;
    char * title;
    char * artist;
    char * arturl = NULL;

    libvlc_exception_init( &ex );

    title = libvlc_media_descriptor_get_meta( md, libvlc_meta_Title, &ex );
    quit_on_exception( &ex );

    artist = libvlc_media_descriptor_get_meta( md, libvlc_meta_Artist, &ex );
    quit_on_exception( &ex );

    arturl = libvlc_media_descriptor_get_meta( md, libvlc_meta_ArtworkURL, &ex );
    quit_on_exception( &ex );

    temp = [NSMutableDictionary dictionaryWithCapacity: 2];

    if (title)
        [temp setObject: [NSString stringWithUTF8String: title] forKey: VLCMetaInformationTitle];
    
#if 0
    /* We need to perform that in a thread because this takes long. */
    if (arturl)
    {
        NSString *plainStringURL = [NSString stringWithUTF8String:arturl];
        NSString *escapedStringURL = [plainStringURL stringByAddingPercentEscapesUsingEncoding:NSUTF8StringEncoding];
        NSURL *aURL = [NSURL URLWithString:escapedStringURL];
        NSImage * art =  [[NSImage alloc] initWithContentsOfURL: aURL];
        [art autorelease];
        if( art )
            [temp setObject: art forKey: VLCMetaInformationArtwork];
    }
#endif

    free( title );
    free( artist );
    free( arturl );

    [self willChangeValueForKey:@"metaInformation"];
    if( metaInformation )
        [metaInformation release];
    metaInformation = [[NSMutableDictionary alloc] initWithDictionary:temp];
    [self didChangeValueForKey:@"metaInformation"];
}
@end

@implementation VLCMedia
- (id)initWithURL:(NSString *)anURL
{
    if (self = [super init])
    {
        libvlc_exception_t ex;
        url = [anURL copy];

        libvlc_exception_init( &ex );
        
        md = libvlc_media_descriptor_new( [VLCLibrary sharedInstance],
                                          [anURL cString],
                                          &ex );
        quit_on_exception( &ex );
        metaInformation = nil;
        [self initializeInternalMediaDescriptor];
    }
    return self;
}

+ (id)mediaWithURL:(NSString *)anURL;
{
    return [[(VLCMedia*)[VLCMedia alloc] initWithURL: anURL ] autorelease];
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
            libvlc_event_manager_t * p_em = libvlc_media_descriptor_event_manager( md, NULL );
            libvlc_event_detach( p_em, libvlc_MediaDescriptorSubItemAdded, HandleMediaSubItemAdded, self, NULL );
            libvlc_event_detach( p_em, libvlc_MediaDescriptorMetaChanged, HandleMediaMetaChanged, self, NULL );
        }
        [super release];
    }
}

- (void) dealloc
{
    if( subitems )
        [subitems release];
    if( metaInformation )
        [metaInformation release];
    libvlc_media_descriptor_release( md );
    [url release];
    [super dealloc];
}

- (NSString *)url
{
    return [[url copy] autorelease];
}

- (VLCPlaylist *)subitems
{
    return subitems;
}

/* Returns a dictionary with corresponding object associated with a meta */
- (NSDictionary *)metaInformation
{
    return metaInformation;
}
/* Not supported yet */
- (id)delegate
{
    return nil;
}
@end

@implementation VLCMedia (LibVLCBridging)
- (id) initWithLibVLCMediaDescriptor: (libvlc_media_descriptor_t *)p_md
{
    if (self = [super init])
    {
        libvlc_exception_t ex;
        char * p_url;
        libvlc_exception_init( &ex );

        p_url = libvlc_media_descriptor_get_mrl( p_md, &ex );
        quit_on_exception( &ex );
        url = [[NSString stringWithCString: p_url] retain];
        libvlc_media_descriptor_retain( p_md );   
        md = p_md;
        [self initializeInternalMediaDescriptor];
    }
    return self;
}

+ (id) mediaWithLibVLCMediaDescriptor: (libvlc_media_descriptor_t *)p_md
{
    return [[[VLCMedia alloc] initWithLibVLCMediaDescriptor: p_md] autorelease];
}

- (libvlc_media_descriptor_t *) libVLCMediaDescriptor
{
    libvlc_media_descriptor_retain( md );
    return md;
}
@end