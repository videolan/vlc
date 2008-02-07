//
//  VLCMediaLayer.h
//  FRVLC
//
//  Created by hyei on 11/09/07.
//  Copyright 2007 __MyCompanyName__. All rights reserved.
//

#import <Cocoa/Cocoa.h>
#import <QuartzCore/QuartzCore.h>
#import <VLCKit/VLCKit.h>
#import <OpenGL/OpenGL.h>

@interface VLCMediaLayer : CALayer
{
    VLCVideoLayer * _videoLayer;
    VLCMediaPlayer * _player;
}

@property(retain, nonatomic) NSURL * url;

@property(readonly) VLCMediaPlayer * player;

- (void)goFaster;
- (void)goSlower;
- (void)playPause;

@end
