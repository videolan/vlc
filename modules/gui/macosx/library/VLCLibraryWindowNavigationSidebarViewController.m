/*****************************************************************************
 * VLCLibraryWindowNavigationSidebarViewController.h: MacOS X interface module
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

#import "VLCLibraryWindowNavigationSidebarViewController.h"

#import "library/VLCLibraryDataTypes.h"
#import "library/VLCLibraryModel.h"
#import "library/VLCLibrarySegment.h"
#import "library/VLCLibrarySegmentBookmarkedLocation.h"
#import "library/VLCLibraryWindow.h"
#import "library/VLCLibraryWindowNavigationSidebarOutlineView.h"

// This needs to match whatever identifier has been set in the library window XIB
static NSString * const VLCLibrarySegmentCellIdentifier = @"VLCLibrarySegmentCellIdentifier";

@interface VLCLibraryWindowNavigationSidebarViewController ()

@property BOOL ignoreSegmentSelectionChanges;

@end

@implementation VLCLibraryWindowNavigationSidebarViewController

- (instancetype)initWithLibraryWindow:(VLCLibraryWindow *)libraryWindow
{
    self = [super initWithNibName:@"VLCLibraryWindowNavigationSidebarView" bundle:nil];
    if (self) {
        _libraryWindow = libraryWindow;
        _segments = VLCLibrarySegment.librarySegments;
        _ignoreSegmentSelectionChanges = NO;
    }
    return self;
}

- (void)viewDidLoad
{
    _treeController = [[NSTreeController alloc] init];
    _treeController.objectClass = VLCLibrarySegment.class;
    _treeController.countKeyPath = @"childCount";
    _treeController.childrenKeyPath = @"childNodes";
    _treeController.leafKeyPath = @"leaf";
    [_treeController bind:@"contentArray" toObject:self withKeyPath:@"segments" options:nil];

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

    NSNotificationCenter * const defaultCenter = NSNotificationCenter.defaultCenter;
    [defaultCenter addObserver:self
                      selector:@selector(internalNodesChanged:)
                          name:VLCLibraryBookmarkedLocationsChanged
                        object:nil];
    [defaultCenter addObserver:self
                      selector:@selector(internalNodesChanged:)
                          name:VLCLibraryModelListOfGroupsReset
                        object:nil];
    [defaultCenter addObserver:self
                      selector:@selector(internalNodesChanged:)
                          name:VLCLibraryModelGroupUpdated
                        object:nil];
    [defaultCenter addObserver:self
                      selector:@selector(internalNodesChanged:)
                          name:VLCLibraryModelGroupDeleted
                        object:nil];
}

- (void)internalNodesChanged:(NSNotification *)notification
{
    const VLCLibrarySegmentType currentSegmentType = self.libraryWindow.librarySegmentType;

    self.ignoreSegmentSelectionChanges = YES;
    
    [self.treeController rearrangeObjects];
    [self.outlineView reloadData];

    NSTreeNode * const targetNode = [self nodeForSegmentType:currentSegmentType];
    const NSInteger segmentIndex = [self.outlineView rowForItem:targetNode];
    [self.outlineView selectRowIndexes:[NSIndexSet indexSetWithIndex:segmentIndex]
                  byExtendingSelection:NO];

    self.ignoreSegmentSelectionChanges = NO;
}

- (NSTreeNode *)nodeForSegmentType:(VLCLibrarySegmentType)segmentType
{
    NSArray<NSTreeNode *> *nodes = self.treeController.arrangedObjects.childNodes;
    while (nodes.count != 0) {
        NSMutableArray<NSTreeNode *> * const nextLevelNodes = NSMutableArray.array;

        const NSInteger nodeIdx = [nodes indexOfObjectPassingTest:^BOOL(NSTreeNode * const obj,
                                                                        NSUInteger idx,
                                                                        BOOL * const stop) {
            VLCLibrarySegment * const segment = obj.representedObject;
            const BOOL matching = segment.segmentType == segmentType;
            if (!matching) {
                [nextLevelNodes addObjectsFromArray:obj.childNodes];
            }
            return matching;
        }];

        if (nodeIdx != NSNotFound) {
            return nodes[nodeIdx];
        }

        nodes = nextLevelNodes.copy;
    }
    NSAssert(NO, @"Could not find node for segment type %ld", segmentType);
    return nil;
}

- (void)selectSegment:(NSInteger)segmentType
{
    NSAssert(segmentType > VLCLibraryLowSentinelSegment &&
             segmentType < VLCLibraryHighSentinelSegment,
             @"Invalid segment type value provided");

    VLCLibrarySegment * const segment = [VLCLibrarySegment segmentWithSegmentType:segmentType];
    self.libraryWindow.librarySegmentType = segment.segmentType;

    if (segmentType > VLCLibraryVideoSegment && segmentType <= VLCLibraryShowsVideoSubSegment) {
        [self.outlineView expandItem:[self nodeForSegmentType:VLCLibraryVideoSegment]];
    } else if (segmentType >= VLCLibraryMusicSegment && segmentType <= VLCLibraryGenresMusicSubSegment) {
        [self.outlineView expandItem:[self nodeForSegmentType:VLCLibraryMusicSegment]];
    } else if (segmentType >= VLCLibraryBrowseSegment &&
               segmentType <= VLCLibraryBrowseBookmarkedLocationSubSegment) {
        [self.outlineView expandItem:[self nodeForSegmentType:VLCLibraryBrowseSegment]];
    } else if (segmentType == VLCLibraryGroupsGroupSubSegment) {
        [self.outlineView expandItem:[self nodeForSegmentType:VLCLibraryGroupsSegment]];
    }

    NSTreeNode * const targetNode = [self nodeForSegmentType:segmentType];
    const NSInteger segmentIndex = [self.outlineView rowForItem:targetNode];
    [self.outlineView selectRowIndexes:[NSIndexSet indexSetWithIndex:segmentIndex]
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

- (NSIndexSet *)outlineView:(NSOutlineView *)outlineView selectionIndexesForProposedSelection:(nonnull NSIndexSet *)proposedSelectionIndexes
{
    NSAssert(outlineView == _outlineView, @"VLCLibraryWindowNavigationSidebarController should only be a delegate for the libraryWindow nav sidebar outline view!");

    if (proposedSelectionIndexes.count == 0 || proposedSelectionIndexes.firstIndex != VLCLibraryMusicSegment) {
        return proposedSelectionIndexes;
    } else {
        [self.outlineView expandItem:[self nodeForSegmentType:VLCLibraryMusicSegment]];
        NSTreeNode * const artistsNode = [self nodeForSegmentType:VLCLibraryArtistsMusicSubSegment];
        const NSInteger artistsIndex = [self.outlineView rowForItem:artistsNode];
        return [NSIndexSet indexSetWithIndex:artistsIndex];
    }
}

- (void)outlineViewSelectionDidChange:(NSNotification *)notification
{
    if (self.ignoreSegmentSelectionChanges) {
        return;
    }
    
    NSTreeNode * const node = (NSTreeNode *)[_outlineView itemAtRow:_outlineView.selectedRow];
    NSParameterAssert(node != nil);
    VLCLibrarySegment * const segment = (VLCLibrarySegment *)node.representedObject;
    NSObject * const representedObject = segment.representedObject;
    NSParameterAssert(representedObject != nil);

    if ([representedObject isKindOfClass:NSNumber.class]) {
        self.libraryWindow.librarySegmentType = segment.segmentType;
    } else if ([representedObject isKindOfClass:VLCLibrarySegmentBookmarkedLocation.class]) {
        VLCLibrarySegmentBookmarkedLocation * const bookmarkedLocation =
            (VLCLibrarySegmentBookmarkedLocation *)representedObject;
        self.libraryWindow.librarySegmentType = bookmarkedLocation.segmentType;
        [self.libraryWindow goToLocalFolderMrl:bookmarkedLocation.mrl];
    } else if ([representedObject isKindOfClass:VLCMediaLibraryGroup.class]) {
        [self.libraryWindow presentLibraryItem:(VLCMediaLibraryGroup *)representedObject];
    }
}

@end
