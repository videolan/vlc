/*****************************************************************************
 * VLCLibraryController.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2019 VLC authors and VideoLAN
 *
 * Authors: Felix Paul Kühne <fkuehne # videolan -dot- org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#import "VLCLibraryController.h"

#import "main/VLCMain.h"
#import "playlist/VLCPlayerController.h"
#import "library/VLCLibraryModel.h"

#import <vlc_media_library.h>

@interface VLCLibraryController()
{
    vlc_medialibrary_t *_p_libraryInstance;
}
@end

@implementation VLCLibraryController

- (instancetype)init
{
    self = [super init];
    if (self) {
        _p_libraryInstance = vlc_ml_instance_get(getIntf());
        _libraryModel = [[VLCLibraryModel alloc] initWithLibrary:_p_libraryInstance];

        NSNotificationCenter *defaultNotificationCenter = [NSNotificationCenter defaultCenter];
        [defaultNotificationCenter addObserver:self
                                      selector:@selector(applicationWillEnterBackground:)
                                          name:NSApplicationWillResignActiveNotification
                                        object:nil];
        [defaultNotificationCenter addObserver:self
                                      selector:@selector(applicationWillBecomeActive:)
                                          name:NSApplicationWillBecomeActiveNotification
                                        object:nil];
        [defaultNotificationCenter addObserver:self
                                      selector:@selector(playbackStateChanged:)
                                          name:VLCPlayerStateChanged
                                        object:nil];
    }
    return self;
}

- (void)dealloc
{
    [[NSNotificationCenter defaultCenter] removeObserver:self];
    _p_libraryInstance = NULL;
}

- (void)applicationWillEnterBackground:(NSNotification *)aNotification
{
    vlc_ml_resume_background(_p_libraryInstance);
}

- (void)applicationWillBecomeActive:(NSNotification *)aNotification
{
    vlc_ml_pause_background(_p_libraryInstance);
}

- (void)playbackStateChanged:(NSNotification *)aNotification
{
    VLCPlayerController *playerController = aNotification.object;
    if (playerController.playerState == VLC_PLAYER_STATE_PLAYING) {
        vlc_ml_pause_background(_p_libraryInstance);
    } else {
        vlc_ml_resume_background(_p_libraryInstance);
    }
}

@end
