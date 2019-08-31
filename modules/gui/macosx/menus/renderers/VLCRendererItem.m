/*****************************************************************************
 * VLCRendererItem.m: Wrapper class for vlc_renderer_item_t
 *****************************************************************************
 * Copyright (C) 2016, 2019 VLC authors and VideoLAN
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

#import "extensions/NSString+Helpers.h"
#import "playlist/VLCPlayerController.h"

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

- (NSString *)description
{
    return [NSString stringWithFormat:@"%@: name: %@, type: %@", [self className], self.name, self.type];
}

- (NSString *)name
{
    return toNSStr(vlc_renderer_item_name(_rendererItem));
}

- (NSString*)identifier
{
    return toNSStr(vlc_renderer_item_sout(_rendererItem));
}

- (NSString*)iconURI
{
    return toNSStr(vlc_renderer_item_icon_uri(_rendererItem));
}

- (NSString *)type
{
    return toNSStr(vlc_renderer_item_type(_rendererItem));
}

- (NSString *)userReadableType
{
    NSString *type = [self type];
    if ([type isEqualToString:@"stream_out_dlna"]) {
        return @"DLNA";
    } else if ([type isEqualToString:@"chromecast"]) {
        return @"Chromecast";
    }
    return type;
}

- (int)capabilityFlags
{
    return vlc_renderer_item_flags(_rendererItem);
}

- (void)setRendererForPlayerController:(VLCPlayerController *)playerController
{
    [playerController setRendererItem:_rendererItem];
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
