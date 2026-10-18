/*****************************************************************************
 * VLCLibraryHomeViewBrowseCarouselItemCell.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2026 VLC authors and VideoLAN
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

#import "VLCLibraryHomeViewBrowseCarouselItemCell.h"

#import "extensions/NSColor+VLCAdditions.h"
#import "extensions/NSFont+VLCAdditions.h"
#import "views/VLCUIUnits.h"

@implementation VLCLibraryHomeViewBrowseCarouselItemCell

- (NSSize)cellSize
{
    return VLCUIUnits.libraryCompactCardSize;
}

- (void)drawWithFrame:(NSRect)cellFrame inView:(NSView *)controlView
{
    const CGFloat borderThickness = VLCUIUnits.borderThickness;
    const CGFloat contentSpacing = VLCUIUnits.mediumSpacing + VLCUIUnits.smallSpacing;
    const CGFloat cardCornerRadius = VLCUIUnits.largeCornerRadius;
    NSBezierPath * const cardPath =
        [NSBezierPath bezierPathWithRoundedRect:NSInsetRect(cellFrame, borderThickness, borderThickness)
                                        xRadius:cardCornerRadius
                                        yRadius:cardCornerRadius];
    NSColor * const fillColor = self.isHighlighted
        ? NSColor.selectedControlColor
        : NSColor.VLCLibraryCardBackgroundColor;
    [fillColor setFill];
    [cardPath fill];
    [NSColor.VLCSubtleBorderColor setStroke];
    cardPath.lineWidth = borderThickness;
    [cardPath stroke];

    const CGFloat tileSize = VLCUIUnits.largeIconContainerSize;
    const NSRect tileRect = NSMakeRect(NSMinX(cellFrame) + contentSpacing,
                                       NSMidY(cellFrame) - tileSize / 2., tileSize, tileSize);
    const CGFloat tileCornerRadius = VLCUIUnits.mediumCornerRadius;
    NSBezierPath * const tilePath = [NSBezierPath bezierPathWithRoundedRect:tileRect
                                                                    xRadius:tileCornerRadius
                                                                    yRadius:tileCornerRadius];
    NSColor * const tileColor = self.isHighlighted
        ? self.iconTint.VLCHighlightedTintColor
        : self.iconTint.VLCSubtleTintColor;
    [tileColor setFill];
    [tilePath fill];

    const CGFloat iconSize = VLCUIUnits.mediumIconSize;
    NSImage * const sourceImage = self.image;
    NSColor * const iconTint = self.iconTint;
    NSImage * const tintedIcon = [NSImage imageWithSize:NSMakeSize(iconSize, iconSize)
                                                flipped:NO
                                         drawingHandler:^BOOL(NSRect iconRect) {
        const NSSize imageSize = sourceImage.size;
        if (imageSize.width <= 0. || imageSize.height <= 0.) {
            return YES;
        }
        const CGFloat scale = MIN(NSWidth(iconRect) / imageSize.width,
                                  NSHeight(iconRect) / imageSize.height);
        const NSSize scaledSize = NSMakeSize(imageSize.width * scale,
                                             imageSize.height * scale);
        const NSRect imageRect = NSMakeRect(NSMidX(iconRect) - scaledSize.width / 2.,
                                            NSMidY(iconRect) - scaledSize.height / 2.,
                                            scaledSize.width,
                                            scaledSize.height);
        [sourceImage drawInRect:imageRect];
        [iconTint setFill];
        NSRectFillUsingOperation(iconRect, NSCompositingOperationSourceIn);
        return YES;
    }];
    const NSRect iconRect = NSMakeRect(NSMidX(tileRect) - iconSize / 2.,
                                       NSMidY(tileRect) - iconSize / 2., iconSize, iconSize);
    [tintedIcon drawInRect:iconRect
                  fromRect:NSZeroRect
                 operation:NSCompositingOperationSourceOver
                  fraction:self.isEnabled ? 1. : VLCUIUnits.disabledOpacity
            respectFlipped:YES
                     hints:nil];

    NSFont * const titleFont = NSFont.VLCLibrarySubsectionSubheaderFont;
    NSFont * const subtitleFont = NSFont.VLCLibraryItemAnnotationFont;
    const CGFloat titleHeight = ceil(titleFont.ascender - titleFont.descender + titleFont.leading);
    const CGFloat subtitleHeight = ceil(subtitleFont.ascender - subtitleFont.descender + subtitleFont.leading);
    const CGFloat textSpacing = VLCUIUnits.extraSmallSpacing;
    const CGFloat textHeight = titleHeight + textSpacing + subtitleHeight;
    const CGFloat textY = NSMidY(cellFrame) - textHeight / 2.;
    const CGFloat textX = NSMaxX(tileRect) + contentSpacing;
    const CGFloat textWidth = MAX(0., NSMaxX(cellFrame) - contentSpacing - textX);
    NSMutableParagraphStyle * const paragraphStyle = [[NSMutableParagraphStyle alloc] init];
    paragraphStyle.lineBreakMode = NSLineBreakByTruncatingTail;
    NSDictionary * const titleAttributes = @{
        NSFontAttributeName: titleFont,
        NSForegroundColorAttributeName: self.isEnabled
            ? NSColor.controlTextColor
            : NSColor.disabledControlTextColor,
        NSParagraphStyleAttributeName: paragraphStyle
    };
    NSDictionary * const subtitleAttributes = @{
        NSFontAttributeName: subtitleFont,
        NSForegroundColorAttributeName: NSColor.secondaryLabelColor,
        NSParagraphStyleAttributeName: paragraphStyle
    };
    [self.title drawInRect:NSMakeRect(textX, textY, textWidth, titleHeight)
            withAttributes:titleAttributes];
    [self.subtitle drawInRect:NSMakeRect(textX, textY + titleHeight + textSpacing, textWidth, subtitleHeight)
               withAttributes:subtitleAttributes];
}

- (void)drawFocusRingMaskWithFrame:(NSRect)cellFrame inView:(NSView *)controlView
{
    const CGFloat borderThickness = VLCUIUnits.borderThickness;
    const CGFloat cornerRadius = VLCUIUnits.largeCornerRadius;
    [[NSBezierPath bezierPathWithRoundedRect:NSInsetRect(cellFrame, borderThickness, borderThickness)
                                     xRadius:cornerRadius
                                     yRadius:cornerRadius] fill];
}

- (NSRect)focusRingMaskBoundsForFrame:(NSRect)cellFrame inView:(NSView *)controlView
{
    return cellFrame;
}

@end
