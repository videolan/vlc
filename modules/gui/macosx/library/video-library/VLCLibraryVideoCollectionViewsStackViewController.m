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

#import "library/VLCLibraryCollectionViewFlowLayout.h"
#import "library/VLCLibraryCollectionViewSupplementaryElementView.h"
#import "library/VLCLibraryModel.h"

#import "library/video-library/VLCLibraryVideoCollectionViewContainerView.h"
#import "library/video-library/VLCLibraryVideoCollectionViewContainerViewDataSource.h"
#import "library/video-library/VLCLibraryVideoGroupDescriptor.h"

#import "views/VLCSubScrollView.h"

@interface VLCLibraryVideoCollectionViewsStackViewController()
{
    NSArray *_collectionViewContainers;
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
    [self generateCollectionViewContainers];
}

- (void)generateCollectionViewContainers
{
    NSMutableArray *collectionViewContainers = [[NSMutableArray alloc] init];

    for (NSUInteger i = VLCLibraryVideoRecentsGroup; i < VLCLibraryVideoSentinel; ++i) {
        VLCLibraryVideoCollectionViewContainerView *containerView = [[VLCLibraryVideoCollectionViewContainerView alloc] init];
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

- (void)setCollectionsStackView:(NSStackView *)collectionsStackView
{
    NSParameterAssert(collectionsStackView);

    if (_collectionsStackView) {
        for (VLCLibraryVideoCollectionViewContainerView *containerView in _collectionViewContainers) {
            if (containerView.constraintsWithSuperview.count > 0) {
                [_collectionsStackView removeConstraints:containerView.constraintsWithSuperview];
            }
        }
    }

    _collectionsStackView = collectionsStackView;
    _collectionsStackView.spacing = 20.;
    _collectionsStackView.orientation = NSUserInterfaceLayoutOrientationVertical;
    _collectionsStackView.alignment = NSLayoutAttributeLeading;
    _collectionsStackView.distribution = NSStackViewDistributionFill;
    [_collectionsStackView setHuggingPriority:NSLayoutPriorityDefaultHigh
                               forOrientation:NSLayoutConstraintOrientationVertical];


    for (VLCLibraryVideoCollectionViewContainerView *containerView in _collectionViewContainers) {
        containerView.translatesAutoresizingMaskIntoConstraints = NO;
        NSArray<NSLayoutConstraint*> *constraintsWithSuperview = @[
            [NSLayoutConstraint constraintWithItem:containerView
                                         attribute:NSLayoutAttributeLeft
                                         relatedBy:NSLayoutRelationEqual
                                            toItem:_collectionsStackView
                                         attribute:NSLayoutAttributeLeft
                                        multiplier:1
                                          constant:0
            ],
            [NSLayoutConstraint constraintWithItem:containerView
                                         attribute:NSLayoutAttributeRight
                                         relatedBy:NSLayoutRelationEqual
                                            toItem:_collectionsStackView
                                         attribute:NSLayoutAttributeRight
                                        multiplier:1
                                          constant:0
            ],
        ];
        containerView.constraintsWithSuperview = constraintsWithSuperview;
        [_collectionsStackView addConstraints:constraintsWithSuperview];
        [_collectionsStackView addArrangedSubview:containerView];
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
        containerView.collectionViewLayout.itemSize = collectionViewItemSize;
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

- (void) setCollectionViewMinimumInteritemSpacing:(CGFloat)collectionViewMinimumInteritemSpacing
{
    _collectionViewMinimumInteritemSpacing = collectionViewMinimumInteritemSpacing;

    for (VLCLibraryVideoCollectionViewContainerView *containerView in _collectionViewContainers) {
        containerView.collectionViewLayout.minimumInteritemSpacing = collectionViewMinimumInteritemSpacing;
    }
}

@end
