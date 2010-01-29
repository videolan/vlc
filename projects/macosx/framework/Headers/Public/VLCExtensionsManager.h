//
//  VLCExtensionsManager.h
//  VLCKit
//
//  Created by Pierre d'Herbemont on 1/26/10.
//  Copyright 2010 __MyCompanyName__. All rights reserved.
//

#import <Cocoa/Cocoa.h>

@class VLCExtension;
@class VLCMediaPlayer;

@interface VLCExtensionsManager : NSObject {
    void *instance;
    NSMutableArray *_extensions;
    VLCMediaPlayer *_player;
    void *_previousInput;
}
+ (VLCExtensionsManager *)sharedManager;
- (NSArray *)extensions;
- (void)runExtension:(VLCExtension *)extension;

@property (readwrite, retain) VLCMediaPlayer *mediaPlayer;
@end
