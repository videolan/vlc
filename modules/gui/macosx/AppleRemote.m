//
//  AppleRemote.m
//  AppleRemote
//
//  Created by Martin Kahr on 11.03.06.
//  Copyright 2006 martinkahr.com. All rights reserved.
//

#import "AppleRemote.h"

const char* AppleRemoteDeviceName = "AppleIRController";
const int REMOTE_SWITCH_COOKIE=19;

@implementation AppleRemote

#pragma public interface

- (id) init {	
	if ( self = [super init] ) {
		openInExclusiveMode = YES;
		queue = NULL;
		hidDeviceInterface = NULL;
		cookieToButtonMapping = [[NSMutableDictionary alloc] init];
		
		[cookieToButtonMapping setObject:[NSNumber numberWithInt:kRemoteButtonVolume_Plus]	forKey:@"14_12_11_6_5_"];
		[cookieToButtonMapping setObject:[NSNumber numberWithInt:kRemoteButtonVolume_Minus] forKey:@"14_13_11_6_5_"];		
		[cookieToButtonMapping setObject:[NSNumber numberWithInt:kRemoteButtonMenu]			forKey:@"14_7_6_5_14_7_6_5_"];		
		[cookieToButtonMapping setObject:[NSNumber numberWithInt:kRemoteButtonPlay]			forKey:@"14_8_6_5_14_8_6_5_"];
		[cookieToButtonMapping setObject:[NSNumber numberWithInt:kRemoteButtonRight]		forKey:@"14_9_6_5_14_9_6_5_"];
		[cookieToButtonMapping setObject:[NSNumber numberWithInt:kRemoteButtonLeft]			forKey:@"14_10_6_5_14_10_6_5_"];
		[cookieToButtonMapping setObject:[NSNumber numberWithInt:kRemoteButtonRight_Hold]	forKey:@"14_6_5_4_2_"];
		[cookieToButtonMapping setObject:[NSNumber numberWithInt:kRemoteButtonLeft_Hold]	forKey:@"14_6_5_3_2_"];
		[cookieToButtonMapping setObject:[NSNumber numberWithInt:kRemoteButtonMenu_Hold]	forKey:@"14_6_5_14_6_5_"];
		[cookieToButtonMapping setObject:[NSNumber numberWithInt:kRemoteButtonPlay_Sleep]	forKey:@"18_14_6_5_18_14_6_5_"];
		[cookieToButtonMapping setObject:[NSNumber numberWithInt:kRemoteControl_Switched]	forKey:@"19_"];		
	}
	
	return self;
}

- (void) dealloc {
	[self stopListening:self];
	[cookieToButtonMapping release];
	[super dealloc];
}

- (void) setRemoteId: (int) value {
	remoteId = value;
}
- (int) remoteId {
	return remoteId;
}

- (BOOL) isRemoteAvailable {	
	io_object_t hidDevice = [self findAppleRemoteDevice];
	if (hidDevice != 0) {
		IOObjectRelease(hidDevice);
		return YES;
	} else {
		return NO;		
	}
}

- (BOOL) isListeningToRemote {
	return (hidDeviceInterface != NULL && allCookies != NULL && queue != NULL);	
}

- (void) setListeningToRemote: (BOOL) value {
	if (value == NO) {
		[self stopListening:self];
	} else {
		[self startListening:self];
	}
}

- (void) setDelegate: (id) _delegate {
	if ([_delegate respondsToSelector:@selector(appleRemoteButton:pressedDown:)]==NO) return;
	
	[_delegate retain];
	[delegate release];
	delegate = _delegate;
}
- (id) delegate {
	return delegate;
}

- (BOOL) isOpenInExclusiveMode {
	return openInExclusiveMode;
}
- (void) setOpenInExclusiveMode: (BOOL) value {
	openInExclusiveMode = value;
}

- (IBAction) startListening: (id) sender {	
	if ([self isListeningToRemote]) return;
	
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
	if (queue != NULL) {
		(*queue)->stop(queue);		
		
		//dispose of queue
		(*queue)->dispose(queue);		
		
		//release the queue we allocated
		(*queue)->Release(queue);	
		
		queue = NULL;
	}
	
	if (allCookies != nil) {
		[allCookies autorelease];
		allCookies = nil;
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
- (unsigned)retainCount {
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

- (IOHIDQueueInterface**) queue {
	return queue;
}

- (IOHIDDeviceInterface**) hidDeviceInterface {
	return hidDeviceInterface;
}


- (NSDictionary*) cookieToButtonMapping {
	return cookieToButtonMapping;
}

- (void) handleEventWithCookieString: (NSString*) cookieString sumOfValues: (SInt32) sumOfValues {
	NSNumber* buttonId = [[self cookieToButtonMapping] objectForKey: cookieString];
	if (buttonId != nil) {
		if (delegate) {		
			[delegate appleRemoteButton:[buttonId intValue] pressedDown: (sumOfValues>0)];
		}		
	} else {
		NSLog(@"Unknown button for cookiestring %@", cookieString);
	}
}

@end

/*	Callback method for the device queue
Will be called for any event of any type (cookie) to which we subscribe
*/
static void QueueCallbackFunction(void* target,  IOReturn result, void* refcon, void* sender) {	
	AppleRemote* remote = (AppleRemote*)target;
	
	IOHIDEventStruct event;	
	AbsoluteTime 	 zeroTime = {0,0};
	NSMutableString* cookieString = [NSMutableString string];
	SInt32			 sumOfValues = 0;
	while (result == kIOReturnSuccess)
	{
		result = (*[remote queue])->getNextEvent([remote queue], &event, zeroTime, 0);		
		if ( result != kIOReturnSuccess )
			continue;
		
		if (REMOTE_SWITCH_COOKIE == (int)event.elementCookie) {
			[remote setRemoteId: event.value];
			[remote handleEventWithCookieString: @"19_" sumOfValues: 0];
		} else {
			sumOfValues+=event.value;
			[cookieString appendString:[NSString stringWithFormat:@"%d_", event.elementCookie]];					
		}
		
		//printf("%d %d %d\n", event.elementCookie, event.value, event.longValue);		
	}
	
	[remote handleEventWithCookieString: cookieString sumOfValues: sumOfValues];	
	
}

@implementation AppleRemote (IOKitMethods)

- (IOHIDDeviceInterface**) createInterfaceForDevice: (io_object_t) hidDevice {
	io_name_t				className;
	IOCFPlugInInterface**   plugInInterface = NULL;
	HRESULT					plugInResult = S_OK;
	SInt32					score = 0;
	IOReturn				ioReturnValue = kIOReturnSuccess;
	
	hidDeviceInterface = NULL;
	
	ioReturnValue = IOObjectGetClass(hidDevice, className);
	
	if (ioReturnValue != kIOReturnSuccess) {
		NSLog(@"Error: Failed to get class name.");
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
			NSLog(@"Error: Couldn't create HID class device interface");
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
	io_object_t	hidDevice = 0;
	
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
	IOHIDElementCookie		cookie;
	long					usage;
	long					usagePage;
	id						object;
	NSArray*				elements = nil;
	NSDictionary*			element;
	IOReturn success;
	
	if (!handle || !(*handle)) return NO;
	
	// Copy all elements, since we're grabbing most of the elements
	// for this device anyway, and thus, it's faster to iterate them
	// ourselves. When grabbing only one or two elements, a matching
	// dictionary should be passed in here instead of NULL.
	success = (*handle)->copyMatchingElements(handle, NULL, (CFArrayRef*)&elements);
	
	if (success == kIOReturnSuccess) {
		
		[elements autorelease];		
		/*
		cookies = calloc(NUMBER_OF_APPLE_REMOTE_ACTIONS, sizeof(IOHIDElementCookie)); 
		memset(cookies, 0, sizeof(IOHIDElementCookie) * NUMBER_OF_APPLE_REMOTE_ACTIONS);
		*/
		allCookies = [[NSMutableArray alloc] init];
		int i;
		for (i=0; i< [elements count]; i++) {
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

			[allCookies addObject: [NSNumber numberWithInt:(int)cookie]];
		}
	} else {
		return NO;
	}
	
	return YES;
}

- (BOOL) openDevice {
	HRESULT  result;
	
	IOHIDOptionsType openMode = kIOHIDOptionsTypeNone;
	if ([self isOpenInExclusiveMode]) openMode = kIOHIDOptionsTypeSeizeDevice;	
	IOReturn ioReturnValue = (*hidDeviceInterface)->open(hidDeviceInterface, openMode);	
	
	if (ioReturnValue == KERN_SUCCESS) {
		queue = (*hidDeviceInterface)->allocQueue(hidDeviceInterface);
		if (queue) {
			result = (*queue)->create(queue, 0, 12);	//depth: maximum number of elements in queue before oldest elements in queue begin to be lost.

			int i=0;
			for(i=0; i<[allCookies count]; i++) {
				IOHIDElementCookie cookie = (IOHIDElementCookie)[[allCookies objectAtIndex:i] intValue];
				(*queue)->addElement(queue, cookie, 0);
			}
									  
			// add callback for async events
			CFRunLoopSourceRef eventSource;
			ioReturnValue = (*queue)->createAsyncEventSource(queue, &eventSource);
			if (ioReturnValue == KERN_SUCCESS) {
				ioReturnValue = (*queue)->setEventCallout(queue,QueueCallbackFunction, self, NULL);
				if (ioReturnValue == KERN_SUCCESS) {
					CFRunLoopAddSource(CFRunLoopGetCurrent(), eventSource, kCFRunLoopDefaultMode);					
					//start data delivery to queue
					(*queue)->start(queue);	
					return YES;
				} else {
					NSLog(@"Error when setting event callout");
				}
			} else {
				NSLog(@"Error when creating async event source");
			}
		} else {
			NSLog(@"Error when opening device");
		}
	}
	return NO;				
}

@end