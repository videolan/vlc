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

#import "VLCLibraryHomeViewStackViewController.h"

#import "library/VLCLibraryCollectionViewDelegate.h"
#import "library/VLCLibraryCollectionViewFlowLayout.h"
#import "library/VLCLibraryCollectionViewSupplementaryElementView.h"
#import "library/VLCLibraryController.h"
#import "library/VLCLibraryHeroView.h"
#import "library/VLCLibraryModel.h"
#import "library/VLCLibraryUIUnits.h"

#import "library/home-library/VLCLibraryHomeViewActionsView.h"
#import "library/home-library/VLCLibraryHomeViewAudioCarouselContainerView.h"
#import "library/home-library/VLCLibraryHomeViewContainerView.h"
#import "library/home-library/VLCLibraryHomeViewVideoCarouselContainerView.h"
#import "library/home-library/VLCLibraryHomeViewVideoContainerView.h"
#import "library/home-library/VLCLibraryHomeViewVideoContainerViewDataSource.h"
#import "library/home-library/VLCLibraryHomeViewVideoGridContainerView.h"

#import "library/video-library/VLCLibraryVideoGroupDescriptor.h"

#import "main/VLCMain.h"

#import "views/VLCSubScrollView.h"

@interface VLCLibraryHomeViewStackViewController()
{
    NSArray<NSView<VLCLibraryHomeViewContainerView> *> *_containers;
    NSUInteger _leadingContainerCount;
}
@end

@implementation VLCLibraryHomeViewStackViewController

- (instancetype)init
{
    self = [super init];
    if (self) {
        [self setup];
    }
    return self;
}

- (void)dealloc
{
    self.collectionsStackView.subviews = @[];
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
    [notificationCenter addObserver:self
                           selector:@selector(audioRecentsChanged:)
                               name:VLCLibraryModelRecentAudioMediaListReset
                             object:nil];
    [notificationCenter addObserver:self
                           selector:@selector(audioRecentsChanged:)
                               name:VLCLibraryModelRecentAudioMediaItemDeleted
                             object:nil];

    _containers = @[];
    _leadingContainerCount = 0;
    [self generateCustomContainers];
    [self generateGenericCollectionViewContainers];
}

- (void)generateCustomContainers
{
    _actionsView = [VLCLibraryHomeViewActionsView fromNibWithOwner:self];
    _heroView = [VLCLibraryHeroView fromNibWithOwner:self];
    _leadingContainerCount += 2;

    [self addCustomContainerViews];
    [self audioRecentsChanged:nil];
    [self recentsChanged:nil];
}

- (void)addCustomContainerViews
{
    [self addView:self.actionsView toStackView:self.collectionsStackView];
    [self addView:self.heroView toStackView:self.collectionsStackView];
    [self.heroView setOptimalRepresentedItem];
}

- (BOOL)recentMediaPresent
{
    VLCLibraryModel * const model = VLCMain.sharedInstance.libraryController.libraryModel;
    return model.numberOfRecentMedia > 0;
}

- (BOOL)recentAudioMediaPresent
{
    VLCLibraryModel * const model = VLCMain.sharedInstance.libraryController.libraryModel;
    return model.numberOfRecentAudioMedia > 0;
}

- (void)prependRecentItemsContainer:(NSView<VLCLibraryHomeViewContainerView> *)container
{
    NSMutableArray<NSView<VLCLibraryHomeViewContainerView> *> * const mutableContainers = _containers.mutableCopy;
    [self.collectionsStackView insertArrangedSubview:container atIndex:_leadingContainerCount];
    [self setupContainerView:container withStackView:_collectionsStackView];
    [mutableContainers insertObject:container atIndex:0];
    ++_leadingContainerCount;
    _containers = mutableContainers.copy;
}

- (void)removeRecentItemsContainer:(NSView<VLCLibraryHomeViewContainerView> *)container
{
    [self.collectionsStackView removeConstraints:container.constraintsWithSuperview];
    [self.collectionsStackView removeArrangedSubview:container];
    NSMutableArray<NSView<VLCLibraryHomeViewContainerView> *> * const mutableContainers = _containers.mutableCopy;
    [mutableContainers removeObject:container];
    container = nil; // Important to detect whether the view is presented or not
    --_leadingContainerCount;
    _containers = mutableContainers.copy;
}

- (void)recentsChanged:(NSNotification *)notification
{
    const BOOL shouldShowRecentsContainer = [self recentMediaPresent];
    const BOOL recentsContainerPresent = self.recentsView != nil;

    if (recentsContainerPresent == shouldShowRecentsContainer) {
        return;
    } else if (shouldShowRecentsContainer) {
        _recentsView = [[VLCLibraryHomeViewVideoCarouselContainerView alloc] init];
        self.recentsView.videoGroup = VLCMediaLibraryParentGroupTypeRecentVideos;
        [self prependRecentItemsContainer:self.recentsView];
    } else {
        [self removeRecentItemsContainer:_recentsView];
    }
}

- (void)audioRecentsChanged:(NSNotification *)notification
{
    const BOOL shouldShowAudioRecentsContainer = [self recentAudioMediaPresent];
    const BOOL audioRecentsContainerPresent = self.audioRecentsView != nil;

    if (audioRecentsContainerPresent == shouldShowAudioRecentsContainer) {
        return;
    } else if (shouldShowAudioRecentsContainer) {
        _audioRecentsView = [[VLCLibraryHomeViewAudioCarouselContainerView alloc] init];
        self.audioRecentsView.audioLibrarySegment = VLCAudioLibraryRecentsSegment;
        [self prependRecentItemsContainer:self.audioRecentsView];
    } else {
        [self removeRecentItemsContainer:_audioRecentsView];
    }
}

- (void)generateGenericCollectionViewContainers
{
    NSMutableArray<NSView<VLCLibraryHomeViewContainerView> *> * const mutableContainers = _containers.mutableCopy;
    NSUInteger i = VLCMediaLibraryParentGroupTypeRecentVideos + 1;

    for (; i <= VLCMediaLibraryParentGroupTypeVideoLibrary; ++i) {
        VLCLibraryHomeViewVideoGridContainerView * const containerView = [[VLCLibraryHomeViewVideoGridContainerView alloc] init];
        containerView.videoGroup = i;
        [mutableContainers addObject:containerView];
    }

    _containers = mutableContainers.copy;
}

- (void)reloadData
{
    dispatch_async(dispatch_get_main_queue(), ^{
        [self.heroView setOptimalRepresentedItem];

        for (NSView<VLCLibraryHomeViewContainerView> * const containerView in self->_containers) {
            if ([containerView isKindOfClass:VLCLibraryHomeViewBaseCarouselContainerView.class]) {
                VLCLibraryHomeViewBaseCarouselContainerView * const baseContainerView = (VLCLibraryHomeViewBaseCarouselContainerView *)containerView;
                [baseContainerView.dataSource reloadData];
            }
        }
    });
}

- (void)connectContainers
{
    for (NSView<VLCLibraryHomeViewContainerView> * const container in _containers) {
        [container connect];
    }
}

- (void)disconnectContainers
{
    for (NSView<VLCLibraryHomeViewContainerView> * const container in _containers) {
        [container disconnect];
    }
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

- (void)setupContainerView:(NSView<VLCLibraryHomeViewContainerView> *)containerView
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

- (void)addContainerView:(NSView<VLCLibraryHomeViewContainerView> *)containerView
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
        for (NSView<VLCLibraryHomeViewContainerView> * const containerView in _containers) {
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

    [self addCustomContainerViews];

    for (NSView<VLCLibraryHomeViewContainerView> * const containerView in _containers) {
        [self addContainerView:containerView toStackView:_collectionsStackView];
    }
}

- (void)setCollectionsStackViewScrollView:(NSScrollView *)newScrollView
{
    NSParameterAssert(newScrollView);

    _collectionsStackViewScrollView = newScrollView;

    for (NSView<VLCLibraryHomeViewContainerView> * const containerView in _containers) {
        if ([containerView isKindOfClass:VLCLibraryHomeViewVideoGridContainerView.class]) {
            VLCLibraryHomeViewVideoGridContainerView * const collectionViewContainerView = (VLCLibraryHomeViewVideoGridContainerView *)containerView;
            collectionViewContainerView.scrollView.parentScrollView = _collectionsStackViewScrollView;
        }
    }
}

- (void)setCollectionViewItemSize:(NSSize)collectionViewItemSize
{
    _collectionViewItemSize = collectionViewItemSize;

     for (NSView<VLCLibraryHomeViewContainerView> *containerView in _containers) {
        if ([containerView isKindOfClass:VLCLibraryHomeViewVideoGridContainerView.class]) {
            VLCLibraryHomeViewVideoGridContainerView * const collectionViewContainerView = (VLCLibraryHomeViewVideoGridContainerView *)containerView;
            collectionViewContainerView.collectionViewDelegate.staticItemSize = collectionViewItemSize;
        }
    }
}

- (void)setCollectionViewSectionInset:(NSEdgeInsets)collectionViewSectionInset
{
    _collectionViewSectionInset = collectionViewSectionInset;

     for (NSView<VLCLibraryHomeViewContainerView> *containerView in _containers) {
        if ([containerView isKindOfClass:VLCLibraryHomeViewVideoGridContainerView.class]) {
            VLCLibraryHomeViewVideoGridContainerView * const collectionViewContainerView = (VLCLibraryHomeViewVideoGridContainerView *)containerView;
            collectionViewContainerView.collectionViewLayout.sectionInset = collectionViewSectionInset;
        }
     }
}

- (void)setCollectionViewMinimumLineSpacing:(CGFloat)collectionViewMinimumLineSpacing
{
    _collectionViewMinimumLineSpacing = collectionViewMinimumLineSpacing;

     for (NSView<VLCLibraryHomeViewContainerView> *containerView in _containers) {
        if ([containerView isKindOfClass:VLCLibraryHomeViewVideoGridContainerView.class]) {
            VLCLibraryHomeViewVideoGridContainerView * const collectionViewContainerView = (VLCLibraryHomeViewVideoGridContainerView *)containerView;
            collectionViewContainerView.collectionViewLayout.minimumLineSpacing = collectionViewMinimumLineSpacing;
        }
    }
}

- (void)setCollectionViewMinimumInteritemSpacing:(CGFloat)collectionViewMinimumInteritemSpacing
{
    _collectionViewMinimumInteritemSpacing = collectionViewMinimumInteritemSpacing;

    for (NSView<VLCLibraryHomeViewContainerView> * const containerView in _containers) {
        if ([containerView isKindOfClass:VLCLibraryHomeViewVideoGridContainerView.class]) {
            VLCLibraryHomeViewVideoGridContainerView * const collectionViewContainerView = (VLCLibraryHomeViewVideoGridContainerView *)containerView;
            collectionViewContainerView.collectionViewLayout.minimumInteritemSpacing = collectionViewMinimumInteritemSpacing;
        }
    }
}


- (NSView<VLCLibraryHomeViewContainerView> *)containerViewForGroup:(VLCMediaLibraryParentGroupType)group
{
    const NSUInteger index = [_containers indexOfObjectPassingTest:^BOOL(NSView<VLCLibraryHomeViewContainerView> * const container, const NSUInteger __unused idx, BOOL * const __unused stop) {
        if ([container conformsToProtocol:@protocol(VLCLibraryHomeViewVideoContainerView)]) {
            NSView<VLCLibraryHomeViewVideoContainerView> * const videoContainer = (NSView<VLCLibraryHomeViewVideoContainerView> *)container;
            return videoContainer.videoGroup == group;
        }
        return NO;
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

    // TODO: Make this work agnostically of video type
    NSView<VLCLibraryHomeViewVideoContainerView> * const containerView = (NSView<VLCLibraryHomeViewVideoContainerView> *)[self containerViewForGroup:VLCMediaLibraryParentGroupTypeVideoLibrary];
    if (containerView == nil) {
        return;
    }

    [containerView presentLibraryItem:libraryItem];
}

@end
