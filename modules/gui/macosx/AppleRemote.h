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
 * (i.e. changes that were checked in into one of VideoLAN's source code
 * repositories) are licensed under the GNU General Public License version 2,
 * or (at your option) any later version. 
 * Thus, the following statements apply to our changes:
 *
 * Copyright (C) 2006 the VideoLAN team
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
	kRemoteButtonVolume_Plus=0,
	kRemoteButtonVolume_Minus,
	kRemoteButtonMenu,
	kRemoteButtonPlay,
	kRemoteButtonRight,	
	kRemoteButtonLeft,	
	kRemoteButtonRight_Hold,	
	kRemoteButtonLeft_Hold,
	kRemoteButtonMenu_Hold,
	kRemoteButtonPlay_Sleep,
	kRemoteControl_Switched
};
typedef enum AppleRemoteEventIdentifier AppleRemoteEventIdentifier;

/*	Encapsulates usage of the apple remote control
	This class is implemented as a singleton as there is exactly one remote per machine (until now)
	The class is not thread safe
*/
@interface AppleRemote : NSObject {
	IOHIDDeviceInterface** hidDeviceInterface;
	IOHIDQueueInterface**  queue;
	NSMutableArray*		   allCookies;
	NSMutableDictionary*   cookieToButtonMapping;

	BOOL openInExclusiveMode;
	
	int remoteId;

	IBOutlet id delegate;
}

- (void) setRemoteId: (int) aValue;
- (int) remoteId;

- (BOOL) isRemoteAvailable;

- (BOOL) isListeningToRemote;
- (void) setListeningToRemote: (BOOL) value;

- (BOOL) isOpenInExclusiveMode;
- (void) setOpenInExclusiveMode: (BOOL) value;

- (void) setDelegate: (id) delegate;
- (id) delegate;

- (IBAction) startListening: (id) sender;
- (IBAction) stopListening: (id) sender;
@end

@interface AppleRemote (Singleton)

+ (AppleRemote*) sharedRemote;

@end

/*	Method definitions for the delegate of the AppleRemote class
*/
@interface NSObject(NSAppleRemoteDelegate)

- (void) appleRemoteButton: (AppleRemoteEventIdentifier)buttonIdentifier pressedDown: (BOOL) pressedDown;

@end

@interface AppleRemote (PrivateMethods) 
- (NSDictionary*) cookieToButtonMapping;
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