/*****************************************************************************
* VLCPlaylistImageCache.m: MacOS X interface module
*****************************************************************************
* Copyright (C) 2020 VLC authors and VideoLAN
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

#import "VLCPlaylistImageCache.h"
#import "extensions/NSString+Helpers.h"
#import <vlc_input.h>
#import <vlc_url.h>

NSUInteger kVLCMaximumPlaylistImageCacheSize = 100;

@interface VLCPlaylistImageCache()
{
    NSCache *_imageCache;
}
@end

@implementation VLCPlaylistImageCache

- (instancetype)init
{
    self = [super init];
    if (self) {
        _imageCache = [[NSCache alloc] init];
        _imageCache.countLimit = kVLCMaximumPlaylistImageCacheSize;
    }
    return self;
}

+ (instancetype)sharedImageCache
{
    static dispatch_once_t onceToken;
    static VLCPlaylistImageCache *sharedImageCache;
    dispatch_once(&onceToken, ^{
        sharedImageCache = [[VLCPlaylistImageCache alloc] init];
    });
    return sharedImageCache;
}

+ (NSImage *)artworkForPlaylistItemWithURL:(NSURL *)artworkURL
{
    return [[VLCPlaylistImageCache sharedImageCache] imageForPlaylistItemWithArtworkURL:artworkURL];
}

- (NSImage *)imageForPlaylistItemWithArtworkURL:(NSURL *)artworkURL
{
    if (artworkURL == nil) {
        return nil;
    }

    NSImage *artwork = [_imageCache objectForKey:artworkURL];
    if (artwork) {
        return artwork;
    }

    artwork = [[NSImage alloc] initWithContentsOfURL:artworkURL];
    if (artwork) {
        [_imageCache setObject:artwork forKey:artworkURL];
    }

    return artwork;
}

@end
