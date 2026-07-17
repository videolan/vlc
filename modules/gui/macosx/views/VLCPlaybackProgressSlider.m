/*****************************************************************************
 * VLCPlaybackProgressSlider.m
 *****************************************************************************
 * Copyright (C) 2017 VLC authors and VideoLAN
 *
 * Authors: Marvin Scholz <epirat07 at gmail dot com>
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

#import "VLCPlaybackProgressSlider.h"

#import "extensions/NSString+Helpers.h"
#import "extensions/NSView+VLCAdditions.h"
#import "main/CompatibilityFixes.h"
#import "views/VLCPlaybackProgressSliderCell.h"

@implementation VLCPlaybackProgressSlider {
    NSTrackingArea *_hoverTrackingArea;
    NSWindow *_hoverWindow;
    NSTextField *_hoverLabel;
}

+ (Class)cellClass
{
    return VLCPlaybackProgressSliderCell.class;
}

- (instancetype)initWithCoder:(NSCoder *)coder
{
    self = [super initWithCoder:coder];

    if (self) {
        NSAssert([self.cell isKindOfClass:[VLCPlaybackProgressSlider cellClass]],
                 @"VLCPlaybackProgressSlider cell is not a VLCPlaybackProgressSliderCell");
        self.scrollable = YES;
        if (@available(macOS 10.14, *)) {
            [self viewDidChangeEffectiveAppearance];
            self.clipsToBounds = YES;
        } else {
            [(VLCPlaybackProgressSliderCell*)self.cell setSliderStyleLight];
        }

    }
    return self;
}

- (void)scrollWheel:(NSEvent *)event
{
    if (!self.scrollable) {
        return [super scrollWheel:event];
    }

    double increment;
    CGFloat deltaY = [event scrollingDeltaY];
    double range = [self maxValue] - [self minValue];

    // Scroll less for high precision, else it's too fast
    if (event.hasPreciseScrollingDeltas) {
        increment = (range * 0.002) * deltaY;
    } else {
        if (deltaY == 0.0) {
            return;
        }
        increment = (range * 0.01 * deltaY);
    }

    // If scrolling is inversed, increment in other direction
    if (!event.isDirectionInvertedFromDevice) {
        increment = -increment;
    }

    self.doubleValue = self.doubleValue - increment;
    [self sendAction:self.action to:self.target];
}

// Workaround for 10.7
// http://stackoverflow.com/questions/3985816/custom-nsslidercell
- (void)setNeedsDisplayInRect:(NSRect)invalidRect
{
    [super setNeedsDisplayInRect:self.bounds];
}

- (BOOL)indefinite
{
    return [(VLCPlaybackProgressSliderCell*)self.cell indefinite];
}

- (void)setIndefinite:(BOOL)indefinite
{
    [(VLCPlaybackProgressSliderCell*)self.cell setIndefinite:indefinite];
}

- (BOOL)knobHidden
{
    return [(VLCPlaybackProgressSliderCell*)self.cell knobHidden];
}

- (void)setKnobHidden:(BOOL)knobHidden
{
    [(VLCPlaybackProgressSliderCell*)self.cell setKnobHidden:knobHidden];
}

- (BOOL)isFlipped
{
    return NO;
}

- (void)viewDidChangeEffectiveAppearance
{
    if (self.shouldShowDarkAppearance) {
        [(VLCPlaybackProgressSliderCell*)self.cell setSliderStyleDark];
    } else {
        [(VLCPlaybackProgressSliderCell*)self.cell setSliderStyleLight];
    }
    [self applyHoverAppearance];
}

#pragma mark -
#pragma mark Hover tracking

- (void)installHoverTrackingAreaIfNeeded
{
    if (_hoverTrackingArea != nil) {
        return;
    }
    _hoverTrackingArea = [[NSTrackingArea alloc] initWithRect:NSZeroRect
                                                      options:NSTrackingMouseEnteredAndExited | NSTrackingMouseMoved | NSTrackingInVisibleRect | NSTrackingActiveInKeyWindow
                                                        owner:self
                                                     userInfo:nil];
    [self addTrackingArea:_hoverTrackingArea];
}

- (void)updateTrackingAreas
{
    [super updateTrackingAreas];
    [self installHoverTrackingAreaIfNeeded];
}

- (void)viewDidMoveToWindow
{
    [super viewDidMoveToWindow];
    if (self.window == nil) {
        [self hideHoverWindow];
        return;
    }
    [self installHoverTrackingAreaIfNeeded];
}

- (void)dealloc
{
    [self hideHoverWindow];
}

- (BOOL)hoverIsAvailable
{
    return self.enabled && !self.indefinite && self.mediaDuration > 0;
}

- (void)createHoverWindowIfNeeded
{
    if (_hoverWindow != nil) {
        return;
    }

    _hoverLabel = [NSTextField labelWithString:@""];
    _hoverLabel.font =
        [NSFont monospacedDigitSystemFontOfSize:NSFont.smallSystemFontSize
                                         weight:NSFontWeightRegular];
    _hoverLabel.alignment = NSTextAlignmentCenter;
    _hoverLabel.textColor = NSColor.controlTextColor;

    NSView * const contentView = [[NSView alloc] initWithFrame:NSZeroRect];
    contentView.wantsLayer = YES;
    contentView.layer.borderWidth = 0.5;
    contentView.layer.cornerRadius = 3.0;
    [contentView addSubview:_hoverLabel];

    _hoverWindow = [[NSPanel alloc] initWithContentRect:NSMakeRect(0, 0, 60, 20)
                                              styleMask:NSWindowStyleMaskBorderless | NSWindowStyleMaskNonactivatingPanel
                                                backing:NSBackingStoreBuffered
                                                  defer:NO];
    _hoverWindow.contentView = contentView;
    _hoverWindow.opaque = NO;
    _hoverWindow.backgroundColor = NSColor.clearColor;
    _hoverWindow.hasShadow = YES;
    _hoverWindow.ignoresMouseEvents = YES;
    _hoverWindow.releasedWhenClosed = NO;

    [self applyHoverAppearance];
}

- (void)applyHoverAppearance
{
    if (_hoverWindow == nil) {
        return;
    }
    NSView * const contentView = _hoverWindow.contentView;
    if (@available(macOS 11.0, *)) {
        _hoverWindow.appearance = self.effectiveAppearance;
        [self.effectiveAppearance performAsCurrentDrawingAppearance:^{
            contentView.layer.backgroundColor =
                [NSColor.controlBackgroundColor colorWithAlphaComponent:0.95].CGColor;
            contentView.layer.borderColor = NSColor.gridColor.CGColor;
        }];
        return;
    }
    if (@available(macOS 10.14, *)) {
        _hoverWindow.appearance = self.effectiveAppearance;
    }
    contentView.layer.backgroundColor =
        [NSColor.controlBackgroundColor colorWithAlphaComponent:0.95].CGColor;
    contentView.layer.borderColor = NSColor.gridColor.CGColor;
}

- (void)hideHoverWindow
{
    if (_hoverWindow == nil) {
        return;
    }
    NSWindow * const parent = _hoverWindow.parentWindow;
    if (parent != nil) {
        [parent removeChildWindow:_hoverWindow];
    }
    [_hoverWindow orderOut:nil];
}

- (void)showHoverAtSliderX:(CGFloat)xInSlider time:(vlc_tick_t)time
{
    NSWindow * const parentWindow = self.window;
    if (parentWindow == nil) {
        return;
    }

    [self createHoverWindowIfNeeded];

    _hoverLabel.stringValue = [NSString stringWithTimeFromTicks:time];
    [_hoverLabel sizeToFit];

    const NSSize textSize = _hoverLabel.frame.size;
    const CGFloat horizontalPadding = 6.0;
    const CGFloat verticalPadding = 2.0;
    const CGFloat verticalGap = 6.0;
    const NSSize windowSize = NSMakeSize(textSize.width + horizontalPadding * 2.0,
                                         textSize.height + verticalPadding * 2.0);
    _hoverLabel.frame = NSMakeRect(horizontalPadding, verticalPadding,
                                   textSize.width, textSize.height);

    const NSRect sliderOnScreen =
        [parentWindow convertRectToScreen:[self convertRect:self.bounds toView:nil]];
    const CGFloat anchorScreenX = NSMinX(sliderOnScreen) + xInSlider;

    CGFloat originX = anchorScreenX - windowSize.width / 2.0;
    originX = MAX(NSMinX(sliderOnScreen),
                  MIN(originX, NSMaxX(sliderOnScreen) - windowSize.width));
    const CGFloat originY = NSMaxY(sliderOnScreen) + verticalGap;

    [_hoverWindow setFrame:NSMakeRect(originX, originY,
                                      windowSize.width, windowSize.height)
                   display:YES];

    if (_hoverWindow.parentWindow != parentWindow) {
        [parentWindow addChildWindow:_hoverWindow ordered:NSWindowAbove];
    } else if (!_hoverWindow.visible) {
        [_hoverWindow orderFront:nil];
    }
}

- (void)reportHoverAtPoint:(NSPoint)locationInSelf
{
    if (![self hoverIsAvailable]) {
        [self hideHoverWindow];
        return;
    }

    const NSRect trackRect = [(NSSliderCell *)self.cell barRectFlipped:NO];
    if (NSWidth(trackRect) <= 0) {
        [self hideHoverWindow];
        return;
    }

    CGFloat fraction = (locationInSelf.x - NSMinX(trackRect)) / NSWidth(trackRect);
    if (fraction < 0.0) {
        fraction = 0.0;
    } else if (fraction > 1.0) {
        fraction = 1.0;
    }

    const vlc_tick_t time = (vlc_tick_t)(fraction * self.mediaDuration);
    [self showHoverAtSliderX:locationInSelf.x time:time];
}

- (void)mouseEntered:(NSEvent *)event
{
    [self reportHoverAtPoint:[self convertPoint:event.locationInWindow fromView:nil]];
}

- (void)mouseMoved:(NSEvent *)event
{
    [self reportHoverAtPoint:[self convertPoint:event.locationInWindow fromView:nil]];
}

- (void)mouseExited:(NSEvent *)event
{
    [self hideHoverWindow];
}

- (void)mouseDown:(NSEvent *)event
{
    // NSSlider runs its own tracking loop here; mouseMoved is not delivered
    // during the drag, so the hover would otherwise show a stale time.
    [self hideHoverWindow];
    [super mouseDown:event];
}

- (void)setMediaDuration:(vlc_tick_t)mediaDuration
{
    _mediaDuration = mediaDuration;
    if (![self hoverIsAvailable]) {
        [self hideHoverWindow];
    }
}

@end
