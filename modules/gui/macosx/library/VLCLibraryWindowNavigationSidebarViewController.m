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

#import "extensions/NSColor+VLCAdditions.h"
#import "extensions/NSWindow+VLCAdditions.h"

#import "views/VLCStatusNotifierView.h"

// This needs to match whatever identifier has been set in the library window XIB
static NSString * const VLCLibrarySegmentCellIdentifier = @"VLCLibrarySegmentCellIdentifier";

@interface VLCLibraryWindowNavigationSidebarViewController ()

@property BOOL ignoreSegmentSelectionChanges;
@property (readonly) NSEdgeInsets scrollViewInsets;

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
    self.treeController.objectClass = VLCLibrarySegment.class;
    self.treeController.countKeyPath = @"childCount";
    self.treeController.childrenKeyPath = @"childNodes";
    self.treeController.leafKeyPath = @"leaf";

    self.outlineView.rowSizeStyle = NSTableViewRowSizeStyleMedium;
    self.outlineView.allowsMultipleSelection = NO;
    self.outlineView.allowsEmptySelection = NO;
    self.outlineView.allowsColumnSelection = NO;
    self.outlineView.allowsColumnReordering = NO;

    [self internalNodesChanged:nil];

    const NSEdgeInsets scrollViewInsets = self.outlineViewScrollView.contentInsets;
    _scrollViewInsets =
        NSEdgeInsetsMake(scrollViewInsets.top + self.libraryWindow.titlebarHeight,
                         scrollViewInsets.left,
                         scrollViewInsets.bottom,
                         scrollViewInsets.right);

    self.statusNotifierView.postsFrameChangedNotifications = YES;

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
    [defaultCenter addObserver:self
                      selector:@selector(statusViewActivated:)
                          name:VLCStatusNotifierViewActivated
                        object:nil];
    [defaultCenter addObserver:self
                      selector:@selector(statusViewDeactivated:)
                          name:VLCStatusNotifierViewDeactivated
                        object:nil];
    [defaultCenter addObserver:self
                      selector:@selector(statusViewSizeChanged:)
                          name:NSViewFrameDidChangeNotification
                        object:nil];
}

- (void)internalNodesChanged:(NSNotification *)notification
{
    const VLCLibrarySegmentType currentSegmentType = self.libraryWindow.librarySegmentType;

    self.ignoreSegmentSelectionChanges = YES;

    _segments = VLCLibrarySegment.librarySegments;
    [self.treeController bind:@"contentArray" toObject:self withKeyPath:@"segments" options:nil];
    [self.outlineView bind:@"content"
                  toObject:self.treeController
               withKeyPath:@"arrangedObjects"
                   options:nil];
    [self.outlineView reloadData];

    NSTreeNode * const targetNode = [self nodeForSegmentType:currentSegmentType];
    const NSInteger segmentIndex = [self.outlineView rowForItem:targetNode];
    [self expandParentsOfNode:targetNode];
    [self.outlineView selectRowIndexes:[NSIndexSet indexSetWithIndex:segmentIndex]
                  byExtendingSelection:NO];

    self.ignoreSegmentSelectionChanges = NO;
}

- (void)statusViewActivated:(NSNotification *)notification
{
    const CGFloat statusNotifierHeight = self.statusNotifierView.frame.size.height;
    self.outlineViewScrollView.contentInsets =
        NSEdgeInsetsMake(self.scrollViewInsets.top,
                         self.scrollViewInsets.left,
                         self.scrollViewInsets.bottom + statusNotifierHeight,
                         self.scrollViewInsets.right);
    self.statusNotifierView.hidden = NO;
    self.statusNotifierView.animator.alphaValue = 1.0;
}

- (void)statusViewDeactivated:(NSNotification *)notification
{
    [NSAnimationContext runAnimationGroup:^(NSAnimationContext * const context) {
        self.statusNotifierView.animator.alphaValue = 0.0;
    } completionHandler:^{
        self.statusNotifierView.hidden = YES;
        self.outlineViewScrollView.contentInsets = self.scrollViewInsets;
    }];
}

- (void)statusViewSizeChanged:(NSNotification *)notification
{
    if (self.statusNotifierView.hidden) {
        return;
    }

    const CGFloat statusNotifierHeight = self.statusNotifierView.frame.size.height;
    self.outlineViewScrollView.contentInsets =
        NSEdgeInsetsMake(self.scrollViewInsets.top,
                         self.scrollViewInsets.left,
                         self.scrollViewInsets.bottom + statusNotifierHeight,
                         self.scrollViewInsets.right);
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

- (void)expandParentsOfNode:(NSTreeNode *)targetNode
{
    NSMutableArray * const parentNodes = NSMutableArray.array;
    NSTreeNode *currentNode = targetNode.parentNode;
    while (currentNode != nil) {
        [parentNodes insertObject:currentNode atIndex:0];
        currentNode = currentNode.parentNode;
    }

    for (NSTreeNode * const node in parentNodes) {
        [self.outlineView expandItem:node];
    }
}

- (void)selectSegment:(NSInteger)segmentType
{
    NSAssert(segmentType > VLCLibraryLowSentinelSegment &&
             segmentType < VLCLibraryHighSentinelSegment,
             @"Invalid segment type value provided");

    if (segmentType == VLCLibraryHeaderSegmentType) {
        return;
    }

    self.libraryWindow.librarySegmentType = segmentType;

    if (segmentType == VLCLibraryMusicSegmentType) {
        [self.outlineView expandItem:[self nodeForSegmentType:VLCLibraryMusicSegmentType]];
    }

    NSTreeNode * const targetNode = [self nodeForSegmentType:segmentType];
    const NSInteger segmentIndex = [self.outlineView rowForItem:targetNode];
    [self expandParentsOfNode:targetNode];
    [self.outlineView selectRowIndexes:[NSIndexSet indexSetWithIndex:segmentIndex]
                  byExtendingSelection:NO];
}

# pragma mark - NSOutlineView delegation

- (NSView *)outlineView:(NSOutlineView *)outlineView
     viewForTableColumn:(NSTableColumn *)tableColumn
                   item:(id)item
{
    NSAssert(outlineView == _outlineView, @"VLCLibraryWindowNavigationSidebarController should only be a delegate for the libraryWindow nav sidebar outline view!");

    const BOOL isHeader = [self outlineView:outlineView isGroupItem:item];
    NSTableCellView * const cellView = isHeader
        ? [outlineView makeViewWithIdentifier:@"VLCLibrarySegmentHeaderCellIdentifier" owner:self]
        : [outlineView makeViewWithIdentifier:@"VLCLibrarySegmentCellIdentifier" owner:self];
    NSAssert(cellView != nil, @"Provided cell view for navigation outline view should be valid!");
    [cellView.textField bind:NSValueBinding toObject:cellView withKeyPath:@"objectValue.displayString" options:nil];
    [cellView.imageView bind:NSImageBinding toObject:cellView withKeyPath:@"objectValue.displayImage" options:nil];

    if (@available(macOS 10.14, *)) {
        cellView.imageView.contentTintColor = NSColor.VLCAccentColor;
    }

    return cellView;
}

- (NSIndexSet *)outlineView:(NSOutlineView *)outlineView selectionIndexesForProposedSelection:(nonnull NSIndexSet *)proposedSelectionIndexes
{
    NSAssert(outlineView == _outlineView, @"VLCLibraryWindowNavigationSidebarController should only be a delegate for the libraryWindow nav sidebar outline view!");

    if (proposedSelectionIndexes.count > 0) {
        NSTreeNode * const node = [self.outlineView itemAtRow:proposedSelectionIndexes.firstIndex];
        VLCLibrarySegment * const segment = (VLCLibrarySegment *)node.representedObject;

        if (segment.segmentType == VLCLibraryHeaderSegmentType) {
            return NSIndexSet.indexSet;
        } else if (segment.segmentType == VLCLibraryMusicSegmentType) {
            [self.outlineView expandItem:[self nodeForSegmentType:VLCLibraryMusicSegmentType]];
            NSTreeNode * const artistsNode = [self nodeForSegmentType:VLCLibraryArtistsMusicSubSegmentType];
            const NSInteger artistsIndex = [self.outlineView rowForItem:artistsNode];
            return [NSIndexSet indexSetWithIndex:artistsIndex];
        }
    }

    return proposedSelectionIndexes;
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

- (BOOL)outlineView:(NSOutlineView*)outlineView isGroupItem:(id)item
{
    NSTreeNode * const treeNode = (NSTreeNode *)item;
    VLCLibrarySegment * const segment = (VLCLibrarySegment *)treeNode.representedObject;
    return segment.segmentType == VLCLibraryHeaderSegmentType;
}

@end
