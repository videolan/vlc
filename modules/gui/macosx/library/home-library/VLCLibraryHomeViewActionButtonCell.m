/*****************************************************************************
 * VLCLibraryHomeViewActionButtonCell.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2024 VLC authors and VideoLAN
 *
 * Authors: Claudio Cambra <developer@claudiocambra.com>
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

#import "VLCLibraryHomeViewActionButtonCell.h"

#import "extensions/NSColor+VLCAdditions.h"
#import "extensions/NSFont+VLCAdditions.h"

#import "views/VLCUIUnits.h"

@implementation VLCLibraryHomeViewActionButtonCell

- (CGFloat)contentPaddingForBounds:(NSRect)bounds
{
    return NSWidth(bounds) < VLCUIUnits.actionButtonNarrowWidthThreshold
        ? VLCUIUnits.mediumSpacing
        : VLCUIUnits.mediumSpacing + VLCUIUnits.smallSpacing;
}

- (NSRect)imageRectForBounds:(NSRect)bounds
{
    return [self imageRectForBounds:bounds flipped:self.controlView.isFlipped];
}

- (NSRect)imageRectForBounds:(NSRect)bounds flipped:(BOOL)flipped
{
    if (NSWidth(bounds) < VLCUIUnits.actionButtonCompactWidthThreshold) {
        const CGFloat side = MIN(VLCUIUnits.smallIconSize,
                                 MAX(0., NSHeight(bounds) - VLCUIUnits.mediumSpacing));
        const CGFloat originY = flipped
            ? NSMinY(bounds) + VLCUIUnits.smallSpacing
            : NSMaxY(bounds) - VLCUIUnits.smallSpacing - side;
        return NSMakeRect(NSMidX(bounds) - side / 2., originY, side, side);
    }

    const CGFloat padding = [self contentPaddingForBounds:bounds];
    const CGFloat preferredSide = NSWidth(bounds) < VLCUIUnits.actionButtonNarrowWidthThreshold
        ? VLCUIUnits.mediumIconSize
        : VLCUIUnits.largeIconSize;
    const CGFloat side = MIN(preferredSide, MAX(0., NSHeight(bounds) - 2. * padding));
    const BOOL rightToLeft = self.userInterfaceLayoutDirection == NSUserInterfaceLayoutDirectionRightToLeft;
    const CGFloat originX = rightToLeft ? NSMaxX(bounds) - padding - side : NSMinX(bounds) + padding;
    return NSMakeRect(originX, NSMidY(bounds) - side / 2., side, side);
}

- (NSRect)titleRectForBounds:(NSRect)bounds
{
    return [self titleRectForBounds:bounds flipped:self.controlView.isFlipped];
}

- (NSRect)titleRectForBounds:(NSRect)bounds flipped:(BOOL)flipped
{
    if (NSWidth(bounds) < VLCUIUnits.actionButtonCompactWidthThreshold) {
        const NSRect imageRect = [self imageRectForBounds:bounds flipped:flipped];
        const CGFloat originY = flipped
            ? NSMaxY(imageRect) + VLCUIUnits.smallSpacing / 2.
            : NSMinY(bounds) + VLCUIUnits.smallSpacing;
        return NSMakeRect(NSMinX(bounds) + VLCUIUnits.compactControlPadding, originY,
                          MAX(0., NSWidth(bounds) - VLCUIUnits.compactControlPadding * 2.),
                          MAX(0., NSHeight(bounds) - NSHeight(imageRect) - 2. * VLCUIUnits.smallSpacing - VLCUIUnits.smallSpacing / 2.));
    }

    const CGFloat padding = [self contentPaddingForBounds:bounds];
    const CGFloat gap = VLCUIUnits.mediumSpacing;
    const NSRect imageRect = [self imageRectForBounds:bounds flipped:flipped];
    const BOOL rightToLeft = self.userInterfaceLayoutDirection == NSUserInterfaceLayoutDirectionRightToLeft;
    const CGFloat originX = rightToLeft ? NSMinX(bounds) + padding : NSMaxX(imageRect) + gap;
    const CGFloat width = MAX(0., NSWidth(bounds) - 2. * padding - NSWidth(imageRect) - gap);
    return NSMakeRect(originX, NSMinY(bounds) + VLCUIUnits.mediumSpacing,
                      width, MAX(0., NSHeight(bounds) - 2. * VLCUIUnits.mediumSpacing));
}

- (void)drawImage:(NSImage *)image withFrame:(NSRect)frame inView:(NSView *)controlView
{
    if (image.size.width <= 0. || image.size.height <= 0. || NSIsEmptyRect(frame)) {
        return;
    }

    NSColor * const tint = self.isEnabled ? NSColor.VLCSubtlerAccentColor : NSColor.disabledControlTextColor;
    NSImage * const tintedImage = [NSImage imageWithSize:image.size
                                                 flipped:NO
                                          drawingHandler:^BOOL(NSRect destinationRect) {
        [image drawInRect:destinationRect];
        [tint setFill];
        NSRectFillUsingOperation(destinationRect, NSCompositingOperationSourceIn);
        return YES;
    }];

    const CGFloat scale = MIN(NSWidth(frame) / image.size.width, NSHeight(frame) / image.size.height);
    const NSSize scaledSize = NSMakeSize(image.size.width * scale, image.size.height * scale);
    const NSRect imageRect = NSMakeRect(NSMidX(frame) - scaledSize.width / 2.,
                                        NSMidY(frame) - scaledSize.height / 2.,
                                        scaledSize.width,
                                        scaledSize.height);
    [tintedImage drawInRect:imageRect
                   fromRect:NSZeroRect
                  operation:NSCompositingOperationSourceOver
                   fraction:1.
             respectFlipped:YES
                      hints:nil];
}

- (void)drawInteriorWithFrame:(NSRect)cellFrame inView:(NSView *)controlView
{
    [self drawImage:self.image
          withFrame:[self imageRectForBounds:cellFrame flipped:controlView.isFlipped]
             inView:controlView];

    const NSRect textRect = [self titleRectForBounds:cellFrame flipped:controlView.isFlipped];
    if (NSIsEmptyRect(textRect)) {
        return;
    }

    const BOOL compact = NSWidth(cellFrame) < VLCUIUnits.actionButtonCompactWidthThreshold;
    const BOOL rightToLeft = self.userInterfaceLayoutDirection == NSUserInterfaceLayoutDirectionRightToLeft;
    NSMutableParagraphStyle * const paragraphStyle = [[NSMutableParagraphStyle alloc] init];
    paragraphStyle.alignment = compact ? NSTextAlignmentCenter : rightToLeft ? NSTextAlignmentRight : NSTextAlignmentLeft;
    paragraphStyle.baseWritingDirection = rightToLeft ? NSWritingDirectionRightToLeft : NSWritingDirectionLeftToRight;
    paragraphStyle.lineBreakMode = NSLineBreakByWordWrapping;
    NSFont * const titleFont = NSWidth(cellFrame) < VLCUIUnits.actionButtonNarrowWidthThreshold
        ? (compact ? NSFont.VLCLibrarySmallSubsectionSubheaderFont
                   : NSFont.VLCLibraryCompactSubsectionSubheaderFont)
        : NSFont.VLCLibrarySubsectionSubheaderFont;
    NSColor * const titleColor = self.isEnabled ? NSColor.controlTextColor : NSColor.disabledControlTextColor;
    NSDictionary<NSAttributedStringKey, id> * const titleAttributes = @{
        NSFontAttributeName: titleFont,
        NSForegroundColorAttributeName: titleColor,
        NSParagraphStyleAttributeName: paragraphStyle
    };

    const NSStringDrawingOptions options = NSStringDrawingUsesLineFragmentOrigin | NSStringDrawingTruncatesLastVisibleLine;
    const NSSize measurementSize = NSMakeSize(CGFLOAT_MAX, CGFLOAT_MAX);
    const CGFloat titleLineHeight = ceil(NSHeight([@" " boundingRectWithSize:measurementSize
                                                                     options:NSStringDrawingUsesLineFragmentOrigin
                                                                  attributes:titleAttributes]));
    const CGFloat maximumTitleHeight = MIN(titleLineHeight * VLCUIUnits.actionButtonMaximumTitleLines, NSHeight(textRect));
    const NSRect measuredTitleRect = [self.title boundingRectWithSize:NSMakeSize(NSWidth(textRect), maximumTitleHeight)
                                                              options:options
                                                           attributes:titleAttributes];
    const CGFloat titleHeight = MIN(ceil(NSHeight(measuredTitleRect)), maximumTitleHeight);
    const CGFloat titleMinY = NSMidY(textRect) - titleHeight / 2.;
    const NSRect titleRect = NSMakeRect(NSMinX(textRect), titleMinY, NSWidth(textRect), titleHeight);
    [self.title drawWithRect:titleRect options:options attributes:titleAttributes];
}

- (void)drawBezelWithFrame:(NSRect)frame inView:(NSView *)controlView
{
    const BOOL pressed = self.isEnabled && self.isHighlighted;
    const CGFloat halfBorder = VLCUIUnits.borderThickness / 2.;
    NSBezierPath * const bezelPath =
        [NSBezierPath bezierPathWithRoundedRect:NSInsetRect(frame, halfBorder, halfBorder)
                                        xRadius:VLCUIUnits.mediumSpacing
                                        yRadius:VLCUIUnits.mediumSpacing];
    bezelPath.lineWidth = VLCUIUnits.borderThickness;
    [NSColor.controlBackgroundColor setFill];
    [bezelPath fill];
    [(pressed ? NSColor.VLCActionButtonPressedBackgroundColor
              : NSColor.VLCActionButtonBackgroundColor) setFill];
    [bezelPath fill];
    [(pressed ? NSColor.VLCActionButtonPressedBorderColor
              : NSColor.VLCSubtleBorderColor) setStroke];
    [bezelPath stroke];
}

@end
