/*****************************************************************************
 * VLCLibraryHomeViewBaseCarouselContainerView.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2024 VLC authors and VideoLAN
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

#import "VLCLibraryHomeViewBaseCarouselContainerView.h"

#import "extensions/NSFont+VLCAdditions.h"
#import "extensions/NSString+Helpers.h"
#import "extensions/NSTextField+VLCAdditions.h"

#import "library/VLCLibraryDataTypes.h"
#import "library/VLCLibraryCarouselViewItemView.h"
#import "library/VLCLibraryUIUnits.h"

@interface VLCLibraryHomeViewBaseCarouselContainerView ()
{
    CGFloat _itemHeight;
}

@property (readonly) NSLayoutConstraint *heightConstraint;
@property (readwrite) VLCLibraryCarouselViewItemView *selectedItemView;

@end

@implementation VLCLibraryHomeViewBaseCarouselContainerView

@synthesize constraintsWithSuperview = _constraintsWithSuperview;

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
    [self connect];
}

- (void)setupView
{
    _titleView = [NSTextField defaultLabelWithString:@""];
    self.titleView.font = NSFont.VLClibrarySectionHeaderFont;
    self.titleView.textColor = NSColor.headerTextColor;
    [self addSubview:self.titleView];
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

    const CGFloat buttonWidth = VLCLibraryUIUnits.largeSpacing;
    const CGFloat buttonHeight = 
        VLCLibraryUIUnits.carouselViewItemViewHeight - VLCLibraryUIUnits.largeSpacing;

    NSImage * const leftImage = [NSImage imageNamed:@"NSGoLeftTemplate"];
    _leftButton = [[NSButton alloc] initWithFrame:NSZeroRect];
    self.leftButton.translatesAutoresizingMaskIntoConstraints = NO;
    self.leftButton.image = leftImage;
    self.leftButton.bezelStyle = NSBezelStyleCircular;
    self.leftButton.target = self;
    self.leftButton.action = @selector(scrollLeft:);
    [self addSubview:self.leftButton];
    [NSLayoutConstraint activateConstraints:@[
        [self.leftButton.leadingAnchor constraintEqualToAnchor:self.leadingAnchor],
        [self.leftButton.centerYAnchor constraintEqualToAnchor:self.carouselView.centerYAnchor]
    ]];

    NSImage * const rightImage = [NSImage imageNamed:@"NSGoRightTemplate"];
    _rightButton = [[NSButton alloc] initWithFrame:NSZeroRect];
    self.rightButton.translatesAutoresizingMaskIntoConstraints = NO;
    self.rightButton.image = rightImage;
    self.rightButton.bezelStyle = NSBezelStyleCircular;
    self.rightButton.target = self;
    self.rightButton.action = @selector(scrollRight:);
    [self addSubview:self.rightButton];
    [NSLayoutConstraint activateConstraints:@[
        [self.rightButton.trailingAnchor constraintEqualToAnchor:self.carouselView.trailingAnchor],
        [self.rightButton.centerYAnchor constraintEqualToAnchor:self.carouselView.centerYAnchor]
    ]];

    _itemHeight = VLCLibraryUIUnits.carouselViewItemViewHeight;

    [self updateCarouselViewHeight];
    [self updateCarouselOffset];
    [self updateCarouselButtonVisibility];
}

- (void)connect
{
    [self.dataSource connect];
}

- (void)disconnect
{
    [self.dataSource disconnect];
}

- (void)updateCarouselViewHeight
{
    const CGFloat viewHeight = self.titleView.frame.size.height +
                               VLCLibraryUIUnits.largeSpacing * 2 +
                               _itemHeight;

    if (self.heightConstraint == nil) {
        _heightConstraint = [self.carouselView.heightAnchor constraintEqualToConstant:viewHeight];
        self.heightConstraint.active = YES;
    } else {
        self.heightConstraint.constant = viewHeight;
    }
}

- (void)updateCarouselOffset
{
    const CGFloat widthToFirstItemCenter = self.frame.size.width / 2;
    const CGFloat itemWidth = self.carouselView.itemWidth;
    const CGFloat horizontalOffset = (-(widthToFirstItemCenter - itemWidth / 2));
    self.carouselView.contentOffset = NSMakeSize(horizontalOffset, 0);
}

- (void)updateCarouselButtonVisibility
{
    if (self.carouselView.numberOfItems <= 1) {
        self.leftButton.hidden = YES;
        self.rightButton.hidden = YES;
        return;
    }

    const NSInteger currentItemIndex = self.carouselView.currentItemIndex;
    const NSInteger numberOfItems = self.carouselView.numberOfItems;
    self.leftButton.hidden = currentItemIndex == 0;
    self.rightButton.hidden = currentItemIndex == numberOfItems - 1;
}

- (void)resizeWithOldSuperviewSize:(NSSize)oldSize
{
    [super resizeWithOldSuperviewSize:oldSize];
    [self updateCarouselOffset];
}

- (void)presentLibraryItem:(id<VLCMediaLibraryItemProtocol>)libraryItem
{
    if (libraryItem == nil) {
        return;
    }

    NSIndexPath * const itemIndexPath = [self.dataSource indexPathForLibraryItem:libraryItem];
    if (itemIndexPath == nil) {
        return;
    }

    const NSInteger itemIndex = itemIndexPath.item;
    [self.carouselView scrollToItemAtIndex:itemIndex animated:YES];
}

- (CGFloat)itemHeight
{
    return _itemHeight;
}

- (void)setItemHeight:(CGFloat)itemHeight
{
    if (itemHeight == self.itemHeight) {
        return;
    }

    _itemHeight = itemHeight;
    [self updateCarouselViewHeight];
}

- (void)scrollLeft:(id)sender
{
    [self.carouselView scrollToItemAtIndex:self.carouselView.currentItemIndex - 1 animated:YES];
}

- (void)scrollRight:(id)sender
{
    [self.carouselView scrollToItemAtIndex:self.carouselView.currentItemIndex + 1 animated:YES];
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
    default:
        return value;
    }
}

- (void)carouselCurrentItemIndexDidChange:(iCarousel *)carousel
{
    NSView * const currentItemView = carousel.currentItemView;
    if (currentItemView == nil) {
        return;
    }

    VLCLibraryCarouselViewItemView * const carouselItemView = (VLCLibraryCarouselViewItemView *)currentItemView;
    NSAssert(carouselItemView != nil, @"Expected carousel item view to be non-nil!");
    self.selectedItemView.selected = NO;
    carouselItemView.selected = YES;
    self.selectedItemView = carouselItemView;

    [self updateCarouselButtonVisibility];
}

- (void)carousel:(iCarousel *)carousel didSelectItemAtIndex:(NSInteger)index
{
    VLCLibraryCarouselViewItemView * const carouselItemView = (VLCLibraryCarouselViewItemView *)[carousel itemViewAtIndex:index];
    NSAssert(carouselItemView != nil, @"Expected carousel item view to be non-nil!");
    [carouselItemView playRepresentedItem];
}

@end
