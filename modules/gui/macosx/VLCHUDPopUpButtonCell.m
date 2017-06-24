//
//  VLCHUDPopUpButtonCell.m
//  BGHUDAppKit
//
//  Created by BinaryGod on 5/31/08.
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

#import "VLCHUDPopUpButtonCell.h"
#import "CompatibilityFixes.h"

@implementation VLCHUDPopUpButtonCell

+ (void)load
{
    /* On 10.10+ we do not want custom drawing, therefore we swap out the implementation
     * of the selectors below with their original implementations.
     * Just calling super in the overridden methods is not enough, to get the same drawin
     * a non-subclassed cell would use.
     */
    if (OSX_YOSEMITE_AND_HIGHER) {
        swapoutOverride([VLCHUDPopUpButtonCell class], @selector(initWithCoder:));
        swapoutOverride([VLCHUDPopUpButtonCell class], @selector(drawWithFrame:inView:));
    }
}

- (instancetype)initWithCoder:(NSCoder *)coder
{
    self = [super initWithCoder:coder];
    if (self) {
        _dropShadow = [[NSShadow alloc] init];
        [_dropShadow setShadowColor:[NSColor blackColor]];
        [_dropShadow setShadowBlurRadius:2];
        [_dropShadow setShadowOffset:NSMakeSize(0, -1)];

        _strokeColor = [NSColor colorWithDeviceRed:0.749f green:0.761f blue:0.788f alpha:1.0f];
        _darkStrokeColor = [NSColor colorWithDeviceRed:0.141f green:0.141f blue:0.141f alpha:0.5f];
        _disabledStrokeColor = [NSColor colorWithDeviceRed:0.749f green:0.761f blue:0.788f alpha:0.2f];
        _selectionTextActiveColor = [NSColor whiteColor];
        _selectionTextInActiveColor = [NSColor whiteColor];
        _cellTextColor = [NSColor whiteColor];
        _disabledCellTextColor = [NSColor colorWithDeviceRed:1 green:1 blue:1 alpha:0.2f];

        _normalGradient = [[NSGradient alloc] initWithStartingColor:[NSColor colorWithDeviceRed:0.251f green:0.251f blue:0.255f alpha:0.5f]
                                                        endingColor:[NSColor colorWithDeviceRed:0.118f green:0.118f blue:0.118f alpha:0.5f]];
        _disabledNormalGradient = [[NSGradient alloc] initWithStartingColor:[NSColor colorWithDeviceRed:0.251f green:0.251f blue:0.255f alpha:0.2f]
                                                                endingColor:[NSColor colorWithDeviceRed:0.118f green:0.118f blue:0.118f alpha:0.2f]];
    }
    return self;
}

#pragma mark Drawing Functions

- (void)drawWithFrame:(NSRect)cellFrame inView:(NSView *)controlView {

    NSRect frame = cellFrame;

    // Adjust frame by .5 so lines draw true
    frame.origin.x += .5f;
    frame.origin.y += .5f;
    frame.size.height = [self cellSize].height;

    // Make Adjustments to Frame based on Cell Size
    switch ([self controlSize]) {

        case NSRegularControlSize:
            frame.origin.x += 3;
            frame.size.width -= 7;
            frame.origin.y += 2;
            frame.size.height -= 7;
            break;

        case NSSmallControlSize:
            frame.origin.y += 1;
            frame.size.height -= 6;
            frame.origin.x += 3;
            frame.size.width -= 7;
            break;

        case NSMiniControlSize:
            frame.origin.x += 1;
            frame.size.width -= 4;
            frame.size.height -= 2;
            break;
    }

    if ([self isBordered]) {
        NSBezierPath *path = [NSBezierPath bezierPathWithRoundedRect:frame xRadius: 4 yRadius: 4];

        [NSGraphicsContext saveGraphicsState];
        [_dropShadow set];
        [_darkStrokeColor set];
        [path stroke];
        [NSGraphicsContext restoreGraphicsState];

        if ([self isEnabled]) {
            [_normalGradient drawInBezierPath: path angle: 90];
            [_strokeColor set];
        } else {
            [_disabledNormalGradient drawInBezierPath: path angle: 90];
            [_disabledStrokeColor set];
        }

        [path setLineWidth: 1.0f ];
        [path stroke];
    }

    // Draw the arrows
    [self drawArrowsInRect: frame];

    // Adjust rect for title drawing
    switch ([self controlSize]) {

        case NSRegularControlSize:

            frame.origin.x += 8;
            frame.origin.y += 1;
            frame.size.width -= 29;
            break;

        case NSSmallControlSize:

            frame.origin.x += 8;
            frame.origin.y += 2;
            frame.size.width -= 29;
            break;

        case NSMiniControlSize:

            frame.origin.x += 8;
            frame.origin.y += .5f;
            frame.size.width -= 26;
            break;
    }

    NSMutableAttributedString *aTitle = [[self attributedTitle] mutableCopy];

    // Make sure aTitle actually contains something
    if (aTitle.length > 0) {
        [aTitle beginEditing];
        [aTitle removeAttribute:NSForegroundColorAttributeName range:NSMakeRange(0, aTitle.length)];

        if (self.isEnabled) {
            if (self.isHighlighted) {

                if (self.controlView.window.isKeyWindow) {
                    [aTitle addAttribute:NSForegroundColorAttributeName
                                   value:_selectionTextActiveColor
                                   range:NSMakeRange(0, aTitle.length)];
                } else {
                    [aTitle addAttribute:NSForegroundColorAttributeName
                                   value:_selectionTextInActiveColor
                                   range:NSMakeRange(0, aTitle.length)];
                }
            } else {
                [aTitle addAttribute:NSForegroundColorAttributeName
                               value:_cellTextColor
                               range:NSMakeRange(0, aTitle.length)];
            }
        } else {
            [aTitle addAttribute:NSForegroundColorAttributeName
                           value:_disabledCellTextColor
                           range:NSMakeRange(0, aTitle.length)];
        }
        [aTitle endEditing];
    }

    int arrowAdjustment = 0;

    cellFrame.size.height -= 2;
    if (self.isBordered) {
        cellFrame.origin.x += 5;
    }

    switch (self.controlSize) {
        case NSRegularControlSize:
            arrowAdjustment = 21;
            break;

        case NSSmallControlSize:
            arrowAdjustment = 18;
            break;

        case NSMiniControlSize:
            arrowAdjustment = 15;
            break;
    }

    NSRect titleFrame = NSMakeRect(cellFrame.origin.x + 5, NSMidY(cellFrame) - ([aTitle size].height/2), cellFrame.size.width - arrowAdjustment, [aTitle size].height);
    NSRect imageFrame = NSMakeRect(cellFrame.origin.x, cellFrame.origin.y, cellFrame.size.width - arrowAdjustment, cellFrame.size.height);

    if([self image]) {

        switch ([self imagePosition]) {

            case NSImageLeft:
            case NSNoImage:

                titleFrame.origin.x += 6;
                titleFrame.origin.x += [[self image] size].width;
                break;

            case NSImageOnly:
                titleFrame.size.width = 0;
                break;

            default:
                break;
        }
    }

    if([self imagePosition] != NSImageOnly) {
        [super drawTitle:aTitle withFrame:titleFrame inView:controlView];
    }


    if([self imagePosition] != NSNoImage) {
        [self drawImage:[self image] withFrame:imageFrame inView:controlView];
    }
}

- (void)drawArrowsInRect:(NSRect) frame {

    CGFloat arrowsWidth;
    CGFloat arrowsHeight;
    CGFloat arrowWidth;
    CGFloat arrowHeight;

    int arrowAdjustment = 0;

    //Adjust based on Control size
    switch ([self controlSize]) {
        default: // Silence uninitialized variable warnings
        case NSRegularControlSize:

            if ([self isBordered]) {

                arrowAdjustment = 21;
            } else {

                arrowAdjustment = 11;
            }

            arrowWidth = 3.5f;
            arrowHeight = 2.5f;
            arrowsHeight = 2;
            arrowsWidth = 2.5f;
            break;

        case NSSmallControlSize:

            if ([self isBordered]) {

                arrowAdjustment = 18;
            } else {

                arrowAdjustment = 8;
            }

            arrowWidth = 3.5f;
            arrowHeight = 2.5f;
            arrowsHeight = 2;
            arrowsWidth = 2.5f;

            break;

        case NSMiniControlSize:

            if ([self isBordered]) {

                arrowAdjustment = 15;
            } else {

                arrowAdjustment = 5;
            }

            arrowWidth = 2.5f;
            arrowHeight = 1.5f;
            arrowsHeight = 1.5f;
            arrowsWidth = 2;
            break;
    }

    frame.origin.x += (frame.size.width - arrowAdjustment);
    frame.size.width = arrowAdjustment;

    if ([self pullsDown]) {

        NSBezierPath *arrow = [[NSBezierPath alloc] init];

        NSPoint points[3];

        points[0] = NSMakePoint(frame.origin.x + ((frame.size.width /2) - arrowWidth), frame.origin.y + ((frame.size.height /2) - arrowHeight));
        points[1] = NSMakePoint(frame.origin.x + ((frame.size.width /2) + arrowWidth), frame.origin.y + ((frame.size.height /2) - arrowHeight));
        points[2] = NSMakePoint(frame.origin.x + (frame.size.width /2), frame.origin.y + ((frame.size.height /2) + arrowHeight));

        [arrow appendBezierPathWithPoints: points count: 3];
        
        if ([self isEnabled]) {
            
            if ([self isHighlighted]) {
                
                if ([[[self controlView] window] isKeyWindow]) {
                    
                    [_selectionTextActiveColor set];
                } else {
                    
                    [_selectionTextInActiveColor set];
                }
            } else {
                
                [_cellTextColor set];
            }
        } else {
            
            [_disabledCellTextColor set];
        }
        
        [arrow fill];
    } else {
        
        NSBezierPath *topArrow = [[NSBezierPath alloc] init];
        
        NSPoint topPoints[3];
        
        topPoints[0] = NSMakePoint(frame.origin.x + ((frame.size.width /2) - arrowsWidth), frame.origin.y + ((frame.size.height /2) - arrowsHeight));
        topPoints[1] = NSMakePoint(frame.origin.x + ((frame.size.width /2) + arrowsWidth), frame.origin.y + ((frame.size.height /2) - arrowsHeight));
        topPoints[2] = NSMakePoint(frame.origin.x + (frame.size.width /2), frame.origin.y + ((frame.size.height /2) - ((arrowsHeight * 2) + 2)));
        
        [topArrow appendBezierPathWithPoints: topPoints count: 3];
        
        if([self isEnabled]) {
            
            if([self isHighlighted]) {
                
                if([[[self controlView] window] isKeyWindow])
                {
                    
                    [_selectionTextActiveColor set];
                } else {
                    
                    [_selectionTextInActiveColor set];
                }
            } else {
                
                [_cellTextColor set];
            }
        } else {
            
            [_disabledCellTextColor set];
        }
        [topArrow fill];
        
        NSBezierPath *bottomArrow = [[NSBezierPath alloc] init];
        
        NSPoint bottomPoints[3];
        
        bottomPoints[0] = NSMakePoint(frame.origin.x + ((frame.size.width /2) - arrowsWidth), frame.origin.y + ((frame.size.height /2) + arrowsHeight));
        bottomPoints[1] = NSMakePoint(frame.origin.x + ((frame.size.width /2) + arrowsWidth), frame.origin.y + ((frame.size.height /2) + arrowsHeight));
        bottomPoints[2] = NSMakePoint(frame.origin.x + (frame.size.width /2), frame.origin.y + ((frame.size.height /2) + ((arrowsHeight * 2) + 2)));
        
        [bottomArrow appendBezierPathWithPoints: bottomPoints count: 3];
        
        if ([self isEnabled]) {
            
            if ([self isHighlighted]) {
                
                if ([[[self controlView] window] isKeyWindow]) {
                    
                    [_selectionTextActiveColor set];
                } else {
                    
                    [_selectionTextInActiveColor set];
                }
            } else {
                
                [_cellTextColor set];
            }
        } else {
            
            [_disabledCellTextColor set];
        }
        [bottomArrow fill];
    }
}

@end
