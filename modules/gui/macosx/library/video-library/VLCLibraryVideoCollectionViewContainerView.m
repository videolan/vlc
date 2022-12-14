/*****************************************************************************
 * VLCLibraryVideoCollectionViewContainerView.m: MacOS X interface module
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

#import "VLCLibraryVideoCollectionViewContainerView.h"

#import "library/VLCLibraryCollectionViewFlowLayout.h"
#import "library/VLCLibraryCollectionViewSupplementaryElementView.h"

#import "library/video-library/VLCLibraryVideoCollectionViewContainerViewDataSource.h"
#import "library/video-library/VLCLibraryVideoGroupDescriptor.h"

#import "views/VLCSubScrollView.h"

@implementation VLCLibraryVideoCollectionViewContainerView

- (instancetype)init
{
    self = [super init];

    if(self) {
        [self setupView];
        [self setupDataSource];
        [self setupCollectionViewSizeChangeListener];
    }

    return self;
}

- (void)setupView
{
    [self setupCollectionView];
    [self setupScrollView];

    [self addSubview:_scrollView];
    [self addConstraints:@[
        [NSLayoutConstraint constraintWithItem:_scrollView
                                     attribute:NSLayoutAttributeTop
                                     relatedBy:NSLayoutRelationEqual
                                        toItem:self
                                     attribute:NSLayoutAttributeTop
                                    multiplier:1
                                      constant:0
        ],
        [NSLayoutConstraint constraintWithItem:_scrollView
                                     attribute:NSLayoutAttributeBottom
                                     relatedBy:NSLayoutRelationEqual
                                        toItem:self
                                     attribute:NSLayoutAttributeBottom
                                    multiplier:1
                                      constant:0
        ],
        [NSLayoutConstraint constraintWithItem:_scrollView
                                     attribute:NSLayoutAttributeLeft
                                     relatedBy:NSLayoutRelationEqual
                                        toItem:self
                                     attribute:NSLayoutAttributeLeft
                                    multiplier:1
                                      constant:0
        ],
        [NSLayoutConstraint constraintWithItem:_scrollView
                                     attribute:NSLayoutAttributeRight
                                     relatedBy:NSLayoutRelationEqual
                                        toItem:self
                                     attribute:NSLayoutAttributeRight
                                    multiplier:1
                                      constant:0
        ],
    ]];

    [self setContentHuggingPriority:NSLayoutPriorityDefaultLow
                     forOrientation:NSLayoutConstraintOrientationHorizontal];
    [self setContentCompressionResistancePriority:NSLayoutPriorityDefaultLow
                                   forOrientation:NSLayoutConstraintOrientationHorizontal];

    [self setContentHuggingPriority:NSLayoutPriorityDefaultHigh
                     forOrientation:NSLayoutConstraintOrientationVertical];
    [self setContentCompressionResistancePriority:NSLayoutPriorityDefaultHigh
                     forOrientation:NSLayoutConstraintOrientationVertical];
}

- (void)setupCollectionView
{
    _collectionViewLayout = [[VLCLibraryCollectionViewFlowLayout alloc] init];
    _collectionViewLayout.headerReferenceSize = [VLCLibraryCollectionViewSupplementaryElementView defaultHeaderSize];
    _collectionViewLayout.itemSize = CGSizeMake(214., 260.);

    _collectionView = [[NSCollectionView alloc] initWithFrame:NSZeroRect];
    _collectionView.postsFrameChangedNotifications = YES;
    _collectionView.collectionViewLayout = _collectionViewLayout;
    _collectionView.selectable = YES;
    _collectionView.allowsEmptySelection = YES;
    _collectionView.allowsMultipleSelection = NO;
}

- (void)setupScrollView
{
    _scrollView = [[VLCSubScrollView alloc] init];
    _scrollView.scrollParentY = YES;
    _scrollView.forceHideVerticalScroller = YES;

    _scrollView.translatesAutoresizingMaskIntoConstraints = NO;
    _scrollView.documentView = _collectionView;
}

- (void)setupDataSource
{
    _dataSource = [[VLCLibraryVideoCollectionViewContainerViewDataSource alloc] init];
    _dataSource.collectionView = _collectionView;
    [_dataSource setup];
}

- (void)setupCollectionViewSizeChangeListener
{
    NSNotificationCenter *notificationCenter = [NSNotificationCenter defaultCenter];
    [notificationCenter addObserver:self
                           selector:@selector(collectionViewFrameChanged:)
                               name:NSViewFrameDidChangeNotification
                             object:nil];
}

- (void)setGroupDescriptor:(VLCLibraryVideoCollectionViewGroupDescriptor *)groupDescriptor
{
    _groupDescriptor = groupDescriptor;
    _dataSource.groupDescriptor = groupDescriptor;

    _collectionViewLayout.scrollDirection = _groupDescriptor.isHorizontalBarCollectionView ?
                                            NSCollectionViewScrollDirectionHorizontal :
                                            NSCollectionViewScrollDirectionVertical;
}

- (void)setVideoGroup:(VLCLibraryVideoGroup)group
{
    if (_groupDescriptor.group == group) {
        return;
    }

    VLCLibraryVideoCollectionViewGroupDescriptor *descriptor = [[VLCLibraryVideoCollectionViewGroupDescriptor alloc] initWithVLCVideoLibraryGroup:group];
    [self setGroupDescriptor:descriptor];
}

- (void)collectionViewFrameChanged:(NSNotification *)notification
{
    if ([notification.object class] != [NSCollectionView class] ||
        (NSCollectionView *)notification.object != _collectionView) {
        return;
    }

    // HACK: On app init the vertical collection views will not get their heights updated properly.
    // So let's schedule a check a bit later to correct this issue...
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 100 * NSEC_PER_MSEC), dispatch_get_main_queue(), ^{
        [self invalidateIntrinsicContentSize];
    });
}

- (NSSize)intrinsicContentSize
{
    const NSSize collectionViewContentSize = _collectionViewLayout.collectionViewContentSize;
    const NSEdgeInsets scrollViewInsets = _collectionView.enclosingScrollView.contentInsets;
    const NSEdgeInsets collectionViewLayoutInset = _collectionViewLayout.sectionInset;
    const CGFloat insetsHeight = scrollViewInsets.top +
                                 scrollViewInsets.bottom +
                                 collectionViewLayoutInset.top +
                                 collectionViewLayoutInset.bottom;
    const CGFloat width = scrollViewInsets.left +
                          scrollViewInsets.right +
                          collectionViewLayoutInset.left +
                          collectionViewLayoutInset.right +
                          self.frame.size.width;

    if (collectionViewContentSize.height == 0) {
        // If we don't return a size larger than 0 then we run into issues with the collection
        // view layout not trying to properly calculate its size. So let's return something
        NSLog(@"Unable to provide accurate height for container -- providing rough size");
        const CGFloat roughValue = _collectionViewLayout.itemSize.height + insetsHeight;
        return NSMakeSize(width, roughValue);
    }

    if (_groupDescriptor.isHorizontalBarCollectionView) {
        const CGFloat viewHeight = _collectionViewLayout.itemSize.height +
                                   insetsHeight +
                                   15; // Account for horizontal scrollbar
        return NSMakeSize(width, viewHeight);
    }

    return NSMakeSize(width, collectionViewContentSize.height - 1);
}

@end
