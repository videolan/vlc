//
//  VLCExtensionsManager.h
//  VLCKit
//
//  Created by Pierre d'Herbemont on 1/26/10.
//  Copyright 2010 __MyCompanyName__. All rights reserved.
//

#import <Cocoa/Cocoa.h>

@class VLCExtension;

@interface VLCExtensionsManager : NSObject {
    void *instance;
}
+ (VLCExtensionsManager *)sharedManager;
- (NSArray *)extensions;
- (void)runExtension:(VLCExtension *)extension;
@end
