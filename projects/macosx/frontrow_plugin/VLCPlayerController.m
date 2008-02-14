//
//  VLCPlayerController.m
//  FRVLC
//
//  Created by hyei on 06/09/07.
//  Copyright 2007 __MyCompanyName__. All rights reserved.
//

#import "VLCPlayerController.h"

#import <BackRow/BREvent.h>
#import <BackRow/BRLayer.h>
#import <BackRow/BRControllerStack.h>

@implementation VLCPlayerController

- init
{
    self = [super init];
    
    BRLayer * layer = [self layer];
    _mediaLayer = [VLCMediaLayer layer];
    _mediaLayer.frame = layer.bounds;
    _mediaLayer.autoresizingMask = kCALayerWidthSizable|kCALayerHeightSizable;
    
    [layer addSublayer:_mediaLayer];
    
    return self;
}

- (VLCMedia *)media
{
    return [_mediaLayer media];
}

- (void)setMedia:(VLCMedia *)media
{
    _mediaLayer.media = media;
}

- (void)brEventAction:(BREvent*)event
{
    BREventUsage usage = [event usage];
    BREventValue value = [event value];
    VLCMediaPlayer * player = [_mediaLayer player];
    
    NSLog(@"usage: %d value: %d", usage, value);
    
    switch(usage) {
        case BREventOKUsage:
            [_mediaLayer playPause];
            break;
        case BREventRightUsage:
        {
            NSLog(@"RIGHT");
            float position = [player position];
            position += 0.05;
            position = MIN(1.0, MAX(0.0, position));
            [player setPosition:position];
            break;
        }
        case BREventLeftUsage:
        {
            NSLog(@"LEFT");
            float position = [player position];
            position -= 0.05;
            position = MIN(1.0, MAX(0.0, position));
            [player setPosition:position];
            break;
        }
        case BREventUpUsage:
        {
            NSLog(@"UP");
            [[[VLCLibrary sharedLibrary] audio] setVolume:[[[VLCLibrary sharedLibrary] audio] volume]+20];
            break;
        }
        case BREventDownUsage:
        {
            NSLog(@"DOWN");
            [[[VLCLibrary sharedLibrary] audio] setVolume:[[[VLCLibrary sharedLibrary] audio] volume]-20];
            break;
        }
        case BREventMenuUsage:
            [[self stack] popController];
        default:
            break;
    }
}

- (BOOL)firstResponder
{
    return YES;
}

- (void)controlWillDeactivate
{
    [_mediaLayer.player pause];
    [super controlWillDeactivate];
}

- (void)controlWasActivated
{
    [super controlWasActivated];
    [_mediaLayer.player play];
}

@end
