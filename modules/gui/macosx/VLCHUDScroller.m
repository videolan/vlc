//
//  VLCHUDScroller.m
//  HUDScroller
//
//  Created by BinaryGod on 5/22/08.
//
//  Copyright (c) 2008, Tim Davis (BinaryMethod.com, binary.god@gmail.com)
//  All rights reserved.
//
//  Redistribution and use in source and binary forms, with or without modification,
//  are permitted provided that the following conditions are met:
//
//        Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
//        Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation and/or
//    other materials provided with the distribution.
//
//        Neither the name of the BinaryMethod.com nor the names of its contributors
//    may be used to endorse or promote products derived from this software without
//    specific prior written permission.
//
//    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS AS IS AND
//    ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
//    WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
//    IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
//    INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
//    BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
//    OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
//    WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
//    ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
//    POSSIBILITY OF SUCH DAMAGE.

// Special thanks to Matt Gemmell (http://mattgemmell.com/) for helping me solve the
// transparent drawing issues.  Your awesome man!!!

#import "VLCHUDScroller.h"
#import "CompatibilityFixes.h"

@implementation VLCHUDScroller

- (instancetype)initWithCoder:(NSCoder *)coder
{
    self = [super initWithCoder:coder];
    if (self) {
        _scrollerStroke = [NSColor colorWithDeviceRed: 0.749f green: 0.761f blue: 0.788f alpha: 1.0f];
        _scrollerKnobGradient = [[NSGradient alloc] initWithStartingColor:[NSColor colorWithDeviceRed:0.631f green:0.639f blue:0.655f alpha:1.0f]
                                                              endingColor:[NSColor colorWithDeviceRed:0.439f green:0.447f blue:0.471f alpha:1.0f]];
        _scrollerTrackGradient = [[NSGradient alloc] initWithStartingColor:[NSColor colorWithDeviceRed:0.137f green:0.137f blue:0.137f alpha:.75f]
                                                               endingColor:[NSColor colorWithDeviceRed:0.278f green:0.278f blue:0.278f alpha:.75f]];
        _scrollerArrowNormalGradient = [[NSGradient alloc] initWithStartingColor:[NSColor colorWithDeviceRed:0.251f green:0.251f blue:0.255f alpha:0.5f]
                                                                     endingColor:[NSColor colorWithDeviceRed:0.118f green:0.118f blue:0.118f alpha:0.5f]];
        _scrollerArrowPushedGradient = [[NSGradient alloc] initWithStartingColor:[NSColor colorWithDeviceRed:0.451f green:0.451f blue:0.455f alpha:0.5f]
                                                                                                 endingColor:[NSColor colorWithDeviceRed:0.318f green:0.318f blue:0.318f alpha:0.5f]];
    }
    return self;
}

#pragma mark Drawing Functions

- (void)drawRect:(NSRect)rect {
    if (OSX_YOSEMITE_AND_HIGHER) {
        return [super drawRect:rect];
    }
    // See if we should use system default or supplied value
    if ([self arrowsPosition] == NSScrollerArrowsDefaultSetting) {
        arrowPosition = [[[NSUserDefaults standardUserDefaults] persistentDomainForName:NSGlobalDomain] valueForKey: @"AppleScrollBarVariant"];
        if (arrowPosition == nil) {
            arrowPosition = @"DoubleMax";
        }
    } else {
        if ([self arrowsPosition] == NSScrollerArrowsNone) {
            arrowPosition = @"None";
        }
    }

    NSDisableScreenUpdates();

    [[NSColor colorWithCalibratedWhite:0.0f alpha:0.7f] set];
    NSRectFill([self bounds]);

    // Draw knob-slot.
    [self drawKnobSlotInRect:[self bounds] highlight:YES];

    // Draw knob
    [self drawKnob];

    // Draw arrows
    [self drawArrow:NSScrollerIncrementArrow highlight:([self hitPart] == NSScrollerIncrementLine)];
    [self drawArrow:NSScrollerDecrementArrow highlight:([self hitPart] == NSScrollerDecrementLine)];

    [[self window] invalidateShadow];

    NSEnableScreenUpdates();
}

- (void)drawKnob {
    if (OSX_YOSEMITE_AND_HIGHER) {
        return [super drawKnob];
    }

    if(![self isHoriz]) {

        //Draw Knob
        NSBezierPath *knob = [[NSBezierPath alloc] init];
        NSRect knobRect = [self rectForPart:NSScrollerKnob];


        [knob appendBezierPathWithArcWithCenter:NSMakePoint(knobRect.origin.x + ((knobRect.size.width - .5f) /2), (knobRect.origin.y + ((knobRect.size.width -2) /2)))
                                         radius:(knobRect.size.width -2) /2
                                     startAngle:180
                                       endAngle:0];

        [knob appendBezierPathWithArcWithCenter:NSMakePoint(knobRect.origin.x + ((knobRect.size.width - .5f) /2), ((knobRect.origin.y + knobRect.size.height) - ((knobRect.size.width -2) /2)))
                                         radius:(knobRect.size.width -2) /2
                                     startAngle:0
                                       endAngle:180];

        [_scrollerStroke set];
        [knob fill];

        knobRect.origin.x += 1;
        knobRect.origin.y += 1;
        knobRect.size.width -= 2;
        knobRect.size.height -= 2;

        knob = [[NSBezierPath alloc] init];

        [knob appendBezierPathWithArcWithCenter:NSMakePoint(knobRect.origin.x + ((knobRect.size.width - .5f) /2), (knobRect.origin.y + ((knobRect.size.width -2) /2)))
                                         radius:(knobRect.size.width -2) /2
                                     startAngle:180
                                       endAngle:0];

        [knob appendBezierPathWithArcWithCenter:NSMakePoint(knobRect.origin.x + ((knobRect.size.width - .5f) /2), ((knobRect.origin.y + knobRect.size.height) - ((knobRect.size.width -2) /2)))
                                         radius:(knobRect.size.width -2) /2
                                     startAngle:0
                                       endAngle:180];

        [_scrollerKnobGradient drawInBezierPath:knob angle:0];

    } else {

        //Draw Knob
        NSBezierPath *knob = [[NSBezierPath alloc] init];
        NSRect knobRect = [self rectForPart:NSScrollerKnob];

        [knob appendBezierPathWithArcWithCenter:NSMakePoint(knobRect.origin.x + ((knobRect.size.height - .5f) /2), (knobRect.origin.y + ((knobRect.size.height -1) /2)))
                                         radius:(knobRect.size.height -1) /2
                                     startAngle:90
                                       endAngle:270];

        [knob appendBezierPathWithArcWithCenter:NSMakePoint((knobRect.origin.x + knobRect.size.width) - ((knobRect.size.height - .5f) /2), (knobRect.origin.y + ((knobRect.size.height -1) /2)))
                                         radius:(knobRect.size.height -1) /2
                                     startAngle:270
                                       endAngle:90];

        [_scrollerStroke set];
        [knob fill];

        knobRect.origin.x += 1;
        knobRect.origin.y += 1;
        knobRect.size.width -= 2;
        knobRect.size.height -= 2;

        knob = [[NSBezierPath alloc] init];

        [knob appendBezierPathWithArcWithCenter:NSMakePoint(knobRect.origin.x + ((knobRect.size.height - .5f) /2), (knobRect.origin.y + ((knobRect.size.height -1) /2)))
                                         radius:(knobRect.size.height -1) /2
                                     startAngle:90
                                       endAngle:270];

        [knob appendBezierPathWithArcWithCenter:NSMakePoint((knobRect.origin.x + knobRect.size.width) - ((knobRect.size.height - .5f) /2), (knobRect.origin.y + ((knobRect.size.height -1) /2)))
                                         radius:(knobRect.size.height -1) /2
                                     startAngle:270
                                       endAngle:90];

        [_scrollerKnobGradient drawInBezierPath:knob angle:90];

    }
}

- (void)drawArrow:(NSScrollerArrow)arrow highlightPart:(NSUInteger)part {
    if (OSX_YOSEMITE_AND_HIGHER) {
        return [super drawArrow:arrow highlight:part];
    }

    if (arrow == NSScrollerDecrementArrow) {

        if (part == (NSUInteger)-1 || part == 0) {
            [self drawDecrementArrow: NO];
        } else {
            [self drawDecrementArrow: YES];
        }
    }

    if (arrow == NSScrollerIncrementArrow) {

        if(part == 1 || part == (NSUInteger)-1) {

            [self drawIncrementArrow: NO];
        } else {

            [self drawIncrementArrow: YES];
        }
    }
}

- (void)drawKnobSlotInRect:(NSRect)rect highlight:(BOOL)highlight {
    if (OSX_YOSEMITE_AND_HIGHER) {
        return [super drawKnobSlotInRect:rect highlight:highlight];
    }

    if (![self isHoriz]) {

        // Draw Knob Slot
        [_scrollerTrackGradient drawInRect:rect angle:0];

        if ([arrowPosition isEqualToString:@"DoubleMax"]) {

            // Adjust rect height for top base
            rect.size.height = 8;

            // Draw Top Base
            NSBezierPath *path = [[NSBezierPath alloc] init];
            NSPoint basePoints[4];

            [path appendBezierPathWithArcWithCenter:NSMakePoint(rect.size.width /2, rect.size.height + (rect.size.width /2) -5)
                                             radius:(rect.size.width ) /2
                                         startAngle:180
                                           endAngle:0];

            // Add the rest of the points
            basePoints[3] = NSMakePoint( rect.origin.x, rect.origin.y + rect.size.height);
            basePoints[2] = NSMakePoint( rect.origin.x, rect.origin.y);
            basePoints[1] = NSMakePoint( rect.origin.x + rect.size.width, rect.origin.y);
            basePoints[0] = NSMakePoint( rect.origin.x + rect.size.width, rect.origin.y + rect.size.height);

            [path appendBezierPathWithPoints:basePoints count:4];

            [_scrollerArrowNormalGradient drawInBezierPath:path angle:0];

        } else if ([arrowPosition isEqualToString:@"None"]) {

            // Adjust rect height for top base
            NSRect topRect = rect;
            topRect.size.height = 8;

            // Draw Top Base
            NSBezierPath *path = [[NSBezierPath alloc] init];
            NSPoint basePoints[4];

            [path appendBezierPathWithArcWithCenter:NSMakePoint(topRect.size.width /2, topRect.size.height + (topRect.size.width /2) -5)
                                             radius:(topRect.size.width ) /2
                                         startAngle:180
                                           endAngle:0];

            // Add the rest of the points
            basePoints[3] = NSMakePoint( topRect.origin.x, topRect.origin.y + topRect.size.height);
            basePoints[2] = NSMakePoint( topRect.origin.x, topRect.origin.y);
            basePoints[1] = NSMakePoint( topRect.origin.x + topRect.size.width, topRect.origin.y);
            basePoints[0] = NSMakePoint( topRect.origin.x + topRect.size.width, topRect.origin.y + topRect.size.height);

            [path appendBezierPathWithPoints:basePoints count:4];

            [_scrollerArrowNormalGradient drawInBezierPath:path angle:0];


            // Draw Decrement Button
            NSRect bottomRect = rect;
            bottomRect.origin.y = rect.size.height - 4;
            bottomRect.size.height = 4;

            path = [[NSBezierPath alloc] init];

            // Add Notch
            [path appendBezierPathWithArcWithCenter:NSMakePoint((bottomRect.size.width ) /2, (bottomRect.origin.y  - ((bottomRect.size.width ) /2) + 1))
                                             radius:(bottomRect.size.width ) /2
                                         startAngle:0
                                           endAngle:180];

            // Add the rest of the points
            basePoints[0] = NSMakePoint( bottomRect.origin.x, bottomRect.origin.y);
            basePoints[1] = NSMakePoint( bottomRect.origin.x, bottomRect.origin.y + bottomRect.size.height);
            basePoints[2] = NSMakePoint( bottomRect.origin.x + bottomRect.size.width, bottomRect.origin.y + bottomRect.size.height);
            basePoints[3] = NSMakePoint( bottomRect.origin.x + bottomRect.size.width, bottomRect.origin.y);

            // Add Points to Path
            [path appendBezierPathWithPoints:basePoints count:4];

            [_scrollerArrowNormalGradient drawInBezierPath:path angle:0];
        }
    } else {

        // Draw Knob Slot
        [_scrollerTrackGradient drawInRect:rect angle:90];

        if ([arrowPosition isEqualToString:@"DoubleMax"]) {

            // Adjust rect height for top base
            rect.size.width = 8;

            // Draw Top Base
            NSBezierPath *path = [[NSBezierPath alloc] init];
            NSPoint basePoints[4];

            [path appendBezierPathWithArcWithCenter:NSMakePoint((rect.size.height /2) +5, rect.origin.y + (rect.size.height /2) )
                                             radius:(rect.size.height ) /2
                                         startAngle:90
                                           endAngle:270];

            // Add the rest of the points
            basePoints[2] = NSMakePoint( rect.origin.x, rect.origin.y + rect.size.height);
            basePoints[1] = NSMakePoint( rect.origin.x, rect.origin.y);
            basePoints[0] = NSMakePoint( rect.origin.x + rect.size.width, rect.origin.y);
            basePoints[3] = NSMakePoint( rect.origin.x + rect.size.width, rect.origin.y + rect.size.height);

            [path appendBezierPathWithPoints:basePoints count:4];

            [_scrollerArrowNormalGradient drawInBezierPath:path angle:0];
        } else if ([arrowPosition isEqualToString:@"None"]) {

            // Adjust rect height for top base
            NSRect topRect = rect;
            topRect.size.width = 8;

            // Draw Top Base
            NSBezierPath *path = [[NSBezierPath alloc] init];
            NSPoint basePoints[4];

            [path appendBezierPathWithArcWithCenter:NSMakePoint((topRect.size.height /2) +5, topRect.origin.y + (topRect.size.height /2) )
                                             radius:(topRect.size.height ) /2
                                         startAngle:90
                                           endAngle:270];

            // Add the rest of the points
            basePoints[2] = NSMakePoint( topRect.origin.x, topRect.origin.y + topRect.size.height);
            basePoints[1] = NSMakePoint( topRect.origin.x, topRect.origin.y);
            basePoints[0] = NSMakePoint( topRect.origin.x + topRect.size.width, topRect.origin.y);
            basePoints[3] = NSMakePoint( topRect.origin.x + topRect.size.width, topRect.origin.y + topRect.size.height);

            [path appendBezierPathWithPoints:basePoints count:4];

            [_scrollerArrowNormalGradient drawInBezierPath:path angle:0];

            // Bottom Base
            // Draw Decrement Button
            NSRect bottomRect = rect;
            bottomRect.origin.x = rect.size.width - 4;
            bottomRect.size.width = 4;

            path = [[NSBezierPath alloc] init];

            // Add Notch
            [path appendBezierPathWithArcWithCenter:NSMakePoint(bottomRect.origin.x - ((bottomRect.size.height ) /2), (bottomRect.origin.y  + ((bottomRect.size.height ) /2) ))
                                             radius:(bottomRect.size.height ) /2
                                         startAngle:270
                                           endAngle:90];

            // Add the rest of the points
            basePoints[3] = NSMakePoint( bottomRect.origin.x - (((bottomRect.size.height ) /2) -1), bottomRect.origin.y);
            basePoints[0] = NSMakePoint( bottomRect.origin.x - (((bottomRect.size.height ) /2) -1), bottomRect.origin.y + bottomRect.size.height);
            basePoints[1] = NSMakePoint( bottomRect.origin.x + bottomRect.size.width, bottomRect.origin.y + bottomRect.size.height);
            basePoints[2] = NSMakePoint( bottomRect.origin.x + bottomRect.size.width, bottomRect.origin.y);

            // Add Points to Path
            [path appendBezierPathWithPoints:basePoints count:4];

            [_scrollerArrowNormalGradient drawInBezierPath:path angle:90];
        }
    }
}

- (void)drawDecrementArrow:(bool)highlighted {

    if (![self isHoriz]) {

        if ([arrowPosition isEqualToString:@"DoubleMax"]) {

            // Draw Decrement Button
            NSRect rect = [self rectForPart: NSScrollerDecrementLine];
            NSBezierPath *path = [[NSBezierPath alloc] init];
            NSPoint basePoints[4];

            // Add Notch
            [path appendBezierPathWithArcWithCenter:NSMakePoint((rect.size.width ) /2, (rect.origin.y  - ((rect.size.width ) /2) + 1))
                                             radius:(rect.size.width ) /2
                                         startAngle:0
                                           endAngle:180];

            // Add the rest of the points
            basePoints[0] = NSMakePoint( rect.origin.x, rect.origin.y);
            basePoints[1] = NSMakePoint( rect.origin.x, rect.origin.y + rect.size.height);
            basePoints[2] = NSMakePoint( rect.origin.x + rect.size.width, rect.origin.y + rect.size.height);
            basePoints[3] = NSMakePoint( rect.origin.x + rect.size.width, rect.origin.y);

            // Add Points to Path
            [path appendBezierPathWithPoints:basePoints count:4];

            // Fill Path
            if (!highlighted) {
                [_scrollerArrowNormalGradient drawInBezierPath:path angle:0];
            } else {
                [_scrollerArrowPushedGradient drawInBezierPath:path angle:0];
            }

            // Create Arrow Glyph
            NSBezierPath *arrow = [[NSBezierPath alloc] init];

            NSPoint points[3];
            points[0] = NSMakePoint( rect.size.width /2, rect.origin.y + (rect.size.height /2) -3);
            points[1] = NSMakePoint( (rect.size.width /2) +3.5f, rect.origin.y + (rect.size.height /2) +3);
            points[2] = NSMakePoint( (rect.size.width /2) -3.5f, rect.origin.y + (rect.size.height /2) +3);

            [arrow appendBezierPathWithPoints:points count:3];

            [_scrollerStroke set];
            [arrow fill];

            // Create Devider Line
            [_scrollerStroke set];

            [NSBezierPath strokeLineFromPoint:NSMakePoint(0, (rect.origin.y + rect.size.height) +.5f)
                                      toPoint:NSMakePoint(rect.size.width, (rect.origin.y + rect.size.height) +.5f)];

        } else if ([arrowPosition isEqualToString: @"Single"]) {

            NSRect rect = [self rectForPart: NSScrollerDecrementLine];

            NSBezierPath *path = [[NSBezierPath alloc] init];
            NSPoint basePoints[4];

            [path appendBezierPathWithArcWithCenter:NSMakePoint(rect.size.width /2, rect.size.height + (rect.size.width /2) -3)
                                             radius:(rect.size.width ) /2
                                         startAngle:180
                                           endAngle:0];

            // Add the rest of the points
            basePoints[3] = NSMakePoint( rect.origin.x, rect.origin.y + rect.size.height);
            basePoints[2] = NSMakePoint( rect.origin.x, rect.origin.y);
            basePoints[1] = NSMakePoint( rect.origin.x + rect.size.width, rect.origin.y);
            basePoints[0] = NSMakePoint( rect.origin.x + rect.size.width, rect.origin.y + rect.size.height);

            [path appendBezierPathWithPoints: basePoints count: 4];

            // Fill Path
            if (!highlighted) {
                [_scrollerArrowNormalGradient drawInBezierPath:path angle:0];
            } else {

                [_scrollerArrowPushedGradient drawInBezierPath:path angle:0];
            }

            // Create Arrow Glyph
            NSBezierPath *arrow = [[NSBezierPath alloc] init];

            NSPoint points[3];
            points[0] = NSMakePoint( rect.size.width /2, rect.origin.y + (rect.size.height /2) -3);
            points[1] = NSMakePoint( (rect.size.width /2) +3.5f, rect.origin.y + (rect.size.height /2) +3);
            points[2] = NSMakePoint( (rect.size.width /2) -3.5f, rect.origin.y + (rect.size.height /2) +3);

            [arrow appendBezierPathWithPoints:points count:3];

            [_scrollerStroke set];
            [arrow fill];
        }
    } else {

        if ([arrowPosition isEqualToString: @"DoubleMax"]) {

            // Draw Decrement Button
            NSRect rect = [self rectForPart:NSScrollerDecrementLine];
            NSBezierPath *path = [[NSBezierPath alloc] init];
            NSPoint basePoints[4];

            // Add Notch
            [path appendBezierPathWithArcWithCenter:NSMakePoint(rect.origin.x - ((rect.size.height ) /2), (rect.origin.y  + ((rect.size.height ) /2) ))
                                             radius:(rect.size.height ) /2
                                         startAngle:270
                                           endAngle:90];

            // Add the rest of the points
            basePoints[3] = NSMakePoint( rect.origin.x - (((rect.size.height ) /2) -1), rect.origin.y);
            basePoints[0] = NSMakePoint( rect.origin.x - (((rect.size.height ) /2) -1), rect.origin.y + rect.size.height);
            basePoints[1] = NSMakePoint( rect.origin.x + rect.size.width, rect.origin.y + rect.size.height);
            basePoints[2] = NSMakePoint( rect.origin.x + rect.size.width, rect.origin.y);

            // Add Points to Path
            [path appendBezierPathWithPoints:basePoints count:4];

            // Fill Path
            if (!highlighted) {
                [_scrollerArrowNormalGradient drawInBezierPath:path angle:90];
            } else {
                [_scrollerArrowPushedGradient drawInBezierPath:path angle:90];
            }

            // Create Arrow Glyph
            NSBezierPath *arrow = [[NSBezierPath alloc] init];

            NSPoint points[3];
            points[0] = NSMakePoint( rect.origin.x + (rect.size.width /2) -3, rect.size.height /2);
            points[1] = NSMakePoint( rect.origin.x + (rect.size.height /2) +3, (rect.size.height /2) +3.5f);
            points[2] = NSMakePoint( rect.origin.x + (rect.size.height /2) +3, (rect.size.height /2) -3.5f);

            [arrow appendBezierPathWithPoints:points count:3];

            [_scrollerStroke set];
            [arrow fill];

            // Create Devider Line
            [_scrollerStroke set];

            [NSBezierPath strokeLineFromPoint:NSMakePoint(rect.origin.x + rect.size.width -.5f, rect.origin.y)
                                      toPoint:NSMakePoint(rect.origin.x + rect.size.width -.5f, rect.origin.y + rect.size.height)];

        } else if ([arrowPosition isEqualToString: @"Single"]) {

            NSRect rect = [self rectForPart: NSScrollerDecrementLine];

            NSBezierPath *path = [[NSBezierPath alloc] init];
            NSPoint basePoints[4];

            [path appendBezierPathWithArcWithCenter:NSMakePoint(rect.origin.x + (rect.size.width -2) + ((rect.size.height ) /2), (rect.origin.y  + ((rect.size.height ) /2) ))
                                             radius:(rect.size.height ) /2
                                         startAngle:90
                                           endAngle:270];

            // Add the rest of the points
            basePoints[2] = NSMakePoint( rect.origin.x, rect.origin.y + rect.size.height);
            basePoints[1] = NSMakePoint( rect.origin.x, rect.origin.y);
            basePoints[0] = NSMakePoint( rect.origin.x + rect.size.width, rect.origin.y);
            basePoints[3] = NSMakePoint( rect.origin.x + rect.size.width, rect.origin.y + rect.size.height);

            [path appendBezierPathWithPoints: basePoints count: 4];

            // Fill Path
            if (!highlighted) {
                [_scrollerArrowNormalGradient drawInBezierPath:path angle:90];
            } else {
                [_scrollerArrowPushedGradient drawInBezierPath:path angle:90];
            }

            // Create Arrow Glyph
            NSBezierPath *arrow = [[NSBezierPath alloc] init];

            NSPoint points[3];
            points[0] = NSMakePoint( rect.origin.x + (rect.size.width /2) -3, rect.size.height /2);
            points[1] = NSMakePoint( rect.origin.x + (rect.size.height /2) +3, (rect.size.height /2) +3.5f);
            points[2] = NSMakePoint( rect.origin.x + (rect.size.height /2) +3, (rect.size.height /2) -3.5f);

            [arrow appendBezierPathWithPoints:points count:3];

            [_scrollerStroke set];
            [arrow fill];
        }
    }
}

- (void)drawIncrementArrow:(bool)highlighted {

    if (![self isHoriz]) {

        if ([arrowPosition isEqualToString:@"DoubleMax"]) {

            // Draw Increment Button
            NSRect rect = [self rectForPart: NSScrollerIncrementLine];

            if(!highlighted) {

                [_scrollerArrowNormalGradient drawInRect:rect angle:0];
            } else {

                [_scrollerArrowPushedGradient drawInRect:rect angle:0];
            }

            // Create Arrow Glyph
            NSBezierPath *arrow = [[NSBezierPath alloc] init];

            NSPoint points[3];
            points[0] = NSMakePoint( rect.size.width /2, rect.origin.y + (rect.size.height /2) +3);
            points[1] = NSMakePoint( (rect.size.width /2) +3.5f, rect.origin.y + (rect.size.height /2) -3);
            points[2] = NSMakePoint( (rect.size.width /2) -3.5f, rect.origin.y + (rect.size.height /2) -3);

            [arrow appendBezierPathWithPoints: points count: 3];

            [_scrollerStroke set];
            [arrow fill];
        } else if ([arrowPosition isEqualToString:@"Single"]) {

            // Draw Decrement Button
            NSRect rect = [self rectForPart: NSScrollerIncrementLine];
            NSBezierPath *path = [[NSBezierPath alloc] init];
            NSPoint basePoints[4];

            // Add Notch
            [path appendBezierPathWithArcWithCenter:NSMakePoint((rect.size.width ) /2, (rect.origin.y  - ((rect.size.width ) /2) + 2))
                                             radius:(rect.size.width ) /2
                                         startAngle:0
                                           endAngle:180];

            // Add the rest of the points
            basePoints[0] = NSMakePoint( rect.origin.x, rect.origin.y);
            basePoints[1] = NSMakePoint( rect.origin.x, rect.origin.y + rect.size.height);
            basePoints[2] = NSMakePoint( rect.origin.x + rect.size.width, rect.origin.y + rect.size.height);
            basePoints[3] = NSMakePoint( rect.origin.x + rect.size.width, rect.origin.y);

            // Add Points to Path
            [path appendBezierPathWithPoints:basePoints count:4];

            // Fill Path
            if (!highlighted) {
                [_scrollerArrowNormalGradient drawInBezierPath:path angle:0];
            } else {
                [_scrollerArrowPushedGradient drawInBezierPath:path angle:0];
            }

            // Create Arrow Glyph
            NSBezierPath *arrow = [[NSBezierPath alloc] init];

            NSPoint points[3];
            points[0] = NSMakePoint( rect.size.width /2, rect.origin.y + (rect.size.height /2) +3);
            points[1] = NSMakePoint( (rect.size.width /2) +3.5f, rect.origin.y + (rect.size.height /2) -3);
            points[2] = NSMakePoint( (rect.size.width /2) -3.5f, rect.origin.y + (rect.size.height /2) -3);

            [arrow appendBezierPathWithPoints:points count:3];

            [_scrollerStroke set];
            [arrow fill];
        }
    } else {

        if ([arrowPosition isEqualToString:@"DoubleMax"]) {

            // Draw Increment Button
            NSRect rect = [self rectForPart:NSScrollerIncrementLine];

            if (!highlighted) {
                [_scrollerArrowNormalGradient drawInRect:rect angle:90];
            } else {
                [_scrollerArrowPushedGradient drawInRect:rect angle:90];
            }

            // Create Arrow Glyph
            NSBezierPath *arrow = [[NSBezierPath alloc] init];

            NSPoint points[3];
            points[0] = NSMakePoint( rect.origin.x + (rect.size.width /2) +3, rect.size.height /2);
            points[1] = NSMakePoint( rect.origin.x + (rect.size.height /2) -3, (rect.size.height /2) +3.5f);
            points[2] = NSMakePoint( rect.origin.x + (rect.size.height /2) -3, (rect.size.height /2) -3.5f);

            [arrow appendBezierPathWithPoints:points count:3];

            [_scrollerStroke set];
            [arrow fill];
        } else if ([arrowPosition isEqualToString:@"Single"]) {

            // Draw Decrement Button
            NSRect rect = [self rectForPart:NSScrollerIncrementLine];
            NSBezierPath *path = [[NSBezierPath alloc] init];
            NSPoint basePoints[4];

            // Add Notch
            [path appendBezierPathWithArcWithCenter:NSMakePoint(rect.origin.x - (((rect.size.height ) /2) -2), (rect.origin.y  + ((rect.size.height ) /2) ))
                                             radius:(rect.size.height ) /2
                                         startAngle:270
                                           endAngle:90];

            // Add the rest of the points
            basePoints[3] = NSMakePoint( rect.origin.x - (((rect.size.height ) /2) -1), rect.origin.y);
            basePoints[0] = NSMakePoint( rect.origin.x - (((rect.size.height ) /2) -1), rect.origin.y + rect.size.height);
            basePoints[1] = NSMakePoint( rect.origin.x + rect.size.width, rect.origin.y + rect.size.height);
            basePoints[2] = NSMakePoint( rect.origin.x + rect.size.width, rect.origin.y);

            // Add Points to Path
            [path appendBezierPathWithPoints:basePoints count:4];

            // Fill Path
            if (!highlighted) {
                [_scrollerArrowNormalGradient drawInBezierPath:path angle:0];
            } else {
                [_scrollerArrowPushedGradient drawInBezierPath:path angle:0];
            }

            // Create Arrow Glyph
            NSBezierPath *arrow = [[NSBezierPath alloc] init];

            NSPoint points[3];
            points[0] = NSMakePoint( rect.origin.x + (rect.size.width /2) +3, rect.size.height /2);
            points[1] = NSMakePoint( rect.origin.x + (rect.size.height /2) -3, (rect.size.height /2) +3.5f);
            points[2] = NSMakePoint( rect.origin.x + (rect.size.height /2) -3, (rect.size.height /2) -3.5f);

            [arrow appendBezierPathWithPoints:points count:3];

            [_scrollerStroke set];
            [arrow fill];
        }
    }
}

#pragma mark -
#pragma mark Helper Methods
- (NSUsableScrollerParts)usableParts {
    if (OSX_YOSEMITE_AND_HIGHER) {
        return [super usableParts];
    }

    if ([self arrowsPosition] != NSScrollerArrowsNone) {

        if ([self isHoriz]) {

            // Now Figure out if we can actually show all parts
            CGFloat arrowSpace = NSWidth([self rectForPart: NSScrollerIncrementLine]) + NSWidth([self rectForPart: NSScrollerDecrementLine]) +
                NSMidY([self rectForPart: NSScrollerIncrementLine]);
            CGFloat knobSpace = NSWidth([self rectForPart: NSScrollerKnob]);

            if ((arrowSpace + knobSpace) > NSWidth([self bounds])) {

                if (arrowSpace > NSWidth([self bounds])) {
                    return NSNoScrollerParts;
                } else {
                    return NSOnlyScrollerArrows;
                }
            }

        } else {

            // Now Figure out if we can actually show all parts
            CGFloat arrowSpace = NSHeight([self rectForPart: NSScrollerIncrementLine]) + NSHeight([self rectForPart: NSScrollerDecrementLine]) +
                NSMidX([self rectForPart: NSScrollerIncrementLine]);
            CGFloat knobSpace = NSHeight([self rectForPart: NSScrollerKnob]);

            if ((arrowSpace + knobSpace) > NSHeight([self bounds])) {

                if (arrowSpace > NSHeight([self bounds])) {
                    return NSNoScrollerParts;
                } else {
                    return NSOnlyScrollerArrows;
                }
            }
        }
    }

    return NSAllScrollerParts;
}

- (bool)isHoriz {
    if ([self bounds].size.width > [self bounds].size.height) {
        return YES;
    }
    return NO;
}

@end
