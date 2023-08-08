/*****************************************************************************
 * VLCLibraryPlaylistDataSource.m: MacOS X interface module
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

#import "VLCLibraryPlaylistDataSource.h"

#import "library/VLCLibraryController.h"
#import "library/VLCLibraryModel.h"

#import "main/VLCMain.h"

@interface VLCLibraryPlaylistDataSource ()

@property (readwrite, atomic) NSArray<VLCMediaLibraryPlaylist *> *playlists;

@end

@implementation VLCLibraryPlaylistDataSource

- (instancetype)init
{
    self = [super init];
    if (self) {
        [self setup];
    }
    return self;
}

- (void)setup
{
    _libraryModel = VLCMain.sharedInstance.libraryController.libraryModel;
    [self reloadPlaylists];
}

- (void)reloadPlaylists
{
    self.playlists = _libraryModel.listOfPlaylists;
}

@end
