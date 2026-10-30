/*****************************************************************************
 * VLCLibraryHomeViewVideoCarouselContainerView.m: MacOS X interface module
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

#import "VLCLibraryHomeViewVideoCarouselContainerView.h"

#import "extensions/NSFont+VLCAdditions.h"
#import "extensions/NSString+Helpers.h"
#import "extensions/NSView+VLCAdditions.h"

#import "library/VLCLibraryCollectionView.h"
#import "library/VLCLibraryCollectionViewDelegate.h"
#import "library/VLCLibraryCarouselCollectionViewItem.h"
#import "library/VLCLibraryDataTypes.h"
#import "library/VLCLibraryHeroView.h"
#import "library/VLCLibraryRepresentedItem.h"
#import "library/VLCLibraryCollectionViewFlowLayout.h"
#import "library/VLCLibraryWindow.h"
#import "library/VLCLibraryWindowSplitViewController.h"
#import "library/VLCLibrarySegment.h"
#import "library/VLCLibraryWindowNavigationSidebarViewController.h"

#import "library/home-library/VLCLibraryHomeViewMediaContainerViewDataSource.h"

#import "views/VLCUIUnits.h"
#import "main/VLCMain.h"

#include "library/VLCLibraryMediaGroupDescriptor.h"

static NSUserInterfaceItemIdentifier const VLCLibraryHomeVideoHeroIdentifier = @"VLCLibraryHomeVideoHero";
static void * const VLCLibraryHomeVideoHeroContext = (void *)&VLCLibraryHomeVideoHeroContext;

@interface VLCLibraryHomeVideoCarouselDataSource : VLCLibraryHomeViewMediaContainerViewDataSource

@property (strong, nullable) VLCLibraryHeroView *heroView;

@end

@implementation VLCLibraryHomeVideoCarouselDataSource

- (void)setup
{
    [super setup];
    [self.collectionView registerClass:NSView.class
            forSupplementaryViewOfKind:NSCollectionElementKindSectionHeader
                        withIdentifier:VLCLibraryHomeVideoHeroIdentifier];
}

- (NSView *)
                       collectionView:(NSCollectionView *)collectionView
    viewForSupplementaryElementOfKind:(NSCollectionViewSupplementaryElementKind)kind
                          atIndexPath:(NSIndexPath *)indexPath
{
    if (![kind isEqualToString:NSCollectionElementKindSectionHeader]) {
        return [super collectionView:collectionView viewForSupplementaryElementOfKind:kind atIndexPath:indexPath];
    }

    NSView * const header = [collectionView makeSupplementaryViewOfKind:kind
                                                         withIdentifier:VLCLibraryHomeVideoHeroIdentifier
                                                           forIndexPath:indexPath];
    if (self.heroView != nil && self.heroView.superview != header) {
        [header addSubview:self.heroView];
        self.heroView.translatesAutoresizingMaskIntoConstraints = NO;
        [NSLayoutConstraint activateConstraints:@[
            [self.heroView.leadingAnchor constraintEqualToAnchor:header.leadingAnchor constant:VLCUIUnits.mediumSpacing],
            [self.heroView.trailingAnchor constraintEqualToAnchor:header.trailingAnchor],
            [self.heroView.topAnchor constraintEqualToAnchor:header.topAnchor],
            [self.heroView.bottomAnchor constraintEqualToAnchor:header.bottomAnchor],
        ]];
    }
    return header;
}

@end

@implementation VLCLibraryHomeViewVideoCarouselContainerView
{
    BOOL _updatingHeroLayout;
}

@synthesize groupDescriptor = _groupDescriptor;
@synthesize videoGroup = _videoGroup;
@synthesize dataSource = _dataSource;

- (void)dealloc
{
    [self.heroView removeObserver:self forKeyPath:@"representedItem" context:VLCLibraryHomeVideoHeroContext];
}

- (NSNotificationName)displayedCollectionChangedNotification
{
    return VLCLibraryMediaCollectionViewDataSourceDisplayedCollectionChangedNotification;
}

- (void)setupDataSource
{
    self.collectionViewDelegate.itemsAspectRatio = VLCLibraryCollectionViewItemAspectRatioVideoItem;
    self.collectionViewDelegate.staticItemSize = NSMakeSize(VLCUIUnits.carouselViewVideoItemViewWidth,
                                                            VLCUIUnits.carouselViewItemViewHeight);
    self.itemHeight = VLCUIUnits.carouselViewItemViewHeight;
    // This nib has no File's Owner connections. Passing the carousel as its
    // owner would reenter setup through awakeFromNib before the data source exists.
    _heroView = [VLCLibraryHeroView fromNibWithOwner:nil];
    self.heroView.carouselView = self.collectionView;
    self.heroView.selectionMode = VLCLibraryHeroViewSelectionModeMostRecentVideo;
    [self.heroView addObserver:self forKeyPath:@"representedItem" options:0 context:VLCLibraryHomeVideoHeroContext];

    _dataSource = [[VLCLibraryHomeVideoCarouselDataSource alloc] init];
    _dataSource.collectionViewItemIdentifier = VLCLibraryCarouselCollectionViewItemIdentifier;
    self.dataSource.collectionView = self.collectionView;
    [self.dataSource setup];
}

- (void)setGroupDescriptor:(VLCLibraryMediaCollectionViewGroupDescriptor *)groupDescriptor
{
    _groupDescriptor = groupDescriptor;
    _videoGroup = groupDescriptor.group;
    self.dataSource.groupDescriptor = groupDescriptor;
    if (groupDescriptor.group == VLCMediaLibraryParentGroupTypeRecentVideos) {
        self.headerView.title = _NS("Recently watched");
        self.numberOfRows = 2;
    } else {
        self.headerView.title = groupDescriptor.name;
        self.numberOfRows = 1;
    }
    [self updateHeroLayout];
}

- (void)updateHeroLayout
{
    if (_updatingHeroLayout) {
        return;
    }
    _updatingHeroLayout = YES;
    const BOOL recentVideos = self.groupDescriptor != nil &&
        self.videoGroup == VLCMediaLibraryParentGroupTypeRecentVideos;
    const BOOL hasRecentVideos = recentVideos && self.dataSource.numberOfMediaItems > 0;
    // Select and exclude from the same snapshot, including while a newer model
    // list is still loading, so the featured video cannot also become a card.
    if (hasRecentVideos && self.heroView.representedItem.item != self.dataSource.firstMediaItem) {
        self.heroView.representedItem = [[VLCLibraryRepresentedItem alloc] initWithItem:self.dataSource.firstMediaItem
                                                                             parentType:VLCMediaLibraryParentGroupTypeRecentVideos];
    } else if (!hasRecentVideos && self.heroView.representedItem != nil) {
        self.heroView.representedItem = nil;
    }
    const BOOL showsHero = hasRecentVideos && self.heroView.representedItem != nil;
    self.dataSource.excludedLibraryID = showsHero
        ? self.heroView.representedItem.item.libraryID
        : 0;
    VLCLibraryHomeVideoCarouselDataSource * const dataSource = (id)self.dataSource;
    dataSource.heroView = showsHero
        ? self.heroView
        : nil;

    // Match two video columns, including their gap. The section header itself
    // starts before the flow layout's leading inset, so include that padding.
    const CGFloat heroWidth = self.collectionViewDelegate.staticItemSize.width * 2. +
        self.collectionViewLayout.minimumLineSpacing;
    const CGFloat height = self.itemHeight * self.numberOfRows +
        (self.numberOfRows - 1) * self.collectionViewLayout.minimumInteritemSpacing;
    const NSSize headerSize = showsHero
        ? NSMakeSize(heroWidth + VLCUIUnits.mediumSpacing, height)
        : NSZeroSize;
    if (!NSEqualSizes(self.collectionViewLayout.headerReferenceSize, headerSize)) {
        self.collectionViewLayout.headerReferenceSize = headerSize;
        [self.collectionViewLayout invalidateLayout];
    }
    self.hidden = recentVideos && !hasRecentVideos;
    _updatingHeroLayout = NO;
}

- (void)observeValueForKeyPath:(NSString *)keyPath
                      ofObject:(id)object
                        change:(NSDictionary<NSKeyValueChangeKey, id> *)change
                       context:(void *)context
{
    if (context == VLCLibraryHomeVideoHeroContext) {
        [self updateHeroLayout];
        return;
    }
    [super observeValueForKeyPath:keyPath ofObject:object change:change context:context];
}

- (void)displayedCollectionChanged:(NSNotification *)notification
{
    [super displayedCollectionChanged:notification];
    if (self.videoGroup == VLCMediaLibraryParentGroupTypeRecentVideos) {
        self.headerView.itemCount = self.dataSource.numberOfMediaItems;
    }
    [self updateHeroLayout];
}

- (void)reloadData
{
    [self.heroView setOptimalRepresentedItem];
    [super reloadData];
}

- (void)openLibrary:(id)sender
{
    [VLCMain.sharedInstance.libraryWindow.splitViewController.navSidebarViewController
        selectSegment:VLCLibraryVideoSegmentType];
}

- (void)setVideoGroup:(VLCMediaLibraryParentGroupType)group
{
    if (_groupDescriptor.group == group) {
        return;
    }

    VLCLibraryMediaCollectionViewGroupDescriptor * const descriptor = [[VLCLibraryMediaCollectionViewGroupDescriptor alloc] initWithLibraryGroup:group];
    [self setGroupDescriptor:descriptor];
}

@end
