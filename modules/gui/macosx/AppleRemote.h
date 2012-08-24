/*****************************************************************************
 * AppleRemote.h
 * AppleRemote
 * $Id$
 *
 * Created by Martin Kahr on 11.03.06 under a MIT-style license.
 * Copyright (c) 2006 martinkahr.com. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 *****************************************************************************
 *
 * Note that changes made by any members or contributors of the VideoLAN team
 * (i.e. changes that were checked in exclusively into one of VideoLAN's source code
 * repositories) are licensed under the GNU General Public License version 2,
 * or (at your option) any later version.
 * Thus, the following statements apply to our changes:
 *
 * Copyright (C) 2006-2007 VLC authors and VideoLAN
 * Authors: Eric Petit <titer@m0k.org>
 *          Felix Kühne <fkuehne at videolan dot org>
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
#import <mach/mach.h>
#import <mach/mach_error.h>
#import <IOKit/IOKitLib.h>
#import <IOKit/IOCFPlugIn.h>
#import <IOKit/hid/IOHIDLib.h>
#import <IOKit/hid/IOHIDKeys.h>

enum AppleRemoteEventIdentifier
{
    kRemoteButtonVolume_Plus        =1<<1,
    kRemoteButtonVolume_Minus       =1<<2,
    kRemoteButtonMenu               =1<<3,
    kRemoteButtonPlay               =1<<4,
    kRemoteButtonRight              =1<<5,
    kRemoteButtonLeft               =1<<6,
    kRemoteButtonRight_Hold         =1<<7,
    kRemoteButtonLeft_Hold          =1<<8,
    kRemoteButtonMenu_Hold          =1<<9,
    kRemoteButtonPlay_Sleep         =1<<10,
    kRemoteControl_Switched         =1<<11,
    kRemoteButtonVolume_Plus_Hold   =1<<12,
    kRemoteButtonVolume_Minus_Hold  =1<<13,
    k2009RemoteButtonPlay

   =1<<14,
    k2009RemoteButtonFullscreen
 =1<<15
};
typedef enum AppleRemoteEventIdentifier AppleRemoteEventIdentifier;

/*  Encapsulates usage of the apple remote control
This class is implemented as a singleton as there is exactly one remote per machine (until now)
The class is not thread safe
*/
@interface AppleRemote : NSObject {
    IOHIDDeviceInterface** hidDeviceInterface;
    IOHIDQueueInterface**  queue;
    NSArray*        _allCookies;
    NSDictionary*   _cookieToButtonMapping;
    CFRunLoopSourceRef     eventSource;

    BOOL _openInExclusiveMode;
    BOOL _simulatePlusMinusHold;
    BOOL _processesBacklog;

    /* state for simulating plus/minus hold */
    BOOL lastEventSimulatedHold;
    AppleRemoteEventIdentifier lastPlusMinusEvent;
    NSTimeInterval lastPlusMinusEventTime;

    int remoteId;
    unsigned int _clickCountEnabledButtons;
    NSTimeInterval _maxClickTimeDifference;
    NSTimeInterval lastClickCountEventTime;
    AppleRemoteEventIdentifier lastClickCountEvent;
    unsigned int eventClickCount;

    id delegate;
}
+ (AppleRemote *)sharedInstance;

@property (readonly) int remoteId;
@property (readonly) BOOL remoteAvailable;
@property (readwrite) BOOL listeningToRemote;
@property (readwrite) BOOL openInExclusiveMode;

/* click counting makes it possible to recognize if the user has pressed a button repeatedly
 * click counting does delay each event as it has to wait if there is another event (second click)
 * therefore there is a slight time difference (maximumClickCountTimeDifference) between a single click
 * of the user and the call of your delegate method
 * click counting can be enabled individually for specific buttons. Use the property clickCountEnableButtons
 * to set the buttons for which click counting shall be enabled */
@property (readwrite) BOOL clickCountingEnabled;

@property (readwrite) unsigned int clickCountEnabledButtons;

/* the maximum time difference till which clicks are recognized as multi clicks */
@property (readwrite) NSTimeInterval maximumClickCountTimeDifference;

/* When your application needs to much time on the main thread when processing an event other events
 * may already be received which are put on a backlog. As soon as your main thread
 * has some spare time this backlog is processed and may flood your delegate with calls.
 * Backlog processing is turned off by default. */
@property (readwrite) BOOL processesBacklog;

/* Sets an NSApplication delegate which starts listening when application is becoming active
 * and stops listening when application resigns being active.
 * If an NSApplication delegate has been already set all method calls will be forwarded to this delegate, too. */
@property (readwrite) BOOL listeningOnAppActivate;

/* Simulating plus/minus hold does deactivate sending of individual requests for plus/minus pressed down/released.
 * Instead special hold events are being triggered when the user is pressing and holding plus/minus for a small period.
 * With simulating enabled the plus/minus buttons do behave as the left/right buttons */
@property (readwrite) BOOL simulatesPlusMinusHold;

/* Delegates are not retained */
@property (readwrite, assign) id delegate;

- (IBAction) startListening: (id) sender;
- (IBAction) stopListening: (id) sender;
@end

@interface AppleRemote (Singleton)

+ (AppleRemote*) sharedRemote;

@end

/*  Method definitions for the delegate of the AppleRemote class */
@interface NSObject(NSAppleRemoteDelegate)

- (void) appleRemoteButton: (AppleRemoteEventIdentifier)buttonIdentifier pressedDown: (BOOL) pressedDown clickCount: (unsigned int) count;

@end

@interface AppleRemote (PrivateMethods)
@property (readonly) NSDictionary * cookieToButtonMapping;

- (void) setRemoteId: (int) aValue;
- (IOHIDQueueInterface**) queue;
- (IOHIDDeviceInterface**) hidDeviceInterface;
- (void) handleEventWithCookieString: (NSString*) cookieString sumOfValues: (SInt32) sumOfValues;
@end

@interface AppleRemote (IOKitMethods)
- (io_object_t) findAppleRemoteDevice;
- (IOHIDDeviceInterface**) createInterfaceForDevice: (io_object_t) hidDevice;
- (BOOL) initializeCookies;
- (BOOL) openDevice;
@end

/* A NSApplication delegate which is used to activate and deactivate listening to the remote control
 * dependent on the activation state of your application.
 * All events are delegated to the original NSApplication delegate if necessary */
@interface AppleRemoteApplicationDelegate : NSObject {
    id applicationDelegate;
}

- (id) initWithApplicationDelegate: (id) delegate;
- (id) applicationDelegate;
@end
