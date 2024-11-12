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
#import "library/VLCLibraryUIUnits.h"

@interface VLCLibraryHomeViewActionButtonCell ()

@property NSImage *cachedImage;
@property BOOL prevIsHighlighted;
@property NSRect prevFrame;

@end

@implementation VLCLibraryHomeViewActionButtonCell

- (void)drawWithFrame:(NSRect)cellFrame inView:(NSView *)controlView
{
    [NSColor.VLCSubtleBorderColor setStroke];
    [NSColor.windowBackgroundColor setFill];

    const CGFloat cellMinX = NSMinX(cellFrame);
    const CGFloat cellMinY = NSMinY(cellFrame);
    const CGFloat cellMaxX = NSMaxX(cellFrame);
    const CGFloat cellMaxY = NSMaxY(cellFrame);

    NSBezierPath * const separatorPath =
        [NSBezierPath bezierPathWithRoundedRect:cellFrame
                                        xRadius:VLCLibraryUIUnits.cornerRadius
                                        yRadius:VLCLibraryUIUnits.cornerRadius];
    separatorPath.lineWidth = VLCLibraryUIUnits.borderThickness;
    [separatorPath stroke];
    [separatorPath fill];

    const CGSize cellSize = cellFrame.size;
    const CGFloat cellWidth = cellSize.width;
    const CGFloat cellHeight = cellSize.height;

    NSMutableParagraphStyle * const titleParagraphStyle = [[NSMutableParagraphStyle alloc] init];
    titleParagraphStyle.alignment = NSTextAlignmentCenter;
    NSDictionary<NSAttributedStringKey, id> * const titleAttributes = @{
        NSForegroundColorAttributeName: NSColor.controlTextColor,
        NSFontAttributeName: NSFont.VLCLibrarySubsectionSubheaderFont,
        NSParagraphStyleAttributeName: titleParagraphStyle
    };
    const NSSize titleSize = [self.title sizeWithAttributes:titleAttributes];
    const CGFloat titleHeight = titleSize.height + VLCLibraryUIUnits.smallSpacing;
    [self.title drawInRect:CGRectMake(cellMinX + VLCLibraryUIUnits.smallSpacing,
                                      cellMaxY - titleHeight,
                                      cellWidth - VLCLibraryUIUnits.smallSpacing * 2,
                                      titleHeight)
            withAttributes:titleAttributes];

    const CGSize imageSize = self.image.size;

    if (self.cachedImage != self.image ||
        self.prevIsHighlighted != self.isHighlighted ||
        !NSEqualRects(self.prevFrame, cellFrame)) {

        self.cachedImage = [NSImage imageWithSize:imageSize
                                          flipped:NO
                                   drawingHandler:^BOOL(NSRect dstRect) {
            if (self.isHighlighted) {
                [NSColor.VLCSubtleBorderColor set];
            } else {
                [NSColor.VLCSubtlerAccentColor set];
            }
            const NSRect imageRect = {NSZeroPoint, imageSize};
            [self.image drawInRect:imageRect];
            NSRectFillUsingOperation(imageRect, NSCompositingOperationSourceIn);
            return YES;
        }];

        self.prevIsHighlighted = self.isHighlighted;
        self.prevFrame = cellFrame;
    } 

    const CGFloat originalImageAspectRatio = imageSize.width / imageSize.height;
    const CGFloat imageAvailableVerticalSpace =
        cellHeight - titleHeight - VLCLibraryUIUnits.largeSpacing * 2;
    CGFloat imageWidth, imageHeight;

    // Try to scale focusing on width first, if this yields a height that is too large, switch
    if (cellWidth / originalImageAspectRatio > imageAvailableVerticalSpace) {
        imageHeight = imageAvailableVerticalSpace;
        imageWidth = imageHeight * originalImageAspectRatio;
    } else {
        imageWidth = cellWidth;
        imageHeight = imageWidth / originalImageAspectRatio;
    }

    const CGPoint cellOrigin = cellFrame.origin;
    const NSRect imageRect = NSMakeRect(cellOrigin.x + (cellWidth - imageWidth) / 2,
                                        cellOrigin.y + (cellHeight - imageHeight) / 2,
                                        imageWidth,
                                        imageHeight);
    [self.cachedImage drawInRect:imageRect];
}

@end
