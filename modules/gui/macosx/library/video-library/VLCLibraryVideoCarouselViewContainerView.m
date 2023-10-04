/*****************************************************************************
 * VLCLibraryVideoCarouselViewContainerView.m: MacOS X interface module
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

#import "VLCLibraryVideoCarouselViewContainerView.h"

#import "library/video-library/VLCLibraryVideoCollectionViewContainerViewDataSource.h"

#import "views/iCarousel/iCarousel.h"

@implementation VLCLibraryVideoCarouselViewContainerView

@synthesize groupDescriptor = _groupDescriptor;
@synthesize videoGroup = _videoGroup;
@synthesize constraintsWithSuperview = _constraintsWithSuperview;
@synthesize dataSource = _dataSource;

- (instancetype)init
{
    self = [super init];
    if (self) {
        [self setup];
    }
    return self;
}

- (instancetype)initWithFrame:(NSRect)frameRect
{
    self = [super initWithFrame:frameRect];
    if (self) {
        [self setup];
    }
    return self;
}

- (instancetype)initWithCoder:(NSCoder *)coder
{
    self = [super initWithCoder:coder];
    if (self) {
        [self setup];
    }
    return self;
}

- (void)awakeFromNib
{
    [super awakeFromNib];
    [self setup];
}

- (void)setup
{
    [self setupView];
    [self setupDataSource];
}

- (void)setupView
{
    _carouselView = [[iCarousel alloc] initWithFrame:self.bounds];
    self.carouselView.translatesAutoresizingMaskIntoConstraints = NO;
    [self addSubview:self.carouselView];

    [NSLayoutConstraint activateConstraints:@[
        [self.leadingAnchor constraintEqualToAnchor:self.carouselView.leadingAnchor],
        [self.trailingAnchor constraintEqualToAnchor:self.carouselView.trailingAnchor],
        [self.topAnchor constraintEqualToAnchor:self.carouselView.topAnchor],
        [self.bottomAnchor constraintEqualToAnchor:self.carouselView.bottomAnchor]
    ]];
    NSLayoutConstraint * const heightConstraint = [self.carouselView.heightAnchor constraintEqualToConstant:300];
    heightConstraint.active = YES;

    [self updateCarouselOffset];
}

- (void)setupDataSource
{
    _dataSource = [[VLCLibraryVideoCollectionViewContainerViewDataSource alloc] init];
    self.dataSource.carouselView = self.carouselView;
    [self.dataSource setup];
}

- (void)setGroupDescriptor:(VLCLibraryVideoCollectionViewGroupDescriptor *)groupDescriptor
{
    _groupDescriptor = groupDescriptor;
    _videoGroup = groupDescriptor.group;
    self.dataSource.groupDescriptor = groupDescriptor;
 }

- (void)setVideoGroup:(VLCMediaLibraryParentGroupType)group
{
    if (_groupDescriptor.group == group) {
        return;
    }

    VLCLibraryVideoCollectionViewGroupDescriptor * const descriptor = [[VLCLibraryVideoCollectionViewGroupDescriptor alloc] initWithVLCVideoLibraryGroup:group];
    [self setGroupDescriptor:descriptor];
}

- (void)updateCarouselOffset
{
    const CGFloat widthToFirstItemCenter = self.frame.size.width / 2;
    const CGFloat itemWidth = self.carouselView.itemWidth;
    const CGFloat horizontalOffset = -(widthToFirstItemCenter - itemWidth / 2);
    self.carouselView.contentOffset = NSMakeSize(horizontalOffset, 0);
}

- (void)resizeWithOldSuperviewSize:(NSSize)oldSize
{
    [super resizeWithOldSuperviewSize:oldSize];
    [self updateCarouselOffset];
}

@end
