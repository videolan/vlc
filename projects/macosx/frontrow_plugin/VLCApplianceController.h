//
//  VLCApplianceController.h
//  FRVLC
//
//  Created by hyei on 06/09/07.
//  Copyright 2007 __MyCompanyName__. All rights reserved.
//

#import <Cocoa/Cocoa.h>
#import <BackRow/BRMenuController.h>

@interface VLCApplianceController : BRMenuController
{
    NSString * _path;
    NSMutableArray * _contents;
}

- initWithPath:(NSString*)path;

@end
