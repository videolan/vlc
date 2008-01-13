//
//  VLCStreamSession.h
//  VLCKit
//
//  Created by Pierre d'Herbemont on 1/12/08.
//  Copyright 2008 __MyCompanyName__. All rights reserved.
//

#import <Cocoa/Cocoa.h>
#import <VLCKit/VLCStreamOutput.h>
#import <VLCKit/VLCMediaPlayer.h>
#import <VLCKit/VLCMedia.h>


@interface VLCStreamSession : VLCMediaPlayer {
    VLCStreamOutput * streamOutput;
    VLCMedia * originalMedia;
}

+ (id)streamSession;

@property (retain) VLCMedia * media;
@property (retain) VLCStreamOutput * streamOutput;
@property (readonly) BOOL isComplete;

- (void)startStreaming;
@end
