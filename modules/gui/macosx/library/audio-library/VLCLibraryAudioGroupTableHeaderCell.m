/*****************************************************************************
 * VLCLibraryAudioGroupTableHeaderCell.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2025 VLC authors and VideoLAN
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

#import "VLCLibraryAudioGroupTableHeaderCell.h"

@implementation VLCLibraryAudioGroupTableHeaderCell

- (void)drawWithFrame:(NSRect)cellFrame inView:(NSView *)controlView
{
    // Intentionally do not call super to prevent AppKit from drawing its default background.
    // The header view hosts its own background visuals (glass/effect/solid) and draws content there.
    [NSColor.clearColor setFill];
    NSRectFillUsingOperation(cellFrame, NSCompositingOperationClear);
}

@end
