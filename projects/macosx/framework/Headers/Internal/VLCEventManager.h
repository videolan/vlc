/*****************************************************************************
 * VLCEventManager.h: VLCKit.framework VLCEventManager header
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

#import <pthread.h>

/**
 * The VLCEventManager class provides a safe way for inter-thread communications.
 */
@interface VLCEventManager : NSObject
{
    NSMutableArray *messageQueue;      //< Holds a queue of messages.
    NSMutableArray *pendingMessagesOnMainThread;   //< Holds the message that are being posted on main thread.
    NSLock          *pendingMessagesLock;
    pthread_t        dispatcherThread;  //< Thread responsible for dispatching messages.
    pthread_mutex_t  queueLock;         //< Queue lock.
    pthread_cond_t   signalData;        //< Data lock.
}

/* Factories */
/**
 * Returns the shared VLCEventManager.  There should only be one instance of this class.
 * \return Shared event manager.
 */
+ (id)sharedManager;

/* Operations */
/**
 * Sends a message to the target's delegate on the main thread.
 * \discussion The main thread is the one in which the main run loop is run, which usually
 * means the one in which the NSApplication object receives events. The method is performed
 * when the main thread runs the run loop in one of the common run loop modes (as specified
 * in the CFRunLoop documentation).
 *
 * The receiver is retained until the call is finished.
 * \param aTarget The target object who's delegate should receive the specified message.
 * \param aSelector A selector that identifies the method to invoke. The method should not
 * have a significant return value and should take a single argument of type NSNotification,
 * or no arguments.
 *
 * See “Selectors” for a description of the SEL type.
 * \param aNotificiationName The name of the notification that should be sent to the
 * distributed notification center.
 */
- (void)callOnMainThreadDelegateOfObject:(id)aTarget
                      withDelegateMethod:(SEL)aSelector
                    withNotificationName:(NSString *)aNotificationName;

/**
 * Sends a message to the target on the main thread.
 * \discussion The main thread is the one in which the main run loop is run, which usually
 * means the one in which the NSApplication object receives events. The method is performed
 * when the main thread runs the run loop in one of the common run loop modes (as specified
 * in the CFRunLoop documentation).
 *
 * The receiver and arg are retained until the call is finished.
 * \param aTarget The target object who should receive the specified message.
 * \param aSelector A selector that identifies the method to invoke. The method should not
 * have a significant return value and should take a single argument of type id,
 * or no arguments.
 *
 * See “Selectors” for a description of the SEL type.
 * \param arg The argument to pass in the message.  Pass nil if the method does not take an
 * argument.
 * distributed notification center.
 */
- (void)callOnMainThreadObject:(id)aTarget
                    withMethod:(SEL)aSelector
          withArgumentAsObject:(id)arg;

- (void)cancelCallToObject:(id)target;
@end
