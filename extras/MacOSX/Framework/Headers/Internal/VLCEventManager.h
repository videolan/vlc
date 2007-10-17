/*****************************************************************************
 * VLCEventManager.h: VLC.framework VLCEventManager header
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
#import <pthread.h>

/* This object is here to ensure safe inter thread communication */

@interface VLCEventManager : NSObject
{
    NSMutableArray *    messageQueue;
    pthread_t           dispatcherThread;
    pthread_mutex_t     queueLock;
    pthread_cond_t      signalData;
}

/* Return the default manager */
+ (id)sharedManager;

- (void)callOnMainThreadDelegateOfObject:(id)aTarget withDelegateMethod:(SEL)aSelector withNotificationName:(NSString *)aNotificationName;
- (void)callOnMainThreadObject:(id)aTarget withMethod:(SEL)aSelector withArgumentAsObject: (id)arg;
@end
