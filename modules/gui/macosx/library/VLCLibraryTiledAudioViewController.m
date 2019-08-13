/*****************************************************************************
 * VLCLibraryTiledAudioViewController.m: MacOS X interface module
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

#import "VLCLibraryTiledAudioViewController.h"

#import "library/VLCLibraryModel.h"
#import "library/VLCLibraryCollectionViewItem.h"
#import "library/VLCLibraryDataTypes.h"

#import "extensions/NSString+Helpers.h"
#import "views/VLCImageView.h"

@interface VLCLibraryTiledAudioViewController () <NSCollectionViewDelegate, NSCollectionViewDataSource>
{
    NSArray *_displayedCollection;
    enum vlc_ml_parent_type _currentParentType;
}
@end

@implementation VLCLibraryTiledAudioViewController

- (void)setupAppearance
{
    NSArray *availableCollections = [VLCLibraryModel availableAudioCollections];
    NSUInteger availableCollectionsCount = availableCollections.count;
    self.segmentedControl.segmentCount = availableCollectionsCount;
    for (NSUInteger x = 0; x < availableCollectionsCount; x++) {
        [self.segmentedControl setLabel:availableCollections[x] forSegment:x];
    }

    _collectionView.dataSource = self;
    _collectionView.delegate = self;
    [_collectionView registerClass:[VLCLibraryCollectionViewItem class] forItemWithIdentifier:VLCLibraryCellIdentifier];
    NSCollectionViewFlowLayout *flowLayout = _collectionView.collectionViewLayout;
    flowLayout.itemSize = CGSizeMake(214., 260.);
    flowLayout.sectionInset = NSEdgeInsetsMake(20., 20., 20., 20.);
    flowLayout.minimumLineSpacing = 20.;
    flowLayout.minimumInteritemSpacing = 20.;

    [self.segmentedControl setTarget:self];
    [self.segmentedControl setAction:@selector(segmentedControlAction:)];
    [self segmentedControlAction:nil];
}

- (void)reloadAppearance
{
    [self.segmentedControl setTarget:self];
    [self.segmentedControl setAction:@selector(segmentedControlAction:)];
    [self segmentedControlAction:nil];

    [self.collectionView reloadData];
}

- (IBAction)segmentedControlAction:(id)sender
{
    // FIXME: this relies on knowledge internal to VLCLibraryModel
    switch (_segmentedControl.selectedSegment) {
        case 0:
            _displayedCollection = [self.libraryModel listOfArtists];
            _currentParentType = VLC_ML_PARENT_ARTIST;
            break;
        case 1:
            _displayedCollection = [self.libraryModel listOfAlbums];
            _currentParentType = VLC_ML_PARENT_ALBUM;
            break;
        case 2:
            _displayedCollection = [self.libraryModel listOfAudioMedia];
            _currentParentType = VLC_ML_PARENT_UNKNOWN;
            break;
        case 3:
            _displayedCollection = [self.libraryModel listOfGenres];
            _currentParentType = VLC_ML_PARENT_GENRE;
            break;

        default:
            NSAssert(1, @"reached the unreachable");
            break;
    }
    [self.collectionView reloadData];
}

- (NSInteger)collectionView:(NSCollectionView *)collectionView
     numberOfItemsInSection:(NSInteger)section
{
    return _displayedCollection.count;
}

- (NSInteger)numberOfSectionsInCollectionView:(NSCollectionView *)collectionView
{
    return 1;
}

- (NSCollectionViewItem *)collectionView:(NSCollectionView *)collectionView
     itemForRepresentedObjectAtIndexPath:(NSIndexPath *)indexPath
{
    VLCLibraryCollectionViewItem *viewItem = [collectionView makeItemWithIdentifier:VLCLibraryCellIdentifier forIndexPath:indexPath];
    switch (_currentParentType) {
        case VLC_ML_PARENT_ARTIST:
        {
            VLCMediaLibraryArtist *artist = _displayedCollection[indexPath.item];
            viewItem.mediaTitleTextField.stringValue = artist.name;
            viewItem.durationTextField.stringValue = [NSString stringWithFormat:_NS("%u albums, %u songs"), artist.numberOfAlbums, artist.numberOfTracks];
            NSImage *image;
            if (artist.artworkMRL.length > 0) {
                image = [[NSImage alloc] initWithContentsOfURL:[NSURL URLWithString:artist.artworkMRL]];
            }
            if (!image) {
                image = [NSImage imageNamed: @"noart.png"];
            }
            viewItem.mediaImageView.image = image;
            break;
        }
        case VLC_ML_PARENT_ALBUM:
        {
            VLCMediaLibraryAlbum *album = _displayedCollection[indexPath.item];
            viewItem.mediaTitleTextField.stringValue = album.title;
            viewItem.durationTextField.stringValue = [NSString stringWithFormat:_NS("%u songs"), album.numberOfTracks];
            NSImage *image;
            if (album.artworkMRL.length > 0) {
                image = [[NSImage alloc] initWithContentsOfURL:[NSURL URLWithString:album.artworkMRL]];
            }
            if (!image) {
                image = [NSImage imageNamed: @"noart.png"];
            }
            viewItem.mediaImageView.image = image;
            break;
        }
        case VLC_ML_PARENT_UNKNOWN:
        {
            VLCMediaLibraryMediaItem *mediaItem = _displayedCollection[indexPath.item];
            viewItem.representedMediaItem = mediaItem;
            break;
        }
        case VLC_ML_PARENT_GENRE:
        {
            VLCMediaLibraryGenre *genre = _displayedCollection[indexPath.item];
            viewItem.mediaTitleTextField.stringValue = genre.name;
            viewItem.durationTextField.stringValue = [NSString stringWithFormat:_NS("%u items"), genre.numberOfTracks];
            viewItem.mediaImageView.image = [NSImage imageNamed: @"noart.png"];
        }

        default:
            break;
    }

    return viewItem;
}

- (void)collectionView:(NSCollectionView *)collectionView didSelectItemsAtIndexPaths:(NSSet<NSIndexPath *> *)indexPaths
{
    NSLog(@"library selection changed: %@", indexPaths);
}


@end
