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

#import "library/VLCInputItem.h"
#import "library/VLCLibraryController.h"
#import "library/VLCLibraryModel.h"

#import "main/VLCMain.h"

#import <vlc_media_library.h>

@interface VLCBookmarksTableViewDataSource ()
{
    vlc_ml_bookmark_list_t *_bookmarks;
    vlc_medialibrary_t *_mediaLibrary;
}
@end

@implementation VLCBookmarksTableViewDataSource

- (instancetype)init
{
    self = [super init];
    if (self) {
        _mediaLibrary = vlc_ml_instance_get(getIntf());
    }
    return self;
}

- (void)setLibraryItemId:(const int64_t)libraryItemId
{
    if (libraryItemId == _libraryItemId) {
        return;
    }

    _libraryItemId = libraryItemId;
    _bookmarks = vlc_ml_list_media_bookmarks(_mediaLibrary, nil, libraryItemId);
}

- (NSInteger)numberOfRowsInTableView:(NSTableView *)tableView
{
    if (_bookmarks == NULL) {
        return 0;
    }

    return _bookmarks->i_nb_items;
}

- (VLCBookmark *)bookmarkForRow:(NSInteger)row
{
    NSParameterAssert(row >= 0 || row < _bookmarks->i_nb_items);
    vlc_ml_bookmark_t bookmark = _bookmarks->p_items[row];
    return [[VLCBookmark alloc] initWithVlcBookmark:bookmark];
}

@end
