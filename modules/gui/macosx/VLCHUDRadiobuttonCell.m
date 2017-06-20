//
//  VLCHUDRadiobuttonCell.m
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

#import "VLCHUDRadiobuttonCell.h"

@implementation VLCHUDRadiobuttonCell

- (instancetype) initWithCoder:(NSCoder *)coder
{
    self = [super initWithCoder:coder];
    if (self) {
        _normalGradient    = [[NSGradient alloc] initWithStartingColor:[NSColor colorWithDeviceRed:0.251f green:0.251f blue:0.255f alpha:1.0f]
                                                           endingColor:[NSColor colorWithDeviceRed:0.118f green:0.118f blue:0.118f alpha:1.0f]];
        _highlightGradient = [[NSGradient alloc] initWithStartingColor:[NSColor colorWithDeviceRed:0.451f green:0.451f blue:0.455f alpha:1.0f]
                                                           endingColor:[NSColor colorWithDeviceRed:0.318f green:0.318f blue:0.318f alpha:1.0f]];
        _pushedGradient    = [[NSGradient alloc] initWithStartingColor:[NSColor colorWithDeviceRed:0.451f green:0.451f blue:0.455f alpha:1.0f]
                                                           endingColor:[NSColor colorWithDeviceRed:0.318f green:0.318f blue:0.318f alpha:1.0f]];
        _textColor         = [NSColor whiteColor];
    }
    return self;
}

- (void) drawImage:(NSImage *)image withFrame:(NSRect)frame inView:(NSView *)controlView
{
    // Set text color
    NSMutableAttributedString *colorTitle = [[NSMutableAttributedString alloc] initWithAttributedString:self.attributedTitle];
    NSRange titleRange = NSMakeRange(0, [colorTitle length]);
    [colorTitle addAttribute:NSForegroundColorAttributeName value:_textColor range:titleRange];
    [self setAttributedTitle:colorTitle];

    // Set frame size correctly
    NSRect innerFrame = frame;
    if (self.controlSize == NSSmallControlSize) {
        frame.origin.x += 3.5;
        frame.origin.y += 4;
        frame.size.width -= 7;
        frame.size.height -= 7;
        innerFrame = NSInsetRect(frame, 2, 2);
    } else if (self.controlSize == NSMiniControlSize) {
        frame.origin.x += 2;
        frame.origin.y += 4;
        frame.size.width -= 6.5;
        frame.size.height -= 6.5;
        innerFrame = NSInsetRect(frame, 4, 4);
    }

    // Set fill frame
    NSBezierPath *backgroundPath = [NSBezierPath bezierPathWithOvalInRect:frame];

    // Draw border and background
    [NSColor.whiteColor setStroke];

    [NSGraphicsContext saveGraphicsState];
    if ([super showsFirstResponder] && [[[self controlView] window] isKeyWindow] &&
        ([self focusRingType] == NSFocusRingTypeDefault ||
         [self focusRingType] == NSFocusRingTypeExterior)) {
        NSSetFocusRingStyle(NSFocusRingOnly);
    }
    [backgroundPath setLineWidth:1.5];
    [backgroundPath stroke];
    [NSGraphicsContext restoreGraphicsState];

    if ([self isEnabled]) {
        if ([self isHighlighted]) {
            [_normalGradient drawInBezierPath:backgroundPath angle:90.0];
        } else {
            [_pushedGradient drawInBezierPath:backgroundPath angle:90.0];
        }
    } else {
        [[NSColor lightGrayColor] setFill];
        [backgroundPath fill];
    }

    // Draw dot
    if ([self integerValue] && [self isEnabled]) {
        NSBezierPath* bezierPath = [NSBezierPath bezierPathWithOvalInRect:innerFrame];
        [[NSColor whiteColor] setFill];
        [bezierPath fill];
    }
}

- (NSRect)drawTitle:(NSAttributedString *)title withFrame:(NSRect)frame inView:(NSView *)controlView
{
    NSMutableAttributedString *coloredTitle = [title mutableCopy];

    if (self.isEnabled) {
        [coloredTitle addAttribute:NSForegroundColorAttributeName
                             value:[NSColor whiteColor]
                             range:NSMakeRange(0, coloredTitle.length)];
    } else {
        [coloredTitle addAttribute:NSForegroundColorAttributeName
                             value:[NSColor grayColor]
                             range:NSMakeRange(0, coloredTitle.length)];
    }

    return [super drawTitle:coloredTitle withFrame:frame inView:controlView];
}

@end
