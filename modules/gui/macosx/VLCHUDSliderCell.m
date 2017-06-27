//
//  VLCHUDSliderCell.m
//  BGHUDAppKit
//
//  Created by BinaryGod on 5/30/08.
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

#import "VLCHUDSliderCell.h"
#import "CompatibilityFixes.h"

@implementation VLCHUDSliderCell

- (instancetype)initWithCoder:(NSCoder *)coder
{
    self = [super initWithCoder:coder];
    if (self) {
        // Custom colors for the slider
        _sliderColor            = [NSColor colorWithCalibratedRed:0.318 green:0.318 blue:0.318 alpha:0.6];
        _disabledSliderColor    = [NSColor colorWithCalibratedRed:0.318 green:0.318 blue:0.318 alpha:0.2];
        _strokeColor            = [NSColor colorWithCalibratedRed:0.749 green:0.761 blue:0.788 alpha:1.0];
        _disabledStrokeColor    = [NSColor colorWithCalibratedRed:0.749 green:0.761 blue:0.788 alpha:0.2];

        // Custom knob gradients
        _knobGradient           = [[NSGradient alloc] initWithStartingColor:[NSColor colorWithDeviceRed:0.251 green:0.251 blue:0.255 alpha:1.0]
                                                                endingColor:[NSColor colorWithDeviceRed:0.118 green:0.118 blue:0.118 alpha:1.0]];
        _disableKnobGradient    = [[NSGradient alloc] initWithStartingColor:[NSColor colorWithDeviceRed:0.251 green:0.251 blue:0.255 alpha:1.0]
                                                                endingColor:[NSColor colorWithDeviceRed:0.118 green:0.118 blue:0.118 alpha:1.0]];
    }
    return self;
}

NSAffineTransform* RotationTransform(const CGFloat angle, const NSPoint point)
{
    NSAffineTransform*	transform = [NSAffineTransform transform];
    [transform translateXBy:point.x yBy:point.y];
    [transform rotateByRadians:angle];
    [transform translateXBy:-(point.x) yBy:-(point.y)];
    return transform;
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpartial-availability"
- (void)drawKnob:(NSRect)smallRect
{
    if (OSX_YOSEMITE_AND_HIGHER) {
        return [super drawKnob:smallRect];
    }
    NSBezierPath *path = [NSBezierPath bezierPath];
    // Inset rect to have enough room for the stroke
    smallRect = NSInsetRect(smallRect, 1.0, 1.0);

    // Get min/max/mid coords for shape calculations
    CGFloat minX = NSMinX(smallRect);
    CGFloat minY = NSMinY(smallRect);
    CGFloat maxX = NSMaxX(smallRect);
    CGFloat maxY = NSMaxY(smallRect);
    CGFloat midX = NSMidX(smallRect);
    CGFloat midY = NSMidY(smallRect);

    // Draw the knobs shape
    if (self.numberOfTickMarks > 0) {
        // We have tickmarks, draw an arrow-like shape
        if (self.isVertical) {
            // For some reason the rect is not alligned correctly at
            // tickmarks and clipped, so this ugly thing is necessary:
            maxY = maxY - 2;
            midY = midY - 1;

            // Right pointing arrow
            [path moveToPoint:NSMakePoint(minX + 3, minY)];
            [path lineToPoint:NSMakePoint(midX + 2, minY)];
            [path lineToPoint:NSMakePoint(maxX, midY)];
            [path lineToPoint:NSMakePoint(midX + 2, maxY)];
            [path lineToPoint:NSMakePoint(minX + 3, maxY)];
            [path appendBezierPathWithArcFromPoint:NSMakePoint(minX, maxY)
                                           toPoint:NSMakePoint(minX, maxY - 3)
                                            radius:2.5f];
            [path lineToPoint:NSMakePoint(minX, maxY - 3)];
            [path lineToPoint:NSMakePoint(minX, minY + 3)];
            [path appendBezierPathWithArcFromPoint:NSMakePoint(minX, minY)
                                           toPoint:NSMakePoint(minX + 3, minY)
                                            radius:2.5f];
            [path closePath];
        } else {
            // Down pointing arrow
            [path moveToPoint:NSMakePoint(minX + 3, minY)];
            [path lineToPoint:NSMakePoint(maxX - 3, minY)];
            [path appendBezierPathWithArcFromPoint:NSMakePoint(maxX, minY)
                                           toPoint:NSMakePoint(maxX, minY + 3)
                                            radius:2.5f];
            [path lineToPoint:NSMakePoint(maxX, minY + 3)];
            [path lineToPoint:NSMakePoint(maxX, midY + 2)];
            [path lineToPoint:NSMakePoint(midX, maxY)];
            [path lineToPoint:NSMakePoint(minX, midY + 2)];
            [path lineToPoint:NSMakePoint(minX, minY + 3)];
            [path appendBezierPathWithArcFromPoint:NSMakePoint(minX, minY)
                                           toPoint:NSMakePoint(minX + 3, minY)
                                            radius:2.5f];
            [path closePath];
        }

        // Rotate our knob if needed to the correct position
        if (self.tickMarkPosition == NSTickMarkAbove) {
            NSAffineTransform *transform = nil;
            transform = RotationTransform(M_PI, NSMakePoint(midX, midY));
            [path transformUsingAffineTransform:transform];
        }
    } else {
        // We have no tickmarks, draw a round knob
        [path appendBezierPathWithOvalInRect:NSInsetRect(smallRect, 0.5, 0.5)];
    }

    // Draw the knobs background
    if (self.isEnabled && self.isHighlighted) {
        [_knobGradient drawInBezierPath:path angle:270.0f];
    } else if (self.isEnabled) {
        [_knobGradient drawInBezierPath:path angle:90.0f];
    } else {
        [_disableKnobGradient drawInBezierPath:path angle:90.0f];
    }

    // Draw white stroke around the knob
    if (self.isEnabled) {
        [_strokeColor setStroke];
    } else {
        [_disabledStrokeColor setStroke];
    }

    [path setLineWidth:1.0];
    [path stroke];
}

- (void)drawBarInside:(NSRect)fullRect flipped:(BOOL)flipped
{
    if (OSX_YOSEMITE_AND_HIGHER) {
        return [super drawBarInside:fullRect flipped:flipped];
    }
    if (self.isVertical) {
        return [self drawVerticalBarInFrame:fullRect];
    } else {
        return [self drawHorizontalBarInFrame:fullRect];
    }
}
#pragma clang diagnostic pop

- (void)drawVerticalBarInFrame:(NSRect)frame
{
    // Adjust frame based on ControlSize
    switch ([self controlSize]) {

        case NSRegularControlSize:

            if ([self numberOfTickMarks] != 0) {
                if ([self tickMarkPosition] == NSTickMarkRight) {
                    frame.origin.x += 4;
                } else {
                    frame.origin.x += frame.size.width - 9;
                }
            } else {
                frame.origin.x = frame.origin.x + (((frame.origin.x + frame.size.width) /2) - 2.5f);
            }
            frame.origin.x += 0.5f;
            frame.origin.y += 2.5f;
            frame.size.height -= 6;
            frame.size.width = 5;
            break;

        case NSSmallControlSize:

            if ([self numberOfTickMarks] != 0) {
                if ([self tickMarkPosition] == NSTickMarkRight) {
                    frame.origin.x += 3;
                } else {
                    frame.origin.x += frame.size.width - 8;
                }
            } else {
                frame.origin.x = frame.origin.x + (((frame.origin.x + frame.size.width) /2) - 2.5f);
            }
            frame.origin.y += 0.5f;
            frame.size.height -= 1;
            frame.origin.x += 0.5f;
            frame.size.width = 5;
            break;

        case NSMiniControlSize:

            if ([self numberOfTickMarks] != 0) {
                if ([self tickMarkPosition] == NSTickMarkRight) {
                    frame.origin.x += 2.5f;
                } else {
                    frame.origin.x += frame.size.width - 6.5f;
                }
            } else {
                frame.origin.x = frame.origin.x + (((frame.origin.x + frame.size.width) /2) - 2);
            }
            frame.origin.x += 1;
            frame.origin.y += 0.5f;
            frame.size.height -= 1;
            frame.size.width = 3;
            break;
    }
    
    // Draw Bar
    NSBezierPath *path = [NSBezierPath bezierPathWithRoundedRect: frame xRadius: 2 yRadius: 2];
    
    if ([self isEnabled]) {
        [_sliderColor set];
        [path fill];

        [_strokeColor set];
        [path stroke];
    } else {
        [_disabledSliderColor set];
        [path fill];

        [_disabledStrokeColor set];
        [path stroke];
    }
}

- (void)drawHorizontalBarInFrame:(NSRect)frame
{
    // Adjust frame based on ControlSize
    switch ([self controlSize]) {

        case NSRegularControlSize:

            if ([self numberOfTickMarks] != 0) {
                if ([self tickMarkPosition] == NSTickMarkBelow) {
                    frame.origin.y += 4;
                } else {
                    frame.origin.y += frame.size.height - 10;
                }
            } else {
                frame.origin.y = frame.origin.y + (((frame.origin.y + frame.size.height) /2) - 2.5f);
            }
            frame.origin.x += 2.5f;
            frame.origin.y += 0.5f;
            frame.size.width -= 5;
            frame.size.height = 5;
            break;

        case NSSmallControlSize:

            if ([self numberOfTickMarks] != 0) {
                if ([self tickMarkPosition] == NSTickMarkBelow) {
                    frame.origin.y += 2;
                } else {
                    frame.origin.y += frame.size.height - 8;
                }
            } else {
                frame.origin.y = frame.origin.y + (((frame.origin.y + frame.size.height) /2) - 2.5f);
            }
            frame.origin.x += 0.5f;
            frame.origin.y += 0.5f;
            frame.size.width -= 1;
            frame.size.height = 5;
            break;

        case NSMiniControlSize:

            if ([self numberOfTickMarks] != 0) {
                if ([self tickMarkPosition] == NSTickMarkBelow) {
                    frame.origin.y += 2;
                } else {
                    frame.origin.y += frame.size.height - 6;
                }
            } else {
                frame.origin.y = frame.origin.y + (((frame.origin.y + frame.size.height) /2) - 2);
            }
            frame.origin.x += 0.5f;
            frame.origin.y += 0.5f;
            frame.size.width -= 1;
            frame.size.height = 3;
            break;
    }
    
    // Draw Bar
    NSBezierPath *path = [NSBezierPath bezierPathWithRoundedRect:frame xRadius:2 yRadius:2];
    
    if ([self isEnabled]) {
        [_sliderColor set];
        [path fill];
        
        [_strokeColor set];
        [path stroke];
    } else {
        [_disabledSliderColor set];
        [path fill];
        
        [_disabledStrokeColor set];
        [path stroke];
    }
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpartial-availability"
- (void)drawTickMarks
{
    if (OSX_YOSEMITE_AND_HIGHER) {
        return [super drawTickMarks];
    }
    for (int i = 0; i < self.numberOfTickMarks; i++) {
        NSRect tickMarkRect = [self rectOfTickMarkAtIndex:i];
        if (self.isEnabled) {
            [_strokeColor setFill];
        } else {
            [_disabledStrokeColor setFill];
        }
        NSRectFill(tickMarkRect);
    }
}
#pragma clang diagnostic pop

@end
