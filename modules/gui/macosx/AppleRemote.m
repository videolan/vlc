/*****************************************************************************
 * AppleRemote.m
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
 * (i.e. changes that were exclusively checked in to one of VideoLAN's source code
 * repositories) are licensed under the GNU General Public License version 2,
 * or (at your option) any later version.
 * Thus, the following statements apply to our changes:
 *
 * Copyright (C) 2006-2009 VLC authors and VideoLAN
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

#import "AppleRemote.h"

/* this was added by the VideoLAN team to ensure Leopard-compatibility and is VLC-only */
#import "intf.h"
#import "CompatibilityFixes.h"

const char* AppleRemoteDeviceName = "AppleIRController";
const int REMOTE_SWITCH_COOKIE=19;
const NSTimeInterval DEFAULT_MAXIMUM_CLICK_TIME_DIFFERENCE=0.35;
const NSTimeInterval HOLD_RECOGNITION_TIME_INTERVAL=0.4;

@implementation AppleRemote

@synthesize openInExclusiveMode = _openInExclusiveMode, clickCountEnabledButtons = _clickCountEnabledButtons, maximumClickCountTimeDifference = _maxClickTimeDifference, processesBacklog=_processesBacklog, simulatesPlusMinusHold = _simulatePlusMinusHold;

#pragma public interface

static AppleRemote *_o_sharedInstance = nil;

+ (AppleRemote *)sharedInstance
{
    return _o_sharedInstance ? _o_sharedInstance : [[self alloc] init];
}

- (id)init
{
    if (_o_sharedInstance) {
        [self dealloc];
    } else {
        _o_sharedInstance = [super init];
        _openInExclusiveMode = YES;
        queue = NULL;
        hidDeviceInterface = NULL;
        NSMutableDictionary * mutableCookieToButtonMapping = [[NSMutableDictionary alloc] init];

        [mutableCookieToButtonMapping setObject:[NSNumber numberWithInt:kRemoteButtonVolume_Plus]    forKey:@"33_31_30_21_20_2_"];
        [mutableCookieToButtonMapping setObject:[NSNumber numberWithInt:kRemoteButtonVolume_Minus]   forKey:@"33_32_30_21_20_2_"];
        [mutableCookieToButtonMapping setObject:[NSNumber numberWithInt:kRemoteButtonMenu]           forKey:@"33_22_21_20_2_33_22_21_20_2_"];
        [mutableCookieToButtonMapping setObject:[NSNumber numberWithInt:kRemoteButtonPlay]           forKey:@"33_23_21_20_2_33_23_21_20_2_"];
        [mutableCookieToButtonMapping setObject:[NSNumber numberWithInt:kRemoteButtonRight]          forKey:@"33_24_21_20_2_33_24_21_20_2_"];
        [mutableCookieToButtonMapping setObject:[NSNumber numberWithInt:kRemoteButtonLeft]           forKey:@"33_25_21_20_2_33_25_21_20_2_"];
        [mutableCookieToButtonMapping setObject:[NSNumber numberWithInt:kRemoteButtonRight_Hold]     forKey:@"33_21_20_14_12_2_"];
        [mutableCookieToButtonMapping setObject:[NSNumber numberWithInt:kRemoteButtonLeft_Hold]      forKey:@"33_21_20_13_12_2_"];
        [mutableCookieToButtonMapping setObject:[NSNumber numberWithInt:kRemoteButtonMenu_Hold]      forKey:@"33_21_20_2_33_21_20_2_"];
        [mutableCookieToButtonMapping setObject:[NSNumber numberWithInt:kRemoteButtonPlay_Sleep]     forKey:@"37_33_21_20_2_37_33_21_20_2_"];
        [mutableCookieToButtonMapping setObject:[NSNumber numberWithInt:k2009RemoteButtonPlay]       forKey:@"33_21_20_8_2_33_21_20_8_2_"];
        [mutableCookieToButtonMapping setObject:[NSNumber numberWithInt:k2009RemoteButtonFullscreen] forKey:@"33_21_20_3_2_33_21_20_3_2_"];

        if( OSX_SNOW_LEOPARD )
            /* 10.6.2+ Snow Leopard cookies */
            [mutableCookieToButtonMapping setObject:[NSNumber numberWithInt:kRemoteControl_Switched]     forKey:@"19_"];
        else
        /* Lion cookies */
            [mutableCookieToButtonMapping setObject:[NSNumber numberWithInt:kRemoteControl_Switched]     forKey:@"42_33_23_21_20_2_33_23_21_20_2_"];

        _cookieToButtonMapping = [[NSDictionary alloc] initWithDictionary: mutableCookieToButtonMapping];
        [mutableCookieToButtonMapping release];

        /* defaults */
        _simulatePlusMinusHold = YES;
        _maxClickTimeDifference = DEFAULT_MAXIMUM_CLICK_TIME_DIFFERENCE;
    }

    return _o_sharedInstance;
}

- (void) dealloc {
    [self stopListening:self];
    [_cookieToButtonMapping release];
    [super dealloc];
}

- (int) remoteId {
    return remoteId;
}

- (BOOL) remoteAvailable {
    io_object_t hidDevice = [self findAppleRemoteDevice];
    if (hidDevice != 0) {
        IOObjectRelease(hidDevice);
        return YES;
    } else {
        return NO;
    }
}

- (BOOL) listeningToRemote {
    return (hidDeviceInterface != NULL && _allCookies != NULL && queue != NULL);
}

- (void) setListeningToRemote: (BOOL) value {
    if (value == NO) {
        [self stopListening:self];
    } else {
        [self startListening:self];
    }
}

/* Delegates are not retained!
 * http://developer.apple.com/documentation/Cocoa/Conceptual/CocoaFundamentals/CommunicatingWithObjects/chapter_6_section_4.html
 * Delegating objects do not (and should not) retain their delegates.
 * However, clients of delegating objects (applications, usually) are responsible for ensuring that their delegates are around
 * to receive delegation messages. To do this, they may have to retain the delegate. */
- (void) setDelegate: (id) _delegate {
    if (_delegate && [_delegate respondsToSelector:@selector(appleRemoteButton:pressedDown:clickCount:)]==NO) return;

    delegate = _delegate;
}
- (id) delegate {
    return delegate;
}

- (BOOL) clickCountingEnabled {
    return self.clickCountEnabledButtons != 0;
}
- (void) setClickCountingEnabled: (BOOL) value {
    if (value) {
        [self setClickCountEnabledButtons: kRemoteButtonVolume_Plus | kRemoteButtonVolume_Minus | kRemoteButtonPlay | kRemoteButtonLeft | kRemoteButtonRight | kRemoteButtonMenu | k2009RemoteButtonPlay | k2009RemoteButtonFullscreen];
    } else {
        [self setClickCountEnabledButtons: 0];
    }
}

- (BOOL) listeningOnAppActivate {
    id appDelegate = [NSApp delegate];
    return (appDelegate!=nil && [appDelegate isKindOfClass: [AppleRemoteApplicationDelegate class]]);
}
- (void) setListeningOnAppActivate: (BOOL) value {
    if (value) {
        if ([self listeningOnAppActivate]) return;
        AppleRemoteApplicationDelegate* appDelegate = [[AppleRemoteApplicationDelegate alloc] initWithApplicationDelegate: [NSApp delegate]];
        /* NSApp does not retain its delegate therefore we keep retain count on 1 */
        [NSApp setDelegate: appDelegate];
    } else {
        if ([self listeningOnAppActivate]==NO) return;
        AppleRemoteApplicationDelegate* appDelegate = (AppleRemoteApplicationDelegate*)[NSApp delegate];
        id previousAppDelegate = [appDelegate applicationDelegate];
        [NSApp setDelegate: previousAppDelegate];
        [appDelegate release];
    }
}

- (IBAction) startListening: (id) sender {
    if ([self listeningToRemote]) return;

    io_object_t hidDevice = [self findAppleRemoteDevice];
    if (hidDevice == 0) return;

    if ([self createInterfaceForDevice:hidDevice] == NULL) {
        goto error;
    }

    if ([self initializeCookies]==NO) {
        goto error;
    }

    if ([self openDevice]==NO) {
        goto error;
    }
    goto cleanup;

error:
    [self stopListening:self];

cleanup:
    IOObjectRelease(hidDevice);
}

- (IBAction) stopListening: (id) sender {
    if (eventSource != NULL) {
        CFRunLoopRemoveSource(CFRunLoopGetCurrent(), eventSource, kCFRunLoopDefaultMode);
        CFRelease(eventSource);
        eventSource = NULL;
    }
    if (queue != NULL) {
        (*queue)->stop(queue);

        //dispose of queue
        (*queue)->dispose(queue);

        //release the queue we allocated
        (*queue)->Release(queue);

        queue = NULL;
    }

    if (_allCookies != nil) {
        [_allCookies autorelease];
        _allCookies = nil;
    }

    if (hidDeviceInterface != NULL) {
        //close the device
        (*hidDeviceInterface)->close(hidDeviceInterface);

        //release the interface
        (*hidDeviceInterface)->Release(hidDeviceInterface);

        hidDeviceInterface = NULL;
    }
}

@end

@implementation AppleRemote (Singleton)

static AppleRemote* sharedInstance=nil;

+ (AppleRemote*) sharedRemote {
    @synchronized(self) {
        if (sharedInstance == nil) {
            sharedInstance = [[self alloc] init];
        }
    }
    return sharedInstance;
}
+ (id)allocWithZone:(NSZone *)zone {
    @synchronized(self) {
        if (sharedInstance == nil) {
            return [super allocWithZone:zone];
        }
    }
    return sharedInstance;
}
- (id)copyWithZone:(NSZone *)zone {
    return self;
}
- (id)retain {
    return self;
}
- (NSUInteger)retainCount {
    return UINT_MAX;  //denotes an object that cannot be released
}
- (void)release {
    //do nothing
}
- (id)autorelease {
    return self;
}

@end

@implementation AppleRemote (PrivateMethods)

- (void) setRemoteId: (int) value {
    remoteId = value;
}

- (IOHIDQueueInterface**) queue {
    return queue;
}

- (IOHIDDeviceInterface**) hidDeviceInterface {
    return hidDeviceInterface;
}

- (NSDictionary*) cookieToButtonMapping {
    return _cookieToButtonMapping;
}

- (NSString*) validCookieSubstring: (NSString*) cookieString {
    if (cookieString == nil || [cookieString length] == 0) return nil;
    NSEnumerator* keyEnum = [[self cookieToButtonMapping] keyEnumerator];
    NSString* key;
    while((key = [keyEnum nextObject])) {
        NSRange range = [cookieString rangeOfString:key];
        if (range.location == 0) return key;
    }
    return nil;
}

- (void) sendSimulatedPlusMinusEvent: (id) time {
    BOOL startSimulateHold = NO;
    AppleRemoteEventIdentifier event = lastPlusMinusEvent;
    @synchronized(self) {
        startSimulateHold = (lastPlusMinusEvent>0 && lastPlusMinusEventTime == [time doubleValue]);
    }
    if (startSimulateHold) {
        lastEventSimulatedHold = YES;
        event = (event==kRemoteButtonVolume_Plus) ? kRemoteButtonVolume_Plus_Hold : kRemoteButtonVolume_Minus_Hold;
        [delegate appleRemoteButton:event pressedDown: YES clickCount: 1];
    }
}

- (void) sendRemoteButtonEvent: (AppleRemoteEventIdentifier) event pressedDown: (BOOL) pressedDown {
    if (delegate) {
        if (self.simulatesPlusMinusHold) {
            if (event == kRemoteButtonVolume_Plus || event == kRemoteButtonVolume_Minus) {
                if (pressedDown) {
                    lastPlusMinusEvent = event;
                    lastPlusMinusEventTime = [NSDate timeIntervalSinceReferenceDate];
                    [self performSelector:@selector(sendSimulatedPlusMinusEvent:)
                               withObject:[NSNumber numberWithDouble:lastPlusMinusEventTime]
                               afterDelay:HOLD_RECOGNITION_TIME_INTERVAL];
                    return;
                } else {
                    if (lastEventSimulatedHold) {
                        event = (event==kRemoteButtonVolume_Plus) ? kRemoteButtonVolume_Plus_Hold : kRemoteButtonVolume_Minus_Hold;
                        lastPlusMinusEvent = 0;
                        lastEventSimulatedHold = NO;
                    } else {
                        @synchronized(self) {
                            lastPlusMinusEvent = 0;
                        }
                        pressedDown = YES;
                    }
                }
            }
        }

        if ((self.clickCountEnabledButtons & event) == event) {
            if (pressedDown==NO && (event == kRemoteButtonVolume_Minus || event == kRemoteButtonVolume_Plus)) {
                return; // this one is triggered automatically by the handler
            }
            NSNumber* eventNumber;
            NSNumber* timeNumber;
            @synchronized(self) {
                lastClickCountEventTime = [NSDate timeIntervalSinceReferenceDate];
                if (lastClickCountEvent == event) {
                    eventClickCount = eventClickCount + 1;
                } else {
                    eventClickCount = 1;
                }
                lastClickCountEvent = event;
                timeNumber = [NSNumber numberWithDouble:lastClickCountEventTime];
                eventNumber= [NSNumber numberWithUnsignedInt:event];
            }
            [self performSelector: @selector(executeClickCountEvent:)
                       withObject: [NSArray arrayWithObjects: eventNumber, timeNumber, nil]
                       afterDelay: _maxClickTimeDifference];
        } else {
            [delegate appleRemoteButton:event pressedDown: pressedDown clickCount:1];
        }
    }
}

- (void) executeClickCountEvent: (NSArray*) values {
    AppleRemoteEventIdentifier event = [[values objectAtIndex:0] unsignedIntValue];
    NSTimeInterval eventTimePoint = [[values objectAtIndex:1] doubleValue];

    BOOL finishedClicking = NO;
    int finalClickCount = eventClickCount;

    @synchronized(self) {
        finishedClicking = (event != lastClickCountEvent || eventTimePoint == lastClickCountEventTime);
        if (finishedClicking) eventClickCount = 0;
    }

    if (finishedClicking) {
        [delegate appleRemoteButton:event pressedDown: YES clickCount:finalClickCount];
        if ([self simulatesPlusMinusHold]==NO && (event == kRemoteButtonVolume_Minus || event == kRemoteButtonVolume_Plus)) {
            // trigger a button release event, too
            [NSThread sleepUntilDate: [NSDate dateWithTimeIntervalSinceNow:0.1]];
            [delegate appleRemoteButton:event pressedDown: NO clickCount:finalClickCount];
        }
    }

}

- (void) handleEventWithCookieString: (NSString*) cookieString sumOfValues: (SInt32) sumOfValues {
    /*
    if (previousRemainingCookieString) {
        cookieString = [previousRemainingCookieString stringByAppendingString: cookieString];
        NSLog(@"New cookie string is %@", cookieString);
        [previousRemainingCookieString release], previousRemainingCookieString=nil;
    }*/
    if (cookieString == nil || [cookieString length] == 0) return;
    NSNumber* buttonId = [[self cookieToButtonMapping] objectForKey: cookieString];
    if (buttonId != nil) {
        [self sendRemoteButtonEvent: [buttonId intValue] pressedDown: (sumOfValues>0)];
    } else {
        // let's see if a number of events are stored in the cookie string. this does
        // happen when the main thread is too busy to handle all incoming events in time.
        NSString* subCookieString;
        NSString* lastSubCookieString=nil;
        while((subCookieString = [self validCookieSubstring: cookieString])) {
            cookieString = [cookieString substringFromIndex: [subCookieString length]];
            lastSubCookieString = subCookieString;
            if (self.processesBacklog) [self handleEventWithCookieString: subCookieString sumOfValues:sumOfValues];
        }
        if (self.processesBacklog == NO && lastSubCookieString != nil) {
            // process the last event of the backlog and assume that the button is not pressed down any longer.
            // The events in the backlog do not seem to be in order and therefore (in rare cases) the last event might be
            // a button pressed down event while in reality the user has released it.
            // NSLog(@"processing last event of backlog");
            [self handleEventWithCookieString: lastSubCookieString sumOfValues:0];
        }
        if ([cookieString length] > 0) {
            msg_Warn( VLCIntf, "Unknown AR button for cookiestring %s", [cookieString UTF8String]);
        }
    }
}

@end

/*  Callback method for the device queue
Will be called for any event of any type (cookie) to which we subscribe
*/
static void QueueCallbackFunction(void* target,  IOReturn result, void* refcon, void* sender) {
    AppleRemote* remote = (AppleRemote*)target;

    IOHIDEventStruct event;
    AbsoluteTime     zeroTime = {0,0};
    NSMutableString* cookieString = [NSMutableString string];
    SInt32           sumOfValues = 0;
    while (result == kIOReturnSuccess)
    {
        result = (*[remote queue])->getNextEvent([remote queue], &event, zeroTime, 0);
        if ( result != kIOReturnSuccess )
            continue;

        //printf("%d %d %d\n", event.elementCookie, event.value, event.longValue);

        if (REMOTE_SWITCH_COOKIE == (int)event.elementCookie) {
            [remote setRemoteId: event.value];
            [remote handleEventWithCookieString: @"19_" sumOfValues: 0];
        } else {
            if (((int)event.elementCookie)!=5) {
                sumOfValues+=event.value;
                [cookieString appendString:[NSString stringWithFormat:@"%d_", event.elementCookie]];
            }
        }
    }

    [remote handleEventWithCookieString: cookieString sumOfValues: sumOfValues];
}

@implementation AppleRemote (IOKitMethods)

- (IOHIDDeviceInterface**) createInterfaceForDevice: (io_object_t) hidDevice {
    io_name_t               className;
    IOCFPlugInInterface**   plugInInterface = NULL;
    HRESULT                 plugInResult = S_OK;
    SInt32                  score = 0;
    IOReturn                ioReturnValue = kIOReturnSuccess;

    hidDeviceInterface = NULL;

    ioReturnValue = IOObjectGetClass(hidDevice, className);

    if (ioReturnValue != kIOReturnSuccess) {
        msg_Err( VLCIntf, "Failed to get IOKit class name.");
        return NULL;
    }

    ioReturnValue = IOCreatePlugInInterfaceForService(hidDevice,
                                                      kIOHIDDeviceUserClientTypeID,
                                                      kIOCFPlugInInterfaceID,
                                                      &plugInInterface,
                                                      &score);
    if (ioReturnValue == kIOReturnSuccess)
    {
        //Call a method of the intermediate plug-in to create the device interface
        plugInResult = (*plugInInterface)->QueryInterface(plugInInterface, CFUUIDGetUUIDBytes(kIOHIDDeviceInterfaceID), (LPVOID) &hidDeviceInterface);

        if (plugInResult != S_OK) {
            msg_Err( VLCIntf, "Couldn't create HID class device interface");
        }
        // Release
        if (plugInInterface) (*plugInInterface)->Release(plugInInterface);
    }
    return hidDeviceInterface;
}

- (io_object_t) findAppleRemoteDevice {
    CFMutableDictionaryRef hidMatchDictionary = NULL;
    IOReturn ioReturnValue = kIOReturnSuccess;
    io_iterator_t hidObjectIterator = 0;
    io_object_t hidDevice = 0;

    // Set up a matching dictionary to search the I/O Registry by class
    // name for all HID class devices
    hidMatchDictionary = IOServiceMatching(AppleRemoteDeviceName);

    // Now search I/O Registry for matching devices.
    ioReturnValue = IOServiceGetMatchingServices(kIOMasterPortDefault, hidMatchDictionary, &hidObjectIterator);

    if ((ioReturnValue == kIOReturnSuccess) && (hidObjectIterator != 0)) {
        hidDevice = IOIteratorNext(hidObjectIterator);
    }

    // release the iterator
    IOObjectRelease(hidObjectIterator);

    return hidDevice;
}

- (BOOL) initializeCookies {
    IOHIDDeviceInterface122** handle = (IOHIDDeviceInterface122**)hidDeviceInterface;
    IOHIDElementCookie      cookie;
    long                    usage;
    long                    usagePage;
    id                      object;
    NSArray*                elements;
    NSDictionary*           element;
    IOReturn success;

    if (!handle || !(*handle)) return NO;

    /* Copy all elements, since we're grabbing most of the elements
     * for this device anyway, and thus, it's faster to iterate them
     * ourselves. When grabbing only one or two elements, a matching
     * dictionary should be passed in here instead of NULL. */
    success = (*handle)->copyMatchingElements(handle, NULL, (CFArrayRef*)&elements);

    if (success == kIOReturnSuccess) {
        /*
        cookies = calloc(NUMBER_OF_APPLE_REMOTE_ACTIONS, sizeof(IOHIDElementCookie));
        memset(cookies, 0, sizeof(IOHIDElementCookie) * NUMBER_OF_APPLE_REMOTE_ACTIONS);
        */
        NSMutableArray *mutableAllCookies = [[NSMutableArray alloc] init];
        NSUInteger elementCount = [elements count];
        for (NSUInteger i=0; i< elementCount; i++) {
            element = [elements objectAtIndex:i];

            //Get cookie
            object = [element valueForKey: (NSString*)CFSTR(kIOHIDElementCookieKey) ];
            if (object == nil || ![object isKindOfClass:[NSNumber class]]) continue;
            if (object == 0 || CFGetTypeID(object) != CFNumberGetTypeID()) continue;
            cookie = (IOHIDElementCookie) [object longValue];

            //Get usage
            object = [element valueForKey: (NSString*)CFSTR(kIOHIDElementUsageKey) ];
            if (object == nil || ![object isKindOfClass:[NSNumber class]]) continue;
            usage = [object longValue];

            //Get usage page
            object = [element valueForKey: (NSString*)CFSTR(kIOHIDElementUsagePageKey) ];
            if (object == nil || ![object isKindOfClass:[NSNumber class]]) continue;
            usagePage = [object longValue];

            [mutableAllCookies addObject: [NSNumber numberWithInt:(int)cookie]];
        }
        _allCookies = [[NSArray alloc] initWithArray: mutableAllCookies];
        [mutableAllCookies release];
        [elements release];
    } else {
        if (elements)
            [elements release];
        return NO;
    }

    return YES;
}

- (BOOL) openDevice {
    HRESULT  result;

    IOHIDOptionsType openMode = kIOHIDOptionsTypeNone;
    if ([self openInExclusiveMode]) openMode = kIOHIDOptionsTypeSeizeDevice;
    IOReturn ioReturnValue = (*hidDeviceInterface)->open(hidDeviceInterface, openMode);

    if (ioReturnValue == KERN_SUCCESS) {
        queue = (*hidDeviceInterface)->allocQueue(hidDeviceInterface);
        if (queue) {
            result = (*queue)->create(queue, 0, 12);    //depth: maximum number of elements in queue before oldest elements in queue begin to be lost.

            NSUInteger cookieCount = [_allCookies count];
            for(NSUInteger i=0; i<cookieCount; i++) {
                IOHIDElementCookie cookie = (IOHIDElementCookie)[[_allCookies objectAtIndex:i] intValue];
                (*queue)->addElement(queue, cookie, 0);
            }

            // add callback for async events
            ioReturnValue = (*queue)->createAsyncEventSource(queue, &eventSource);
            if (ioReturnValue == KERN_SUCCESS) {
                ioReturnValue = (*queue)->setEventCallout(queue,QueueCallbackFunction, self, NULL);
                if (ioReturnValue == KERN_SUCCESS) {
                    CFRunLoopAddSource(CFRunLoopGetCurrent(), eventSource, kCFRunLoopDefaultMode);
                    //start data delivery to queue
                    (*queue)->start(queue);
                    return YES;
                } else {
                    msg_Err( VLCIntf, "Error when setting event callout");
                }
            } else {
                msg_Err( VLCIntf, "Error when creating async event source");
            }
        } else {
            msg_Err( VLCIntf, "Error when opening HUD device");
        }
    }
    return NO;
}

@end

@implementation AppleRemoteApplicationDelegate

- (id) initWithApplicationDelegate: (id) delegate {
    if((self = [super init]))
        applicationDelegate = [delegate retain];
    return self;
}

- (void) dealloc {
    [applicationDelegate release];
    [super dealloc];
}

- (id) applicationDelegate {
    return applicationDelegate;
}

- (void)applicationWillBecomeActive:(NSNotification *)aNotification {
    if ([applicationDelegate respondsToSelector: @selector(applicationWillBecomeActive:)]) {
        [applicationDelegate applicationWillBecomeActive: aNotification];
    }
}
- (void)applicationDidBecomeActive:(NSNotification *)aNotification {
    [[AppleRemote sharedRemote] setListeningToRemote: YES];

    if ([applicationDelegate respondsToSelector: @selector(applicationDidBecomeActive:)]) {
        [applicationDelegate applicationDidBecomeActive: aNotification];
    }
}
- (void)applicationWillResignActive:(NSNotification *)aNotification {
    [[AppleRemote sharedRemote] setListeningToRemote: NO];

    if ([applicationDelegate respondsToSelector: @selector(applicationWillResignActive:)]) {
        [applicationDelegate applicationWillResignActive: aNotification];
    }
}
- (void)applicationDidResignActive:(NSNotification *)aNotification {
    if ([applicationDelegate respondsToSelector: @selector(applicationDidResignActive:)]) {
        [applicationDelegate applicationDidResignActive: aNotification];
    }
}

- (NSMethodSignature *)methodSignatureForSelector:(SEL)aSelector {
    NSMethodSignature* signature = [super methodSignatureForSelector: aSelector];
    if (signature == nil && applicationDelegate != nil) {
        signature = [applicationDelegate methodSignatureForSelector: aSelector];
    }
    return signature;
}

- (void)forwardInvocation:(NSInvocation *)invocation {
    SEL aSelector = [invocation selector];

    if (applicationDelegate==nil || [applicationDelegate respondsToSelector:aSelector]==NO) {
        [super forwardInvocation: invocation];
        return;
    }

    [invocation invokeWithTarget:applicationDelegate];
}
@end
