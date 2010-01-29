//
//  VLCExtension.m
//  VLCKit
//
//  Created by Pierre d'Herbemont on 1/26/10.
//  Copyright 2010 __MyCompanyName__. All rights reserved.
//

#import "VLCExtension.h"
#import <vlc_extensions.h>

@implementation VLCExtension
- (NSString *)description
{
    return [NSString stringWithFormat:@"VLC Extension %@", [self name]];
}

- (id)initWithInstance:(struct extension_t *)instance
{
    self = [super init];
    if (!self)
        return nil;
    _instance = instance;
    return self;
}

- (struct extension_t *)instance
{
    return _instance;
}

- (NSString *)name
{
    return [NSString stringWithUTF8String:_instance->psz_name];
}

- (NSString *)title
{
    return [NSString stringWithUTF8String:_instance->psz_title];
}

@end
