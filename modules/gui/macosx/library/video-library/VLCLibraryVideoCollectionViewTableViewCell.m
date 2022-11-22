/*****************************************************************************
 * VLCLibraryVideoCollectionViewTableViewCell.m: MacOS X interface module
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

#import "VLCLibraryVideoCollectionViewTableViewCell.h"

#import "library/VLCLibraryCollectionViewFlowLayout.h"
#import "library/VLCLibraryCollectionViewSupplementaryElementView.h"

#import "library/video-library/VLCLibraryVideoCollectionViewTableViewCellDataSource.h"
#import "library/video-library/VLCLibraryVideoGroupDescriptor.h"

#import "views/VLCSubScrollView.h"

@implementation VLCLibraryVideoCollectionViewTableViewCell

- (instancetype)init
{
    self = [super init];

    if(self) {
        [self setupView];
        [self setupDataSource];
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
}

- (void)setupCollectionView
{
    VLCLibraryCollectionViewFlowLayout *collectionViewLayout = [[VLCLibraryCollectionViewFlowLayout alloc] init];
    collectionViewLayout.headerReferenceSize = [VLCLibraryCollectionViewSupplementaryElementView defaultHeaderSize];
    collectionViewLayout.itemSize = CGSizeMake(214., 260.);

    _collectionView = [[NSCollectionView alloc] initWithFrame:NSZeroRect];
    _collectionView.postsFrameChangedNotifications = YES;
    _collectionView.collectionViewLayout = collectionViewLayout;
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
    _dataSource = [[VLCLibraryVideoCollectionViewTableViewCellDataSource alloc] init];
    _dataSource.collectionView = _collectionView;
    [_dataSource setup];
}

- (void)setGroupDescriptor:(VLCLibraryVideoCollectionViewGroupDescriptor *)groupDescriptor
{
    _groupDescriptor = groupDescriptor;
    _dataSource.groupDescriptor = groupDescriptor;

    NSCollectionViewFlowLayout *collectionViewLayout = _collectionView.collectionViewLayout;
    collectionViewLayout.scrollDirection = _groupDescriptor.isHorizontalBarCollectionView ?
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

@end
