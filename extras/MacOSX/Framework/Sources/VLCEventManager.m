/*****************************************************************************
 * VLCTime.h: VLC.framework VLCTime implementation
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

#import "VLCEventManager.h"
#import <pthread.h>

static VLCEventManager * defaultManager = NULL;

enum message_type_t
{
    VLCNotification,
    VLCObjectMethodWithObjectArg
};

struct message {
    id target;
    SEL sel;
    union u
    {
        NSString * name;
        id object;
    } u;
    enum message_type_t type;
};

@interface VLCEventManager (Private)

- (void)callDelegateOfObjectAndSendNotificationWithArgs:(NSData*)data;
- (void)callObjectMethodWithArgs:(NSData*)data;
- (void)callDelegateOfObject:(id) aTarget withDelegateMethod:(SEL)aSelector withNotificationName: (NSString *)aNotificationName;
- (pthread_cond_t *)signalData;
- (pthread_mutex_t *)queueLock;
- (NSMutableArray *)messageQueue;

@end

static void * EventDispatcherMainLoop(void * user_data)
{
    VLCEventManager * self = user_data;

    for(;;)
    {
        NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];
        struct message * message, * message_newer = NULL;
        NSData *dataMessage;
        int i;

        /* Wait for some data */
        pthread_mutex_lock( [self queueLock] );
        
        /* Wait until we have something on the queue */
        while([[self messageQueue] count] <= 0 )
        {
            pthread_cond_wait( [self signalData], [self queueLock] );
        }
        message = (struct message *)[(NSData *)[[self messageQueue] lastObject] bytes];
        
        /* Don't send the same notification twice */
        if( message->type == VLCNotification )
        {
            for( i = 0; i < [[self messageQueue] count]-1; i++ )
            {
                message_newer = (struct message *)[(NSData *)[[self messageQueue] objectAtIndex: i] bytes];
                if( message_newer->type != VLCNotification )
                    continue;
                
                if( message_newer->target == message->target && message_newer->target == message->target && [message_newer->u.name isEqualToString:message->u.name] )
                {
                    [message_newer->target release];
                    [message->u.name release];
                    [[self messageQueue] removeObjectAtIndex: i];
                    i--;
                }
            }
        }

        dataMessage = [[self messageQueue] lastObject];
        
        pthread_mutex_unlock( [self queueLock] );
        
        if( message->type == VLCNotification )
            [self performSelectorOnMainThread:@selector(callDelegateOfObjectAndSendNotificationWithArgs:) withObject:[dataMessage retain] waitUntilDone: NO];
        else
            [self performSelectorOnMainThread:@selector(callObjectMethodWithArgs:) withObject:[dataMessage retain] waitUntilDone: NO];

        pthread_mutex_lock( [self queueLock] );
        [[self messageQueue] removeLastObject];
        pthread_mutex_unlock( [self queueLock] );
    
        [pool release];
    };
    return nil;
}

@implementation VLCEventManager

+ (id)sharedManager
{
    /* We do want a lock here to avoid leaks */
    if ( !defaultManager )
    {
        defaultManager = [[VLCEventManager alloc] init];
    }

    return defaultManager;
}

- (id)init
{
    if( self = [super init] )
    {
        pthread_mutex_init( &queueLock, NULL );
        pthread_cond_init( &signalData, NULL );
        pthread_create( &dispatcherThread, NULL, EventDispatcherMainLoop, self );
        messageQueue = [[NSMutableArray alloc] initWithCapacity:10];
    }
    return self;
}

- (void)dealloc
{
    pthread_kill( dispatcherThread, SIGKILL );
    pthread_join( dispatcherThread, NULL );

    [messageQueue release];
    [super dealloc];
}

- (void)callOnMainThreadDelegateOfObject:(id)aTarget withDelegateMethod:(SEL)aSelector withNotificationName: (NSString *)aNotificationName
{
    NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];
    
    struct message message = 
    { 
        [aTarget retain], 
        aSelector, 
        [aNotificationName retain], 
        VLCNotification 
    };
    
    pthread_mutex_lock( [self queueLock] );
    [[self messageQueue] insertObject:[NSData dataWithBytes:&message length:sizeof(struct message)] atIndex:0];
    pthread_cond_signal( [self signalData] );
    pthread_mutex_unlock( [self queueLock] );
    
    [pool release];
}

- (void)callOnMainThreadObject:(id)aTarget withMethod:(SEL)aSelector withArgumentAsObject: (id)arg
{
    NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];

    struct message message = 
    { 
        [aTarget retain], 
        aSelector, 
        [arg retain], 
        VLCObjectMethodWithObjectArg 
    };
    
    pthread_mutex_lock( [self queueLock] );
    [[self messageQueue] insertObject:[NSData dataWithBytes:&message length:sizeof(struct message)] atIndex:0];
    pthread_cond_signal( [self signalData] );
    pthread_mutex_unlock( [self queueLock] );
    
    [pool release];
}
@end

@implementation VLCEventManager (Private)

- (void)callDelegateOfObjectAndSendNotificationWithArgs:(NSData*)data
{
    struct message * message = (struct message *)[data bytes];

    [self callDelegateOfObject:message->target withDelegateMethod:message->sel withNotificationName:message->u.name];
    [message->u.name release];
    [message->target release];
    [data release];
}

- (void)callObjectMethodWithArgs:(NSData*)data
{
    struct message * message = (struct message *)[data bytes];
    void (*method)(id, SEL, id) = (void (*)(id, SEL, id))[message->target methodForSelector: message->sel];

    method( message->target, message->sel, message->u.object);
    [message->u.object release];
    [message->target release];
    [data release];
}

- (NSMutableArray *)messageQueue
{
    return messageQueue;
}

- (pthread_cond_t *)signalData
{
    return &signalData;
}

- (pthread_mutex_t *)queueLock
{
    return &queueLock;
}

- (void)callDelegateOfObject:(id) aTarget withDelegateMethod:(SEL)aSelector withNotificationName: (NSString *)aNotificationName
{
//    [[NSNotificationCenter defaultCenter] postNotification: [NSNotification notificationWithName:aNotificationName object:aTarget]];

    if (![aTarget delegate] || ![[aTarget delegate] respondsToSelector: aSelector])
        return;

    void (*method)(id, SEL, id) = (void (*)(id, SEL, id))[[aTarget delegate] methodForSelector: aSelector];
    method( [aTarget delegate], aSelector, [NSNotification notificationWithName:aNotificationName object:aTarget]);
}

@end
