/*****************************************************************************
 * VLCLibrarySongsTableView.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2022 VLC authors and VideoLAN
 *
 * Authors: Claudio Cambra <claudio.cambra@gmail.com>
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

#import "VLCLibrarySongsTableView.h"

#import "extensions/NSString+Helpers.h"

@implementation VLCLibrarySongsTableView

- (instancetype)init
{
    self = [super init];

    if (self) {
        [self setupColumns];
    }

    return self;
}

- (void)setupColumns
{
    _titleColumn = [self createAndAddNewColumnWithIdentifier:@"VLCLibrarySongsTableViewTitleColumnIdentifier"
                                                   withTitle:_NS("Title")];
    _durationColumn = [self createAndAddNewColumnWithIdentifier:@"VLCLibrarySongsTableViewDurationColumnIdentifier"
                                                      withTitle:_NS("Duration")];
    _artistColumn = [self createAndAddNewColumnWithIdentifier:@"VLCLibrarySongsTableViewArtistColumnIdentifier"
                                                    withTitle:_NS("Artist")];
    _albumColumn = [self createAndAddNewColumnWithIdentifier:@"VLCLibrarySongsTableViewAlbumColumnIdentifier"
                                                   withTitle:_NS("Album")];
    _genreColumn = [self createAndAddNewColumnWithIdentifier:@"VLCLibrarySongsTableViewGenreColumnIdentifier"
                                                   withTitle:_NS("Genre")];
    _playCountColumn = [self createAndAddNewColumnWithIdentifier:@"VLCLibrarySongsTableViewPlayCountColumnIdentifier"
                                                       withTitle:_NS("Play count")];
    _insertionDateColumn = [self createAndAddNewColumnWithIdentifier:@"VLCLibrarySongsTableViewInsertionDateColumnIdentifier"
                                                           withTitle:_NS("Insertion date")];
    _lastModificationDateColumn = [self createAndAddNewColumnWithIdentifier:@"VLCLibrarySongsTableViewLastModificationDateColumnIdentifier"
                                                                  withTitle:_NS("Last modification date")];
    _fileNameColumn = [self createAndAddNewColumnWithIdentifier:@"VLCLibrarySongsTableViewFileNameColumnIdentifier"
                                                      withTitle:_NS("File name")];
    _fileSizeColumn = [self createAndAddNewColumnWithIdentifier:@"VLCLibrarySongsTableViewFileSizeColumnIdentifier"
                                                      withTitle:_NS("File size")];
}

- (NSTableColumn *)createAndAddNewColumnWithIdentifier:(NSString *)identifier withTitle:(NSString *)title
{
    NSTableColumn *newColumn = [[NSTableColumn alloc] initWithIdentifier:identifier];
    newColumn.title = title;
    [self addTableColumn:newColumn];
    return newColumn;
}

@end
