/*****************************************************************************
 * VLCAppliance.m: Front Row plugin
 *****************************************************************************
 * Copyright (C) 2007 - 2008 the VideoLAN Team
 * $Id$
 *
 * Authors: hyei
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
    // VLCMediaListAspect * mediaListAspect = [[[[[VLCMediaDiscoverer alloc] initWithName:@"freebox"] discoveredMedia] retain] hierarchicalAspect];
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
