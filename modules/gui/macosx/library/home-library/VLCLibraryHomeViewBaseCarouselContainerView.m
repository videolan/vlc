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
#import "extensions/NSView+VLCAdditions.h"

#import "library/VLCLibraryCollectionView.h"
#import "library/VLCLibraryCarouselView.h"
#import "library/VLCLibraryCarouselCollectionViewItem.h"
#import "library/VLCLibraryCollectionViewDelegate.h"
#import "library/VLCLibraryCollectionViewFlowLayout.h"

#import "views/VLCSubScrollView.h"
#import "views/VLCUIUnits.h"

@interface VLCLibraryHomeViewBaseCarouselContainerView ()
{
    CGFloat _itemHeight;
}

@property (readonly) NSLayoutConstraint *heightConstraint;
@end

@implementation VLCLibraryHomeViewBaseCarouselContainerView

@synthesize constraintsWithSuperview = _constraintsWithSuperview;

- (void)dealloc
{
    [NSNotificationCenter.defaultCenter removeObserver:self];
}

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

    _collectionViewLayout = [[VLCLibraryCollectionViewFlowLayout alloc] init];
    _collectionViewLayout.scrollDirection = NSCollectionViewScrollDirectionHorizontal;
    _collectionViewLayout.sectionInset = NSEdgeInsetsMake(0., VLCUIUnits.mediumSpacing, 0., VLCUIUnits.mediumSpacing);
    _collectionViewLayout.minimumLineSpacing = VLCUIUnits.mediumSpacing;
    _collectionViewLayout.minimumInteritemSpacing = VLCUIUnits.mediumSpacing;

    _collectionView = [[VLCLibraryCarouselView alloc] initWithFrame:NSZeroRect];
    _collectionView.postsFrameChangedNotifications = YES;
    _collectionView.collectionViewLayout = _collectionViewLayout;
    [_collectionView registerClass:VLCLibraryCarouselCollectionViewItem.class
            forItemWithIdentifier:VLCLibraryCarouselCollectionViewItemIdentifier];
    _collectionView.selectable = NO;

    _collectionViewDelegate = [[VLCLibraryCollectionViewDelegate alloc] init];
    _collectionViewDelegate.dynamicItemSizing = NO;
    _collectionView.delegate = _collectionViewDelegate;

    _scrollView = [[VLCSubScrollView alloc] init];
    _scrollView.scrollSelf = YES;
    _scrollView.scrollParentY = YES;
    _scrollView.forceHideVerticalScroller = YES;
    _scrollView.forceHideHorizontalScroller = YES;
    _scrollView.drawsBackground = NO;
    _scrollView.translatesAutoresizingMaskIntoConstraints = NO;
    _scrollView.documentView = _collectionView;
    [self addSubview:self.scrollView];
    [NSLayoutConstraint activateConstraints:@[
        [self.leadingAnchor constraintEqualToAnchor:self.scrollView.leadingAnchor constant:VLCUIUnits.mediumSpacing],
        [self.trailingAnchor constraintEqualToAnchor:self.scrollView.trailingAnchor constant:-VLCUIUnits.mediumSpacing],
        [self.titleView.bottomAnchor constraintEqualToAnchor:self.scrollView.topAnchor constant:-VLCUIUnits.mediumSpacing],
        [self.bottomAnchor constraintEqualToAnchor:self.scrollView.bottomAnchor]
    ]];

    const CGFloat buttonSize = VLCUIUnits.largeSpacing;

    NSImage * const leftImage = [NSImage imageNamed:NSImageNameGoLeftTemplate];
    _leftButton = [[NSButton alloc] initWithFrame:NSZeroRect];
    self.leftButton.translatesAutoresizingMaskIntoConstraints = NO;
    self.leftButton.image = leftImage;
    self.leftButton.target = self;
    self.leftButton.action = @selector(scrollLeft:);
    [self addSubview:self.leftButton];
    [NSLayoutConstraint activateConstraints:@[
        [self.leftButton.leadingAnchor constraintEqualToAnchor:self.scrollView.leadingAnchor],
        [self.leftButton.centerYAnchor constraintEqualToAnchor:self.scrollView.centerYAnchor],
        [self.leftButton.widthAnchor constraintEqualToConstant:buttonSize],
        [self.leftButton.heightAnchor constraintEqualToConstant:buttonSize],
    ]];

    NSImage * const rightImage = [NSImage imageNamed:NSImageNameGoRightTemplate];
    _rightButton = [[NSButton alloc] initWithFrame:NSZeroRect];
    self.rightButton.translatesAutoresizingMaskIntoConstraints = NO;
    self.rightButton.image = rightImage;
    self.rightButton.target = self;
    self.rightButton.action = @selector(scrollRight:);
    [self addSubview:self.rightButton];
    [NSLayoutConstraint activateConstraints:@[
        [self.rightButton.trailingAnchor constraintEqualToAnchor:self.scrollView.trailingAnchor],
        [self.rightButton.centerYAnchor constraintEqualToAnchor:self.scrollView.centerYAnchor],
        [self.rightButton.widthAnchor constraintEqualToConstant:buttonSize],
        [self.rightButton.heightAnchor constraintEqualToConstant:buttonSize],
    ]];

#if __MAC_OS_X_VERSION_MAX_ALLOWED >= 260000
    if (@available(macOS 26.0, *)) {
        self.leftButton.bordered = YES;
        self.leftButton.bezelStyle = NSBezelStyleGlass;
        self.leftButton.borderShape = NSControlBorderShapeCircle;
        self.rightButton.bordered = YES;
        self.rightButton.bezelStyle = NSBezelStyleGlass;
        self.rightButton.borderShape = NSControlBorderShapeCircle;
    } else {
#endif
        self.leftButton.bezelStyle = NSBezelStyleCircular;
        self.rightButton.bezelStyle = NSBezelStyleCircular;
#if __MAC_OS_X_VERSION_MAX_ALLOWED >= 260000
    }
#endif

    _itemHeight = VLCUIUnits.carouselViewItemViewHeight;
    _collectionViewDelegate.staticItemSize = NSMakeSize(_itemHeight, _itemHeight);

    [self updateCarouselViewHeight];
    [self updateCarouselButtonVisibility];

    self.scrollView.contentView.postsBoundsChangedNotifications = YES;
    [NSNotificationCenter.defaultCenter addObserver:self
                                           selector:@selector(scrollViewBoundsChanged:)
                                               name:NSViewBoundsDidChangeNotification
                                             object:self.scrollView.contentView];
    [NSNotificationCenter.defaultCenter addObserver:self
                                           selector:@selector(scrollViewBoundsChanged:)
                                               name:NSViewFrameDidChangeNotification
                                             object:self.collectionView];
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
    if (self.heightConstraint == nil) {
        _heightConstraint = [self.scrollView.heightAnchor constraintEqualToConstant:_itemHeight];
        self.heightConstraint.active = YES;
    } else {
        self.heightConstraint.constant = _itemHeight;
    }
}

- (void)updateCarouselButtonVisibility
{
    const NSRect visibleRect = self.scrollView.documentVisibleRect;
    const NSRect documentRect = self.collectionView.frame;
    self.leftButton.hidden = NSMinX(visibleRect) <= NSMinX(documentRect);
    self.rightButton.hidden = NSMaxX(visibleRect) >= NSMaxX(documentRect);
}

- (void)scrollViewBoundsChanged:(NSNotification *)notification
{
    [self updateCarouselButtonVisibility];
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
    self.collectionViewDelegate.staticItemSize = NSMakeSize(self.collectionViewDelegate.staticItemSize.width,
                                                            itemHeight);
    [self updateCarouselViewHeight];
}

- (void)scrollLeft:(__unused id)sender
{
    [self.collectionView scrollByOneItemForward:NO];
}

- (void)scrollRight:(__unused id)sender
{
    [self.collectionView scrollByOneItemForward:YES];
}

@end
