//
//  VLCMediaListPlayer.m
//  VLCKit
//
//  Created by Pierre d'Herbemont on 8/24/09.
//  Copyright 2009 __MyCompanyName__. All rights reserved.
//

#import "VLCMediaListPlayer.h"
#import "VLCMedia.h"
#import "VLCMediaPlayer.h"
#import "VLCMediaList.h"
#import "VLCLibVLCBridging.h"

@implementation VLCMediaListPlayer
- (id)init
{
    if (self = [super init])
    {
        _mediaPlayer = [[VLCMediaPlayer alloc] init];

        libvlc_exception_t ex;
        libvlc_exception_init(&ex);
        instance = libvlc_media_list_player_new([VLCLibrary sharedInstance], &ex);
        catch_exception(&ex);
        libvlc_media_list_player_set_media_player(instance, [_mediaPlayer libVLCMediaPlayer], &ex);
        catch_exception(&ex);
    }
    return self;
}

- (void)dealloc
{
    libvlc_media_list_player_release(instance);
    [_mediaPlayer release];
    [_rootMedia release];
    [super dealloc];
}
- (VLCMediaPlayer *)mediaPlayer
{
    return _mediaPlayer;
}

- (void)setMediaList:(VLCMediaList *)mediaList
{
    if (_mediaList == mediaList)
        return;
    [_mediaList release];
    _mediaList = [mediaList retain];
    
    libvlc_exception_t ex;
    libvlc_exception_init(&ex);
    libvlc_media_list_player_set_media_list(instance, [mediaList libVLCMediaList], &ex);
    catch_exception(&ex);
    [self willChangeValueForKey:@"rootMedia"];
    [_rootMedia release];
    _rootMedia = nil;
    [self didChangeValueForKey:@"rootMedia"];
}

- (VLCMediaList *)mediaList
{
    return _mediaList;
}

- (void)setRootMedia:(VLCMedia *)media
{
    if (_rootMedia == media)
        return;
    [_rootMedia release];
    _rootMedia = [media retain];
    VLCMediaList *mediaList = [[VLCMediaList alloc] init];
    if (media)
        [mediaList addMedia:media];
    [self setMediaList:mediaList];
    [mediaList release];
}

- (VLCMedia *)rootMedia
{
    return _rootMedia;
}

- (void)play
{
    libvlc_exception_t ex;
    libvlc_exception_init(&ex);
    libvlc_media_list_player_play(instance, &ex);
    catch_exception(&ex);    
}

- (void)stop
{
    libvlc_exception_t ex;
    libvlc_exception_init(&ex);
    libvlc_media_list_player_stop(instance, &ex);
    catch_exception(&ex);        
}
@end
