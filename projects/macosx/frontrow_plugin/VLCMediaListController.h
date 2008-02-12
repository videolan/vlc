//
//  VLCMediaListController.h
//  FRVLC
//
//  Created by Pierre d'Herbemont on 2/11/08.
//  Copyright 2008 __MyCompanyName__. All rights reserved.
//

#import <Cocoa/Cocoa.h>
#import <VLCKit/VLCKit.h>
#import <BackRow/BRMenuController.h>


@interface VLCMediaListController : BRMenuController {
    VLCMediaListAspect * mediaListAspect;
    BOOL isReloading;
}

- initWithMediaListAspect:(VLCMediaListAspect *)mediaListAspect;

@end
