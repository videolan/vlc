/*****************************************************************************
 * VLCHUDTextFieldCell.m: Custom textfield cell UI for dark HUD Panels
 *****************************************************************************
 * Copyright (C) 2016 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Marvin Scholz <epirat07 -at- gmail -dot- com>
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

#import "VLCHUDTextFieldCell.h"

@implementation VLCHUDTextFieldCell {
    BOOL myCustomDrawsBackground;
    BOOL myCustomDrawsBorder;
}

- (instancetype) initWithCoder:(NSCoder *)coder
{
    self = [super initWithCoder:coder];
    if (self) {
        [self setupSelf];
    }

    return self;
}

- (instancetype) initTextCell:(NSString *)aString
{
    self = [super initTextCell:aString];
    if (self) {
        [self setupSelf];
    }

    return self;
}

- (instancetype) initImageCell:(NSImage *)image
{
    self = [super initImageCell:image];
    if (self) {
        [self setupSelf];
    }

    return self;
}

- (void)setupSelf
{
    myCustomDrawsBorder = self.bordered || self.bezeled;
    myCustomDrawsBackground = self.drawsBackground;
    _enabledTextColor  = [NSColor greenColor];
    _disabledTextColor = [NSColor blueColor];
    _enabledBorderColor = [NSColor yellowColor];
    _disabledBorderColor = [NSColor greenColor];
    _enabledBackgroundColor = [NSColor purpleColor];
    _disabledBackgroundColor = [NSColor blackColor];
    _borderWidth = 1.0;

    /* Disable border, enable bezeled, disable background
     * in case we need background (TextField instead of Label)
     *
     * This is kind of redundant, as enabling bezeled will
     * disable bordered anyway, but I've done it for clarity.
     *
     * ORDER IS IMPORTANT!
     * Disabling background and enabling bezeled afterwards
     * will re-enable background!
     */
    if (self.drawsBackground) {
        [self setBordered:NO];
        [self setBezeled:YES];
        [self setDrawsBackground:NO];
    } else {
        [self setBordered:NO];
        [self setBezeled:NO];
        [self setDrawsBackground:NO];
    }
}

- (NSText *)setUpFieldEditorAttributes:(NSText *)text
{
    NSText *newText = [super setUpFieldEditorAttributes:text];

    // Set the text color for entered text
    [newText setTextColor:_enabledTextColor];

    // Set the cursor color
    [(NSTextView *)newText setInsertionPointColor:_enabledTextColor];
    return newText;
}


- (void)drawWithFrame:(NSRect)cellFrame inView:(NSView *)controlView
{
    NSRect borderRect = NSInsetRect(cellFrame, _borderWidth, _borderWidth);

    if (self.enabled) {
        [_enabledBackgroundColor setFill];
        [_enabledBorderColor setStroke];
        [self setTextColor:_enabledTextColor];
    } else {
        [_disabledBackgroundColor setFill];
        [_disabledBorderColor setStroke];
        [self setTextColor:_disabledTextColor];
    }

    // Draw background
    if (myCustomDrawsBackground) {
        NSRectFill(cellFrame);
    }

    // Draw Border
    if (myCustomDrawsBorder) {
        NSBezierPath *borderPath = [NSBezierPath bezierPathWithRect:borderRect];
        [borderPath setLineWidth:_borderWidth];
        [borderPath stroke];
    }

    /* Call draw interior to position text correctly
     *
     * For this to work, bezeled has to be enabled and drawsBackground
     * needs to be disabled, else we still get a background drawn.
     * When using bordered instead of bezeled, we get wrong cursor position.
     */
    [self drawInteriorWithFrame:cellFrame inView:controlView];
}


@end
