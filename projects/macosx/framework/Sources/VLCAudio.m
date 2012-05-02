/*****************************************************************************
 * VLCAudio.m: VLCKit.framework VLCAudio implementation
 *****************************************************************************
 * Copyright (C) 2007 Faustino E. Osuna
 * Copyright (C) 2007 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Faustino E. Osuna <enrique.osuna # gmail.com>
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

#import "VLCAudio.h"
#import "VLCLibVLCBridging.h"

#define VOLUME_STEP                6
#define VOLUME_MAX                 200
#define VOLUME_MIN                 0

/* Notification Messages */
NSString * VLCMediaPlayerVolumeChanged = @"VLCMediaPlayerVolumeChanged";

/* libvlc event callback */
// TODO: Callbacks


@implementation VLCAudio
/**
 * Use this method instead of instance directly as this one is type checked.
 */
- (libvlc_media_player_t *)instance
{
    return instance;
}

- (id)init
{
    return nil;
}

- (id)initWithMediaPlayer:(VLCMediaPlayer *)mediaPlayer
{
    self = [super init];
    if (!self)
        return nil;
    instance = [mediaPlayer libVLCMediaPlayer];
    libvlc_media_player_retain([self instance]);
    return self;
}

- (void) dealloc
{
    libvlc_media_player_release([self instance]);
    [super dealloc];
}

- (void)setMute:(BOOL)value
{
    libvlc_audio_set_mute([self instance], value);
}

- (BOOL)isMuted
{
    return libvlc_audio_get_mute([self instance]);
}

- (void)setVolume:(NSUInteger)value
{
    if (value < VOLUME_MIN)
        value = VOLUME_MIN;
    else if (value > VOLUME_MAX)
        value = VOLUME_MAX;
    libvlc_audio_set_volume([self instance], value);
}

- (void)volumeUp
{
    NSUInteger tempVolume = [self volume] + VOLUME_STEP;
    if (tempVolume > VOLUME_MAX)
        tempVolume = VOLUME_MAX;
    else if (tempVolume < VOLUME_MIN)
        tempVolume = VOLUME_MIN;
    [self setVolume: tempVolume];
}

- (void)volumeDown
{
    NSUInteger tempVolume = [self volume] - VOLUME_STEP;
    if (tempVolume > VOLUME_MAX)
        tempVolume = VOLUME_MAX;
    else if (tempVolume < VOLUME_MIN)
        tempVolume = VOLUME_MIN;
    [self setVolume: tempVolume];
}

- (NSUInteger)volume
{
    return libvlc_audio_get_volume([self instance]);
}
@end
