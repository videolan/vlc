//
//  AppleRemote.h
//  AppleRemote
//
//  Created by Martin Kahr on 11.03.06.
//  Copyright 2006 martinkahr.com. All rights reserved.
//

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