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
#import "library/VLCLibraryModel.h"
#import "library/VLCLibraryUIUnits.h"

#import "library/video-library/VLCLibraryVideoCollectionViewContainerView.h"
#import "library/video-library/VLCLibraryVideoCollectionViewContainerViewDataSource.h"
#import "library/video-library/VLCLibraryVideoGroupDescriptor.h"

#import "main/VLCMain.h"

#import "views/VLCSubScrollView.h"

@interface VLCLibraryVideoCollectionViewsStackViewController()
{
    NSArray *_collectionViewContainers;
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
    [self generateCollectionViewContainers];
}

- (BOOL)recentMediaPresent
{
    VLCLibraryModel * const model = VLCMain.sharedInstance.libraryController.libraryModel;
    return model.numberOfRecentMedia > 0;
}

- (void)recentsChanged:(NSNotification *)notification
{
    const BOOL shouldShowRecentsContainer = [self recentMediaPresent];
    const NSUInteger recentsContainerIndex = [_collectionViewContainers indexOfObjectPassingTest:^BOOL(VLCLibraryVideoCollectionViewContainerView * const container, const NSUInteger idx, BOOL * const stop) {
        return container.videoGroup == VLCLibraryVideoRecentsGroup;
    }];
    const BOOL recentsContainerPresent = recentsContainerIndex != NSNotFound;

    if (recentsContainerPresent == shouldShowRecentsContainer) {
        return;
    }

    NSMutableArray * const mutableContainers = _collectionViewContainers.mutableCopy;

    if (shouldShowRecentsContainer) {
        VLCLibraryVideoCollectionViewContainerView * const containerView = [[VLCLibraryVideoCollectionViewContainerView alloc] init];
        containerView.videoGroup = VLCLibraryVideoRecentsGroup;
        // Insert at top after leading containers, hence _leadingContainerCount
        [mutableContainers insertObject:containerView atIndex:_leadingContainerCount];

        [_collectionsStackView insertArrangedSubview:containerView atIndex:_leadingContainerCount];
        [self setupContainerView:containerView forStackView:_collectionsStackView];
    } else {
        [mutableContainers removeObjectAtIndex:recentsContainerIndex];
        VLCLibraryVideoCollectionViewContainerView * const existingContainer = [_collectionViewContainers objectAtIndex:recentsContainerIndex];
        [_collectionsStackView removeConstraints:existingContainer.constraintsWithSuperview];
        [_collectionsStackView removeArrangedSubview:existingContainer];
    }

    _collectionViewContainers = mutableContainers.copy;
}

- (void)generateCollectionViewContainers
{
    NSMutableArray * const collectionViewContainers = [[NSMutableArray alloc] init];
    const BOOL anyRecents = [self recentMediaPresent];
    NSUInteger i = anyRecents ? VLCLibraryVideoRecentsGroup : VLCLibraryVideoRecentsGroup + 1;

    for (; i < VLCLibraryVideoSentinel; ++i) {
        VLCLibraryVideoCollectionViewContainerView * const containerView = [[VLCLibraryVideoCollectionViewContainerView alloc] init];
        containerView.videoGroup = i;
        [collectionViewContainers addObject:containerView];
    }

    _collectionViewContainers = collectionViewContainers;
}

- (void)reloadData
{
    dispatch_async(dispatch_get_main_queue(), ^{
        for (VLCLibraryVideoCollectionViewContainerView *containerView in self->_collectionViewContainers) {
            [containerView.collectionView reloadData];
        }
    });
}

- (NSArray<NSLayoutConstraint*> *)setupViewConstraints:(NSView *)view
                                          forStackView:(NSStackView *)stackView
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

- (void)setupContainerView:(VLCLibraryVideoCollectionViewContainerView *)containerView
              forStackView:(NSStackView *)stackView
{
    if (containerView == nil || stackView == nil) {
        return;
    }

    NSArray<NSLayoutConstraint*> * const constraintsWithSuperview = [self setupViewConstraints:containerView forStackView:stackView];
    containerView.constraintsWithSuperview = constraintsWithSuperview;
}

- (void)addContainerView:(VLCLibraryVideoCollectionViewContainerView *)containerView
             toStackView:(NSStackView *)stackView
{
    if (containerView == nil || stackView == nil) {
        return;
    }
    
    [stackView addArrangedSubview:containerView];
    [self setupContainerView:containerView forStackView:stackView];
}

- (void)setCollectionsStackView:(NSStackView *)collectionsStackView
{
    NSParameterAssert(collectionsStackView);

    if (_collectionsStackView) {
        for (VLCLibraryVideoCollectionViewContainerView * const containerView in _collectionViewContainers) {
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


    for (VLCLibraryVideoCollectionViewContainerView * const containerView in _collectionViewContainers) {
        [self addContainerView:containerView toStackView:_collectionsStackView];
    }
}

- (void)setCollectionsStackViewScrollView:(NSScrollView *)newScrollView
{
    NSParameterAssert(newScrollView);

    _collectionsStackViewScrollView = newScrollView;

    for (VLCLibraryVideoCollectionViewContainerView *containerView in _collectionViewContainers) {
        containerView.scrollView.parentScrollView = _collectionsStackViewScrollView;
    }
}

- (void)setCollectionViewItemSize:(NSSize)collectionViewItemSize
{
    _collectionViewItemSize = collectionViewItemSize;

    for (VLCLibraryVideoCollectionViewContainerView *containerView in _collectionViewContainers) {
        containerView.collectionViewDelegate.staticItemSize = collectionViewItemSize;
    }
}

- (void)setCollectionViewSectionInset:(NSEdgeInsets)collectionViewSectionInset
{
    _collectionViewSectionInset = collectionViewSectionInset;

    for (VLCLibraryVideoCollectionViewContainerView *containerView in _collectionViewContainers) {
        containerView.collectionViewLayout.sectionInset = collectionViewSectionInset;
    }
}

- (void)setCollectionViewMinimumLineSpacing:(CGFloat)collectionViewMinimumLineSpacing
{
    _collectionViewMinimumLineSpacing = collectionViewMinimumLineSpacing;

    for (VLCLibraryVideoCollectionViewContainerView *containerView in _collectionViewContainers) {
        containerView.collectionViewLayout.minimumLineSpacing = collectionViewMinimumLineSpacing;
    }
}

- (void)setCollectionViewMinimumInteritemSpacing:(CGFloat)collectionViewMinimumInteritemSpacing
{
    _collectionViewMinimumInteritemSpacing = collectionViewMinimumInteritemSpacing;

    for (VLCLibraryVideoCollectionViewContainerView *containerView in _collectionViewContainers) {
        containerView.collectionViewLayout.minimumInteritemSpacing = collectionViewMinimumInteritemSpacing;
    }
}

@end
