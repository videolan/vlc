/*****************************************************************************
 * VLCLibraryWindowNavigationSidebarController.h: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2023 VLC authors and VideoLAN
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

#import "VLCLibraryWindowNavigationSidebarController.h"

#import "library/VLCLibraryWindow.h"
#import "library/VLCLibrarySegment.h"

// This needs to match whatever identifier has been set in the library window XIB
static NSString * const VLCLibrarySegmentCellIdentifier = @"VLCLibrarySegmentCellIdentifier";

@implementation VLCLibraryWindowNavigationSidebarController

- (instancetype)initWithLibraryWindow:(VLCLibraryWindow *)libraryWindow
{
    self = [super init];
    if (self) {
        _libraryWindow = libraryWindow;
        _segments = VLCLibrarySegment.librarySegments;
        [self setupOutlineView];
    }
    return self;
}

- (void)setupOutlineView
{
    _treeController = [[NSTreeController alloc] init];
    _treeController.objectClass = VLCLibrarySegment.class;
    _treeController.countKeyPath = @"childCount";
    _treeController.childrenKeyPath = @"childNodes";
    _treeController.leafKeyPath = @"leaf";
    [_treeController bind:@"contentArray" toObject:self withKeyPath:@"segments" options:nil];

    _outlineView = _libraryWindow.navSidebarOutlineView;
    _outlineView.delegate = self;

    _outlineView.rowSizeStyle = NSTableViewRowSizeStyleMedium;
    _outlineView.allowsMultipleSelection = NO;
    _outlineView.allowsEmptySelection = NO;
    _outlineView.allowsColumnSelection = NO;
    _outlineView.allowsColumnReordering = NO;

    [_outlineView bind:@"content"
              toObject:_treeController
           withKeyPath:@"arrangedObjects"
               options:nil];

    [_outlineView reloadData];
}

- (void)selectSegment:(NSInteger)segmentType
{
    NSAssert(segmentType > VLCLibraryLowSentinelSegment &&
             segmentType < VLCLibraryHighSentinelSegment,
             @"Invalid segment type value provided");

    VLCLibrarySegment * const segment = [VLCLibrarySegment segmentWithSegmentType:segmentType];
    self.libraryWindow.librarySegmentType = segment.segmentType;

    if (segmentType >= VLCLibraryMusicSegment) {
        NSTreeNode * const itemNode = (NSTreeNode *)[_outlineView itemAtRow:VLCLibraryMusicSegment];
        [self.outlineView expandItem:itemNode];
    }

    [self.outlineView selectRowIndexes:[NSIndexSet indexSetWithIndex:segmentType]
                  byExtendingSelection:NO];
}

# pragma mark - NSOutlineView delegation

- (NSView *)outlineView:(NSOutlineView *)outlineView
     viewForTableColumn:(NSTableColumn *)tableColumn
                   item:(id)item
{
    NSAssert(outlineView == _outlineView, @"VLCLibraryWindowNavigationSidebarController should only be a delegate for the libraryWindow nav sidebar outline view!");

    NSTableCellView * const cellView = [outlineView makeViewWithIdentifier:@"VLCLibrarySegmentCellIdentifier" owner:self];
    NSAssert(cellView != nil, @"Provided cell view for navigation outline view should be valid!");
    [cellView.textField bind:NSValueBinding toObject:cellView withKeyPath:@"objectValue.displayString" options:nil];
    [cellView.imageView bind:NSImageBinding toObject:cellView withKeyPath:@"objectValue.displayImage" options:nil];
    return cellView;
}

- (void)outlineViewSelectionDidChange:(NSNotification *)notification
{
    NSTreeNode * const node = (NSTreeNode *)[_outlineView itemAtRow:_outlineView.selectedRow];
    NSParameterAssert(node != nil);
    VLCLibrarySegment * const segment = (VLCLibrarySegment *)node.representedObject;
    _libraryWindow.librarySegmentType = segment.segmentType;
}

@end
