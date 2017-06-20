//
//  VLCHUDSegmentedCell.m
//  BGHUDAppKit
//
//  Created by BinaryGod on 7/1/08.
//
//  Copyright (c) 2008, Tim Davis (BinaryMethod.com, binary.god@gmail.com)
//  All rights reserved.
//
//  Redistribution and use in source and binary forms, with or without modification,
//  are permitted provided that the following conditions are met:
//
//		Redistributions of source code must retain the above copyright notice, this
//	list of conditions and the following disclaimer.
//
//		Redistributions in binary form must reproduce the above copyright notice,
//	this list of conditions and the following disclaimer in the documentation and/or
//	other materials provided with the distribution.
//
//		Neither the name of the BinaryMethod.com nor the names of its contributors
//	may be used to endorse or promote products derived from this software without
//	specific prior written permission.
//
//	THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS AS IS AND
//	ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
//	WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
//	IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
//	INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
//	BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
//	OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
//	WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
//	ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
//	POSSIBILITY OF SUCH DAMAGE.

#import "VLCHUDSegmentedCell.h"
#import "CompatibilityFixes.h"

@interface NSSegmentedCell (Private)

- (NSRect)rectForSegment:(NSInteger)segment inFrame:(NSRect)frame;
- (NSInteger)_keySegment;

@end


@implementation VLCHUDSegmentedCell

- (instancetype)initWithCoder:(NSCoder *)decoder
{

    self = [super initWithCoder:decoder];

    if (self) {
        _strokeColor = [NSColor colorWithDeviceRed:0.749f green:0.761f blue:0.788f alpha:1.0f];
        _highlightGradient = [[NSGradient alloc] initWithStartingColor:[NSColor colorWithDeviceRed:0.451f green:0.451f blue:0.455f alpha:0.5f]
                                                           endingColor:[NSColor colorWithDeviceRed:0.318f green:0.318f blue:0.318f alpha:0.5f]];
        _normalGradient = [[NSGradient alloc] initWithStartingColor:[NSColor colorWithDeviceRed:0.251f green:0.251f blue:0.255f alpha:0.5f]
                                                        endingColor:[NSColor colorWithDeviceRed:0.118f green:0.118f blue:0.118f alpha:0.5f]];
        _disabledNormalGradient = [[NSGradient alloc] initWithStartingColor:[NSColor colorWithDeviceRed:0.251f green:0.251f blue:0.255f alpha:0.5f]
                                                                endingColor:[NSColor colorWithDeviceRed:0.118f green:0.118f blue:0.118f alpha:0.5f]];
        _cellTextColor = [NSColor whiteColor];
        _disabledCellTextColor = [NSColor colorWithDeviceRed:1 green:1 blue:1 alpha:0.5f];
    }

    return self;
}

- (void)drawWithFrame:(NSRect)frame inView:(NSView *)view
{
    if (OSX_YOSEMITE_AND_HIGHER) {
        return [super drawWithFrame:frame inView:view];
    }

    for (NSInteger segment = 0; segment < self.segmentCount; segment++) {
        NSRect segmentRect = [self rectForSegment:segment inFrame:frame];
        [self drawBackgroundForSegment:segment inFrame:segmentRect];
        [self drawSegment:segment inFrame:segmentRect withView:view];
        [self drawDividerForSegment:segment inFrame:segmentRect];
    }

    NSBezierPath* rectanglePath = [NSBezierPath bezierPathWithRoundedRect:NSInsetRect(frame, 0.55, 0.55) xRadius:3.0 yRadius:3.0];
    [_strokeColor setStroke];
    [rectanglePath setLineWidth:1.0];
    [rectanglePath stroke];
}

- (void)drawSegment:(NSInteger)segment inFrame:(NSRect)frame withView:(NSView *)view
{
    if (OSX_YOSEMITE_AND_HIGHER) {
        return [super drawSegment:segment inFrame:frame withView:view];
    }

    NSString *segmentText = [self labelForSegment:segment];

    NSSize textSize = [segmentText sizeWithAttributes:nil];

    frame.origin.y += (frame.size.height - textSize.height) / 2;
    frame.size.height -= (frame.size.height - textSize.height) / 2;

    NSMutableParagraphStyle *paragraphStyle = [[NSMutableParagraphStyle alloc]init] ;
    [paragraphStyle setAlignment:NSTextAlignmentCenter];

    NSDictionary *attributes = @{ NSForegroundColorAttributeName : (self.isEnabled) ? _cellTextColor : _disabledCellTextColor,
                                  NSParagraphStyleAttributeName  : paragraphStyle };
    [segmentText drawInRect:frame withAttributes:attributes];
}

- (void)drawBackgroundForSegment:(NSInteger)segment inFrame:(NSRect)frame
{
    NSGradient *gradient;

    if (self.isEnabled) {
        gradient = (segment == self.selectedSegment) ? _highlightGradient : _normalGradient;
    } else {
        gradient = _disabledNormalGradient;
    }

    if (segment > 0 && segment < (self.segmentCount - 1)) {
        // Middle segments
        [NSGraphicsContext saveGraphicsState];
        if ([super showsFirstResponder] && [[[self controlView] window] isKeyWindow] &&
            ([self focusRingType] == NSFocusRingTypeDefault ||
             [self focusRingType] == NSFocusRingTypeExterior) &&
            [self respondsToSelector:@selector(_keySegment)] && self._keySegment == segment) {
            NSSetFocusRingStyle(NSFocusRingOnly);
            NSRectFill(frame);
        }
        [NSGraphicsContext restoreGraphicsState];
        [gradient drawInRect:frame angle:90];
        return;
    }

    CGFloat radius = 3.0;
    NSBezierPath* fillPath = [NSBezierPath bezierPath];

    if (segment == 0) {
        // First segment
        [fillPath appendBezierPathWithArcWithCenter: NSMakePoint(NSMinX(frame) + radius, NSMinY(frame) + radius) radius:radius startAngle:180 endAngle:270];
        [fillPath lineToPoint: NSMakePoint(NSMaxX(frame), NSMinY(frame))];
        [fillPath lineToPoint: NSMakePoint(NSMaxX(frame), NSMaxY(frame))];
        [fillPath appendBezierPathWithArcWithCenter: NSMakePoint(NSMinX(frame) + radius, NSMaxY(frame) - radius) radius:radius startAngle:90 endAngle:180];
    } else {
        // Last segment
        [fillPath moveToPoint: NSMakePoint(NSMinX(frame), NSMinY(frame))];
        [fillPath appendBezierPathWithArcWithCenter: NSMakePoint(NSMaxX(frame) - radius, NSMinY(frame) + radius) radius:radius startAngle:270 endAngle:360];
        [fillPath appendBezierPathWithArcWithCenter: NSMakePoint(NSMaxX(frame) - radius, NSMaxY(frame) - radius) radius:radius startAngle:0 endAngle:90];
        [fillPath lineToPoint: NSMakePoint(NSMinX(frame), NSMaxY(frame))];
    }
    [fillPath closePath];
    [NSGraphicsContext saveGraphicsState];
    if ([super showsFirstResponder] && [[[self controlView] window] isKeyWindow] &&
        ([self focusRingType] == NSFocusRingTypeDefault ||
         [self focusRingType] == NSFocusRingTypeExterior) &&
        [self respondsToSelector:@selector(_keySegment)] && self._keySegment == segment) {
        NSSetFocusRingStyle(NSFocusRingOnly);
        [fillPath fill];
    }
    [NSGraphicsContext restoreGraphicsState];
    [gradient drawInBezierPath:fillPath angle:90];
}

- (void)drawDividerForSegment:(NSInteger)segment inFrame:(NSRect)frame
{
    if (segment == 0) {
        // Do not draw for first segment
        return;
    }

    // Draw divider on the left of the segment
    NSBezierPath* dividerPath = [NSBezierPath bezierPath];
    [dividerPath moveToPoint:NSMakePoint(NSMinX(frame), NSMinY(frame))];
    [dividerPath lineToPoint:NSMakePoint(NSMinX(frame), NSMaxY(frame))];
    [_strokeColor setStroke];
    [dividerPath setLineWidth:1.0];
    [dividerPath stroke];
}

- (BOOL)_canAnimate {
    return NO;
}

- (BOOL)_isSliderStyle {
    return NO;
}

@end
