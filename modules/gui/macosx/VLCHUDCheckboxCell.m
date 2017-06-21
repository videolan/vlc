//
//  VLCHUDCheckboxCell.m
//  BGHUDAppKit
//
//  Created by BinaryGod on 5/25/08.
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

#import "VLCHUDCheckboxCell.h"
#import "CompatibilityFixes.h"

@implementation VLCHUDCheckboxCell

+ (void)load
{
    /* On 10.10+ we do not want custom drawing, therefore we swap out the implementation
     * of the selectors below with their original implementations.
     * Just calling super will not work, as the AppKit implementation for the NSButton
     * checkbox checks if the drawing related selectors below are overriden, and if
     * that is the case, will fall back to legacy drawing, without animation,
     * without vibrancy and non-layer-based.
     */
    if (OSX_YOSEMITE_AND_HIGHER) {
        swapoutOverride([VLCHUDCheckboxCell class], @selector(initWithCoder:));
        swapoutOverride([VLCHUDCheckboxCell class], @selector(drawImage:withFrame:inView:));
        swapoutOverride([VLCHUDCheckboxCell class], @selector(drawTitle:withFrame:inView:));
    }
}

- (instancetype)initWithCoder:(NSCoder *)coder
{
    self = [super initWithCoder:coder];
    if (self) {
        /* Colors */
        _cellTextColor         = [NSColor whiteColor];
        _disabledCellTextColor = [NSColor colorWithCalibratedWhite:1.0f alpha:0.5f];
        _strokeColor           = [NSColor whiteColor];
        _disabledStrokeColor   = [NSColor colorWithCalibratedWhite:1.0f alpha:0.5f];

        /* Gradients */
        _normalGradient    = [[NSGradient alloc] initWithStartingColor:[NSColor colorWithDeviceRed:0.251f green:0.251f blue:0.255f alpha:1.0f]
                                                           endingColor:[NSColor colorWithDeviceRed:0.118f green:0.118f blue:0.118f alpha:1.0f]];
        _highlightGradient = [[NSGradient alloc] initWithStartingColor:[NSColor colorWithDeviceRed:0.451f green:0.451f blue:0.455f alpha:1.0f]
                                                           endingColor:[NSColor colorWithDeviceRed:0.318f green:0.318f blue:0.318f alpha:1.0f]];
        _pushedGradient    = [[NSGradient alloc] initWithStartingColor:[NSColor colorWithDeviceRed:0.451f green:0.451f blue:0.455f alpha:1.0f]
                                                           endingColor:[NSColor colorWithDeviceRed:0.318f green:0.318f blue:0.318f alpha:1.0f]];
    }
    return self;
}

- (void)drawImage:(NSImage *)image withFrame:(NSRect)frame inView:(NSView *)controlView
{
    // Set frame size correctly
    NSRect backgroundFrame = frame;
    if (self.controlSize == NSSmallControlSize) {
        frame.origin.x += 4;
        frame.origin.y += 2.5;
        frame.size.width -= 5;
        frame.size.height -= 5;
        backgroundFrame = NSInsetRect(frame, 1.5, 1.5);
    } else if (self.controlSize == NSMiniControlSize) {
        frame.origin.x += 2.5;
        frame.origin.y += 4;
        frame.size.width -= 5;
        frame.size.height -= 5;
        backgroundFrame = NSInsetRect(frame, 2, 2);
    }

    // Set fill frame
    NSBezierPath *backgroundPath = [NSBezierPath bezierPathWithRoundedRect:backgroundFrame
                                                                   xRadius:2
                                                                   yRadius:2];

    // Draw background and stroke
    if([self isEnabled]) {
        if ([self isHighlighted]) {
            [_highlightGradient drawInBezierPath:backgroundPath angle:90.0];
        } else {
            [_normalGradient drawInBezierPath:backgroundPath angle:90.0];
        }
        [_strokeColor setStroke];
    } else {
        [_disabledStrokeColor setStroke];
    }

    [NSGraphicsContext saveGraphicsState];
    if ([super showsFirstResponder] && [[[self controlView] window] isKeyWindow] &&
       ([self focusRingType] == NSFocusRingTypeDefault ||
        [self focusRingType] == NSFocusRingTypeExterior)) {
        NSSetFocusRingStyle(NSFocusRingOnly);
    }
    [backgroundPath setLineWidth:1.0];
    [backgroundPath stroke];
    [NSGraphicsContext restoreGraphicsState];

    // Now drawing tick
    if ([self intValue]) {
        NSBezierPath* bezierPath = [NSBezierPath bezierPath];
        [bezierPath moveToPoint: NSMakePoint(NSMinX(frame) + 3.0, NSMidY(frame) - 2.0)];
        [bezierPath lineToPoint: NSMakePoint(NSMidX(frame), NSMidY(frame) + 2.0)];
        [bezierPath lineToPoint: NSMakePoint((NSMinX(frame) + NSWidth(frame) - 1), NSMinY(frame) - 2.0)];
        [bezierPath setLineWidth: 1.5];

        if([self isEnabled]) {
            [_strokeColor setStroke];
        } else {
            [_disabledStrokeColor setStroke];
        }
        [bezierPath stroke];
    }
}

- (NSRect)drawTitle:(NSAttributedString *)title withFrame:(NSRect)frame inView:(NSView *)controlView
{
    NSMutableAttributedString *newTitle = [title mutableCopy];

    if([self isEnabled]) {
        [newTitle addAttribute:NSForegroundColorAttributeName
                         value:_cellTextColor
                         range:NSMakeRange(0, [newTitle length])];
    } else {
        [newTitle addAttribute:NSForegroundColorAttributeName
                         value:_disabledCellTextColor
                         range:NSMakeRange(0, [newTitle length])];
    }

    return [super drawTitle: newTitle withFrame: frame inView: controlView];
}

@end
