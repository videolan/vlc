//
//  VLCHUDTextFieldCell.m
//  BGHUDAppKit
//
//  Created by BinaryGod on 6/2/08.
//
//		Copyright (c) 2008, Tim Davis (BinaryMethod.com, binary.god@gmail.com)
//  All rights reserved.
//
//		Redistribution and use in source and binary forms, with or without modification,
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
//
//  History
//
//		8/30/2010 - Fixed placeholder alignment not rendering while in design view,
//					provided by [tylerb](GitHub).

#import "VLCHUDTextFieldCell.h"
#import "CompatibilityFixes.h"

@interface NSTextFieldCell (Private)

- (void)_drawKeyboardFocusRingWithFrame:(NSRect)rect inView:(NSView*)view;

@end

@interface VLCHUDTextFieldCell () {
    bool fillsBackground;
}

@end

@implementation VLCHUDTextFieldCell

+ (void)load
{
    /* On 10.10+ we do not want custom drawing, therefore we swap out the implementation
     * of the selectors below with their original implementations.
     */
    if (OSX_YOSEMITE_AND_HIGHER) {
        swapoutOverride([VLCHUDTextFieldCell class], @selector(initTextCell:));
        swapoutOverride([VLCHUDTextFieldCell class], @selector(initWithCoder:));
        swapoutOverride([VLCHUDTextFieldCell class], @selector(setUpFieldEditorAttributes:));
        swapoutOverride([VLCHUDTextFieldCell class], @selector(drawWithFrame:inView:));
        swapoutOverride([VLCHUDTextFieldCell class], @selector(_drawKeyboardFocusRingWithFrame:inView:));
    }
}

#pragma mark Drawing Functions

- (instancetype)initTextCell:(NSString *)aString
{
    self = [super initTextCell: aString];

    if (self) {
        [self commonInit];
    }

    return self;
}

- (instancetype)initWithCoder:(NSCoder *)decoder
{
    self = [super initWithCoder:decoder];

    if (self) {
        [self commonInit];
    }

    return self;
}

- (void)commonInit
{
    // Init colors
    _focusRing = [[NSShadow alloc] init];
    [_focusRing setShadowColor:NSColor.whiteColor];
    [_focusRing setShadowBlurRadius:3];
    [_focusRing setShadowOffset:NSMakeSize(0, 0)];

    _strokeColor = [NSColor colorWithDeviceRed:0.749f green:0.761f blue:0.788f alpha:1.0f];
    _disabledStrokeColor = [NSColor colorWithDeviceRed:1 green:1 blue:1 alpha:0.2f];

    _selectionHighlightActiveColor = [NSColor darkGrayColor];
    _selectionTextActiveColor = [NSColor whiteColor];
    _selectionHighlightInActiveColor = [NSColor darkGrayColor];
    _selectionTextInActiveColor = [NSColor whiteColor];

    _placeholderTextColor = [NSColor grayColor];
    _cellTextColor = [NSColor whiteColor];
    _textFillColor = [NSColor colorWithDeviceRed:.224f green:.224f blue:.224f alpha:.95f];

    // Init some properties
    [self setTextColor:_cellTextColor];

    if ([self drawsBackground]) {
        fillsBackground = YES;
    }

    [self setDrawsBackground: NO];
}

- (NSText *)setUpFieldEditorAttributes:(NSText *)textObj
{
    NSText *newText = [super setUpFieldEditorAttributes:textObj];
    NSColor *textColor = _cellTextColor;
    [(NSTextView *)newText setInsertionPointColor:textColor];
    return newText;
}

- (void)drawWithFrame:(NSRect)cellFrame inView:(NSView *)controlView
{
    // Adjust Rect
    cellFrame = NSInsetRect(cellFrame, 0.5f, 0.5f);

    // Create Path
    NSBezierPath *path = [NSBezierPath bezierPath];

    if ([self bezelStyle] == NSTextFieldRoundedBezel) {

        [path appendBezierPathWithArcWithCenter:NSMakePoint(cellFrame.origin.x + (cellFrame.size.height /2), cellFrame.origin.y + (cellFrame.size.height /2))
                                         radius:cellFrame.size.height /2
                                     startAngle:90
                                       endAngle:270];

        [path appendBezierPathWithArcWithCenter:NSMakePoint(cellFrame.origin.x + (cellFrame.size.width - (cellFrame.size.height /2)), cellFrame.origin.y + (cellFrame.size.height /2))
                                         radius:cellFrame.size.height /2
                                     startAngle:270
                                       endAngle:90];

        [path closePath];

    } else {
        [path appendBezierPathWithRoundedRect: cellFrame xRadius: 3.0f yRadius: 3.0f];
    }

    // Draw Background
    if (fillsBackground) {
        [_textFillColor set];
        [path fill];
    }

    if ([self isBezeled] || [self isBordered]) {

        [NSGraphicsContext saveGraphicsState];

        if ([super showsFirstResponder] && [[[self controlView] window] isKeyWindow] &&
           ([self focusRingType] == NSFocusRingTypeDefault ||
            [self focusRingType] == NSFocusRingTypeExterior)) {
               [_focusRing set];
           }

        // Check State
        if ([self isEnabled]) {
            [_strokeColor set];
        } else {
            [_disabledStrokeColor set];
        }

        [path setLineWidth:1.0f];
        [path stroke];

        [NSGraphicsContext restoreGraphicsState];
    }

    // Get TextView for this editor
    NSTextView* view = (NSTextView*)[[controlView window] fieldEditor: NO forObject: controlView];

    // If window/app is active draw the highlight/text in active colors
    if (![self isHighlighted]) {

        if ([view selectedRange].length > 0) {

            // Get Attributes of the selected text
            NSMutableDictionary *dict = [[view selectedTextAttributes] mutableCopy];

            if ([[[self controlView] window] isKeyWindow]) {
                [dict setObject:_selectionHighlightActiveColor
                         forKey:NSBackgroundColorAttributeName];

                [view setTextColor:_selectionTextActiveColor
                             range:[view selectedRange]];
            } else {
                [dict setObject:_selectionHighlightInActiveColor
                         forKey:NSBackgroundColorAttributeName];

                [view setTextColor:_selectionTextInActiveColor
                             range:[view selectedRange]];
            }

            [view setSelectedTextAttributes:dict];
        } else {
            // Only change color (marks view as dirty) if it had a selection at some point,
            // thus changing the colors.
            if ([view textColor] != _cellTextColor) {
                [self setTextColor:_cellTextColor];
                [view setTextColor:_cellTextColor];
            }
        }
    } else {

        if ([self isEnabled]) {
            if ([self isHighlighted]) {
                if ([[[self controlView] window] isKeyWindow]){
                    [self setTextColor:_selectionTextActiveColor];
                } else {
                    [self setTextColor:_selectionTextInActiveColor];
                }
            } else {
                [self setTextColor:_cellTextColor];
            }
        } else {
            [self setTextColor:_disabledCellTextColor];
        }
    }

    // Check to see if the attributed placeholder has been set or not
    if (![self placeholderAttributedString] && [self placeholderString]) {

        NSMutableParagraphStyle *style = [[NSMutableParagraphStyle alloc] init];

        // Set the paragraph style
        [style setAlignment: [self alignment]];

        // Attributed string doesn't exist lets create it
        NSDictionary *attributes = @{
                                     NSForegroundColorAttributeName : _placeholderTextColor,
                                     NSParagraphStyleAttributeName  : style
                                     };
        NSAttributedString *attributedPlaceholder = [[NSAttributedString alloc] initWithString:self.placeholderString attributes:attributes];
        [self setPlaceholderAttributedString:attributedPlaceholder];
    } else if ([self placeholderAttributedString] && [[self placeholderAttributedString] length] > 0) {

        // Check to see if the proper styles have been applied
        if ([[[self placeholderAttributedString] attribute:NSParagraphStyleAttributeName atIndex:1 effectiveRange:nil] alignment] != [self alignment]) {

            NSMutableParagraphStyle *style = [[NSMutableParagraphStyle alloc] init];

            // Set the paragraph style
            [style setAlignment:[self alignment]];
            
            // Get current attr string
            NSMutableAttributedString *adjPlaceholder = [[NSMutableAttributedString alloc] initWithAttributedString:[self placeholderAttributedString]];
            
            // Add style attr
            [adjPlaceholder addAttribute:NSParagraphStyleAttributeName value:style range:NSMakeRange(0, [adjPlaceholder length])];
            
            // Reset Placeholder to correct placeholder
            [self setPlaceholderAttributedString:adjPlaceholder];
        }
    }
    
    // Adjust Frame so Text Draws correctly
    switch (self.controlSize) {
        case NSRegularControlSize:
            cellFrame.origin.y += (self.bezelStyle != NSTextFieldRoundedBezel) ? 1 : 0;
            break;
            
        case NSSmallControlSize:
            cellFrame.origin.y += (self.bezelStyle == NSTextFieldRoundedBezel) ? 1 : 0;
            break;
            
        case NSMiniControlSize:
            cellFrame.origin.x += (self.bezelStyle == NSTextFieldRoundedBezel) ? 1 : 0;
            break;
            
        default:
            break;
    }

    [self drawInteriorWithFrame: cellFrame inView: controlView];
}

- (void)_drawKeyboardFocusRingWithFrame:(NSRect)rect inView:(NSView*)view
{
    // Do nothing
}

@end
