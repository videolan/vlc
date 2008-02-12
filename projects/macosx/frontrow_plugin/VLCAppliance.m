//
//  VLCAppliance.m
//  FRVLC
//
//  Created by hyei on 31/08/07.
//  Copyright 2007 __MyCompanyName__. All rights reserved.
//

#import <VLCKit/VLCKit.h>

#import "VLCAppliance.h"

#import "VLCApplianceController.h"
#import "VLCMediaListController.h"

@implementation VLCAppliance

+ (NSString *) className {
	return @"RUIMoviesAppliance";
}

- (void)dealloc
{
    NSLog(@"DEALLOC");
    [super dealloc];
}

- (id)applianceController
{
    // Disabled until we properly display a menu for that. You can test it by uncommenting those lines, and comment the following line.
    // VLCMediaListAspect * mediaListAspect = [[[[VLCMediaDiscoverer alloc] initWithName:@"freebox"] discoveredMedia] hierarchicalAspect];
    // VLCApplianceController * controller = [[VLCMediaListController alloc] initWithMediaListAspect:mediaListAspect];

    VLCApplianceController * controller = [[VLCApplianceController alloc] initWithPath:[NSHomeDirectory() stringByAppendingPathComponent:@"Movies"]];
    
    return [controller autorelease];
}


//
//- (id)initWithSettings:(id)fp8
//{
//    self = [super initWithSettings:fp8];
//    NSLog(@"settings: %@", fp8);
//    return [self retain];
//}


@end
