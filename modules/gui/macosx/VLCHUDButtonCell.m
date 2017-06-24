//
//  VLCHUDButtonCell.m
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

#import "VLCHUDButtonCell.h"
#import "CompatibilityFixes.h"

@implementation VLCHUDButtonCell


+ (void)load
{
    /* On 10.10+ we do not want custom drawing, therefore we swap out the implementation
     * of the selectors below with their original implementations.
     * Just calling super is not enough, as it would still result in different drawing
     * due to lack of vibrancy.
     */
    if (OSX_YOSEMITE_AND_HIGHER) {
        swapoutOverride([VLCHUDButtonCell class], @selector(initWithCoder:));
        swapoutOverride([VLCHUDButtonCell class], @selector(drawBezelWithFrame:inView:));
        swapoutOverride([VLCHUDButtonCell class], @selector(drawTitle:withFrame:inView:));
    }
}

- (instancetype)initWithCoder:(NSCoder *)coder
{
    self = [super initWithCoder:coder];
    if (self) {
        _disabledGradient  = [[NSGradient alloc] initWithStartingColor:[NSColor colorWithDeviceRed:0.251f green:0.251f blue:0.255f alpha:1.0f]
                                                           endingColor:[NSColor colorWithDeviceRed:0.118f green:0.118f blue:0.118f alpha:1.0f]];
        _normalGradient    = [[NSGradient alloc] initWithStartingColor:[NSColor colorWithDeviceRed:0.251f green:0.251f blue:0.255f alpha:0.7f]
                                                           endingColor:[NSColor colorWithDeviceRed:0.118f green:0.118f blue:0.118f alpha:0.7f]];
        _highlightGradient = [[NSGradient alloc] initWithStartingColor:[NSColor colorWithDeviceRed:0.451f green:0.451f blue:0.455f alpha:1.0f]
                                                           endingColor:[NSColor colorWithDeviceRed:0.318f green:0.318f blue:0.318f alpha:1.0f]];
        _pushedGradient    = [[NSGradient alloc] initWithStartingColor:[NSColor colorWithDeviceRed:0.451f green:0.451f blue:0.455f alpha:1.0f]
                                                           endingColor:[NSColor colorWithDeviceRed:0.318f green:0.318f blue:0.318f alpha:1.0f]];
        _enabledTextColor  = [NSColor whiteColor];
        _disabledTextColor = [NSColor grayColor];
    }
    return self;
}

- (void)drawBezelWithFrame:(NSRect)frame inView:(NSView *)controlView
{
    // Set frame to the correct size
    frame.size.height = self.cellSize.height;

    // Inset rect to have enough room for the stroke
    frame = NSInsetRect(frame, 1, 1);
    if (self.bezelStyle == NSRoundRectBezelStyle) {
        [self drawRoundRectButtonBezelInRect:frame];
    } else {
        [super drawBezelWithFrame:frame inView:controlView];
    }
}

- (NSRect)drawTitle:(NSAttributedString *)title withFrame:(NSRect)frame inView:(NSView *)controlView
{
    NSMutableAttributedString *coloredTitle = [[NSMutableAttributedString alloc]
                                               initWithAttributedString:title];
    if (self.isEnabled) {
        [coloredTitle addAttribute:NSForegroundColorAttributeName
                             value:_enabledTextColor
                             range:NSMakeRange(0, coloredTitle.length)];
    } else {
        [coloredTitle addAttribute:NSForegroundColorAttributeName
                             value:_disabledTextColor
                             range:NSMakeRange(0, coloredTitle.length)];
    }

    return [super drawTitle:coloredTitle withFrame:frame inView:controlView];
}

- (void)drawRoundRectButtonBezelInRect:(NSRect)rect
{
    NSBezierPath *path;
    if (self.controlSize == NSMiniControlSize) {
        rect = NSInsetRect(rect, 1.0, 2.0);
        path = [NSBezierPath bezierPathWithRoundedRect:rect xRadius:3.0 yRadius:3.0];
    } else {
        path = [NSBezierPath bezierPathWithRoundedRect:rect xRadius:8.0 yRadius:8.0];
    }
    if (self.highlighted) {
        [_pushedGradient drawInBezierPath:path angle:90.0f];
    } else if (!self.enabled) {
        [_disabledGradient drawInBezierPath:path angle:90.0f];
    } else {
        [_normalGradient drawInBezierPath:path angle:90.0f];
    }
    [[NSColor colorWithCalibratedWhite:1.0 alpha:1.0] setStroke];
    [path setLineWidth:1.0];
    [path stroke];
}

@end
