/*****************************************************************************
 * VLCCarouselView.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2026 VLC authors and VideoLAN
 *
 * Authors: Serhii Bykov <esphynox@gmail.com>
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

#import "VLCCarouselView.h"

#import "extensions/NSAnimationContext+VLCAdditions.h"
#import "extensions/NSView+VLCAdditions.h"

#import <QuartzCore/QuartzCore.h>

#include <math.h>

// Minimum horizontal release velocity, in points per second, that starts inertia.
static const CGFloat VLCCarouselMinimumInertialVelocity = 40.;
// Horizontal velocity, in points per second, at which inertia stops.
static const CGFloat VLCCarouselDecelerationStopVelocity = 8.;
// Remaining velocity proportion after one second of deceleration.
static const CGFloat VLCCarouselDecelerationRate = .02;
// Time, in seconds, between inertial scrolling updates.
static const NSTimeInterval VLCCarouselDecelerationInterval = 1. / 60.;
// Duration, in seconds, of animated item-to-item navigation.
static const NSTimeInterval VLCCarouselItemScrollDuration = .2;
// Weight of the previous velocity estimate when smoothing mouse drag velocity.
static const CGFloat VLCCarouselDragVelocitySmoothingFactor = .75;
// Minimum horizontal scroll delta handled by the carousel.
static const CGFloat VLCCarouselMinimumScrollWheelDelta = .1;
// Minimum vertical scroll delta forwarded to the enclosing scroll view.
static const CGFloat VLCCarouselMinimumVerticalScrollDelta = .5;
// Minimum movement, in points, before deceleration stops at a content edge.
static const CGFloat VLCCarouselDecelerationEdgeTolerance = .01;
// Ignore sub-point differences when navigating or checking scrollability.
static const CGFloat VLCCarouselScrollPositionTolerance = 1.;

@interface VLCCarouselView ()

- (void)scrollByHorizontalDelta:(CGFloat)delta;
- (void)scrollToHorizontalOffset:(CGFloat)offset
                        animated:(BOOL)animated
                        duration:(NSTimeInterval)duration;
- (void)startDeceleratingWithVelocity:(CGFloat)velocity;
- (void)decelerate;
- (void)stopDecelerating;
- (BOOL)canScrollInDirection:(BOOL)forward;
- (NSArray<NSCollectionViewLayoutAttributes *> *)itemLayoutAttributes;

@end

@implementation VLCCarouselView
{
    NSTimer *_decelerationTimer;
    CGFloat _decelerationVelocity;
    CFTimeInterval _lastDecelerationTime;
}

#pragma mark - Lifecycle

- (void)dealloc
{
    [self stopDecelerating];
}

#pragma mark - Public methods

- (BOOL)canScrollBackward
{
    return [self canScrollInDirection:NO];
}

- (BOOL)canScrollForward
{
    return [self canScrollInDirection:YES];
}

- (void)scrollByOneItemForward:(BOOL)forward
{
    [self stopDecelerating];

    NSScrollView * const scrollView = self.enclosingScrollView;
    if (scrollView == nil) {
        return;
    }

    const CGFloat visibleOffset = NSMinX(scrollView.documentVisibleRect);
    const CGFloat leadingInset = self.leadingItemInset + scrollView.contentInsets.left;
    NSArray<NSCollectionViewLayoutAttributes *> * const attributes = self.itemLayoutAttributes;
    NSCollectionViewLayoutAttributes *targetAttributes = nil;
    NSEnumerator * const enumerator = forward ? attributes.objectEnumerator : attributes.reverseObjectEnumerator;
    for (NSCollectionViewLayoutAttributes * const attributes in enumerator) {
        const CGFloat itemOffset = NSMinX(attributes.frame) - leadingInset;
        if ((forward && itemOffset > visibleOffset + VLCCarouselScrollPositionTolerance) || (!forward && itemOffset < visibleOffset - VLCCarouselScrollPositionTolerance)) {
            targetAttributes = attributes;
            break;
        }
    }

    if (targetAttributes != nil) {
        const CGFloat targetOffset = NSMinX(targetAttributes.frame) - leadingInset;
        [self scrollToHorizontalOffset:targetOffset
                              animated:YES
                              duration:VLCCarouselItemScrollDuration];
    } else if ([self canScrollInDirection:forward]) {
        // Supplementary views may extend beyond the first or last item, or be
        // the only content when there are no recent videos.
        const CGFloat edgeOffset = [self horizontalOffsetBeyondEdgeInScrollView:scrollView forward:forward];
        [self scrollToHorizontalOffset:edgeOffset
                              animated:YES
                              duration:VLCCarouselItemScrollDuration];
    }
}

#pragma mark - Event handling

- (BOOL)acceptsFirstResponder
{
    return YES;
}

- (void)keyDown:(NSEvent *)event
{
    NSString * const characters = event.charactersIgnoringModifiers;
    if (characters.length == 0) {
        [super keyDown:event];
        return;
    }

    switch ([characters characterAtIndex:0]) {
        case NSLeftArrowFunctionKey:
            [self scrollByOneItemForward:NO];
            return;
        case NSRightArrowFunctionKey:
            [self scrollByOneItemForward:YES];
            return;
        default:
            [super keyDown:event];
            return;
    }
}

- (void)scrollWheel:(NSEvent *)event
{
    [self stopDecelerating];

    const CGFloat horizontalDelta = event.scrollingDeltaX;
    const CGFloat verticalDelta = event.scrollingDeltaY;
    const BOOL isTrackpadGesture = event.hasPreciseScrollingDeltas &&
                                  (event.phase != NSEventPhaseNone || event.momentumPhase != NSEventPhaseNone);

    if (fabs(horizontalDelta) > fabs(verticalDelta) &&
        fabs(horizontalDelta) > VLCCarouselMinimumScrollWheelDelta) {
        if (isTrackpadGesture) {
            [self scrollByHorizontalDelta:horizontalDelta];
        } else {
            [self scrollByOneItemForward:horizontalDelta < 0.];
        }
        return;
    }

    if (fabs(verticalDelta) > VLCCarouselMinimumVerticalScrollDelta &&
        fabs(horizontalDelta) <= VLCCarouselMinimumScrollWheelDelta) {
        [super scrollWheel:event];
    }
}

- (void)mouseDown:(NSEvent *)event
{
    const NSPoint downPoint = [self convertPoint:event.locationInWindow fromView:nil];
    NSIndexPath * const clickedIndexPath = [self indexPathForItemAtPoint:downPoint];
    if (clickedIndexPath != nil) {
        [self.window makeFirstResponder:self];
    }
    NSEvent * const mouseUpEvent = [self trackMouseDown:event];
    if (mouseUpEvent != nil && clickedIndexPath != nil) {
        if (mouseUpEvent.modifierFlags & NSEventModifierFlagControl) {
            [self requestContextMenuForItemAtIndexPath:clickedIndexPath event:mouseUpEvent];
            return;
        }
        [self activateItemAtIndexPath:clickedIndexPath];
    }
}

- (NSEvent *)trackMouseDown:(NSEvent *)event
{
    [self stopDecelerating];

    __block NSTimeInterval previousTimestamp = event.timestamp;
    __block CGFloat horizontalVelocity = 0.;
    NSEvent * const mouseUpEvent =
        [self trackMouseDown:event dragHandler:^(NSEvent * const trackedEvent, const BOOL didDrag) {
        const NSTimeInterval elapsed = trackedEvent.timestamp - previousTimestamp;
        if (elapsed > 0.) {
            const CGFloat instantVelocity = trackedEvent.deltaX / elapsed;
            horizontalVelocity = horizontalVelocity * VLCCarouselDragVelocitySmoothingFactor +
                instantVelocity * (1. - VLCCarouselDragVelocitySmoothingFactor);
        }
        previousTimestamp = trackedEvent.timestamp;

        if (didDrag) {
            [self scrollByHorizontalDelta:trackedEvent.deltaX];
        }
    }];

    if (mouseUpEvent == nil && fabs(horizontalVelocity) >= VLCCarouselMinimumInertialVelocity) {
        [self startDeceleratingWithVelocity:horizontalVelocity];
    }
    return mouseUpEvent;
}

- (void)rightMouseDown:(NSEvent *)event
{
    const NSPoint point = [self convertPoint:event.locationInWindow fromView:nil];
    NSIndexPath * const indexPath = [self indexPathForItemAtPoint:point];

    if (indexPath != nil && [self requestContextMenuForItemAtIndexPath:indexPath event:event]) {
        return;
    }

    [super rightMouseDown:event];
}

#pragma mark - Subclass hooks

- (void)activateItemAtIndexPath:(NSIndexPath *)indexPath
{
}

- (BOOL)requestContextMenuForItemAtIndexPath:(NSIndexPath *)indexPath event:(NSEvent *)event
{
    return NO;
}

#pragma mark - Inertial scrolling

- (void)startDeceleratingWithVelocity:(CGFloat)velocity
{
    [self stopDecelerating];
    _decelerationVelocity = velocity;
    _lastDecelerationTime = CACurrentMediaTime();

    __weak typeof(self) weakSelf = self;
    _decelerationTimer = [NSTimer scheduledTimerWithTimeInterval:VLCCarouselDecelerationInterval
                                                            repeats:YES
                                                              block:^(__unused NSTimer *timer) {
        [weakSelf decelerate];
    }];
}

- (void)decelerate
{
    const CFTimeInterval currentTime = CACurrentMediaTime();
    const CFTimeInterval elapsed = currentTime - _lastDecelerationTime;
    _lastDecelerationTime = currentTime;

    NSScrollView * const scrollView = self.enclosingScrollView;
    const CGFloat previousOffset = NSMinX(scrollView.documentVisibleRect);
    [self scrollByHorizontalDelta:_decelerationVelocity * elapsed];
    const CGFloat currentOffset = NSMinX(scrollView.documentVisibleRect);

    _decelerationVelocity *= pow(VLCCarouselDecelerationRate, elapsed);
    if (fabs(_decelerationVelocity) < VLCCarouselDecelerationStopVelocity ||
        fabs(currentOffset - previousOffset) < VLCCarouselDecelerationEdgeTolerance) {
        [self stopDecelerating];
    }
}

- (void)stopDecelerating
{
    [_decelerationTimer invalidate];
    _decelerationTimer = nil;
}

#pragma mark - Scrolling helpers

- (BOOL)canScrollInDirection:(BOOL)forward
{
    NSScrollView * const scrollView = self.enclosingScrollView;
    if (scrollView == nil || self.collectionViewLayout == nil) {
        return NO;
    }

    NSClipView * const clipView = scrollView.contentView;
    const NSRect bounds = clipView.bounds;
    if (NSWidth(bounds) <= 0.) {
        return NO;
    }

    NSRect proposedBounds = bounds;
    proposedBounds.origin.x = [self horizontalOffsetBeyondEdgeInScrollView:scrollView forward:forward];
    const CGFloat endpoint = NSMinX([clipView constrainBoundsRect:proposedBounds]);
    const CGFloat distance = forward ? endpoint - NSMinX(bounds) : NSMinX(bounds) - endpoint;
    return distance > VLCCarouselScrollPositionTolerance && [self hasItemsOrSupplementaryViews];
}

- (CGFloat)horizontalOffsetBeyondEdgeInScrollView:(NSScrollView *)scrollView
                                          forward:(BOOL)forward
{
    return forward
        ? NSMaxX(self.frame) + fabs(scrollView.contentInsets.right)
        : NSMinX(self.frame) - NSWidth(scrollView.contentView.bounds) - fabs(scrollView.contentInsets.left);
}

- (BOOL)hasItemsOrSupplementaryViews
{
    for (NSInteger section = 0; section < self.numberOfSections; ++section) {
        if ([self numberOfItemsInSection:section] > 0) {
            return YES;
        }
    }

    NSArray<NSCollectionViewLayoutAttributes *> * const attributes =
        [self.collectionViewLayout layoutAttributesForElementsInRect:self.bounds];
    for (NSCollectionViewLayoutAttributes * const element in attributes) {
        if (element.representedElementCategory == NSCollectionElementCategorySupplementaryView &&
            !element.hidden &&
            !NSIsEmptyRect(element.frame)) {
            return YES;
        }
    }
    return NO;
}

- (void)scrollByHorizontalDelta:(CGFloat)delta
{
    NSScrollView * const scrollView = self.enclosingScrollView;
    if (scrollView == nil) {
        return;
    }

    const NSRect visibleRect = scrollView.documentVisibleRect;
    [self scrollToHorizontalOffset:NSMinX(visibleRect) - delta animated:NO duration:0.];
}

- (void)scrollToHorizontalOffset:(CGFloat)offset
                         animated:(BOOL)animated
                         duration:(NSTimeInterval)duration
{
    NSScrollView * const scrollView = self.enclosingScrollView;
    if (scrollView == nil) {
        return;
    }

    NSClipView * const clipView = scrollView.contentView;
    NSRect proposedBounds = clipView.bounds;
    proposedBounds.origin.x = offset;
    const NSPoint point = [clipView constrainBoundsRect:proposedBounds].origin;

    if (!animated) {
        [clipView scrollToPoint:point];
        [scrollView reflectScrolledClipView:clipView];
        return;
    }

    [NSAnimationContext runAnimationRespectingPreferencesWithDuration:duration
                                                            changes:^(NSAnimationContext * const context) {
        context.timingFunction = [CAMediaTimingFunction functionWithName:kCAMediaTimingFunctionEaseInEaseOut];
        [[clipView animator] setBoundsOrigin:point];
    } completionHandler:^{
        [scrollView reflectScrolledClipView:clipView];
    }];
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

@end
