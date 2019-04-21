/*****************************************************************************
 * VLCLibraryDataSource.m: MacOS X interface module
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

#import "VLCLibraryDataSource.h"

#import "library/VLCLibraryCollectionViewItem.h"
#import "library/VLCLibraryModel.h"
#import "library/VLCLibraryDataTypes.h"

#import "extensions/NSString+Helpers.h"

#import "views/VLCImageView.h"

@implementation VLCLibraryDataSource

- (NSInteger)collectionView:(NSCollectionView *)collectionView
     numberOfItemsInSection:(NSInteger)section
{
    switch (_libraryModel.libraryMode) {
        case VLCLibraryModeAudio:
            return [_libraryModel numberOfAudioMedia];
            break;

        case VLCLibraryModeVideo:
            return [_libraryModel numberOfVideoMedia];

        default:
            return 0;
            break;
    }
}

- (NSCollectionViewItem *)collectionView:(NSCollectionView *)collectionView
     itemForRepresentedObjectAtIndexPath:(NSIndexPath *)indexPath
{
    VLCLibraryCollectionViewItem *viewItem = [collectionView makeItemWithIdentifier:VLCLibraryCellIdentifier forIndexPath:indexPath];

    NSArray *mediaArray;
    switch (_libraryModel.libraryMode) {
        case VLCLibraryModeAudio:
            mediaArray = [_libraryModel listOfAudioMedia];
            break;

        case VLCLibraryModeVideo:
            mediaArray = [_libraryModel listOfVideoMedia];
            break;

        default:
            NSAssert(1, @"no representation for selected library mode");
            mediaArray = @[];
            break;
    }

    VLCMediaLibraryMediaItem *mediaItem = mediaArray[indexPath.item];

    viewItem.mediaTitleTextField.stringValue = mediaItem.title;
    viewItem.durationTextField.stringValue = [NSString stringWithTime:mediaItem.duration / 1000];

    NSImage *image;
    if (mediaItem.artworkGenerated) {
        image = [[NSImage alloc] initWithContentsOfURL:[NSURL URLWithString:mediaItem.artworkMRL]];
    }
    if (!image) {
        image = [NSImage imageNamed: @"noart.png"];
    }
    viewItem.mediaImageView.image = image;

    return viewItem;
}

- (void)collectionView:(NSCollectionView *)collectionView didSelectItemsAtIndexPaths:(NSSet<NSIndexPath *> *)indexPaths
{
    NSLog(@"library selection changed: %@", indexPaths);
}

@end
