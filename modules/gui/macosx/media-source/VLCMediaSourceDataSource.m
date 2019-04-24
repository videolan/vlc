/*****************************************************************************
 * VLCMediaSourceDataSource.m: MacOS X interface module
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

#import "VLCMediaSourceDataSource.h"

#import "media-source/VLCMediaSourceProvider.h"
#import "media-source/VLCMediaSource.h"
#import "media-source/VLCMediaSourceCollectionViewItem.h"

#import "main/VLCMain.h"
#import "library/VLCInputItem.h"
#import "extensions/NSString+Helpers.h"

@interface VLCMediaSourceDataSource ()
{
    NSArray *_mediaDiscovery;
}
@end

@implementation VLCMediaSourceDataSource

- (instancetype)init
{
    self = [super init];
    if (self) {
        dispatch_async(dispatch_get_main_queue(), ^{
            [self lazyLoadMediaSources];
        });
    }
    return self;
}

- (void)lazyLoadMediaSources
{
    NSArray *mediaDiscoveryForLAN = [VLCMediaSourceProvider listOfMediaSourcesForCategory:SD_CAT_LAN];
    NSUInteger count = mediaDiscoveryForLAN.count;
    if (count > 0) {
        for (NSUInteger x = 0; x < count; x++) {
            VLCMediaSource *mediaSource = mediaDiscoveryForLAN[x];
            VLCInputNode *rootNode = [mediaSource rootNode];
            [mediaSource preparseInputItemWithinTree:rootNode.inputItem];
        }
    }
    _mediaDiscovery = mediaDiscoveryForLAN;
}

- (NSInteger)collectionView:(NSCollectionView *)collectionView
     numberOfItemsInSection:(NSInteger)section
{
    switch (self.mediaSourceMode) {
        case VLCMediaSourceModeLAN:
            return _mediaDiscovery.count;
            break;

        case VLCMediaSourceModeInternet:
        default:
            return 0;
            break;
    }
}

- (NSCollectionViewItem *)collectionView:(NSCollectionView *)collectionView
     itemForRepresentedObjectAtIndexPath:(NSIndexPath *)indexPath
{
    VLCMediaSourceCollectionViewItem *viewItem = [collectionView makeItemWithIdentifier:VLCMediaSourceCellIdentifier forIndexPath:indexPath];

    NSArray *mediaArray;
    switch (self.mediaSourceMode) {
        case VLCMediaSourceModeLAN:
            mediaArray = _mediaDiscovery;
            break;

        case VLCMediaSourceModeInternet:
        default:
            NSAssert(1, @"no representation for selected media source mode %li", (long)self.mediaSourceMode);
            mediaArray = @[];
            break;
    }

    VLCMediaSource *mediaSource = mediaArray[indexPath.item];
    viewItem.titleTextField.stringValue = mediaSource.mediaSourceDescription;

    return viewItem;
}

- (void)collectionView:(NSCollectionView *)collectionView didSelectItemsAtIndexPaths:(NSSet<NSIndexPath *> *)indexPaths
{
    NSLog(@"media source selection changed: %@", indexPaths);
}

@end
