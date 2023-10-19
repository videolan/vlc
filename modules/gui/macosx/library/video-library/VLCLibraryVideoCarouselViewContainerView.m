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

#import "extensions/NSFont+VLCAdditions.h"
#import "extensions/NSString+Helpers.h"

#import "library/VLCLibraryUIUnits.h"
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
    _titleView = [[NSTextField alloc] init];
    self.titleView.font = NSFont.VLClibrarySectionHeaderFont;
    self.titleView.textColor = NSColor.headerTextColor;
    self.titleView.selectable = NO;
    self.titleView.bordered = NO;
    self.titleView.drawsBackground = NO;
    [self addSubview:self.titleView];
    self.titleView.translatesAutoresizingMaskIntoConstraints = NO;
    [NSLayoutConstraint activateConstraints:@[
        [self.leadingAnchor constraintEqualToAnchor:self.titleView.leadingAnchor],
        [self.trailingAnchor constraintEqualToAnchor:self.titleView.trailingAnchor],
        [self.topAnchor constraintEqualToAnchor:self.titleView.topAnchor],
    ]];

    _carouselView = [[iCarousel alloc] initWithFrame:self.bounds];
    self.carouselView.delegate = self;
    [self addSubview:self.carouselView];
    self.carouselView.translatesAutoresizingMaskIntoConstraints = NO;
    [NSLayoutConstraint activateConstraints:@[
        [self.leadingAnchor constraintEqualToAnchor:self.carouselView.leadingAnchor],
        [self.trailingAnchor constraintEqualToAnchor:self.carouselView.trailingAnchor],
        [self.titleView.bottomAnchor constraintEqualToAnchor:self.carouselView.topAnchor], // titleView bottom
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
    self.titleView.stringValue = groupDescriptor.name;
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
    const CGFloat leadingPadding = VLCLibraryUIUnits.largeSpacing;
    const CGFloat itemWidth = self.carouselView.itemWidth;
    const CGFloat horizontalOffset = (-(widthToFirstItemCenter - itemWidth / 2)) + leadingPadding;
    self.carouselView.contentOffset = NSMakeSize(horizontalOffset, 0);
}

- (void)resizeWithOldSuperviewSize:(NSSize)oldSize
{
    [super resizeWithOldSuperviewSize:oldSize];
    [self updateCarouselOffset];
}

// pragma mark - iCarousel delegate methods

- (CGFloat)carousel:(iCarousel *)carousel
     valueForOption:(iCarouselOption)option
        withDefault:(CGFloat)value
{
    switch (option) {
    case iCarouselOptionSpacing:
    {
        // iCarousel calculates spacing as a multiplier on the item width.
        // So a spacing of 1.2 means the item's width will grow to 1.2x its
        // width, with the extra width as spacing.
        //
        // Because of this... interesting approach to spacing, we calculate
        // the constant VLC-wide spacing relative to the width of the carousel's
        // itemWidth.
        const CGFloat itemWidth = carousel.itemWidth;
        const CGFloat bothSidesSpacing = VLCLibraryUIUnits.mediumSpacing * 2;
        const CGFloat desiredWidthWithSpacing = itemWidth + bothSidesSpacing;
        const CGFloat desiredMultiple = desiredWidthWithSpacing / itemWidth;
        return desiredMultiple;
    }
    case iCarouselOptionWrap:
        return YES;
    default:
        return value;
    }
}

@end
