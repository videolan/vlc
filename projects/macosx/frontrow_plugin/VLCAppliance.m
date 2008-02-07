//
//  VLCAppliance.m
//  FRVLC
//
//  Created by hyei on 31/08/07.
//  Copyright 2007 __MyCompanyName__. All rights reserved.
//

#import "VLCAppliance.h"

#import "VLCApplianceController.h"

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
