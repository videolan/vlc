//
//  VLCMediaLayer.h
//  VLC
//
//  Created by Pierre d'Herbemont on 1/14/08.
//  Copyright 2008 __MyCompanyName__. All rights reserved.
//

#import <Cocoa/Cocoa.h>
#import <QuartzCore/QuartzCore.h>
#import <VLCKit/VLCKit.h>


@interface VLCMediaLayer : CALayer {
    BOOL displayFullInformation;
    VLCMedia * media;
    CATextLayer * titleLayer;
    CATextLayer * artistLayer;
    CATextLayer * genreLayer;
    CALayer * artworkLayer;
}

+ (id)layerWithMedia:(VLCMedia *)media;

@property (assign) BOOL displayFullInformation;
@property (retain,readonly) VLCMedia * media;
@property (retain,readonly) CATextLayer * titleLayer;
@property (retain,readonly) CATextLayer * artistLayer;
@property (retain,readonly) CATextLayer * genreLayer;
@property (retain,readonly) CALayer * artworkLayer;

@end
