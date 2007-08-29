/*****************************************************************************
 * VLCPlaylist.h: VLC.framework VLCPlaylist implementation
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

#import <VLC/VLCPlaylist.h>
#import "VLCLibrary.h"
#import "VLCEventManager.h"
#import <vlc/vlc.h>
#import <vlc/libvlc.h>

/* Our notification */
NSString * VLCPlaylistItemChanged = @"VLCPlaylistItemChanged";
NSString * VLCPlaylistItemDeleted = @"VLCPlaylistItemDeleted";
NSString * VLCPlaylistItemAdded   = @"VLCPlaylistItemAdded";

@interface VLCPlaylist (Private)
- (void)initializeInternalMediaList;

- (void)removeObjectFromLibVLCInItemsAtIndex:(int) i;
- (void)insertObjectFromLibVLC:(id)object inItemsAtIndex:(int) i;
@end

@interface VLCPlaylist (Binding)
/* Bindings */
- (unsigned int)countOfItems;
- (id)objectInItemsAtIndex:(unsigned int)index;
- (void)insertObject:(id)anObject inItemsAtIndex:(unsigned int)index;
- (void)replaceObjectInItemsAtIndex:(unsigned int)index withObject:(id)anObject;
@end

/* libvlc event callback */
static void HandleMediaListItemDeleted( const libvlc_event_t * event, void * user_data)
{
    id self = user_data;
    NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];
#ifdef DONT_UPDATE_VARIABLES_FROM_NON_MAIN_THREAD_CAUSE_BINDINGS_DONT_LIKE
    /* That would be nice to sync self's variables on non main thread, but
     * I did experience some Bindings related crash with an NSTreeController */
    [self removeObjectFromLibVLCInItemsAtIndex: event->u.media_list_item_deleted.index];
#endif
    [[VLCEventManager sharedManager] callOnMainThreadObject:self withMethod:@selector(playlistItemRemoved:) withArgumentAsObject:
                                        [NSNumber numberWithInt: event->u.media_list_item_deleted.index] ];

#ifdef DONT_SEND_NOTIFICATION_BECAUSE_WE_DONT_HAVE_TO
    /* This was disabled to avoid non necessary overhead, but we may want to
     * retablish that once the rest becomes more stable */
    [[VLCEventManager sharedManager] callOnMainThreadDelegateOfObject: self
                                     withDelegateMethod: @selector(playlistItemAdded:)
                                     withNotificationName: VLCPlaylistItemDeleted];
#endif
    [pool release];
}

/* libvlc event callback */
static void HandleMediaListItemAdded( const libvlc_event_t * event, void * user_data)
{
    id self = user_data;
    NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];
#ifdef DONT_UPDATE_VARIABLES_FROM_NON_MAIN_THREAD_CAUSE_BINDINGS_DONT_LIKE
    [self insertObjectFromLibVLC: [VLCMedia mediaWithLibVLCMediaDescriptor: event->u.media_list_item_added.item]
          inItemsAtIndex: event->u.media_list_item_added.index ];
#endif
    [[VLCEventManager sharedManager] callOnMainThreadObject:self withMethod:@selector(playlistItemAdded:) withArgumentAsObject:
                            [NSArray arrayWithObjects: [VLCMedia mediaWithLibVLCMediaDescriptor: event->u.media_list_item_added.item],
                                                       [NSNumber numberWithInt: event->u.media_list_item_added.index], nil ] ];

#ifdef DONT_SEND_NOTIFICATION_BECAUSE_WE_DONT_HAVE_TO
    [[VLCEventManager sharedManager] callOnMainThreadDelegateOfObject: self
                                     withDelegateMethod: @selector(playlistItemAdded:)
                                     withNotificationName: VLCPlaylistItemAdded];
#endif
    [pool release];
}

@implementation VLCPlaylist (Private)
- (void)initializeInternalMediaList
{
    libvlc_exception_t p_e;
    int i;
    [self lock];
    items = [[NSMutableArray alloc] init];

    for( i = 0; i < libvlc_media_list_count( p_mlist, NULL ); i++ )
    {
        libvlc_media_descriptor_t * p_md;
        p_md = libvlc_media_list_item_at_index( p_mlist, i, NULL );
        [items addObject:[VLCMedia mediaWithLibVLCMediaDescriptor: p_md]];
        libvlc_media_descriptor_release( p_md );
    }

    libvlc_event_manager_t * p_em = libvlc_media_list_event_manager( p_mlist, &p_e );
    libvlc_exception_init( &p_e );
    libvlc_event_attach( p_em, libvlc_MediaListItemDeleted, HandleMediaListItemDeleted, self, &p_e );
    libvlc_event_attach( p_em, libvlc_MediaListItemAdded,   HandleMediaListItemAdded,   self, &p_e );
    [self unlock];
    quit_on_exception( &p_e );
}

/* LibVLC callback, executed on main thread */
- (void)playlistItemAdded:(NSArray *)args
{
    [self insertObjectFromLibVLC: [args objectAtIndex: 0]
          inItemsAtIndex: [[args objectAtIndex: 1] intValue] ];
}

- (void)playlistItemRemoved:(NSNumber *)index
{
    [self removeObjectFromLibVLCInItemsAtIndex: [index intValue] ];
}

/* When LibVLC change its mlist's item this functions gets called */
- (void)removeObjectFromLibVLCInItemsAtIndex:(int) i
{
    [self willChange:NSKeyValueChangeRemoval valuesAtIndexes:[NSIndexSet indexSetWithIndex:i] forKey:@"items"];
    [items removeObjectAtIndex:i];
    [self didChange:NSKeyValueChangeRemoval valuesAtIndexes:[NSIndexSet indexSetWithIndex:i] forKey:@"items"];
}

- (void)insertObjectFromLibVLC:(id)object inItemsAtIndex:(int) i
{
    [self willChange:NSKeyValueChangeInsertion valuesAtIndexes:[NSIndexSet indexSetWithIndex:i] forKey:@"items"];
    [items insertObject:object atIndex: i];
    [self didChange:NSKeyValueChangeInsertion valuesAtIndexes:[NSIndexSet indexSetWithIndex:i] forKey:@"items"];
}

@end

@implementation VLCPlaylist (Binding)
/* "items" bindings */
- (unsigned int)countOfItems 
{
    return [items count];
}

- (id)objectInItemsAtIndex:(unsigned int)index 
{
    return [items objectAtIndex:index];
}

/* Setters go through LibVLC */
- (void)insertObject:(id)anObject inItemsAtIndex:(unsigned int)index 
{
    [self insertMedia:anObject atIndex:index];
}

- (void)removeObjectFromItemsAtIndex:(unsigned int)index 
{
    [self removeMediaAtIndex:index];
}

- (void)replaceObjectInItemsAtIndex:(unsigned int)index withObject:(id)anObject 
{
    [self removeMediaAtIndex:index];
    [self insertMedia:anObject atIndex:index];
}
@end


@implementation VLCPlaylist
- (id)init
{
    [VLCEventManager sharedManager];

    if (self = [super init])
    {
        libvlc_exception_t p_e;
        libvlc_exception_init( &p_e );
        p_mlist = libvlc_media_list_new( [VLCLibrary sharedInstance], &p_e );
        quit_on_exception( &p_e );
        [self initializeInternalMediaList];
    }
    return self;
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
            libvlc_event_manager_t * p_em = libvlc_media_list_event_manager( p_mlist, NULL );
            libvlc_event_detach( p_em, libvlc_MediaListItemDeleted, HandleMediaListItemDeleted, self, NULL );
            libvlc_event_detach( p_em, libvlc_MediaListItemAdded,   HandleMediaListItemAdded,   self, NULL );
        }
        [super release];
    }
}

- (void)dealloc
{
    libvlc_media_list_release( p_mlist );
    [items release];
    [super dealloc];
}

- (VLCMedia *)mediaAtIndex: (int)index
{
    libvlc_exception_t p_e;
    libvlc_exception_init( &p_e );
    libvlc_media_descriptor_t * p_md = libvlc_media_list_item_at_index( p_mlist, index, &p_e );
    quit_on_exception( &p_e );

    return [VLCMedia mediaWithLibVLCMediaDescriptor: p_md];
}

- (int)countMedia
{
    libvlc_exception_t p_e;
    libvlc_exception_init( &p_e );
    int count = libvlc_media_list_count( p_mlist, &p_e );
    quit_on_exception( &p_e );
    return count;
}

- (NSArray *)sublists
{
    NSMutableArray * ret = [[NSMutableArray alloc] initWithCapacity: 0];
    int i, count;
    libvlc_exception_t p_e;
    libvlc_exception_init( &p_e );
    count = libvlc_media_list_count( p_mlist, &p_e );
    quit_on_exception( &p_e );

    for( i = 0; i < count; i++ )
    {
        libvlc_media_descriptor_t * p_md;
        libvlc_media_list_t * p_submlist;
        p_md = libvlc_media_list_item_at_index( p_mlist, i, NULL );
        p_submlist = libvlc_media_descriptor_subitems( p_md, NULL );
        if( p_submlist )
        {
            [ret addObject: [VLCPlaylist playlistWithLibVLCMediaList: p_submlist]];
            libvlc_media_list_release( p_submlist );
        }
        libvlc_media_descriptor_release( p_md );
    }
    return [ret autorelease];
}

- (VLCPlaylist *)flatPlaylist
{
    VLCPlaylist * flatPlaylist;
    libvlc_exception_t p_e;
    libvlc_exception_init( &p_e );
    libvlc_media_list_t * p_flat_mlist = libvlc_media_list_flat_media_list( p_mlist, &p_e );
    quit_on_exception( &p_e );
    flatPlaylist = [VLCPlaylist playlistWithLibVLCMediaList: p_flat_mlist];
    libvlc_media_list_release( p_flat_mlist );
    return flatPlaylist;
}

- (VLCMedia *)providerMedia
{
    VLCMedia * ret;
    libvlc_exception_t p_e;
    libvlc_exception_init( &p_e );
    libvlc_media_descriptor_t * p_md = libvlc_media_list_media_descriptor( p_mlist, &p_e );
    ret = [VLCMedia mediaWithLibVLCMediaDescriptor: p_md];
    libvlc_media_descriptor_release( p_md );
    quit_on_exception( &p_e );
    return ret;
}

- (void)addMedia: (VLCMedia *)item
{
    libvlc_exception_t p_e;
    libvlc_exception_init( &p_e );
    libvlc_media_descriptor_t * p_md = [item libVLCMediaDescriptor];
    libvlc_media_list_add_media_descriptor( p_mlist, p_md, &p_e );
    libvlc_media_descriptor_release( p_md );
    quit_on_exception( &p_e );
}

- (void)insertMedia: (VLCMedia *)item atIndex: (int)index
{
    libvlc_exception_t p_e;
    libvlc_exception_init( &p_e );
    libvlc_media_descriptor_t * p_md = [item libVLCMediaDescriptor];
    libvlc_media_list_insert_media_descriptor( p_mlist, p_md, index, &p_e );
    libvlc_media_descriptor_release( p_md );
    quit_on_exception( &p_e );

}

- (void)removeMediaAtIndex: (int)index
{
    libvlc_exception_t p_e;
    libvlc_exception_init( &p_e );
    libvlc_media_list_remove_index( p_mlist, index, &p_e );
    quit_on_exception( &p_e );
}

- (void)lock
{
    libvlc_media_list_lock( p_mlist );
}

- (void)unlock
{
    libvlc_media_list_unlock( p_mlist );
}

- (id)delegate
{
    /* No delegate supported */
    return nil;
}
@end

@implementation VLCPlaylist (LibVLCBridging)
- (id) initWithLibVLCMediaList: (libvlc_media_list_t *)p_new_mlist;
{
    if( self = [super init] )
    {
        p_mlist = p_new_mlist;
        libvlc_media_list_retain( p_mlist );
        [self initializeInternalMediaList];
    }
    return self;
}

+ (id) playlistWithLibVLCMediaList: (libvlc_media_list_t *)p_new_mlist;
{
    return [[[VLCPlaylist alloc] initWithLibVLCMediaList: p_new_mlist] autorelease];
}

- (libvlc_media_list_t *) libVLCMediaList
{
    libvlc_media_list_retain( p_mlist );
    return p_mlist;
}
@end