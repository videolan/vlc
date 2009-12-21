/*****************************************************************************
 * VLCMediaListPlayer.m: VLCKit.framework VLCMediaListPlayer implementation
 *****************************************************************************
 * Copyright (C) 2009 Pierre d'Herbemont
 * Partial Copyright (C) 2009 Felix Paul Kühne
 * Copyright (C) 2009 the VideoLAN team
 * $Id$
 *
 * Authors: Pierre d'Herbemont <pdherbemont # videolan.org>
 *          Felix Paul Kühne <fkuehne # videolan.org
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
    libvlc_exception_t ex;
    libvlc_exception_init(&ex);
    libvlc_media_list_player_play_item(instance, [media libVLCMediaDescriptor], &ex);
    catch_exception(&ex);
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

- (void)setRepeatMode:(VLCRepeatMode)repeatMode
{
    libvlc_exception_t ex;
    libvlc_exception_init(&ex);
    switch (repeatMode) {
        case VLCRepeatAllItems:
            libvlc_media_list_player_set_playback_mode(instance, libvlc_playback_mode_default, &ex);
            break;
        case VLCDoNotRepeat:
            libvlc_media_list_player_set_playback_mode(instance, libvlc_playback_mode_default, &ex);
            break;
        case VLCRepeatCurrentItem:
            libvlc_media_list_player_set_playback_mode(instance, libvlc_playback_mode_repeat, &ex);
            break;
        default:
            break;
    }
    catch_exception(&ex);
    _repeatMode = repeatMode;
}

- (VLCRepeatMode)repeatMode
{
    return _repeatMode;
}
@end
