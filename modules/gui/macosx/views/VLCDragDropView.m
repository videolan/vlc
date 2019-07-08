/*****************************************************************************
 * VLCDragDropView.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2003 - 2019 VLC authors and VideoLAN
 *
 * Authors: Derk-Jan Hartman <hartman # videolan dot org>
 *          Felix Paul Kühne <fkuehne # videolan dot org>
 *          David Fuhrmann <dfuhrmann # videolan dot org>
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

#import "VLCDragDropView.h"

@implementation VLCDropDisabledImageView

- (void)awakeFromNib
{
    [self unregisterDraggedTypes];
}

@end

@implementation VLCDragDropView

- (id)initWithFrame:(NSRect)frame
{
    self = [super initWithFrame:frame];
    if (self) {
        // default value
        [self setDrawBorder:YES];
    }
    return self;
}

- (void)enablePlaylistItems
{
    [self registerForDraggedTypes:[NSArray arrayWithObjects:NSFilenamesPboardType, nil]];
}

- (BOOL)mouseDownCanMoveWindow
{
    return YES;
}

- (void)dealloc
{
    [self unregisterDraggedTypes];
}

- (void)awakeFromNib
{
    [self registerForDraggedTypes:[NSArray arrayWithObject:NSFilenamesPboardType]];
    self.wantsLayer = YES;
    self.layer.cornerRadius = 5.;
    [self updateBorderColor];
}

- (void)updateBorderColor
{
    self.layer.borderColor = [NSColor selectedControlColor].CGColor;
}

- (NSDragOperation)draggingEntered:(id <NSDraggingInfo>)sender
{
    if ((NSDragOperationGeneric & [sender draggingSourceOperationMask]) == NSDragOperationGeneric) {
        if (self.drawBorder) {
            self.layer.borderWidth = 5.;
        }
        return NSDragOperationCopy;
    }

    return NSDragOperationNone;
}

- (void)draggingEnded:(id <NSDraggingInfo>)sender
{
    self.layer.borderWidth = 0.;
}

- (void)draggingExited:(id <NSDraggingInfo>)sender
{
    self.layer.borderWidth = 0.;
}

- (BOOL)prepareForDragOperation:(id <NSDraggingInfo>)sender
{
    return YES;
}

- (BOOL)performDragOperation:(id <NSDraggingInfo>)sender
{
    BOOL returnValue = NO;
    NSPasteboard *pasteBoard = [sender draggingPasteboard];
    if (!pasteBoard) {
        return NO;
    }

    if (_dropTarget) {
        returnValue = [_dropTarget handlePasteBoardFromDragSession:pasteBoard];
    }

    [self setNeedsDisplay:YES];
    return returnValue;
}

- (void)concludeDragOperation:(id <NSDraggingInfo>)sender
{
    self.layer.borderWidth = 0.;
}

- (void)viewDidChangeEffectiveAppearance
{
    [self updateBorderColor];
}

@end
