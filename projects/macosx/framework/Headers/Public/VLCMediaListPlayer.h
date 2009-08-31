//
//  VLCMediaListPlayer.h
//  VLCKit
//
//  Created by Pierre d'Herbemont on 8/24/09.
//  Copyright 2009 __MyCompanyName__. All rights reserved.
//

@class VLCMedia, VLCMediaPlayer, VLCMediaList;

@interface VLCMediaListPlayer : NSObject {
    void *instance;
    VLCMedia *_rootMedia;
    VLCMediaPlayer *_mediaPlayer;
    VLCMediaList *_mediaList;
}

@property (readwrite, retain) VLCMediaList *mediaList;

/**
 * rootMedia - Use this method to play a media and its subitems.
 * This will erase mediaList.
 * Setting mediaList will erase rootMedia.
 */
@property (readwrite, retain) VLCMedia *rootMedia;
@property (readonly, retain) VLCMediaPlayer *mediaPlayer;


/**
 * Basic play and stop are here. For other method, use the mediaPlayer.
 * This may change.
 */
- (void)play;
- (void)stop;

/**
 * media must be in the current media list.
 */
- (void)playMedia:(VLCMedia *)media;

@end
