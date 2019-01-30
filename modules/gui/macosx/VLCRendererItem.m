/*****************************************************************************
 * VLCRendererItem.m: Wrapper class for vlc_renderer_item_t
 *****************************************************************************
 * Copyright (C) 2016 VLC authors and VideoLAN
 *
 * Authors: Marvin Scholz <epirat07 at gmail dot com>
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

#import "VLCRendererItem.h"
#import "NSString+Helpers.h"

#include <vlc_common.h>
#include <vlc_renderer_discovery.h>

@implementation VLCRendererItem

- (instancetype)initWithRendererItem:(vlc_renderer_item_t*)item
{
    self = [super init];
    if (self) {
        if (!item)
            [NSException raise:NSInvalidArgumentException
                        format:@"item must not be nil"];
        _rendererItem = vlc_renderer_item_hold(item);
    }
    return self;
}

- (void)dealloc
{
    vlc_renderer_item_release(_rendererItem);
    _rendererItem = nil;
}

- (NSString*)name
{
    const char *name = vlc_renderer_item_name(_rendererItem);
    if (!name)
        return nil;
    return [NSString stringWithUTF8String:name];
}

- (NSString*)identifier
{
    const char *sout = vlc_renderer_item_sout(_rendererItem);
    if (!sout)
        return nil;
    return [NSString stringWithUTF8String:sout];
}

- (NSString*)iconURI
{
    const char *uri = vlc_renderer_item_icon_uri(_rendererItem);
    if (!uri)
        return nil;
    return [NSString stringWithUTF8String:uri];
}

- (int)capabilityFlags
{
    return vlc_renderer_item_flags(_rendererItem);
}

- (void)setRendererForPlaylist:(playlist_t*)playlist
{
    playlist_SetRenderer(playlist, _rendererItem);
}

- (BOOL)isEqual:(id)object
{
    if (![object isKindOfClass:[VLCRendererItem class]]) {
        return NO;
    }
    VLCRendererItem *other = object;
    return (other.rendererItem == self.rendererItem);
}

@end
