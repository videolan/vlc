/*****************************************************************************
 * VLCLibraryNameCache.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2025 VLC authors and VideoLAN
 *
 * Authors: Claudio Cambra <developer@claudiocambra.com>
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

#import "VLCLibraryNameCache.h"

#import "VLCLibraryDataTypes.h"
#import "VLCLibraryModel.h"
#import "extensions/NSString+Helpers.h"

#include <vlc_media_library.h>

@implementation VLCLibraryNameCache {
    NSCache<NSNumber *, NSString *> *_albumTitleCache;
    NSCache<NSNumber *, NSString *> *_albumArtistCache;
    NSCache<NSNumber *, NSString *> *_artistNameCache;
    NSCache<NSNumber *, NSString *> *_genreNameCache;
}

+ (instancetype)sharedInstance
{
    static VLCLibraryNameCache *instance = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        instance = [[VLCLibraryNameCache alloc] init];
    });
    return instance;
}

- (instancetype)init
{
    self = [super init];
    if (self) {
        _albumTitleCache = [NSCache new];
        _albumArtistCache = [NSCache new];
        _artistNameCache = [NSCache new];
        _genreNameCache = [NSCache new];

        NSNotificationCenter * const center = NSNotificationCenter.defaultCenter;
        [center addObserver:self
                   selector:@selector(handleAlbumUpdate:)
                       name:VLCLibraryModelAlbumUpdated
                     object:nil];
        [center addObserver:self
                   selector:@selector(handleGenreUpdate:)
                       name:VLCLibraryModelGenreUpdated
                     object:nil];
        [center addObserver:self
                   selector:@selector(handleArtistUpdate:)
                       name:VLCLibraryModelArtistUpdated
                     object:nil];
    }
    return self;
}

- (void)dealloc
{
    [NSNotificationCenter.defaultCenter removeObserver:self];
}

- (void)handleAlbumUpdate:(NSNotification *)notification
{
    [self invalidateAlbumWithID:((VLCMediaLibraryAlbum *)notification.object).libraryID];
}

- (void)handleGenreUpdate:(NSNotification *)notification
{
    [self invalidateGenreWithID:((VLCMediaLibraryGenre *)notification.object).libraryID];
}

- (void)handleArtistUpdate:(NSNotification *)notification
{
    [self invalidateArtistWithID:((VLCMediaLibraryArtist *)notification.object).libraryID];
}

- (NSString *)albumTitleForID:(int64_t)albumID
{
    if (albumID <= 0) {
        return nil;
    }

    NSNumber * const key = @(albumID);
    NSString * const cached = [_albumTitleCache objectForKey:key];
    if (cached) {
        return cached;
    }

    vlc_medialibrary_t * const p_ml = getMediaLibrary();
    if (!p_ml) {
        return nil;
    }

    vlc_ml_album_t * const p_album = vlc_ml_get_album(p_ml, albumID);
    if (!p_album) {
        return nil;
    }

    NSString * const title = toNSStr(p_album->psz_title);
    NSString * const artist = toNSStr(p_album->psz_artist);

    [_albumTitleCache setObject:title forKey:key];
    [_albumArtistCache setObject:artist forKey:key];

    vlc_ml_album_release(p_album);
    return title;
}

- (NSString *)albumArtistForID:(int64_t)albumID
{
    if (albumID <= 0) {
        return nil;
    }

    NSNumber * const key = @(albumID);
    NSString * const cached = [_albumArtistCache objectForKey:key];
    if (cached) {
        return cached;
    }

    [self albumTitleForID:albumID];
    return [_albumArtistCache objectForKey:key];
}

- (NSString *)genreNameForID:(int64_t)genreID
{
    if (genreID <= 0) {
        return nil;
    }

    NSNumber * const key = @(genreID);
    NSString * const cached = [_genreNameCache objectForKey:key];
    if (cached) {
        return cached;
    }

    vlc_medialibrary_t * const p_ml = getMediaLibrary();
    if (!p_ml) {
        return nil;
    }

    vlc_ml_genre_t * const p_genre = vlc_ml_get_genre(p_ml, genreID);
    if (!p_genre) {
        return nil;
    }

    NSString * const name = toNSStr(p_genre->psz_name);
    [_genreNameCache setObject:name forKey:key];

    vlc_ml_genre_release(p_genre);
    return name;
}

- (NSString *)artistNameForID:(int64_t)artistID
{
    if (artistID <= 0) {
        return nil;
    }

    NSNumber * const key = @(artistID);
    NSString * const cached = [_artistNameCache objectForKey:key];
    if (cached) {
        return cached;
    }

    vlc_medialibrary_t * const p_ml = getMediaLibrary();
    if (!p_ml) {
        return nil;
    }

    vlc_ml_artist_t * const p_artist = vlc_ml_get_artist(p_ml, artistID);
    if (!p_artist) {
        return nil;
    }

    NSString * const name = toNSStr(p_artist->psz_name);
    [_artistNameCache setObject:name forKey:key];

    vlc_ml_artist_release(p_artist);
    return name;
}

- (void)invalidateAlbumWithID:(int64_t)albumID
{
    [_albumTitleCache removeObjectForKey:@(albumID)];
    [_albumArtistCache removeObjectForKey:@(albumID)];
}

- (void)invalidateGenreWithID:(int64_t)genreID
{
    [_genreNameCache removeObjectForKey:@(genreID)];
}

- (void)invalidateArtistWithID:(int64_t)artistID
{
    [_artistNameCache removeObjectForKey:@(artistID)];
}

- (void)invalidateAll
{
    [_albumTitleCache removeAllObjects];
    [_albumArtistCache removeAllObjects];
    [_artistNameCache removeAllObjects];
    [_genreNameCache removeAllObjects];
}

@end
