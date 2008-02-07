//
//  VLCPlayerController.h
//  FRVLC
//
//  Created by hyei on 06/09/07.
//  Copyright 2007 __MyCompanyName__. All rights reserved.
//

#import <Cocoa/Cocoa.h>
#import <BackRow/BRMediaPlayerController.h>
#import "VLCMediaLayer.h"

@interface VLCPlayerController : BRController
{
    VLCMediaLayer * _mediaLayer;
}

@property(retain) NSString * path;

@end
