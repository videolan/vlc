/*****************************************************************************
 * VLCUIWidgets.m: Widgets for VLC's extensions dialogs for Mac OS X
 *****************************************************************************
 * Copyright (C) 2009-2015 the VideoLAN team and authors
 *
 * Authors: Pierre d'Herbemont <pdherbemont # videolan dot>,
 *          Brendon Justin <brendonjustin@gmail.com>
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

#import "VLCUIWidgets.h"

@implementation VLCDialogButton

@end


@implementation VLCDialogPopUpButton

@end


@implementation VLCDialogTextField

@end


@implementation VLCDialogSecureTextField

@end


@implementation VLCDialogLabel
- (void)resetCursorRects {
    [self addCursorRect:[self bounds] cursor:[NSCursor arrowCursor]];
}
@end

@implementation VLCDialogWindow

@end


@implementation VLCDialogList

- (NSInteger)numberOfRowsInTableView:(NSTableView *)aTableView
{
    return [self.contentArray count];
}

- (id)tableView:(NSTableView *)aTableView objectValueForTableColumn:(NSTableColumn *)aTableColumn row:(NSInteger)rowIndex
{
    return [[self.contentArray objectAtIndex:rowIndex] objectForKey:@"text"];
}
@end

@interface VLCDialogGridView()
{
    NSUInteger _rowCount, _colCount;
    NSMutableArray *_griddedViews;
}
@end

@implementation VLCDialogGridView

- (NSUInteger)numViews
{
    return [_griddedViews count];
}

- (id)init
{
    if ((self = [super init])) {
        _colCount = 0;
        _rowCount = 0;
        _griddedViews = [[NSMutableArray alloc] init];
    }

    return self;
}

- (void)recomputeCount
{
    _colCount = 0;
    _rowCount = 0;
    for (NSDictionary *obj in _griddedViews) {
        NSUInteger row = [[obj objectForKey:@"row"] intValue];
        NSUInteger col = [[obj objectForKey:@"col"] intValue];
        if (col + 1 > _colCount)
            _colCount = col + 1;
        if (row + 1 > _rowCount)
            _rowCount = row + 1;
    }
}

- (void)recomputeWindowSize
{
    [NSObject cancelPreviousPerformRequestsWithTarget:self selector:@selector(recomputeWindowSize) object:nil];

    NSWindow *window = [self window];
    NSRect frame = [window frame];
    NSRect contentRect = [window contentRectForFrameRect:frame];
    contentRect.size = [self flexSize:frame.size];
    NSRect newFrame = [window frameRectForContentRect:contentRect];
    newFrame.origin.y -= newFrame.size.height - frame.size.height;
    newFrame.origin.x -= (newFrame.size.width - frame.size.width) / 2;
    [window setFrame:newFrame display:YES animate:YES];
}

- (NSSize)objectSizeToFit:(NSView *)view
{
    if ([view isKindOfClass:[NSControl class]]) {
        NSControl *control = (NSControl *)view;
        return [[control cell] cellSize];
    }
    return [view frame].size;
}

- (CGFloat)marginX
{
    return 16;
}
- (CGFloat)marginY
{
    return 8;
}

- (CGFloat)constrainedHeightOfRow:(NSUInteger)targetRow
{
    CGFloat height = 0;
    for(NSDictionary *obj in _griddedViews) {
        NSUInteger row = [[obj objectForKey:@"row"] intValue];
        if (row != targetRow)
            continue;
        NSUInteger rowSpan = [[obj objectForKey:@"rowSpan"] intValue];
        if (rowSpan != 1)
            continue;
        NSView *view = [obj objectForKey:@"view"];
        if ([view autoresizingMask] & NSViewHeightSizable)
            continue;
        NSSize sizeToFit = [self objectSizeToFit:view];
        if (height < sizeToFit.height)
            height = sizeToFit.height;
    }
    return height;
}

- (CGFloat)remainingRowsHeight
{
    NSUInteger height = [self marginY];
    if (!_rowCount)
        return 0;
    NSUInteger autosizedRows = 0;
    for (NSUInteger i = 0; i < _rowCount; i++) {
        CGFloat constrainedHeight = [self constrainedHeightOfRow:i];
        if (!constrainedHeight)
            autosizedRows++;
        height += constrainedHeight + [self marginY];
    }
    CGFloat remaining = 0;
    if (height < self.bounds.size.height && autosizedRows)
        remaining = (self.bounds.size.height - height) / autosizedRows;
    if (remaining < 0)
        remaining = 0;

    return remaining;
}

- (CGFloat)heightOfRow:(NSUInteger)targetRow
{
    NSAssert(targetRow < _rowCount, @"accessing a non existing row");
    CGFloat height = [self constrainedHeightOfRow:targetRow];
    if (!height)
        height = [self remainingRowsHeight];
    return height;
}


- (CGFloat)topOfRow:(NSUInteger)targetRow
{
    CGFloat top = [self marginY];
    for (NSUInteger i = 1; i < _rowCount - targetRow; i++)
        top += [self heightOfRow:_rowCount - i] + [self marginY];

    return top;
}

- (CGFloat)constrainedWidthOfColumn:(NSUInteger)targetColumn
{
    CGFloat width = 0;
    for(NSDictionary *obj in _griddedViews) {
        NSUInteger col = [[obj objectForKey:@"col"] intValue];
        if (col != targetColumn)
            continue;
        NSUInteger colSpan = [[obj objectForKey:@"colSpan"] intValue];
        if (colSpan != 1)
            continue;
        NSView *view = [obj objectForKey:@"view"];
        if ([view autoresizingMask] & NSViewWidthSizable)
            return 0;
        NSSize sizeToFit = [self objectSizeToFit:view];
        if (width < sizeToFit.width)
            width = sizeToFit.width;
    }
    return width;
}

- (CGFloat)remainingColumnWidth
{
    NSUInteger width = [self marginX];
    if (!_colCount)
        return 0;
    NSUInteger autosizedCol = 0;
    for (NSUInteger i = 0; i < _colCount; i++) {
        CGFloat constrainedWidth = [self constrainedWidthOfColumn:i];
        if (!constrainedWidth)
            autosizedCol++;
        width += constrainedWidth + [self marginX];
    }
    CGFloat remaining = 0;
    if (width < self.bounds.size.width && autosizedCol)
        remaining = (self.bounds.size.width - width) / autosizedCol;
    if (remaining < 0)
        remaining = 0;
    return remaining;
}

- (CGFloat)widthOfColumn:(NSUInteger)targetColumn
{
    CGFloat width = [self constrainedWidthOfColumn:targetColumn];
    if (!width)
        width = [self remainingColumnWidth];
    return width;
}


- (CGFloat)leftOfColumn:(NSUInteger)targetColumn
{
    CGFloat left = [self marginX];
    for (NSUInteger i = 0; i < targetColumn; i++) {
        left += [self widthOfColumn:i] + [self marginX];
    }
    return left;
}

- (void)relayout
{
    for(NSDictionary *obj in _griddedViews) {
        NSUInteger row = [[obj objectForKey:@"row"] intValue];
        NSUInteger col = [[obj objectForKey:@"col"] intValue];
        NSUInteger rowSpan = [[obj objectForKey:@"rowSpan"] intValue];
        NSUInteger colSpan = [[obj objectForKey:@"colSpan"] intValue];
        NSView *view = [obj objectForKey:@"view"];
        NSRect rect;

        // Get the height
        if ([view autoresizingMask] & NSViewHeightSizable || rowSpan > 1) {
            CGFloat height = 0;
            for (NSUInteger r = 0; r < rowSpan; r++) {
                if (row + r >= _rowCount)
                    break;
                height += [self heightOfRow:row + r] + [self marginY];
            }
            rect.size.height = height - [self marginY];
        }
        else
            rect.size.height = [self objectSizeToFit:view].height;

        // Get the width
        if ([view autoresizingMask] & NSViewWidthSizable) {
            CGFloat width = 0;
            for (NSUInteger c = 0; c < colSpan; c++)
                width += [self widthOfColumn:col + c] + [self marginX];
            rect.size.width = width - [self marginX];
        }
        else
            rect.size.width = [self objectSizeToFit:view].width;

        // Top corner
        rect.origin.y = [self topOfRow:row] + ([self heightOfRow:row] - rect.size.height) / 2;
        rect.origin.x = [self leftOfColumn:col];

        [view setFrame:rect];
        [view setNeedsDisplay:YES];
    }
}

- (NSMutableDictionary *)objectForView:(NSView *)view
{
    for (NSMutableDictionary *dict in _griddedViews)
    {
        if ([dict objectForKey:@"view"] == view)
            return dict;
    }
    return nil;
}

- (void)addSubview:(NSView *)view
             atRow:(NSUInteger)row
            column:(NSUInteger)column
           rowSpan:(NSUInteger)rowSpan
           colSpan:(NSUInteger)colSpan
{
    if (row + 1 > _rowCount)
        _rowCount = row + 1;
    if (column + 1 > _colCount)
        _colCount = column + 1;

    NSMutableDictionary *dict = [self objectForView:view];
    if (!dict) {
        dict = [NSMutableDictionary dictionary];
        [dict setObject:view forKey:@"view"];
        [_griddedViews addObject:dict];
    }
    [dict setObject:[NSNumber numberWithUnsignedInteger:rowSpan] forKey:@"rowSpan"];
    [dict setObject:[NSNumber numberWithUnsignedInteger:colSpan] forKey:@"colSpan"];
    [dict setObject:[NSNumber numberWithUnsignedInteger:row] forKey:@"row"];
    [dict setObject:[NSNumber numberWithUnsignedInteger:column] forKey:@"col"];

    [self addSubview:view];
    [self relayout];

    // Recompute the size of the window after making sure we won't see anymore update
    [NSObject cancelPreviousPerformRequestsWithTarget:self selector:@selector(recomputeWindowSize) object:nil];
    [self performSelector:@selector(recomputeWindowSize) withObject:nil afterDelay:0.1];
}

- (void)updateSubview:(NSView *)view
                atRow:(NSUInteger)row
               column:(NSUInteger)column
              rowSpan:(NSUInteger)rowSpan
              colSpan:(NSUInteger)colSpan
{
    NSDictionary *oldDict = [self objectForView:view];
    if (!oldDict) {
        [self addSubview:view
                   atRow:row
                  column:column
                 rowSpan:rowSpan
                 colSpan:colSpan];
        return;
    }
    [self relayout];
}

- (void)removeSubview:(NSView *)view
{
    NSDictionary *dict = [self objectForView:view];
    if (dict)
        [_griddedViews removeObject:dict];
    [view removeFromSuperview];

    [self recomputeCount];
    [self recomputeWindowSize];

    [self relayout];
    [self setNeedsDisplay:YES];
}

- (void)setFrame:(NSRect)frameRect
{
    [super setFrame:frameRect];
    [self relayout];
}

- (NSSize)flexSize:(NSSize)size
{
    if (!_rowCount || !_colCount)
        return size;

    CGFloat minHeight = [self marginY];
    BOOL canFlexHeight = NO;
    for (NSUInteger i = 0; i < _rowCount; i++) {
        CGFloat constrained = [self constrainedHeightOfRow:i];
        if (!constrained) {
            canFlexHeight = YES;
            constrained = 128;
        }
        minHeight += constrained + [self marginY];
    }

    CGFloat minWidth = [self marginX];
    BOOL canFlexWidth = NO;
    for (NSUInteger i = 0; i < _colCount; i++) {
        CGFloat constrained = [self constrainedWidthOfColumn:i];
        if (!constrained) {
            canFlexWidth = YES;
            constrained = 128;
        }
        minWidth += constrained + [self marginX];
    }
    if (size.width < minWidth)
        size.width = minWidth;
    if (size.height < minHeight)
        size.height = minHeight;
    if (!canFlexHeight)
        size.height = minHeight;
    if (!canFlexWidth)
        size.width = minWidth;
    return size;
}

@end
