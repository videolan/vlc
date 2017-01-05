/*****************************************************************************
 * VLCFSPanelDraggableView.m
 *****************************************************************************
 * Copyright (C) 2017 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: David Fuhrmann <dfuhrmann at videolan dot org>
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

#import "VLCFSPanelDraggableView.h"

@implementation VLCFSPanelDraggableView

- (BOOL)mouseDownCanMoveWindow
{
    return NO;
}

- (void)mouseDown:(NSEvent *)event
{
    NSWindow *window = [self window];
    NSRect mouseLocationInWindow = {[event locationInWindow], {0,0}};
    NSPoint originalMouseLocation = [window convertRectToScreen:mouseLocationInWindow].origin;
    NSRect originalFrame = [window frame];

    while (YES)
    {
        // Get all dragged and mouse up events during dragging
        NSEvent *newEvent = [window nextEventMatchingMask:(NSLeftMouseDraggedMask | NSLeftMouseUpMask)];

        if ([newEvent type] == NSLeftMouseUp) {
            break;
        }

        // Calculate delta of dragging
        NSRect newMouseLocationInWindow = {[newEvent locationInWindow], {0,0}};
        NSPoint newMouseLocation = [window convertRectToScreen:newMouseLocationInWindow].origin;
        NSPoint delta = NSMakePoint(newMouseLocation.x - originalMouseLocation.x,
                                    newMouseLocation.y - originalMouseLocation.y);

        NSRect limitFrame = _limitWindow.frame;
        NSRect newFrame = originalFrame;
        newFrame.origin.x += delta.x;
        newFrame.origin.y += delta.y;

        // Limit rect to limitation view
        if (newFrame.origin.x < limitFrame.origin.x)
            newFrame.origin.x = limitFrame.origin.x;
        if (newFrame.origin.y < limitFrame.origin.y)
            newFrame.origin.y = limitFrame.origin.x;

        // Limit size (could be needed after resolution changes)
        if (newFrame.size.height > limitFrame.size.height)
            newFrame.size.height = limitFrame.size.height;
        if (newFrame.size.width > limitFrame.size.width)
            newFrame.size.width = limitFrame.size.width;

        if (newFrame.origin.x + newFrame.size.width > limitFrame.origin.x + limitFrame.size.width)
            newFrame.origin.x = limitFrame.origin.x + limitFrame.size.width - newFrame.size.width;
        if (newFrame.origin.y + newFrame.size.height > limitFrame.origin.y + limitFrame.size.height)
            newFrame.origin.y = limitFrame.origin.y + limitFrame.size.height - newFrame.size.height;

        [window setFrame:newFrame display:YES animate:NO];
    }

}

@end
