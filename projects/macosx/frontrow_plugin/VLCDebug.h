//
//  VLCDebug.h
//  FRVLC
//
//  Created by hyei on 06/09/07.
//  Copyright 2007 __MyCompanyName__. All rights reserved.
//

#import <Cocoa/Cocoa.h>

#define PRINT_ME() NSLog(@"%s", __PRETTY_FUNCTION__)

@interface VLCFakeObject : NSObject
+ (id)sharedFakeObject;
@end
