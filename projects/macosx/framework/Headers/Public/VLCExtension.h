//
//  VLCExtension.h
//  VLCKit
//
//  Created by Pierre d'Herbemont on 1/26/10.
//  Copyright 2010 __MyCompanyName__. All rights reserved.
//

#import <Cocoa/Cocoa.h>


@interface VLCExtension : NSObject {
    struct extension_t *_instance;
}

- (id)initWithInstance:(struct extension_t *)instance; // FIXME: Should be internal
- (struct extension_t *)instance; // FIXME: Should be internal

- (NSString *)name;
- (NSString *)title;

@end
