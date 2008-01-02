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

- (void)mediaListViewItemAdded:(NSArray *)args;
- (void)mediaListViewItemRemoved:(NSNumber *)index;
@end

@implementation VLCMediaListAspectNode
@synthesize media;
@synthesize children;

- (BOOL)isLeaf
{
    return self.children == NULL;
}

-(void)dealloc
{
    [self.children release];
    [super dealloc];
}
@end

@implementation VLCMediaListAspect (KeyValueCodingCompliance)
/* For the @"media" key */
- (int) countOfMedia
{
    return [cachedNode count];
}
- (id) objectInMediaAtIndex:(int)i
{
    return [[cachedNode objectAtIndex:i] media];
}
/* For the @"node" key */
- (int) countOfNode
{
    return [cachedNode count];
}
- (id) objectInNodeAtIndex:(int)i
{
    return [cachedNode objectAtIndex:i];
}
@end

/* libvlc event callback */
static void HandleMediaListViewItemAdded(const libvlc_event_t * event, void * user_data)
{
    NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];
    id self = user_data;
    [[VLCEventManager sharedManager] callOnMainThreadObject:self 
                                                 withMethod:@selector(mediaListViewItemAdded:) 
                                       withArgumentAsObject:[NSArray arrayWithObject:[NSDictionary dictionaryWithObjectsAndKeys:
                                                          [VLCMedia mediaWithLibVLCMediaDescriptor:event->u.media_list_item_added.item], @"media",
                                                          [NSNumber numberWithInt:event->u.media_list_item_added.index], @"index",
                                                          nil]]];
    [pool release];
}

static void HandleMediaListViewItemDeleted( const libvlc_event_t * event, void * user_data)
{
    NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];
    id self = user_data;
    [[VLCEventManager sharedManager] callOnMainThreadObject:self 
                                                 withMethod:@selector(mediaListViewItemRemoved:) 
                                       withArgumentAsObject:[NSNumber numberWithInt:event->u.media_list_item_deleted.index]];
    [pool release];
}

@implementation VLCMediaListAspect
- (void)dealloc
{
    // Release allocated memory
    libvlc_media_list_view_release(p_mlv);
    [cachedNode release];
    if( ownHisMediaList )
        [parentMediaList release];
    [super dealloc];
}
- (NSString *)description
{
    NSMutableString * content = [NSMutableString string];
    int i;
    for( i = 0; i < [self count]; i++)
    {
        [content appendFormat:@"%@\n", [self mediaAtIndex: i]];
    }
    return [NSString stringWithFormat:@"<%@ %p> {\n%@}", [self className], self, content];
}

- (VLCMedia *)mediaAtIndex:(int)index
{
    libvlc_exception_t p_e;
    libvlc_exception_init( &p_e );
    libvlc_media_descriptor_t * p_md = libvlc_media_list_view_item_at_index( p_mlv, index, &p_e );
    quit_on_exception( &p_e );
    
    // Returns local object for media descriptor, searchs for user data first.  If not found it creates a 
    // new cocoa object representation of the media descriptor.
    return [VLCMedia mediaWithLibVLCMediaDescriptor:p_md];
}

- (VLCMediaListAspect *)childrenAtIndex:(int)index
{
    libvlc_exception_t p_e;
    libvlc_exception_init( &p_e );
    libvlc_media_list_view_t * p_sub_mlv = libvlc_media_list_view_children_at_index( p_mlv, index, &p_e );
    quit_on_exception( &p_e );

    if( !p_sub_mlv )
        return nil;

    // Returns local object for media descriptor, searchs for user data first.  If not found it creates a 
    // new cocoa object representation of the media descriptor.
    return [VLCMediaListAspect mediaListAspectWithLibVLCMediaListView:p_sub_mlv];
}

- (VLCMediaListAspectNode *)nodeAtIndex:(int)index
{
    VLCMediaListAspectNode * node = [[[VLCMediaListAspectNode alloc] init] autorelease];
    [node setMedia:[self mediaAtIndex: index]];
    return node;
}

- (int)count
{
    libvlc_exception_t p_e;
    libvlc_exception_init( &p_e );
    int result = libvlc_media_list_view_count( p_mlv, &p_e );
    quit_on_exception( &p_e );

    return result;
}

- (VLCMediaList *)parentMediaList
{
    return parentMediaList;
}
@end

@implementation VLCMediaListAspect (LibVLCBridging)
+ (id)mediaListAspectWithLibVLCMediaListView:(libvlc_media_list_view_t *)p_new_mlv
{
    return [[[VLCMediaListAspect alloc] initWithLibVLCMediaListView:p_new_mlv andMediaList:nil] autorelease];
}

+ (id)mediaListAspectWithLibVLCMediaListView:(libvlc_media_list_view_t *)p_new_mlv andMediaList:(VLCMediaList *)mediaList;
{
    return [[[VLCMediaListAspect alloc] initWithLibVLCMediaListView:p_new_mlv andMediaList:mediaList] autorelease];
}

- (id)initWithLibVLCMediaListView:(libvlc_media_list_view_t *)p_new_mlv andMediaList:(VLCMediaList *)mediaList;
{
    if( self = [super init] )
    {
        p_mlv = p_new_mlv;
        libvlc_media_list_view_retain(p_mlv);

        /* parentMediaList isn't retained, because we need a mediaList to exists, and not the contrary */
        parentMediaList = mediaList;
        ownHisMediaList = NO;
        if( !parentMediaList )
        {
            /* We have to create it then */
            libvlc_media_list_view_retain(p_mlv);
            libvlc_media_list_t * p_mlist = libvlc_media_list_view_parent_media_list(p_mlv, NULL);
            parentMediaList = [[VLCMediaList mediaListWithLibVLCMediaList: p_mlist] retain];
            libvlc_media_list_release( p_mlist );
            /* This is an exception, and we owns it here */
            ownHisMediaList = YES;
        }

        cachedNode = [[NSMutableArray alloc] initWithCapacity:libvlc_media_list_view_count(p_mlv, NULL)];
        libvlc_media_list_t * p_mlist;
        p_mlist = libvlc_media_list_view_parent_media_list( p_mlv, NULL );
        libvlc_media_list_lock( p_mlist );
        int i, count = libvlc_media_list_view_count(p_mlv, NULL);
        for( i = 0; i < count; i++ )
        {
            libvlc_media_descriptor_t * p_md = libvlc_media_list_view_item_at_index(p_mlv, i, NULL);
            libvlc_media_list_view_t * p_sub_mlv = libvlc_media_list_view_children_at_index(p_mlv, i, NULL);
            VLCMediaListAspectNode * node = [[[VLCMediaListAspectNode alloc] init] autorelease];
            [node setMedia:[VLCMedia mediaWithLibVLCMediaDescriptor: p_md]];
            [node setChildren: p_sub_mlv ? [VLCMediaListAspect mediaListAspectWithLibVLCMediaListView: p_sub_mlv] : nil];
            [cachedNode addObject:node];
            libvlc_media_descriptor_release(p_md);
            if( p_sub_mlv ) libvlc_media_list_view_release(p_sub_mlv);
        }
        [self initInternalMediaListView];
        libvlc_media_list_unlock( p_mlist );
        libvlc_media_list_release( p_mlist );
    }
    return self;
}

- (libvlc_media_list_view_t *)libVLCMediaListView
{
    return (libvlc_media_list_view_t *)p_mlv;
}
@end

@implementation VLCMediaListAspect (Private)
- (void)initInternalMediaListView
{
    libvlc_exception_t e;
    libvlc_exception_init( &e );

    libvlc_event_manager_t * p_em = libvlc_media_list_event_manager( p_mlv, &e );

    /* Add internal callback */
    libvlc_event_attach( p_em, libvlc_MediaListViewItemAdded,   HandleMediaListViewItemAdded,   self, &e );
    libvlc_event_attach( p_em, libvlc_MediaListViewItemDeleted, HandleMediaListViewItemDeleted, self, &e );
    quit_on_exception( &e );
}

- (void)mediaListViewItemAdded:(NSArray *)arrayOfArgs
{
    NSAssert([NSThread isMainThread], @"We are not on main thread");

    /* We hope to receive index in a nide range, that could change one day */
    int start = [[[arrayOfArgs objectAtIndex: 0] objectForKey:@"index"] intValue];
    int end = [[[arrayOfArgs objectAtIndex: [arrayOfArgs count]-1] objectForKey:@"index"] intValue];
    NSRange range = NSMakeRange(start, end-start);

    [self willChange:NSKeyValueChangeInsertion valuesAtIndexes:[NSIndexSet indexSetWithIndexesInRange:range] forKey:@"media"];
    [self willChange:NSKeyValueChangeInsertion valuesAtIndexes:[NSIndexSet indexSetWithIndexesInRange:range] forKey:@"node"];
    int i = [[[arrayOfArgs objectAtIndex: 0] objectForKey:@"index"] intValue];
    for( NSDictionary * args in arrayOfArgs )
    {
        int index = [[args objectForKey:@"index"] intValue];
        VLCMedia * media = [args objectForKey:@"media"];
        VLCMediaListAspectNode * node = [[[VLCMediaListAspectNode alloc] init] autorelease];
        [node setMedia:media];
        [node setChildren:[self childrenAtIndex:index]];
        /* Sanity check */
        NSAssert( i == index, @"Expects some troubles, inserted items are not in a range" ); i++;
        if( index && index >= [cachedNode count] )
            index = [cachedNode count] - 1;
        [cachedNode insertObject:node atIndex:index];
    }
    [self didChange:NSKeyValueChangeInsertion valuesAtIndexes:[NSIndexSet indexSetWithIndexesInRange:range] forKey:@"node"];
    [self didChange:NSKeyValueChangeInsertion valuesAtIndexes:[NSIndexSet indexSetWithIndexesInRange:range] forKey:@"media"];
}

- (void)mediaListViewItemRemoved:(NSNumber *)index
{
    [self willChange:NSKeyValueChangeInsertion valuesAtIndexes:[NSIndexSet indexSetWithIndex:[index intValue]] forKey:@"media"];
    [self willChange:NSKeyValueChangeInsertion valuesAtIndexes:[NSIndexSet indexSetWithIndex:[index intValue]] forKey:@"node"];
    [cachedNode removeObjectAtIndex:[index intValue]];
    [self didChange:NSKeyValueChangeInsertion valuesAtIndexes:[NSIndexSet indexSetWithIndex:[index intValue]] forKey:@"node"];
    [self didChange:NSKeyValueChangeInsertion valuesAtIndexes:[NSIndexSet indexSetWithIndex:[index intValue]] forKey:@"media"];
}
@end
