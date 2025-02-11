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

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

// This needs to match whatever identifier has been set in the library window XIB
static NSString * const VLCLibrarySegmentCellIdentifier = @"VLCLibrarySegmentCellIdentifier";

@interface VLCLibraryWindowNavigationSidebarViewController ()

@property BOOL ignoreSegmentSelectionChanges;
@property (readonly) NSEdgeInsets scrollViewInsets;
@property (readonly) NSMutableDictionary<NSString *, dispatch_source_t> *observedPathDispatchSources;

@end

@implementation VLCLibraryWindowNavigationSidebarViewController

- (instancetype)initWithLibraryWindow:(VLCLibraryWindow *)libraryWindow
{
    self = [super initWithNibName:@"VLCLibraryWindowNavigationSidebarView" bundle:nil];
    if (self) {
        _libraryWindow = libraryWindow;
        _segments = VLCLibrarySegment.librarySegments;
        _ignoreSegmentSelectionChanges = NO;
        _observedPathDispatchSources = NSMutableDictionary.dictionary;
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

    NSMutableSet<NSNumber *> * const expandedSegmentTypes = NSMutableSet.set;
    for (VLCLibrarySegment * const segment in self.treeController.content) {
        NSTreeNode * const node = [self nodeForSegmentType:segment.segmentType];
        if ([self.outlineView isItemExpanded:node]) {
            [expandedSegmentTypes addObject:@(segment.segmentType)];
        }
    }

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

    [self updateBookmarkObservation];

    for (VLCLibrarySegment * const segment in self.treeController.content) {
        if ([expandedSegmentTypes containsObject:@(segment.segmentType)]) {
            NSTreeNode * const node = [self nodeForSegmentType:segment.segmentType];
            [self.outlineView expandItem:node];
        }
    }
}

- (void)updateBookmarkObservation
{
    NSUserDefaults * const defaults = NSUserDefaults.standardUserDefaults;
    NSArray<NSString *> * const bookmarkedLocations =
        [defaults stringArrayForKey:VLCLibraryBookmarkedLocationsKey];
    if (bookmarkedLocations.count == 0) {
        return;
    }

    NSMutableArray<NSString *> * const deletedLocations = self.observedPathDispatchSources.allKeys.mutableCopy;
    const __weak typeof(self) weakSelf = self;

    for (NSString * const locationMrl in bookmarkedLocations) {
        [deletedLocations removeObject:locationMrl];
        if ([self.observedPathDispatchSources objectForKey:locationMrl] != nil) {
            continue;
        }
        NSURL * const locationUrl = [NSURL URLWithString:locationMrl];
        const uintptr_t descriptor = open(locationUrl.path.UTF8String, O_EVTONLY);
        if (descriptor == -1) {
            continue;
        }
        struct stat fileStat;
        const int statResult = fstat(descriptor, &fileStat);

        const dispatch_queue_t globalQueue = dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0);
        const dispatch_source_t fileDispatchSource = dispatch_source_create(DISPATCH_SOURCE_TYPE_VNODE, descriptor, DISPATCH_VNODE_DELETE | DISPATCH_VNODE_RENAME, globalQueue);
        dispatch_source_set_event_handler(fileDispatchSource, ^{
            const unsigned long eventFlags = dispatch_source_get_data(fileDispatchSource);
            if (eventFlags & DISPATCH_VNODE_RENAME && statResult != -1) {
                NSURL * const parentLocationUrl = locationUrl.URLByDeletingLastPathComponent;
                NSString * const parentLocationPath = parentLocationUrl.path;
                NSArray<NSString *> * const files = [NSFileManager.defaultManager contentsOfDirectoryAtPath:parentLocationPath error:nil];
                NSString *newFileName = nil;

                for (NSString * const file in files) {
                    NSString * const fullChildPath = [parentLocationPath stringByAppendingPathComponent:file];
                    struct stat currentFileStat;
                    if (stat(fullChildPath.UTF8String, &currentFileStat) == -1) {
                        continue;
                    } else if (currentFileStat.st_ino == fileStat.st_ino) {
                        newFileName = fullChildPath.lastPathComponent;
                        break;
                    }
                }

                if (newFileName != nil) {
                    NSMutableArray<NSString *> * const mutableBookmarkedLocations = bookmarkedLocations.mutableCopy;
                    const NSUInteger locationIndex = [mutableBookmarkedLocations indexOfObject:locationMrl];
                    NSString * const newLocationMrl = [parentLocationUrl URLByAppendingPathComponent:newFileName].absoluteString;
                    [mutableBookmarkedLocations replaceObjectAtIndex:locationIndex withObject:newLocationMrl];
                    [defaults setObject:mutableBookmarkedLocations forKey:VLCLibraryBookmarkedLocationsKey];
                }
            }
            dispatch_async(dispatch_get_main_queue(), ^{
                [weakSelf internalNodesChanged:nil];
            });
        });
        dispatch_source_set_cancel_handler(fileDispatchSource, ^{
            close(descriptor);
        });
        dispatch_resume(fileDispatchSource);
        [self.observedPathDispatchSources setObject:fileDispatchSource forKey:locationMrl];
    }

    [self.observedPathDispatchSources removeObjectsForKeys:deletedLocations];
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
