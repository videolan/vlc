/*****************************************************************************
 * VLCMediaListPlayer.m: VLCKit.framework VLCMediaListPlayer implementation
 *****************************************************************************
 * Copyright (C) 2009 Pierre d'Herbemont
 * Partial Copyright (C) 2009 Felix Paul Kühne
 * Copyright (C) 2009 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Pierre d'Herbemont <pdherbemont # videolan.org>
 *          Felix Paul Kühne <fkuehne # videolan.org
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

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

        instance = libvlc_media_list_player_new([VLCLibrary sharedInstance]);
        libvlc_media_list_player_set_media_player(instance, [_mediaPlayer libVLCMediaPlayer]);
    }
    return self;
}

- (void)dealloc
{
    libvlc_media_list_player_release(instance);
    [_mediaPlayer release];
    [_rootMedia release];
    [_mediaList release];
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

    libvlc_media_list_player_set_media_list(instance, [mediaList libVLCMediaList]);
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
    _rootMedia = nil;

    VLCMediaList *mediaList = [[VLCMediaList alloc] init];
    if (media)
        [mediaList addMedia:media];

    // This will clean rootMedia
    [self setMediaList:mediaList];

    // Thus set rootMedia here.
    _rootMedia = [media retain];

    [mediaList release];
}

- (VLCMedia *)rootMedia
{
    return _rootMedia;
}

- (void)playMedia:(VLCMedia *)media
{
    libvlc_media_list_player_play_item(instance, [media libVLCMediaDescriptor]);
}

- (void)play
{
    libvlc_media_list_player_play(instance);
}

- (void)stop
{
    libvlc_media_list_player_stop(instance);
}

- (void)setRepeatMode:(VLCRepeatMode)repeatMode
{
    libvlc_playback_mode_t mode;
    switch (repeatMode) {
        case VLCRepeatAllItems:
            mode = libvlc_playback_mode_loop;
            break;
        case VLCDoNotRepeat:
            mode = libvlc_playback_mode_default;
            break;
        case VLCRepeatCurrentItem:
            mode = libvlc_playback_mode_repeat;
            break;
        default:
            NSAssert(0, @"Should not be reached");
            break;
    }
    libvlc_media_list_player_set_playback_mode(instance, mode);

    _repeatMode = repeatMode;
}

- (VLCRepeatMode)repeatMode
{
    return _repeatMode;
}
@end
