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

#import "extensions/NSView+VLCAdditions.h"

#import "library/VLCLibraryCollectionViewDelegate.h"
#import "library/VLCLibraryCollectionViewFlowLayout.h"
#import "library/VLCLibraryCollectionViewSupplementaryElementView.h"
#import "library/VLCLibraryController.h"
#import "library/VLCLibraryHeroView.h"
#import "library/VLCLibraryItemPresentingCapable.h"
#import "library/VLCLibraryModel.h"

#import "library/home-library/VLCLibraryHomeViewActionsView.h"
#import "library/home-library/VLCLibraryHomeViewAudioCarouselContainerView.h"
#import "library/home-library/VLCLibraryHomeViewBrowseCarouselContainerView.h"
#import "library/home-library/VLCLibraryHomeViewContainerView.h"
#import "library/home-library/VLCLibraryHomeViewVideoCarouselContainerView.h"
#import "library/home-library/VLCLibraryHomeViewVideoContainerView.h"
#import "library/home-library/VLCLibraryHomeViewVideoContainerViewDataSource.h"
#import "library/home-library/VLCLibraryHomeViewVideoGridContainerView.h"

#import "library/video-library/VLCLibraryVideoGroupDescriptor.h"

#import "main/VLCMain.h"

#import "views/VLCSubScrollView.h"
#import "views/VLCUIUnits.h"

@interface VLCLibraryHomeViewStackViewController()
{
    NSMutableArray<NSView<VLCLibraryHomeViewContainerView> *> *_containers;
    NSArray<NSView *> *_leadingViews;
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
    [[NSNotificationCenter defaultCenter] removeObserver:self];
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
                                name:VLCLibraryModelAllCachesDropped
                                object:nil];
    [notificationCenter addObserver:self
                            selector:@selector(recentsChanged:)
                                name:VLCLibraryModelAllCachesDropped
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

    _containers = [NSMutableArray array];
    [self generateCustomContainers];
    [self generateGenericCollectionViewContainers];
}

- (void)generateCustomContainers
{
    _actionsView = [VLCLibraryHomeViewActionsView fromNibWithOwner:self];
    _heroView = [VLCLibraryHeroView fromNibWithOwner:self];
    _leadingViews = @[self.actionsView, self.heroView];

    [self addCustomContainerViews];
    [self recentsChanged:nil];
    [self audioRecentsChanged:nil];
    _browseView = [[VLCLibraryHomeViewBrowseCarouselContainerView alloc] init];
    [_containers addObject:self.browseView];
    [self addContainerView:self.browseView toStackView:self.collectionsStackView];
}

- (void)addCustomContainerViews
{
    for (NSView * const view in _leadingViews) {
        [self addView:view toStackView:self.collectionsStackView];
    }
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

- (void)insertRecentItemsContainer:(NSView<VLCLibraryHomeViewContainerView> *)container
                          atIndex:(NSUInteger)index
{
    // Actions and Hero precede the ordered video, music, and Browse rows.
    [self.collectionsStackView insertArrangedSubview:container atIndex:_leadingViews.count + index];
    [self setupContainerView:container withStackView:_collectionsStackView];
    [_containers insertObject:container atIndex:index];
}

- (void)removeRecentItemsContainer:(NSView<VLCLibraryHomeViewContainerView> *)container
{
    [self.collectionsStackView removeConstraints:container.constraintsWithSuperview];
    [self.collectionsStackView removeArrangedSubview:container];
    [container removeFromSuperview];
    [container disconnect];
    [_containers removeObject:container];
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
        [self insertRecentItemsContainer:self.recentsView atIndex:0];
    } else {
        [self removeRecentItemsContainer:_recentsView];
        _recentsView = nil;
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
        [self insertRecentItemsContainer:self.audioRecentsView atIndex:self.recentsView != nil ? 1 : 0];
    } else {
        [self removeRecentItemsContainer:_audioRecentsView];
        _audioRecentsView = nil;
    }
}

- (void)generateGenericCollectionViewContainers
{
    NSUInteger i = VLCMediaLibraryParentGroupTypeRecentVideos + 1;

    for (; i <= VLCMediaLibraryParentGroupTypeVideoLibrary; ++i) {
        VLCLibraryHomeViewVideoGridContainerView * const containerView = [[VLCLibraryHomeViewVideoGridContainerView alloc] init];
        containerView.videoGroup = i;
        [_containers addObject:containerView];
    }
}

- (void)reloadData
{
    dispatch_async(dispatch_get_main_queue(), ^{
        [self.heroView setOptimalRepresentedItem];

        for (NSView<VLCLibraryHomeViewContainerView> * const containerView in self->_containers) {
            if ([containerView isKindOfClass:VLCLibraryHomeViewBaseCarouselContainerView.class]) {
                VLCLibraryHomeViewBaseCarouselContainerView * const baseContainerView = (VLCLibraryHomeViewBaseCarouselContainerView *)containerView;
                [baseContainerView reloadData];
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
    _collectionsStackView.spacing = VLCUIUnits.largeSpacing;
    _collectionsStackView.orientation = NSUserInterfaceLayoutOrientationVertical;
    _collectionsStackView.alignment = NSLayoutAttributeLeading;
    _collectionsStackView.distribution = NSStackViewDistributionFill;
    [_collectionsStackView setHuggingPriority:NSLayoutPriorityDefaultHigh
                               forOrientation:NSLayoutConstraintOrientationVertical];

    for (NSView * const view in _collectionsStackView.arrangedSubviews.copy) {
        [view removeFromSuperview];
    }

    [self addCustomContainerViews];

    for (NSView<VLCLibraryHomeViewContainerView> * const containerView in _containers) {
        [self addContainerView:containerView toStackView:_collectionsStackView];
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
    NSView<VLCLibraryHomeViewVideoContainerView, VLCLibraryItemPresentingCapable> * const containerView =
        (NSView<VLCLibraryHomeViewVideoContainerView, VLCLibraryItemPresentingCapable> *)[self containerViewForGroup:VLCMediaLibraryParentGroupTypeVideoLibrary];
    if (containerView == nil) {
        return;
    }

    [containerView presentLibraryItem:libraryItem];
}

@end
