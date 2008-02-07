//
//  VLCDebug.m
//  FRVLC
//
//  Created by hyei on 06/09/07.
//  Copyright 2007 __MyCompanyName__. All rights reserved.
//

#import "VLCDebug.h"

static VLCFakeObject * sharedFakeObject = nil;

@implementation VLCFakeObject

+ (id)sharedFakeObject
{
    if(sharedFakeObject == nil) {
        sharedFakeObject = [[VLCFakeObject alloc] init];
    }
    
    return sharedFakeObject;
}

- (NSMethodSignature *)methodSignatureForSelector:(SEL)aSelector
{
    NSLog(@"methodSignatureForSelector: %@", NSStringFromSelector(aSelector));
    return nil;
}

- (void)forwardInvocation:(NSInvocation *)anInvocation
{
    NSLog(@"forwardInvocation: %@", anInvocation);
}

- (BOOL)respondsToSelector:(SEL)selector
{
    return YES;
}

@end