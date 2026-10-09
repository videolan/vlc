/*****************************************************************************
 * VLCLibraryShowEpisodeTableViewDelegate.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2026 VLC authors and VideoLAN
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

#import "VLCLibraryShowEpisodeTableViewDelegate.h"

#import "extensions/NSView+VLCAdditions.h"

#import "library/VLCLibraryTableCellView.h"

@implementation VLCLibraryShowEpisodeTableViewDelegate

- (instancetype)init
{
    self = [super init];
    if (self) {
        self.cellViewIdentifier = @"VLCLibraryShowEpisodeTableCellViewIdentifier";
    }
    return self;
}

- (NSView<VLCLibraryTableCellViewProtocol> *)makeCellView
{
    return [VLCLibraryTableCellView fromNibNamed:@"VLCLibraryVideoTableCellView"
                                     withClass:VLCLibraryTableCellView.class
                                     withOwner:self];
}

- (void)configureCellView:(NSView<VLCLibraryTableCellViewProtocol> *)cellView
{
    VLCLibraryTableCellView * const libraryCellView = (VLCLibraryTableCellView *)cellView;
    libraryCellView.detailMode = VLCLibraryTableCellDetailModeDuration;
}

@end
