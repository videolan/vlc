//
//  VLCMediaListLayer.h
//  VLC
//
//  Created by Pierre d'Herbemont on 1/14/08.
//  Copyright 2008 __MyCompanyName__. All rights reserved.
//

#import <Cocoa/Cocoa.h>
#import <QuartzCore/QuartzCore.h>
#import <VLCKit/VLCKit.h>
#import "VLCMediaLayer.h"
#import "VLCMediaArrayController.h"


@interface VLCMediaListLayer : CALayer {
    NSArray * content;
    NSUInteger selectedIndex;
    
    VLCMediaLayer * previousLayer;
    VLCMediaLayer * selectedLayer;
    VLCMediaLayer * nextLayer;

    VLCMediaArrayController * controller;
}

+ (id)layer;
+ (id)layerWithMediaArrayController:(VLCMediaArrayController *)aController;

@property (retain,readwrite) NSArray * content;
@property (readwrite) NSUInteger selectedIndex;
@property (retain,readwrite) VLCMediaArrayController * controller;

@end
