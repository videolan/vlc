/*****************************************************************************
 * VLCEventManager.m: VLCKit.framework VLCEventManager implementation
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

/**
 * Defines the type of interthread message on the queue.
 */
typedef enum
{
    VLCNotification,                //< Standard NSNotification.
    VLCObjectMethodWithObjectArg,   //< Method with an object argument.
    VLCObjectMethodWithArrayArg     //< Method with an array argument.
} message_type_t;

/**
 * Data structured used to enqueue messages onto the queue.
 */
typedef struct {
    id target;                      //< Target object that should receive the message (retained until method is called).
    SEL sel;                        //< A selector that identifies the message to be sent to the target.
    union u                         //< Object could either be a VLCNotification or other.
    {
        NSString * name;            //< Name to be used for NSNotification
        id object;                  //< Object argument to pass to the target via the selector.
    } u;
    message_type_t type;            //< Type of queued message.
} message_t;

@interface VLCEventManager (Private)
- (void)callDelegateOfObjectAndSendNotificationWithArgs:(NSData*)data;
- (void)callObjectMethodWithArgs:(NSData*)data;
- (void)callDelegateOfObject:(id) aTarget withDelegateMethod:(SEL)aSelector withNotificationName:(NSString *)aNotificationName;
- (pthread_cond_t *)signalData;
- (pthread_mutex_t *)queueLock;
- (NSMutableArray *)messageQueue;
@end

/**
 * Provides a function for the main entry point for the dispatch thread.  It dispatches any messages that is queued.
 * \param user_data Pointer to the VLCEventManager instance that instiated this thread.
 */
static void * EventDispatcherMainLoop(void * user_data)
{
    VLCEventManager * self = user_data;
    
    for(;;)
    {
        NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];
        message_t * message, * message_newer = NULL;
        NSData * dataMessage;
        NSInteger i;

        /* Sleep a bit not to flood the interface */
        usleep(300);

        /* Wait for some data */

        pthread_mutex_lock( [self queueLock] );
        /* Wait until we have something on the queue */
        while( [[self messageQueue] count] <= 0 )
        {
            pthread_cond_wait( [self signalData], [self queueLock] );
        }

        //if( [[self messageQueue] count] % 100 == 0 || [[self messageQueue] count] < 100 )
          //  NSLog(@"[EVENT_MANAGER] On the stack we have %d elements", [[self messageQueue] count]);

        /* Get the first object off the queue. */
        dataMessage = [[[self messageQueue] lastObject] retain];    // Released in 'call'
        [[self messageQueue] removeLastObject];
        message = (message_t *)[dataMessage bytes];
        
        /* Remove duplicate notifications. */
        if( message->type == VLCNotification )
        {
            for( i = [[self messageQueue] count]-1; i >= 0; i-- )
            {
                message_newer = (message_t *)[(NSData *)[[self messageQueue] objectAtIndex:i] bytes];
                if( message_newer->type == VLCNotification &&
                    message_newer->target == message->target &&
                   [message_newer->u.name isEqualToString:message->u.name] )
                {
                    [message_newer->target release];
                    [message_newer->u.name release];
                    [[self messageQueue] removeObjectAtIndex:i];
                }
            }
        }
        else if( message->type == VLCObjectMethodWithArrayArg )
        {
            NSMutableArray * newArg = nil;

            /* Collapse messages that takes array arg by sending one bigger array */
            for( i = [[self messageQueue] count]-1; i >= 0; i-- )
            {
                message_newer = (message_t *)[(NSData *)[[self messageQueue] objectAtIndex: i] bytes];
                if( message_newer->type == VLCObjectMethodWithArrayArg &&
                    message_newer->target == message->target && 
                    message_newer->sel == message->sel )
                {
                    if(!newArg)
                    {
                        newArg = [NSMutableArray arrayWithArray:message->u.object];
                        [message->u.object release];
                    }
                    
                    [newArg addObjectsFromArray:message_newer->u.object];
                    [message_newer->target release];
                    [message_newer->u.object release];
                    [[self messageQueue] removeObjectAtIndex:i];
                }
                /* It shouldn be a good idea not to collapse event with other kind of event in-between.
                 * This could be particulary problematic when the same object receive two related events
                 * (for instance Added and Removed).
                 * Ignore for now only if target is the same */
                else if( message_newer->target == message->target )
                    break;
            }
            
            if( newArg )
                message->u.object = [newArg retain];
        }
        
        pthread_mutex_unlock( [self queueLock] );

        if( message->type == VLCNotification )
            [self performSelectorOnMainThread:@selector(callDelegateOfObjectAndSendNotificationWithArgs:) 
                                   withObject:dataMessage
                                waitUntilDone: NO];
        else
            [self performSelectorOnMainThread:@selector(callObjectMethodWithArgs:) 
                                   withObject:dataMessage
                                waitUntilDone: YES];

        [pool release];
    }
    return nil;
}

@implementation VLCEventManager
+ (id)sharedManager
{
    static VLCEventManager * defaultManager = NULL;
    
    /* We do want a lock here to avoid leaks */
    if ( !defaultManager )
    {
        defaultManager = [[VLCEventManager alloc] init];
    }

    return defaultManager;
}

- (void)dummy
{
    /* Put Cocoa in multithreaded mode by calling a dummy function */
}

- (id)init
{
    if( self = [super init] )
    {
        if(![NSThread isMultiThreaded])
        {
            [NSThread detachNewThreadSelector:@selector(dummy) toTarget:self withObject:nil];
            NSAssert([NSThread isMultiThreaded], @"Can't put Cocoa in multithreaded mode");
        }

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
    /* Don't send on main thread before this gets sorted out */
    NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];
    
    message_t message = 
    { 
        [aTarget retain], 
        aSelector, 
        [aNotificationName retain], 
        VLCNotification 
    };

    if( [NSThread isMainThread] )
    {
        [self callDelegateOfObjectAndSendNotificationWithArgs:[[NSData dataWithBytes:&message length:sizeof(message_t)] retain] /* released in the call */];
    } 
    else 
    {
        pthread_mutex_lock( [self queueLock] );
        [[self messageQueue] insertObject:[NSData dataWithBytes:&message length:sizeof(message_t)] atIndex:0];
        pthread_cond_signal( [self signalData] );
        pthread_mutex_unlock( [self queueLock] );
    }
    
    [pool release];
}

- (void)callOnMainThreadObject:(id)aTarget withMethod:(SEL)aSelector withArgumentAsObject: (id)arg
{
    NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];
    message_t message = 
    { 
        [aTarget retain], 
        aSelector, 
        [arg retain], 
        [arg isKindOfClass:[NSArray class]] ? VLCObjectMethodWithArrayArg : VLCObjectMethodWithObjectArg 
    };

    pthread_mutex_lock( [self queueLock] );
    [[self messageQueue] insertObject:[NSData dataWithBytes:&message length:sizeof(message_t)] atIndex:0];
    pthread_cond_signal( [self signalData] );
    pthread_mutex_unlock( [self queueLock] );
    
    [pool release];
}
@end

@implementation VLCEventManager (Private)
- (void)callDelegateOfObjectAndSendNotificationWithArgs:(NSData*)data
{
    message_t * message = (message_t *)[data bytes];

    [self callDelegateOfObject:message->target withDelegateMethod:message->sel withNotificationName:message->u.name];
    [message->u.name release];
    [message->target release];
    [data release];
}

- (void)callObjectMethodWithArgs:(NSData*)data
{
    message_t * message = (message_t *)[data bytes];
    void (*method)(id, SEL, id) = (void (*)(id, SEL, id))[message->target methodForSelector: message->sel];

    method( message->target, message->sel, message->u.object);
    [message->u.object release];
    [message->target release];
    [data release];
}

- (void)callDelegateOfObject:(id) aTarget withDelegateMethod:(SEL)aSelector withNotificationName: (NSString *)aNotificationName
{
    [[NSNotificationCenter defaultCenter] postNotification: [NSNotification notificationWithName:aNotificationName object:aTarget]];
    
    if (![aTarget delegate] || ![[aTarget delegate] respondsToSelector:aSelector])
        return;
    
    void (*method)(id, SEL, id) = (void (*)(id, SEL, id))[[aTarget delegate] methodForSelector: aSelector];
    method( [aTarget delegate], aSelector, [NSNotification notificationWithName:aNotificationName object:aTarget]);
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
@end
