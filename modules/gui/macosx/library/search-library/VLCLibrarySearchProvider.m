/*****************************************************************************
 * VLCLibrarySearchProvider.m: MacOS X interface module
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

#import "VLCLibrarySearchProvider.h"

#import "extensions/NSArray+VLCAdditions.h"
#import "extensions/NSImage+VLCAdditions.h"
#import "extensions/NSString+Helpers.h"

#import "library/VLCLibraryDataTypes.h"

NSString * const VLCLibrarySearchProviderResultsUpdated = @"VLCLibrarySearchProviderResultsUpdated";

static NSArray<id<VLCMediaLibraryItemProtocol>> *fetchList(const size_t count, id<VLCMediaLibraryItemProtocol> _Nullable (^itemAtIndex)(size_t))
{
    NSMutableArray<id<VLCMediaLibraryItemProtocol>> * const array = [[NSMutableArray alloc] initWithCapacity:count];
    for (size_t i = 0; i < count; i++) {
        const id<VLCMediaLibraryItemProtocol> item = itemAtIndex(i);
        [array addObject:item];
    }
    return array.copy;
}

typedef vlc_ml_media_list_t *(*media_list_fetch_f)(vlc_medialibrary_t *, const vlc_ml_query_params_t *);

static NSArray<id<VLCMediaLibraryItemProtocol>> *fetchMediaList(media_list_fetch_f fetchFunc, const vlc_ml_query_params_t *params)
{
    vlc_medialibrary_t * const p_ml = getMediaLibrary();
    if (!p_ml) {
        return @[];
    }
    vlc_ml_media_list_t * const p_list = fetchFunc(p_ml, params);
    NSArray<id<VLCMediaLibraryItemProtocol>> * const results = [NSArray arrayFromVlcMediaList:p_list];
    if (p_list) {
        vlc_ml_media_list_release(p_list);
    }
    return results ?: @[];
}

#define FETCH_TYPED_LIST(name, type, Type, fetch_call)                                                      \
static NSArray<id<VLCMediaLibraryItemProtocol>> *name(const vlc_ml_query_params_t *params)                  \
{                                                                                                           \
    vlc_medialibrary_t * const p_ml = getMediaLibrary();                                                    \
    if (!p_ml) {                                                                                            \
        return @[];                                                                                         \
    }                                                                                                       \
    vlc_ml_##type##_list_t * const p_list = fetch_call;                                                     \
    if (!p_list) {                                                                                          \
        return @[];                                                                                         \
    }                                                                                                       \
    NSArray<id<VLCMediaLibraryItemProtocol>> * const results = fetchList(p_list->i_nb_items, ^(size_t i) {  \
        return [[VLCMediaLibrary##Type alloc] initWith##Type:&p_list->p_items[i]];                          \
    });                                                                                                     \
    vlc_ml_##type##_list_release(p_list);                                                                   \
    return results;                                                                                         \
}

FETCH_TYPED_LIST(fetchAlbums, album, Album, vlc_ml_list_albums(p_ml, params))
FETCH_TYPED_LIST(fetchArtists, artist, Artist, vlc_ml_list_artists(p_ml, params, false))
FETCH_TYPED_LIST(fetchGenres, genre, Genre, vlc_ml_list_genres(p_ml, params))
FETCH_TYPED_LIST(fetchPlaylists, playlist, Playlist, vlc_ml_list_playlists(p_ml, params, VLC_ML_PLAYLIST_TYPE_ALL))

@implementation VLCLibrarySearchProvider
{
    VLCLibrarySearchProviderFetchBlock _fetchBlock;
    NSUInteger _searchGeneration;
}

- (instancetype)initWithDisplayTitle:(NSString *)displayTitle
                        displayImage:(NSImage *)displayImage
                          fetchBlock:(VLCLibrarySearchProviderFetchBlock)fetchBlock
{
    self = [super init];
    if (self) {
        _displayTitle = displayTitle;
        _displayImage = displayImage;
        _results = @[];
        _fetchBlock = [fetchBlock copy];
    }
    return self;
}

+ (NSImage *)imageWithSymbolName:(NSString *)symbolName
                   fallbackImage:(NSImage *)fallbackImage
{
    if (@available(macOS 11.0, *)) {
        return [NSImage imageWithSystemSymbolName:symbolName accessibilityDescription:symbolName];
    }
    fallbackImage.template = YES;
    return fallbackImage;
}

+ (NSArray<VLCLibrarySearchProvider *> *)defaultProviders
{
    return @[
        [[VLCLibrarySearchProvider alloc]
            initWithDisplayTitle:_NS("Videos")
                    displayImage:[self imageWithSymbolName:@"film.stack"
                                             fallbackImage:NSImage.VLCSidebarMovieImage]
                      fetchBlock:^(const vlc_ml_query_params_t *params) {
                          return fetchMediaList(vlc_ml_list_video_media, params);
                      }],
        [[VLCLibrarySearchProvider alloc]
            initWithDisplayTitle:_NS("Audio")
                    displayImage:[self imageWithSymbolName:@"music.note"
                                             fallbackImage:NSImage.VLCSidebarMusicImage]
                      fetchBlock:^(const vlc_ml_query_params_t *params) {
                          return fetchMediaList(vlc_ml_list_audio_media, params);
                      }],
        [[VLCLibrarySearchProvider alloc]
            initWithDisplayTitle:_NS("Albums")
                    displayImage:[self imageWithSymbolName:@"square.stack"
                                             fallbackImage:NSImage.VLCSidebarMusicImage]
                      fetchBlock:^(const vlc_ml_query_params_t *params) {
                          return fetchAlbums(params);
                      }],
        [[VLCLibrarySearchProvider alloc]
            initWithDisplayTitle:_NS("Artists")
                    displayImage:[self imageWithSymbolName:@"music.mic"
                                             fallbackImage:NSImage.VLCSidebarMusicImage]
                      fetchBlock:^(const vlc_ml_query_params_t *params) {
                          return fetchArtists(params);
                      }],
        [[VLCLibrarySearchProvider alloc]
            initWithDisplayTitle:_NS("Genres")
                    displayImage:[self imageWithSymbolName:@"guitars"
                                             fallbackImage:NSImage.VLCSidebarMusicImage]
                      fetchBlock:^(const vlc_ml_query_params_t *params) {
                          return fetchGenres(params);
                      }],
        [[VLCLibrarySearchProvider alloc]
            initWithDisplayTitle:_NS("Playlists")
                    displayImage:[self imageWithSymbolName:@"list.triangle"
                                             fallbackImage:[NSImage imageNamed:NSImageNameListViewTemplate]]
                      fetchBlock:^(const vlc_ml_query_params_t *params) {
                          return fetchPlaylists(params);
                      }],
    ];
}

- (void)searchForString:(NSString *)string
{
    if (string.length == 0) {
        [self clearSearch];
        return;
    }

    const NSUInteger generation = ++self->_searchGeneration;

    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INTERACTIVE, 0), ^{
        const vlc_ml_query_params_t params = { .psz_pattern = string.UTF8String };
        NSArray<id<VLCMediaLibraryItemProtocol>> * const results = self->_fetchBlock(&params);
        dispatch_async(dispatch_get_main_queue(), ^{
            if (generation != self->_searchGeneration) {
                return;
            }
            self->_results = results;
            [NSNotificationCenter.defaultCenter postNotificationName:VLCLibrarySearchProviderResultsUpdated
                                                              object:self];
        });
    });
}

- (void)clearSearch
{
    _results = @[];
    [NSNotificationCenter.defaultCenter postNotificationName:VLCLibrarySearchProviderResultsUpdated
                                                      object:self];
}

@end
