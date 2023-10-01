/*****************************************************************************
 * VLCLibraryVideoCollectionViewsStackViewController.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2022 VLC authors and VideoLAN
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

#import "VLCLibraryVideoCollectionViewsStackViewController.h"

#import "library/VLCLibraryCollectionViewDelegate.h"
#import "library/VLCLibraryCollectionViewFlowLayout.h"
#import "library/VLCLibraryCollectionViewSupplementaryElementView.h"
#import "library/VLCLibraryController.h"
#import "library/VLCLibraryHeroView.h"
#import "library/VLCLibraryModel.h"
#import "library/VLCLibraryUIUnits.h"

#import "library/video-library/VLCLibraryVideoCollectionViewContainerView.h"
#import "library/video-library/VLCLibraryVideoCollectionViewContainerViewDataSource.h"
#import "library/video-library/VLCLibraryVideoGroupDescriptor.h"
#import "library/video-library/VLCLibraryVideoViewContainerView.h"

#import "main/VLCMain.h"

#import "views/VLCSubScrollView.h"

@interface VLCLibraryVideoCollectionViewsStackViewController()
{
    NSArray<NSView<VLCLibraryVideoViewContainerView> *> *_containers;
    NSUInteger _leadingContainerCount;
}
@end

@implementation VLCLibraryVideoCollectionViewsStackViewController

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
    NSNotificationCenter * const notificationCenter = NSNotificationCenter.defaultCenter;
    [notificationCenter addObserver:self
                            selector:@selector(recentsChanged:)
                                name:VLCLibraryModelRecentsMediaListReset
                                object:nil];
    [notificationCenter addObserver:self
                            selector:@selector(recentsChanged:)
                                name:VLCLibraryModelRecentsMediaItemDeleted
                                object:nil];

    _leadingContainerCount = 0;
    [self generateCustomContainers];
    [self generateCollectionViewContainers];
}

- (void)generateCustomContainers
{
    _heroView = [VLCLibraryHeroView fromNibWithOwner:self];
    _leadingContainerCount += 1;
    [self addView:self.heroView toStackView:self.collectionsStackView];
    [self.heroView setOptimalRepresentedItem];
}

- (BOOL)recentMediaPresent
{
    VLCLibraryModel * const model = VLCMain.sharedInstance.libraryController.libraryModel;
    return model.numberOfRecentMedia > 0;
}

- (void)recentsChanged:(NSNotification *)notification
{
    const BOOL shouldShowRecentsContainer = [self recentMediaPresent];
    const NSUInteger recentsContainerIndex = [_containers indexOfObjectPassingTest:^BOOL(NSView<VLCLibraryVideoViewContainerView> * const container, const NSUInteger idx, BOOL * const stop) {
        return container.videoGroup == VLCMediaLibraryParentGroupTypeRecentVideos;
    }];
    const BOOL recentsContainerPresent = recentsContainerIndex != NSNotFound;

    if (recentsContainerPresent == shouldShowRecentsContainer) {
        return;
    }

    NSMutableArray<NSView<VLCLibraryVideoViewContainerView> *> * const mutableContainers = _containers.mutableCopy;

    if (shouldShowRecentsContainer) {
        VLCLibraryVideoCollectionViewContainerView * const containerView = [[VLCLibraryVideoCollectionViewContainerView alloc] init];
        containerView.videoGroup = VLCMediaLibraryParentGroupTypeRecentVideos;
        [mutableContainers insertObject:containerView atIndex:0];

        // Insert at top after leading containers, hence _leadingContainerCount
        [_collectionsStackView insertArrangedSubview:containerView atIndex:_leadingContainerCount];
        [self setupContainerView:containerView withStackView:_collectionsStackView];
    } else {
        [mutableContainers removeObjectAtIndex:recentsContainerIndex];
        NSView<VLCLibraryVideoViewContainerView> * const existingContainer = [_containers objectAtIndex:recentsContainerIndex];
        [_collectionsStackView removeConstraints:existingContainer.constraintsWithSuperview];
        [_collectionsStackView removeArrangedSubview:existingContainer];
    }

    _containers = mutableContainers.copy;
}

- (void)generateCollectionViewContainers
{
    NSMutableArray<NSView<VLCLibraryVideoViewContainerView> *> * const collectionViewContainers = [[NSMutableArray alloc] init];
    const BOOL anyRecents = [self recentMediaPresent];
    NSUInteger i = anyRecents ? VLCMediaLibraryParentGroupTypeRecentVideos : VLCMediaLibraryParentGroupTypeRecentVideos + 1;

    for (; i <= VLCMediaLibraryParentGroupTypeVideoLibrary; ++i) {
        VLCLibraryVideoCollectionViewContainerView * const containerView = [[VLCLibraryVideoCollectionViewContainerView alloc] init];
        containerView.videoGroup = i;
        [collectionViewContainers addObject:containerView];
    }

    _containers = collectionViewContainers.copy;
}

- (void)reloadData
{
    dispatch_async(dispatch_get_main_queue(), ^{
        for (NSView<VLCLibraryVideoViewContainerView> *containerView in self->_containers) {
            [self.heroView setOptimalRepresentedItem];
            [containerView.dataSource reloadData];
        }
    });
}

- (NSArray<NSLayoutConstraint*> *)setupViewConstraints:(NSView *)view
                                         withStackView:(NSStackView *)stackView
{
    if (view == nil || stackView == nil) {
        return @[];
    }

    view.translatesAutoresizingMaskIntoConstraints = NO;

    NSArray<NSLayoutConstraint*> * const constraintsWithSuperview = @[
        [NSLayoutConstraint constraintWithItem:view
                                     attribute:NSLayoutAttributeLeft
                                     relatedBy:NSLayoutRelationEqual
                                        toItem:stackView
                                     attribute:NSLayoutAttributeLeft
                                    multiplier:1
                                      constant:0
        ],
        [NSLayoutConstraint constraintWithItem:view
                                     attribute:NSLayoutAttributeRight
                                     relatedBy:NSLayoutRelationEqual
                                        toItem:stackView
                                     attribute:NSLayoutAttributeRight
                                    multiplier:1
                                      constant:0
        ],
    ];
    [stackView addConstraints:constraintsWithSuperview];

    return constraintsWithSuperview;
}

- (void)setupContainerView:(NSView<VLCLibraryVideoViewContainerView> *)containerView
             withStackView:(NSStackView *)stackView
{
    if (containerView == nil || stackView == nil) {
        return;
    }

    containerView.constraintsWithSuperview = [self setupViewConstraints:containerView
                                                          withStackView:stackView];
}

- (void)addView:(NSView *)view
    toStackView:(NSStackView *)stackView
{
    if (view == nil || stackView == nil) {
        return;
    }

    [stackView addArrangedSubview:view];
    [self setupViewConstraints:view withStackView:stackView];
}

- (void)addContainerView:(NSView<VLCLibraryVideoViewContainerView> *)containerView
             toStackView:(NSStackView *)stackView
{
    if (containerView == nil || stackView == nil) {
        return;
    }

    [stackView addArrangedSubview:containerView];
    [self setupContainerView:containerView withStackView:stackView];
}

- (void)setCollectionsStackView:(NSStackView *)collectionsStackView
{
    NSParameterAssert(collectionsStackView);

    if (_collectionsStackView) {
        for (NSView<VLCLibraryVideoViewContainerView> * const containerView in _containers) {
            if (containerView.constraintsWithSuperview.count > 0) {
                [_collectionsStackView removeConstraints:containerView.constraintsWithSuperview];
            }
        }
    }

    _collectionsStackView = collectionsStackView;
    _collectionsStackView.spacing = VLCLibraryUIUnits.largeSpacing;
    _collectionsStackView.orientation = NSUserInterfaceLayoutOrientationVertical;
    _collectionsStackView.alignment = NSLayoutAttributeLeading;
    _collectionsStackView.distribution = NSStackViewDistributionFill;
    [_collectionsStackView setHuggingPriority:NSLayoutPriorityDefaultHigh
                               forOrientation:NSLayoutConstraintOrientationVertical];


    [self addView:self.heroView toStackView:_collectionsStackView];
    [self.heroView setOptimalRepresentedItem];

    for (NSView<VLCLibraryVideoViewContainerView> * const containerView in _containers) {
        [self addContainerView:containerView toStackView:_collectionsStackView];
    }
}

- (void)setCollectionsStackViewScrollView:(NSScrollView *)newScrollView
{
    NSParameterAssert(newScrollView);

    _collectionsStackViewScrollView = newScrollView;

    for (NSView<VLCLibraryVideoViewContainerView> *containerView in _containers) {
        if ([containerView isKindOfClass:VLCLibraryVideoCollectionViewContainerView.class]) {
            VLCLibraryVideoCollectionViewContainerView * const collectionViewContainerView = (VLCLibraryVideoCollectionViewContainerView *)containerView;
            collectionViewContainerView.scrollView.parentScrollView = _collectionsStackViewScrollView;
        }
    }
}

- (void)setCollectionViewItemSize:(NSSize)collectionViewItemSize
{
    _collectionViewItemSize = collectionViewItemSize;

     for (NSView<VLCLibraryVideoViewContainerView> *containerView in _containers) {
        if ([containerView isKindOfClass:VLCLibraryVideoCollectionViewContainerView.class]) {
            VLCLibraryVideoCollectionViewContainerView * const collectionViewContainerView = (VLCLibraryVideoCollectionViewContainerView *)containerView;
            collectionViewContainerView.collectionViewDelegate.staticItemSize = collectionViewItemSize;
        }
    }
}

- (void)setCollectionViewSectionInset:(NSEdgeInsets)collectionViewSectionInset
{
    _collectionViewSectionInset = collectionViewSectionInset;

     for (NSView<VLCLibraryVideoViewContainerView> *containerView in _containers) {
        if ([containerView isKindOfClass:VLCLibraryVideoCollectionViewContainerView.class]) {
            VLCLibraryVideoCollectionViewContainerView * const collectionViewContainerView = (VLCLibraryVideoCollectionViewContainerView *)containerView;
            collectionViewContainerView.collectionViewLayout.sectionInset = collectionViewSectionInset;
        }
     }
}

- (void)setCollectionViewMinimumLineSpacing:(CGFloat)collectionViewMinimumLineSpacing
{
    _collectionViewMinimumLineSpacing = collectionViewMinimumLineSpacing;

     for (NSView<VLCLibraryVideoViewContainerView> *containerView in _containers) {
        if ([containerView isKindOfClass:VLCLibraryVideoCollectionViewContainerView.class]) {
            VLCLibraryVideoCollectionViewContainerView * const collectionViewContainerView = (VLCLibraryVideoCollectionViewContainerView *)containerView;
            collectionViewContainerView.collectionViewLayout.minimumLineSpacing = collectionViewMinimumLineSpacing;
        }
    }
}

- (void)setCollectionViewMinimumInteritemSpacing:(CGFloat)collectionViewMinimumInteritemSpacing
{
    _collectionViewMinimumInteritemSpacing = collectionViewMinimumInteritemSpacing;

    for (NSView<VLCLibraryVideoViewContainerView> *containerView in _containers) {
        if ([containerView isKindOfClass:VLCLibraryVideoCollectionViewContainerView.class]) {
            VLCLibraryVideoCollectionViewContainerView * const collectionViewContainerView = (VLCLibraryVideoCollectionViewContainerView *)containerView;
            collectionViewContainerView.collectionViewLayout.minimumInteritemSpacing = collectionViewMinimumInteritemSpacing;
        }
    }
}

- (VLCLibraryVideoCollectionViewContainerView *)containerViewForGroup:(VLCMediaLibraryParentGroupType)group
{
    const NSUInteger index = [_containers indexOfObjectPassingTest:^BOOL(NSView<VLCLibraryVideoViewContainerView> * const container, const NSUInteger idx, BOOL * const stop) {
        return container.videoGroup == group;
    }];

    if (index == NSNotFound) {
        return nil;
    }

    return [_containers objectAtIndex:index];
}

- (void)presentLibraryItem:(id<VLCMediaLibraryItemProtocol>)libraryItem
{
    if (libraryItem == nil) {
        return;
    }

    VLCLibraryVideoCollectionViewContainerView * const containerView = [self containerViewForGroup:VLCMediaLibraryParentGroupTypeVideoLibrary];
    if (containerView == nil) {
        return;
    }

    [containerView presentLibraryItem:libraryItem];
}

@end
