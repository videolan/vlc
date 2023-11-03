/*****************************************************************************
 * VLCLibraryHeroView.m: MacOS X interface module
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

#import "VLCLibraryHeroView.h"

#import "extensions/NSString+Helpers.h"
#import "extensions/NSView+VLCAdditions.h"

#import "library/VLCLibraryController.h"
#import "library/VLCLibraryDataTypes.h"
#import "library/VLCLibraryImageCache.h"
#import "library/VLCLibraryModel.h"
#import "library/VLCLibraryRepresentedItem.h"

#import "main/VLCMain.h"

#import "views/VLCImageView.h"

@interface VLCLibraryHeroView ()

@property (readonly) VLCMediaLibraryMediaItem *randomItem;
@property (readonly) VLCMediaLibraryMediaItem *latestPartiallyPlayedItem;

@end

@implementation VLCLibraryHeroView

+ (instancetype)fromNibWithOwner:(id)owner
{
    return (VLCLibraryHeroView*)[NSView fromNibNamed:@"VLCLibraryHeroView"
                                                     withClass:VLCLibraryHeroView.class
                                                     withOwner:owner];
}

- (void)awakeFromNib
{
    self.largeImageView.contentGravity = VLCImageViewContentGravityResizeAspectFill;
}

- (void)updateRepresentedItem
{
    NSAssert(self.representedItem != nil, @"Should not update nil represented item!");
    const id<VLCMediaLibraryItemProtocol> actualItem = self.representedItem.item;
    self.largeImageView.image = [VLCLibraryImageCache thumbnailForLibraryItem:actualItem];
    self.titleTextField.stringValue = actualItem.displayString;
    self.detailTextField.stringValue = actualItem.detailString;
}

- (void)setRepresentedItem:(VLCLibraryRepresentedItem *)representedItem
{
    NSParameterAssert(representedItem != nil);
    if (representedItem == self.representedItem) {
        return;
    }

    _representedItem = representedItem;
    [self updateRepresentedItem];
}

- (VLCMediaLibraryMediaItem *)randomItem
{
    VLCLibraryModel * const libraryModel = VLCMain.sharedInstance.libraryController.libraryModel;
    const size_t videoCount = libraryModel.numberOfVideoMedia;
    const uint32_t randIdx = arc4random_uniform((uint32_t)(videoCount - 1));
    return [libraryModel.listOfVideoMedia objectAtIndex:randIdx];
}

- (VLCMediaLibraryMediaItem *)latestPartiallyPlayedItem
{
    VLCLibraryModel * const libraryModel = VLCMain.sharedInstance.libraryController.libraryModel;
    NSArray<VLCMediaLibraryMediaItem *> * const recentMedia = libraryModel.listOfRecentMedia;
    const NSUInteger firstPartialPlayItemIdx = [recentMedia indexOfObjectPassingTest:^BOOL(VLCMediaLibraryMediaItem *testedItem, NSUInteger idx, BOOL *stop) {
        const float playProgress = testedItem.progress;
        return playProgress > 0 && playProgress < 100;
    }];

    if (firstPartialPlayItemIdx == NSNotFound) {
        return nil;
    }

    return [recentMedia objectAtIndex:firstPartialPlayItemIdx];
}

- (void)setOptimalRepresentedItem
{
    VLCMediaLibraryMediaItem * const latestPartialPlayItem = self.latestPartiallyPlayedItem;
    if (latestPartialPlayItem != nil) {
        const BOOL isVideo = latestPartialPlayItem.mediaType == VLC_ML_MEDIA_TYPE_VIDEO;
        const VLCMediaLibraryParentGroupType parentType = isVideo ? VLCMediaLibraryParentGroupTypeRecentVideos : VLCMediaLibraryParentGroupTypeAudioLibrary;
        VLCLibraryRepresentedItem * const representedItem = [[VLCLibraryRepresentedItem alloc] initWithItem:latestPartialPlayItem parentType:parentType];

        self.representedItem = representedItem;
        self.explanationTextField.stringValue = _NS("Last watched");
        self.playButton.title = _NS("Resume playing");
        return;
    }

    VLCMediaLibraryMediaItem * const randomItem = self.randomItem;
    if (randomItem != nil) {
        const BOOL isVideo = randomItem.mediaType == VLC_ML_MEDIA_TYPE_VIDEO;
        const VLCMediaLibraryParentGroupType parentType = isVideo ? VLCMediaLibraryParentGroupTypeVideoLibrary : VLCMediaLibraryParentGroupTypeAudioLibrary;
        VLCLibraryRepresentedItem * const representedItem = [[VLCLibraryRepresentedItem alloc] initWithItem:randomItem parentType:VLCMediaLibraryParentGroupTypeUnknown];

        self.representedItem = representedItem;
        self.explanationTextField.stringValue = _NS("From your library");
        self.playButton.title = _NS("Play now");
        return;
    }

    NSLog(@"Could not find a food media item for hero view!");
}

- (IBAction)playRepresentedItem:(id)sender
{
    [self.representedItem play];
}

@end
