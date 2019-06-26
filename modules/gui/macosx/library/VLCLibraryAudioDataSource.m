/*****************************************************************************
 * VLCLibraryAudioDataSource.m: MacOS X interface module
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

#import "VLCLibraryAudioDataSource.h"

#import "library/VLCLibraryModel.h"
#import "library/VLCLibraryDataTypes.h"
#import "library/VLCLibraryTableCellView.h"
#import "library/VLCLibraryAlbumTableCellView.h"

#import "extensions/NSString+Helpers.h"
#import "views/VLCImageView.h"

static NSString *VLCAudioLibraryCellIdentifier = @"VLCAudioLibraryCellIdentifier";

@interface VLCLibraryAudioDataSource()
{
    NSArray *_availableCollectionsArray;
}
@end

@implementation VLCLibraryAudioDataSource

- (instancetype)init
{
    self = [super init];
    if (self) {
        _availableCollectionsArray = [VLCLibraryModel availableAudioCollections];
    }
    return self;
}

- (NSInteger)numberOfRowsInTableView:(NSTableView *)tableView
{
    if (tableView == self.categorySelectionTableView) {
        return _availableCollectionsArray.count;
    }

    NSInteger ret = 0;

    switch (self.categorySelectionTableView.selectedRow) {
        case 0: // artists
            ret = _libraryModel.numberOfArtists;
            break;

        case 1: // albums
            ret = _libraryModel.numberOfAlbums;
            break;

        case 2: // songs
            ret = _libraryModel.numberOfAudioMedia;
            break;

        case 3: // genres
            ret = _libraryModel.numberOfGenres;
            break;

        default:
            break;
    }

    return ret;
}

- (NSView *)tableView:(NSTableView *)tableView viewForTableColumn:(NSTableColumn *)tableColumn row:(NSInteger)row
{
    VLCLibraryTableCellView *cellView = [tableView makeViewWithIdentifier:VLCAudioLibraryCellIdentifier owner:self];

    if (cellView == nil) {
        /* the following code saves us an instance of NSViewController which we don't need */
        NSNib *nib = [[NSNib alloc] initWithNibNamed:@"VLCLibraryTableCellView" bundle:nil];
        NSArray *topLevelObjects;
        if (![nib instantiateWithOwner:self topLevelObjects:&topLevelObjects]) {
            NSAssert(1, @"Failed to load nib file to show audio library items");
            return nil;
        }

        for (id topLevelObject in topLevelObjects) {
            if ([topLevelObject isKindOfClass:[VLCLibraryTableCellView class]]) {
                cellView = topLevelObject;
                break;
            }
        }
        cellView.identifier = VLCAudioLibraryCellIdentifier;
    }

    if (tableView == self.categorySelectionTableView) {
        cellView.singlePrimaryTitleTextField.hidden = NO;
        cellView.singlePrimaryTitleTextField.stringValue = _availableCollectionsArray[row];
        NSImage *image = [NSImage imageNamed:NSImageNameApplicationIcon];
        cellView.representedImageView.image = image;
        return cellView;
    }

    switch (self.categorySelectionTableView.selectedRow) {
        case 0: // artists
        {
            NSArray *listOfArtists = [_libraryModel listOfArtists];
            VLCMediaLibraryArtist *artist = listOfArtists[row];

            cellView.singlePrimaryTitleTextField.hidden = NO;
            cellView.singlePrimaryTitleTextField.stringValue = artist.name;

            NSImage *image;
            if (artist.artworkMRL.length > 0) {
                image = [[NSImage alloc] initWithContentsOfURL:[NSURL URLWithString:artist.artworkMRL]];
            }
            if (!image) {
                image = [NSImage imageNamed: @"noart.png"];
            }
            cellView.representedImageView.image = image;
            break;
        }
        case 1: // albums
        {
            NSArray *listOfAlbums = [_libraryModel listOfAlbums];
            VLCMediaLibraryAlbum *album = listOfAlbums[row];

            cellView.primaryTitleTextField.hidden = NO;
            cellView.secondaryTitleTextField.hidden = NO;
            cellView.primaryTitleTextField.stringValue = album.title;
            cellView.secondaryTitleTextField.stringValue = album.artistName;

            NSImage *image;
            if (album.artworkMRL.length > 0) {
                image = [[NSImage alloc] initWithContentsOfURL:[NSURL URLWithString:album.artworkMRL]];
            }
            if (!image) {
                image = [NSImage imageNamed: @"noart.png"];
            }
            cellView.representedImageView.image = image;
            break;
        }
        case 2: // songs
        {
            NSArray *listOfAudioMedia = [_libraryModel listOfAudioMedia];
            VLCMediaLibraryMediaItem *mediaItem = listOfAudioMedia[row];

            NSImage *image;
            if (mediaItem.smallArtworkGenerated) {
                if (mediaItem.smallArtworkMRL.length > 0) {
                    image = [[NSImage alloc] initWithContentsOfURL:[NSURL URLWithString:mediaItem.smallArtworkMRL]];
                }
            }
            if (!image) {
                image = [NSImage imageNamed: @"noart.png"];
            }
            cellView.representedImageView.image = image;
            cellView.representedMediaItem = mediaItem;

            NSString *title = mediaItem.title;
            NSString *nameOfArtist;

            VLCMediaLibraryAlbumTrack *albumTrack = mediaItem.albumTrack;
            if (albumTrack) {
                VLCMediaLibraryArtist *artist = [VLCMediaLibraryArtist artistWithID:albumTrack.artistID];
                if (artist) {
                    nameOfArtist = artist.name;
                }
            }

            if (title && nameOfArtist) {
                cellView.primaryTitleTextField.hidden = NO;
                cellView.secondaryTitleTextField.hidden = NO;
                cellView.primaryTitleTextField.stringValue = title;
                cellView.secondaryTitleTextField.stringValue = nameOfArtist;
            } else {
                cellView.singlePrimaryTitleTextField.hidden = NO;
                cellView.singlePrimaryTitleTextField.stringValue = title;
            }
            break;
        }
        case 3: // genres
        {
            NSArray *listOfGenres = [_libraryModel listOfGenres];
            VLCMediaLibraryGenre *genre = listOfGenres[row];

            cellView.primaryTitleTextField.hidden = NO;
            cellView.secondaryTitleTextField.hidden = NO;
            cellView.primaryTitleTextField.stringValue = genre.name;
            cellView.secondaryTitleTextField.stringValue = [NSString stringWithFormat:_NS("%lli items"), genre.numberOfTracks];

            NSImage *image = [NSImage imageNamed: @"noart.png"];
            cellView.representedImageView.image = image;
            break;
        }
        default:
            break;
    }

    return cellView;
}

- (void)tableViewSelectionDidChange:(NSNotification *)notification
{
    if (notification.object == self.categorySelectionTableView) {
        [self.collectionSelectionTableView reloadData];
        return;
    }

    switch (self.categorySelectionTableView.selectedRow) {
        case 0: // artists
        {
            NSArray *listOfArtists = [_libraryModel listOfArtists];
            VLCMediaLibraryArtist *artist = listOfArtists[self.collectionSelectionTableView.selectedRow];
            NSArray *albumsForArtist = [_libraryModel listAlbumsOfParentType:VLC_ML_PARENT_ARTIST forID:artist.artistID];
            _groupDataSource.representedListOfAlbums = albumsForArtist;
            break;
        }
        case 1: // albums
        {
            NSArray *listOfAlbums = [_libraryModel listOfAlbums];
            VLCMediaLibraryAlbum *album = listOfAlbums[self.collectionSelectionTableView.selectedRow];
            _groupDataSource.representedListOfAlbums = @[album];
            break;
        }
        case 2: // songs
        {
            // FIXME: we have nothing to show here
            _groupDataSource.representedListOfAlbums = nil;
            break;
        }
        case 3: // genres
        {
            NSArray *listOfGenres = [_libraryModel listOfGenres];
            VLCMediaLibraryGenre *genre = listOfGenres[self.collectionSelectionTableView.selectedRow];
            NSArray *albumsForGenre = [_libraryModel listAlbumsOfParentType:VLC_ML_PARENT_GENRE forID:genre.genreID];
            _groupDataSource.representedListOfAlbums = albumsForGenre;
            break;
        }
        default:
            break;
    }

    [self.groupSelectionTableView reloadData];
}

@end

@implementation VLCLibraryGroupDataSource

- (NSInteger)numberOfRowsInTableView:(NSTableView *)tableView
{
    if (_representedListOfAlbums != nil) {
        return _representedListOfAlbums.count;
    }

    return 0;
}

- (NSView *)tableView:(NSTableView *)tableView viewForTableColumn:(NSTableColumn *)tableColumn row:(NSInteger)row
{
    VLCLibraryAlbumTableCellView *cellView = [tableView makeViewWithIdentifier:VLCAudioLibraryCellIdentifier owner:self];

    if (cellView == nil) {
        /* the following code saves us an instance of NSViewController which we don't need */
        NSNib *nib = [[NSNib alloc] initWithNibNamed:@"VLCLibraryAlbumTableCellView" bundle:nil];
        NSArray *topLevelObjects;
        if (![nib instantiateWithOwner:self topLevelObjects:&topLevelObjects]) {
            NSAssert(1, @"Failed to load nib file to show audio library items");
            return nil;
        }

        for (id topLevelObject in topLevelObjects) {
            if ([topLevelObject isKindOfClass:[VLCLibraryAlbumTableCellView class]]) {
                cellView = topLevelObject;
                break;
            }
        }
        cellView.identifier = VLCAudioLibraryCellIdentifier;
    }

    VLCMediaLibraryAlbum *album = _representedListOfAlbums[row];
    cellView.representedAlbum = album;

    return cellView;
}

@end
