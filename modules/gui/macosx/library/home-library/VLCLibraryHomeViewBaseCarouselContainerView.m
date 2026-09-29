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
#import "library/VLCLibraryCollectionViewDelegate.h"
#import "library/VLCLibraryCollectionViewFlowLayout.h"
#import "library/VLCLibraryCollectionViewItem.h"
#import "library/VLCLibraryCarouselViewItemView.h"
#import "library/VLCLibraryRepresentedItem.h"

#import "views/VLCSubScrollView.h"
#import "views/VLCUIUnits.h"

typedef NS_ENUM(NSUInteger, VLCLibraryHomeScrollAxis) {
    VLCLibraryHomeScrollAxisUndecided = 0,
    VLCLibraryHomeScrollAxisHorizontal,
};

static const CGFloat VLCLibraryHomeMouseDragThreshold = 3.;

@interface VLCLibraryHomeViewHorizontalCollectionView : VLCLibraryCollectionView
{
    VLCLibraryHomeScrollAxis _scrollAxis;
    BOOL _handlingMouseActivation;
}

@property (readonly, getter=isHandlingMouseActivation) BOOL handlingMouseActivation;

- (void)activateItemAtIndexPath:(NSIndexPath *)indexPath;
@end

@implementation VLCLibraryHomeViewHorizontalCollectionView

- (void)scrollWheel:(NSEvent *)event
{
    const CGFloat horizontalDelta = event.scrollingDeltaX;
    const CGFloat verticalDelta = event.scrollingDeltaY;
    const BOOL isTrackpadGesture = event.hasPreciseScrollingDeltas &&
                                  (event.phase != NSEventPhaseNone || event.momentumPhase != NSEventPhaseNone);

    if (!isTrackpadGesture) {
        if (fabs(horizontalDelta) > fabs(verticalDelta) && fabs(horizontalDelta) > 0.1) {
            [self scrollByOneItemForward:horizontalDelta < 0.];
            return;
        }
        [super scrollWheel:event];
        return;
    }

    // Vertical events are forwarded to the parent scroll view. It owns their
    // gesture lifecycle, so do not retain a vertical lock here: the ending
    // event may not be sent back to this nested collection view.
    if (event.phase == NSEventPhaseBegan) {
        _scrollAxis = VLCLibraryHomeScrollAxisUndecided;
    }

    if (_scrollAxis == VLCLibraryHomeScrollAxisUndecided) {
        if (fabs(horizontalDelta) > fabs(verticalDelta)) {
            _scrollAxis = VLCLibraryHomeScrollAxisHorizontal;
        }
    }

    if (_scrollAxis == VLCLibraryHomeScrollAxisHorizontal) {
        [self scrollByHorizontalDelta:horizontalDelta];
        if (event.phase == NSEventPhaseEnded || event.phase == NSEventPhaseCancelled ||
            event.momentumPhase == NSEventPhaseEnded || event.momentumPhase == NSEventPhaseCancelled) {
            _scrollAxis = VLCLibraryHomeScrollAxisUndecided;
        }
        return;
    }

    [super scrollWheel:event];
}

- (void)mouseDown:(NSEvent *)event
{
    const NSPoint downPoint = [self convertPoint:event.locationInWindow fromView:nil];
    NSIndexPath * const clickedIndexPath = [self indexPathForItemAtPoint:downPoint];
    BOOL didDrag = NO;

    while (true) {
        NSEvent * const trackedEvent =
            [self.window nextEventMatchingMask:NSEventMaskLeftMouseDragged | NSEventMaskLeftMouseUp
                                      untilDate:NSDate.distantFuture
                                         inMode:NSEventTrackingRunLoopMode
                                        dequeue:YES];
        const NSPoint point = [self convertPoint:trackedEvent.locationInWindow fromView:nil];

        if (trackedEvent.type == NSEventTypeLeftMouseDragged) {
            if (!didDrag && fabs(point.x - downPoint.x) >= VLCLibraryHomeMouseDragThreshold) {
                didDrag = YES;
            }
            if (didDrag) {
                [self scrollByHorizontalDelta:trackedEvent.deltaX];
            }
            continue;
        }

        break;
    }

    if (!didDrag && clickedIndexPath != nil) {
        [self activateItemAtIndexPath:clickedIndexPath];
    }
}

- (BOOL)isHandlingMouseActivation
{
    return _handlingMouseActivation;
}

- (void)activateItemAtIndexPath:(NSIndexPath *)indexPath
{
    _handlingMouseActivation = YES;
    self.selectionIndexPaths = [NSSet setWithObject:indexPath];
    _handlingMouseActivation = NO;

    id const item = [self itemAtIndexPath:indexPath];
    if ([item respondsToSelector:@selector(playRepresentedItem)]) {
        [item playRepresentedItem];
    }
}

- (void)scrollByHorizontalDelta:(CGFloat)delta
{
    NSScrollView * const scrollView = self.enclosingScrollView;
    if (scrollView == nil) {
        return;
    }

    const NSRect visibleRect = scrollView.documentVisibleRect;
    const CGFloat minimumOffset = NSMinX(self.frame);
    const CGFloat maximumOffset = MAX(NSMaxX(self.frame) - NSWidth(visibleRect), minimumOffset);
    const CGFloat offset = MIN(MAX(NSMinX(visibleRect) - delta, minimumOffset), maximumOffset);
    [scrollView.contentView scrollToPoint:NSMakePoint(offset, NSMinY(visibleRect))];
    [scrollView reflectScrolledClipView:scrollView.contentView];
}

- (NSArray<NSCollectionViewLayoutAttributes *> *)itemLayoutAttributes
{
    NSArray<NSCollectionViewLayoutAttributes *> * const attributes =
        [self.collectionViewLayout layoutAttributesForElementsInRect:self.bounds];
    NSPredicate * const itemPredicate = [NSPredicate predicateWithBlock:^BOOL(
        NSCollectionViewLayoutAttributes * const attributes, NSDictionary * const __unused bindings
    ) {
        return attributes.representedElementCategory == NSCollectionElementCategoryItem;
    }];
    return [[attributes filteredArrayUsingPredicate:itemPredicate]
        sortedArrayUsingComparator:^NSComparisonResult(
            NSCollectionViewLayoutAttributes * const first,
            NSCollectionViewLayoutAttributes * const second
        ) {
            const CGFloat firstX = NSMinX(first.frame);
            const CGFloat secondX = NSMinX(second.frame);
            return firstX < secondX ? NSOrderedAscending : firstX > secondX ? NSOrderedDescending : NSOrderedSame;
        }];
}

- (void)scrollByOneItemForward:(BOOL)forward
{
    NSScrollView * const scrollView = self.enclosingScrollView;
    if (scrollView == nil) {
        return;
    }

    const CGFloat visibleOffset = NSMinX(scrollView.documentVisibleRect);
    NSArray<NSCollectionViewLayoutAttributes *> * const attributes = self.itemLayoutAttributes;
    VLCLibraryCollectionViewFlowLayout * const layout =
        (VLCLibraryCollectionViewFlowLayout *)self.collectionViewLayout;
    const CGFloat leadingInset = layout.sectionInset.left;
    NSCollectionViewLayoutAttributes *targetAttributes = nil;
    NSEnumerator * const enumerator = forward ? attributes.objectEnumerator : attributes.reverseObjectEnumerator;
    for (NSCollectionViewLayoutAttributes * const attributes in enumerator) {
        const CGFloat itemOffset = MAX(NSMinX(attributes.frame) - leadingInset, NSMinX(self.frame));
        if ((forward && itemOffset > visibleOffset + 1.) ||
            (!forward && itemOffset < visibleOffset - 1.)) {
            targetAttributes = attributes;
            break;
        }
    }

    if (targetAttributes != nil) {
        const CGFloat targetOffset = MAX(NSMinX(targetAttributes.frame) - leadingInset,
                                         NSMinX(self.frame));
        [self scrollByHorizontalDelta:visibleOffset - targetOffset];
    }
}

@end

@interface VLCLibraryHomeCarouselCollectionViewItem : NSCollectionViewItem

@property (readwrite, strong, nonatomic) VLCLibraryRepresentedItem *representedItem;

@end

@implementation VLCLibraryHomeCarouselCollectionViewItem
{
    VLCLibraryCarouselViewItemView *_carouselItemView;
}

@synthesize representedItem = _representedItem;

- (void)loadView
{
    _carouselItemView = [VLCLibraryCarouselViewItemView fromNibWithOwner:self];
    self.view = _carouselItemView;
}

- (void)setRepresentedItem:(VLCLibraryRepresentedItem *)representedItem
{
    _representedItem = representedItem;
    _carouselItemView.representedItem = representedItem;
}

- (void)setSelected:(BOOL)selected
{
    super.selected = selected;
    _carouselItemView.selected = selected;
}

- (void)prepareForReuse
{
    [super prepareForReuse];
    [_carouselItemView prepareForReuse];
}

- (void)playRepresentedItem
{
    [_carouselItemView playRepresentedItem];
}

@end

@interface VLCLibraryHomeViewCollectionViewDelegate : VLCLibraryCollectionViewDelegate
@end

@implementation VLCLibraryHomeViewCollectionViewDelegate

- (void)collectionView:(NSCollectionView *)collectionView
didSelectItemsAtIndexPaths:(NSSet<NSIndexPath *> *)indexPaths
{
    if ([collectionView isKindOfClass:VLCLibraryHomeViewHorizontalCollectionView.class] &&
        ((VLCLibraryHomeViewHorizontalCollectionView *)collectionView).handlingMouseActivation) {
        return;
    }

    if (collectionView.selectionIndexPaths.count != 1) {
        return;
    }

    NSIndexPath * const indexPath = indexPaths.anyObject;
    VLCLibraryHomeCarouselCollectionViewItem * const item =
        (VLCLibraryHomeCarouselCollectionViewItem *)[collectionView itemAtIndexPath:indexPath];
    [item playRepresentedItem];
}

@end

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

    _collectionView = [[VLCLibraryHomeViewHorizontalCollectionView alloc] initWithFrame:NSZeroRect];
    _collectionView.postsFrameChangedNotifications = YES;
    _collectionView.collectionViewLayout = _collectionViewLayout;
    _collectionView.selectable = YES;
    _collectionView.allowsEmptySelection = YES;
    _collectionView.allowsMultipleSelection = NO;

    _collectionViewDelegate = [[VLCLibraryHomeViewCollectionViewDelegate alloc] init];
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

- (void)registerCollectionViewItemClass
{
    [self.collectionView registerClass:VLCLibraryHomeCarouselCollectionViewItem.class
                forItemWithIdentifier:VLCLibraryCollectionViewItemIdentifier];
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

- (void)presentLibraryItem:(id<VLCMediaLibraryItemProtocol>)libraryItem
{
    if (libraryItem == nil) {
        return;
    }

    NSIndexPath * const itemIndexPath = [self.dataSource indexPathForLibraryItem:libraryItem];
    if (itemIndexPath == nil) {
        return;
    }

    [self.collectionView scrollToItemsAtIndexPaths:[NSSet setWithObject:itemIndexPath]
                                    scrollPosition:NSCollectionViewScrollPositionCenteredHorizontally];
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
    [(VLCLibraryHomeViewHorizontalCollectionView *)self.collectionView scrollByOneItemForward:NO];
}

- (void)scrollRight:(__unused id)sender
{
    [(VLCLibraryHomeViewHorizontalCollectionView *)self.collectionView scrollByOneItemForward:YES];
}

@end
