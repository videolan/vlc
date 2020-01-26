/*****************************************************************************
 * VLCPlaylistItem.m: MacOS X interface module
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

#import "VLCPlaylistItem.h"

#import <vlc_input.h>
#import <vlc_url.h>

#import "playlist/VLCPlaylistImageCache.h"
#import "extensions/NSString+Helpers.h"
#import "library/VLCInputItem.h"
#import "library/VLCLibraryDataTypes.h"

NSString *VLCPlaylistItemPasteboardType = @"org.videolan.vlc.playlistitemtype";

@interface VLCPlaylistItem()
{
    NSURL *_artworkURL;
}
@end

@implementation VLCPlaylistItem

- (instancetype)initWithPlaylistItem:(vlc_playlist_item_t *)p_item
{
    self = [super init];
    if (self) {
        _playlistItem = p_item;
        vlc_playlist_item_Hold(_playlistItem);
        [self updateRepresentation];
    }
    return self;
}

- (void)dealloc
{
    vlc_playlist_item_Release(_playlistItem);
}

- (NSString *)description
{
    return [NSString stringWithFormat:@"item %p, title: %@ duration %lli", &_playlistItem, _title, _duration];
}

- (uint64_t)uniqueID
{
    return vlc_playlist_item_GetId(_playlistItem);
}

- (VLCInputItem *)inputItem
{
    if (!_playlistItem) {
        return nil;
    }
    input_item_t *p_input = vlc_playlist_item_GetMedia(_playlistItem);
    if (!p_input) {
        return nil;
    }
    VLCInputItem *inputItem = [[VLCInputItem alloc] initWithInputItem:p_input];
    return inputItem;
}

- (VLCMediaLibraryMediaItem *)mediaLibraryItem
{
    return [VLCMediaLibraryMediaItem mediaItemForURL:self.url];
}

- (void)updateRepresentation
{
    input_item_t *p_media = vlc_playlist_item_GetMedia(_playlistItem);
    vlc_mutex_lock(&p_media->lock);
    _title = toNSStr(p_media->psz_name);
    _duration = p_media->i_duration;

    if (p_media->p_meta) {
        _artistName = toNSStr(vlc_meta_Get(p_media->p_meta, vlc_meta_Artist));
        _albumName = toNSStr(vlc_meta_Get(p_media->p_meta, vlc_meta_Album));

        NSString *artworkURLString = toNSStr(vlc_meta_Get(p_media->p_meta, vlc_meta_ArtworkURL));
        if (artworkURLString.length > 1) {
            _artworkURL = [NSURL URLWithString:artworkURLString];
        }
    }
    vlc_mutex_unlock(&p_media->lock);
}

- (NSURL *)url
{
    if (_playlistItem) {
        return nil;
    }

    input_item_t *p_media = vlc_playlist_item_GetMedia(_playlistItem);
    if (!p_media) {
        return nil;
    }
    char *psz_url = input_item_GetURI(p_media);
    if (!psz_url)
        return nil;

    NSURL *url = [NSURL URLWithString:toNSStr(psz_url)];
    free(psz_url);
    return url;
}

- (NSString *)path
{
    if (!_playlistItem) {
        return nil;
    }
    input_item_t *p_media = vlc_playlist_item_GetMedia(_playlistItem);
    if (!p_media) {
        return nil;
    }
    char *psz_url = input_item_GetURI(p_media);
    if (!psz_url)
        return nil;

    char *psz_path = vlc_uri2path(psz_url);
    NSString *path = toNSStr(psz_path);
    free(psz_url);
    free(psz_path);
    return path;
}

- (NSImage *)artworkImage
{
    NSImage *image = [VLCPlaylistImageCache artworkForPlaylistItemWithURL:_artworkURL];
    if (!image) {
        image = [NSImage imageNamed: @"noart.png"];
    }
    return image;
}

@end
