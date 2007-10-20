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

#import "VLCMediaList.h"
#import "VLCLibrary.h"
#import "VLCEventManager.h"
#import "VLCLibVLCBridging.h"
#include <vlc/vlc.h>
#include <vlc/libvlc.h>

/* Notification Messages */
NSString *VLCMediaListItemAdded        = @"VLCMediaListItemAdded";
NSString *VLCMediaListItemDeleted    = @"VLCMediaListItemDeleted";

// TODO: Documentation
@interface VLCMediaList (Private)
/* Initializers */
- (void)initInternalMediaList;

/* Libvlc event bridges */
- (void)mediaListItemAdded:(NSArray *)args;
- (void)mediaListItemRemoved:(NSNumber *)index;
@end

/* libvlc event callback */
static void HandleMediaListItemAdded(const libvlc_event_t *event, void *user_data)
{
    id self = user_data;
    
    // Check to see if the last item added is this item we're trying to introduce
    // If no, then add the item to the local list, otherwise, the item has already 
    // been added
    [[VLCEventManager sharedManager] callOnMainThreadObject:self 
                                                 withMethod:@selector(mediaListItemAdded:) 
                                       withArgumentAsObject:[NSArray arrayWithObjects:[VLCMedia mediaWithLibVLCMediaDescriptor:event->u.media_list_item_added.item],
                                           [NSNumber numberWithInt:event->u.media_list_item_added.index], nil]];
}

static void HandleMediaListItemDeleted( const libvlc_event_t * event, void * user_data)
{
    id self = user_data;
    
    // Check to see if the last item deleted is this item we're trying delete now.
    // If no, then delete the item from the local list, otherwise, the item has already 
    // been deleted
    [[VLCEventManager sharedManager] callOnMainThreadObject:self 
                                                 withMethod:@selector(mediaListItemRemoved:) 
                                       withArgumentAsObject:[NSNumber numberWithInt:event->u.media_list_item_deleted.index]];
}

@implementation VLCMediaList
- (id)init
{
    if (self = [super init])
    {
        // Create a new libvlc media list instance
        libvlc_exception_t p_e;
        libvlc_exception_init(&p_e);
        p_mlist = libvlc_media_list_new([VLCLibrary sharedInstance], &p_e);
        quit_on_exception(&p_e);
        
        // Initialize internals to defaults
        delegate = nil;
        [self initInternalMediaList];
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
            libvlc_event_manager_t * p_em = libvlc_media_list_event_manager(p_mlist, NULL);
            libvlc_event_detach(p_em, libvlc_MediaListItemDeleted, HandleMediaListItemDeleted, self, NULL);
            libvlc_event_detach(p_em, libvlc_MediaListItemAdded,   HandleMediaListItemAdded,   self, NULL);
        }
        [super release];
    }
}

- (void)dealloc
{
    // Release allocated memory
    libvlc_media_list_release(p_mlist);
    
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

- (void)lock
{
    libvlc_media_list_lock(p_mlist);
}

- (void)unlock
{
    libvlc_media_list_unlock(p_mlist);
}

- (int)addMedia:(VLCMedia *)media
{
    int index = [self count];
    [self insertMedia:media atIndex:index];
    return index;
}

- (void)insertMedia:(VLCMedia *)media atIndex: (int)index
{
    [media retain];
    
    // Add it to the libvlc's medialist
    libvlc_exception_t p_e;
    libvlc_exception_init( &p_e );
    libvlc_media_list_insert_media_descriptor(p_mlist, [media libVLCMediaDescriptor], index, &p_e);
    quit_on_exception(&p_e);
}

- (void)removeMediaAtIndex:(int)index
{
    [[self mediaAtIndex:index] release];

    // Remove it from the libvlc's medialist
    libvlc_exception_t p_e;
    libvlc_exception_init(&p_e);
    libvlc_media_list_remove_index(p_mlist, index, &p_e);
    quit_on_exception(&p_e);
}

- (VLCMedia *)mediaAtIndex:(int)index
{
    libvlc_exception_t p_e;
    libvlc_exception_init(&p_e);
    libvlc_media_descriptor_t *p_md = libvlc_media_list_item_at_index(p_mlist, index, &p_e);
    quit_on_exception(&p_e);
    
    // Returns local object for media descriptor, searchs for user data first.  If not found it creates a 
    // new cocoa object representation of the media descriptor.
    return [VLCMedia mediaWithLibVLCMediaDescriptor:p_md];
}

- (int)count
{
    libvlc_exception_t p_e;
    libvlc_exception_init(&p_e);
    int result = libvlc_media_list_count(p_mlist, &p_e);
    quit_on_exception(&p_e);

    return result;
}

- (int)indexOfMedia:(VLCMedia *)media
{
    libvlc_exception_t p_e;
    libvlc_exception_init(&p_e);
    int result = libvlc_media_list_index_of_item(p_mlist, [media libVLCMediaDescriptor], &p_e);
    quit_on_exception(&p_e);
    
    return result;
}

- (NSArray *)sublists
{
    NSMutableArray *ret = [[NSMutableArray alloc] initWithCapacity: 0];
    int i, count;

    libvlc_exception_t p_e;
    libvlc_exception_init(&p_e);
    count = libvlc_media_list_count(p_mlist, &p_e);
    quit_on_exception(&p_e);

    for(i = 0; i < count; i++)
    {
        libvlc_media_descriptor_t *p_md;
        libvlc_media_list_t *p_submlist;
        p_md = libvlc_media_list_item_at_index(p_mlist, i, NULL);
        p_submlist = libvlc_media_descriptor_subitems(p_md, NULL);
        if(p_submlist)
        {
            [ret addObject:[VLCMediaList medialistWithLibVLCMediaList:p_submlist]];
            libvlc_media_list_release(p_submlist);
        }
        libvlc_media_descriptor_release(p_md);
    }
    return [ret autorelease];
}

//- (VLCMediaList *)flatPlaylist
//{
//    VLCMediaList * flatPlaylist;
//    libvlc_exception_t p_e;
//    libvlc_exception_init( &p_e );
//    libvlc_media_list_t * p_flat_mlist = libvlc_media_list_flat_media_list( p_mlist, &p_e );
//    quit_on_exception( &p_e );
//    flatPlaylist = [VLCMediaList medialistWithLibVLCMediaList: p_flat_mlist];
//    libvlc_media_list_release( p_flat_mlist );
//    return flatPlaylist;
//}
//
//- (VLCMedia *)providerMedia
//{
//    VLCMedia * ret;
//    libvlc_exception_t p_e;
//    libvlc_exception_init( &p_e );
//    libvlc_media_descriptor_t * p_md = libvlc_media_list_media_descriptor( p_mlist, &p_e );
//    ret = [VLCMedia mediaWithLibVLCMediaDescriptor: p_md];
//    libvlc_media_descriptor_release( p_md );
//    quit_on_exception( &p_e );
//    return ret;
//}
@end

@implementation VLCMediaList (LibVLCBridging)
+ (id)medialistWithLibVLCMediaList:(void *)p_new_mlist;
{
    return [[[VLCMediaList alloc] initWithLibVLCMediaList:p_new_mlist] autorelease];
}

- (id)initWithLibVLCMediaList:(void *)p_new_mlist;
{
    if( self = [super init] )
    {
        p_mlist = p_new_mlist;
        libvlc_media_list_retain(p_mlist);
        [self initInternalMediaList];
    }
    return self;
}

- (void *)libVLCMediaList
{
    return p_mlist;
}
@end

@implementation VLCMediaList (Private)
- (void)initInternalMediaList
{
    // Add event callbacks
    [self lock];
    libvlc_exception_t p_e;
    libvlc_exception_init(&p_e);

    libvlc_event_manager_t *p_em = libvlc_media_list_event_manager(p_mlist, &p_e);
    libvlc_event_attach(p_em, libvlc_MediaListItemAdded,   HandleMediaListItemAdded,   self, &p_e);
    libvlc_event_attach(p_em, libvlc_MediaListItemDeleted, HandleMediaListItemDeleted, self, &p_e);
    [self unlock];
    
    quit_on_exception( &p_e );
}

- (void)mediaListItemAdded:(NSArray *)args
{
    VLCMedia *media = [args objectAtIndex:0];
    NSNumber *index = [args objectAtIndex:1];
    
    // Post the notification
    [[NSNotificationCenter defaultCenter] postNotificationName:VLCMediaListItemAdded
                                                        object:self
                                                      userInfo:[NSDictionary dictionaryWithObjectsAndKeys:
                                                          media, @"media",
                                                          index, @"index",
                                                          nil]];
    
    // Let the delegate know that the item was added
    if (delegate && [delegate respondsToSelector:@selector(mediaList:mediaAdded:atIndex:)])
        [delegate mediaList:self mediaAdded:media atIndex:[index intValue]];
}

- (void)mediaListItemRemoved:(NSNumber *)index
{
    // Post the notification
    [[NSNotificationCenter defaultCenter] postNotificationName:VLCMediaListItemDeleted 
                                                        object:self
                                                      userInfo:[NSDictionary dictionaryWithObjectsAndKeys:
                                                          index, @"index",
                                                          nil]];
    
    // Let the delegate know that the item is being removed
    if (delegate && [delegate respondsToSelector:@selector(mediaList:mediaRemovedAtIndex:)])
        [delegate mediaList:self mediaRemovedAtIndex:index];
}
@end

