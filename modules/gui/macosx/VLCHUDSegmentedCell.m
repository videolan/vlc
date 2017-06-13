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

@interface NSSegmentedCell (private)
-(NSRect)rectForSegment:(NSInteger)segment inFrame:(NSRect)frame;
@end

@implementation VLCHUDSegmentedCell

- (instancetype)initWithCoder:(NSCoder *)decoder
{

    self = [super initWithCoder:decoder];

    if (self) {
        _dropShadow = [[NSShadow alloc] init];
        [_dropShadow setShadowColor:[NSColor blackColor]];
        [_dropShadow setShadowBlurRadius:2];
        [_dropShadow setShadowOffset:NSMakeSize(0, -1)];

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

-(NSSegmentStyle)segmentStyle {

    return [super segmentStyle];
}

-(void)setSegmentStyle:(NSSegmentStyle) style {

    if(style == NSSegmentStyleSmallSquare || style == NSSegmentStyleRounded) {

        [super setSegmentStyle: style];
    }
}

- (void)drawWithFrame:(NSRect)frame inView:(NSView *)view {

    NSBezierPath *border;

    switch ([self segmentStyle]) {
        default: // Silence uninitialized variable warnings
        case NSSegmentStyleSmallSquare:

            //Adjust frame for shadow
            frame.origin.x += 1.5f;
            frame.origin.y += .5f;
            frame.size.width -= 3;
            frame.size.height -= 3;

            border = [[NSBezierPath alloc] init];
            [border appendBezierPathWithRect: frame];

            break;

        case NSSegmentStyleRounded: //NSSegmentStyleTexturedRounded:

            //Adjust frame for shadow
            frame.origin.x += 1.5f;
            frame.origin.y += .5f;
            frame.size.width -= 3;
            frame.size.height -= 3;

            border = [[NSBezierPath alloc] init];

            [border appendBezierPathWithRoundedRect: frame
                                            xRadius: 4.0f yRadius: 4.0f];
            break;
    }

    //Setup to Draw Border
    [NSGraphicsContext saveGraphicsState];

    //Set Shadow + Border Color
    if([self isEnabled])
    {
        [_dropShadow set];
    }
    [_strokeColor set];

    //Draw Border + Shadow
    [border stroke];

    [NSGraphicsContext restoreGraphicsState];

    int segCount = 0;

    while (segCount <= [self segmentCount] -1) {

        [self drawSegment: segCount inFrame: frame withView: view];
        segCount++;
    }
}

- (void)drawSegment:(NSInteger)segment inFrame:(NSRect)frame withView:(NSView *)view {

    //Calculate rect for this segment
    NSRect fillRect = [self rectForSegment: segment inFrame: frame];
    NSBezierPath *fillPath;

    switch ([self segmentStyle]) {
        default: // Silence uninitialized variable warnings
        case NSSegmentStyleSmallSquare:

            if(segment == ([self segmentCount] -1)) {

                if(![self hasText]) { fillRect.size.width -= 3; }
            }

            fillPath = [[NSBezierPath alloc] init];
            [fillPath appendBezierPathWithRect: fillRect];

            break;

        case NSSegmentStyleRounded: //NSSegmentStyleTexturedRounded:

            //If this is the first segment, draw rounded corners
            if(segment == 0) {

                fillPath = [[NSBezierPath alloc] init];

                [fillPath appendBezierPathWithRoundedRect: fillRect xRadius: 3 yRadius: 3];

                //Setup our joining rect
                NSRect joinRect = fillRect;
                joinRect.origin.x += 4;
                joinRect.size.width -= 4;

                [fillPath appendBezierPathWithRect: joinRect];

                //If this is the last segment, draw rounded corners
            } else if (segment == ([self segmentCount] -1)) {

                fillPath = [[NSBezierPath alloc] init];

                if(![self hasText]) { fillRect.size.width -= 3; }

                [fillPath appendBezierPathWithRoundedRect: fillRect xRadius: 3 yRadius: 3];

                //Setup our joining rect
                NSRect joinRect = fillRect;
                joinRect.size.width -= 4;

                [fillPath appendBezierPathWithRect: joinRect];

            } else {
                NSAssert(segment != 0 && segment != ([self segmentCount] -1), @"should be a middle segment");
                fillPath = [[NSBezierPath alloc] init];
                [fillPath appendBezierPathWithRect: fillRect];
            }

            break;
    }

    //Fill our pathss

    NSGradient *gradient = nil;

    if([self isEnabled])
    {
        if([self selectedSegment] == segment) {

            gradient = _highlightGradient;
        } else {

            gradient = _normalGradient;
        }
    }
    else {
        gradient = _disabledNormalGradient;
    }

    [gradient drawInBezierPath: fillPath angle: 90];

    //Draw Segment dividers ONLY if they are
    //inside segments
    if(segment != ([self segmentCount] -1)) {

        [_strokeColor set];
        [NSBezierPath strokeLineFromPoint: NSMakePoint(fillRect.origin.x + fillRect.size.width , fillRect.origin.y)
                                  toPoint: NSMakePoint(fillRect.origin.x + fillRect.size.width, fillRect.origin.y + fillRect.size.height)];
    }

    [self drawInteriorForSegment: segment withFrame: fillRect];
}

-(void)drawInteriorForSegment:(NSInteger)segment withFrame:(NSRect)rect {

    NSAttributedString *newTitle;

    //if([self labelForSegment: segment] != nil) {

    NSMutableDictionary *textAttributes = [[NSMutableDictionary alloc] initWithCapacity: 0];

    [textAttributes setValue: [NSFont controlContentFontOfSize: [NSFont systemFontSizeForControlSize: [self controlSize]]] forKey: NSFontAttributeName];
    if([self isEnabled])
    {
        [textAttributes setValue:_cellTextColor forKey: NSForegroundColorAttributeName];
    }
    else {
        [textAttributes setValue:_disabledCellTextColor forKey: NSForegroundColorAttributeName];
    }


    if([self labelForSegment: segment]) {

        newTitle = [[NSAttributedString alloc] initWithString: [self labelForSegment: segment] attributes: textAttributes];
    } else {

        newTitle = [[NSAttributedString alloc] initWithString: @"" attributes: textAttributes];
    }
    //}

    NSRect textRect = rect;
    NSRect imageRect = rect;

    if([super imageForSegment: segment] != nil) {

        NSImage *image = [self imageForSegment: segment];
        [image setFlipped: YES];

        if([self imageScalingForSegment: segment] == NSImageScaleProportionallyDown) {

            CGFloat resizeRatio = (rect.size.height - 4) / [image size].height;

            [image setScalesWhenResized: YES];
            [image setSize: NSMakeSize([image size].width * resizeRatio, rect.size.height -4)];
        }

        if([self labelForSegment: segment] != nil && ![[self labelForSegment: segment] isEqualToString: @""]) {

            imageRect.origin.y += (NSMidY(rect) - ([image size].height /2));
            imageRect.origin.x += (NSMidX(rect) - (([image size].width + [newTitle size].width + 5) /2));
            imageRect.size.height = [image size].height;
            imageRect.size.width = [image size].width;

            textRect.origin.y += (NSMidY(rect) - ([newTitle size].height /2));
            textRect.origin.x += imageRect.origin.x + [image size].width + 5;
            textRect.size.height = [newTitle size].height;
            textRect.size.width = [newTitle size].width;
            
            [image drawInRect: imageRect fromRect: NSZeroRect operation: NSCompositeSourceAtop fraction: 0.5f];
            [newTitle drawInRect: textRect];
            
        } else {
            
            //Draw Image Alone
            imageRect.origin.y += (NSMidY(rect) - ([image size].height /2));
            imageRect.origin.x += (NSMidX(rect) - ([image size].width /2));
            imageRect.size.height = [image size].height;
            imageRect.size.width = [image size].width;
            
            [image drawInRect: imageRect fromRect: NSZeroRect operation: NSCompositeSourceAtop fraction:0.5f];
        }
    } else {
        
        textRect.origin.y += (NSMidY(rect) - ([newTitle size].height /2));
        textRect.origin.x += (NSMidX(rect) - ([newTitle size].width /2));
        textRect.size.height = [newTitle size].height;
        textRect.size.width = [newTitle size].width;
        
        if(textRect.origin.x < 3) { textRect.origin.x = 3; }
        
        [newTitle drawInRect: textRect];
    }
}

-(BOOL)hasText {
    
    int i = 0;
    BOOL flag = NO;
    
    while(i <= [self segmentCount] -1) {
        
        if([self labelForSegment: i] != nil && ![[self labelForSegment: i] isEqualToString: @""]) {
            
            flag = YES;
        }
        i++;
    }
    
    return flag;
}

- (BOOL)_canAnimate {
    
    return NO;
}

- (BOOL)_isSliderStyle {
    return NO;
}

@end
