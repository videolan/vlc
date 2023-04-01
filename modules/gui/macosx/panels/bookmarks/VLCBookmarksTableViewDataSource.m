/*****************************************************************************
 * VLCBookmarksTableViewDataSource.m: MacOS X interface module bookmarking functionality
 *****************************************************************************
 * Copyright (C) 2023 VLC authors and VideoLAN
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

#import "VLCBookmarksTableViewDataSource.h"

#import "VLCBookmark.h"

#import "extensions/NSString+Helpers.h"

#import "library/VLCInputItem.h"
#import "library/VLCLibraryController.h"
#import "library/VLCLibraryDataTypes.h"
#import "library/VLCLibraryModel.h"

#import "main/VLCMain.h"

#import "playlist/VLCPlayerController.h"
#import "playlist/VLCPlaylistController.h"

#import <vlc_media_library.h>

NSString * const VLCBookmarksTableViewCellIdentifier = @"VLCBookmarksTableViewCellIdentifier";

@interface VLCBookmarksTableViewDataSource ()
{
    vlc_medialibrary_t *_mediaLibrary;
    VLCPlayerController *_playerController;
}
@end

@implementation VLCBookmarksTableViewDataSource

- (instancetype)init
{
    self = [super init];
    if (self) {
        _playerController = VLCMain.sharedInstance.playlistController.playerController;
        _mediaLibrary = vlc_ml_instance_get(getIntf());
        [self updateLibraryItemId];

        [NSNotificationCenter.defaultCenter addObserver:self
                                               selector:@selector(currentMediaItemChanged:)
                                                   name:VLCPlayerCurrentMediaItemChanged
                                                 object:nil];
    }
    return self;
}

- (void)updateLibraryItemId
{
    VLCMediaLibraryMediaItem * const currentMediaItem = [VLCMediaLibraryMediaItem mediaItemForURL:_playerController.URLOfCurrentMediaItem];
    if (currentMediaItem == nil) {
        _libraryItemId = -1;
        [self updateBookmarks];
        return;
    }

    const int64_t currentMediaItemId = currentMediaItem.libraryID;
    [self setLibraryItemId:currentMediaItemId];
    [self updateBookmarks];
}

- (void)updateBookmarks
{
    if (_libraryItemId <= 0) {
        _bookmarks = [NSArray array];
        return;
    }

    vlc_ml_bookmark_list_t * const vlcBookmarks = vlc_ml_list_media_bookmarks(_mediaLibrary, nil, _libraryItemId);

    if (vlcBookmarks == NULL) {
        _bookmarks = [NSArray array];
        return;
    }

    NSMutableArray<VLCBookmark *> * const tempBookmarks = [NSMutableArray arrayWithCapacity:vlcBookmarks->i_nb_items];

    for (int i = 0; i < vlcBookmarks->i_nb_items; i++) {
        vlc_ml_bookmark_t vlcBookmark = vlcBookmarks->p_items[i];
        VLCBookmark * const bookmark = [VLCBookmark bookmarkWithVlcBookmark:vlcBookmark];
        [tempBookmarks addObject:bookmark];
    }

    _bookmarks = [tempBookmarks copy];
    vlc_ml_bookmark_list_release(vlcBookmarks);
}

- (void)currentMediaItemChanged:(NSNotification * const)notification
{
    [self updateLibraryItemId];
}

- (void)setLibraryItemId:(const int64_t)libraryItemId
{
    if (libraryItemId == _libraryItemId) {
        return;
    }

    _libraryItemId = libraryItemId;
    [self updateBookmarks];
}

- (NSInteger)numberOfRowsInTableView:(NSTableView *)tableView
{
    if (_bookmarks == nil) {
        return 0;
    }

    return _bookmarks.count;
}

- (VLCBookmark *)bookmarkForRow:(NSInteger)row
{
    NSParameterAssert(row >= 0 || row < _bookmarks.count);
    return [_bookmarks objectAtIndex:row];
}

- (id)tableView:(NSTableView *)tableView objectValueForTableColumn:(NSTableColumn *)tableColumn row:(NSInteger)row
{
    if (_bookmarks == nil || _bookmarks.count == 0) {
        return @"";
    }

    VLCBookmark * const bookmark = [self bookmarkForRow:row];
    NSAssert(bookmark != nil, @"Should be a valid bookmark");

    NSString * const identifier = [tableColumn identifier];

    if ([identifier isEqualToString:@"name"]) {
        return bookmark.bookmarkName;
    } else if ([identifier isEqualToString:@"description"]) {
        return bookmark.bookmarkDescription;
    } else if ([identifier isEqualToString:@"time_offset"]) {
        return [NSString stringWithTime:bookmark.bookmarkTime / 1000];
    }

    return @"";
}

- (void)addBookmark
{
    if (_libraryItemId <= 0) {
        return;
    }

    const vlc_tick_t currentTime = _playerController.time;
    const int64_t bookmarkTime = MS_FROM_VLC_TICK(currentTime);
    vlc_ml_media_add_bookmark(_mediaLibrary, _libraryItemId, bookmarkTime);
    vlc_ml_media_update_bookmark(_mediaLibrary,
                                 _libraryItemId,
                                 bookmarkTime,
                                 [_NS("New bookmark") UTF8String],
                                 [_NS("Description of new bookmark.") UTF8String]);

    [self updateBookmarks];
}

- (void)removeBookmarkWithTime:(const int64_t)bookmarkTime
{
    if (_libraryItemId <= 0) {
        return;
    }

    vlc_ml_media_remove_bookmark(_mediaLibrary, _libraryItemId, bookmarkTime);
    [self updateBookmarks];
}

- (void)removeBookmark:(VLCBookmark *)bookmark
{
    [self removeBookmarkWithTime:bookmark.bookmarkTime];
}

- (void)clearBookmarks
{
    if (_libraryItemId <= 0) {
        return;
    }

    vlc_ml_media_remove_all_bookmarks(_mediaLibrary, _libraryItemId);
    [self updateBookmarks];
}

@end
