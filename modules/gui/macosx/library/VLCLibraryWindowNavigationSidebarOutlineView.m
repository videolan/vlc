/*****************************************************************************
 * VLCLibraryWindowNavigationSidebarOutlineView.m: MacOS X interface module
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

#import "VLCLibraryWindowNavigationSidebarOutlineView.h"

#import "extensions/NSString+Helpers.h"

#import "library/VLCLibrarySegment.h"
#import "library/VLCLibrarySegmentBookmarkedLocation.h"

@implementation VLCLibraryWindowNavigationSidebarOutlineView

- (NSRect)frameOfOutlineCellAtRow:(NSInteger)row
{
    if ([self shouldHideDisclosureCaretAtRow:row]) {
        return NSZeroRect;
    }
    return [super frameOfOutlineCellAtRow:row];
}

- (BOOL)shouldHideDisclosureCaretAtRow:(NSInteger)row
{
    const id item = [self itemAtRow:row];
    if (![self isExpandable:item]) {
        return NO;
    }
    
    NSTreeNode * const treeNode = (NSTreeNode *)item;
    VLCLibrarySegment * const segment = (VLCLibrarySegment *)treeNode.representedObject;
    
    if (self.selectedRow < 0 || self.selectedRow >= self.numberOfRows) {
        return NO; // No valid selection
    }
    
    NSTreeNode * const selectedSegmentItem = (NSTreeNode *)[self itemAtRow:self.selectedRow];
    if (selectedSegmentItem == nil) {
        return NO;
    }
    
    VLCLibrarySegment * const selectedSegment = (VLCLibrarySegment *)selectedSegmentItem.representedObject;
    if (selectedSegment == nil) {
        return NO;
    }
    
    const NSInteger childNodeIndex = [segment.childNodes indexOfObjectPassingTest:^BOOL(NSTreeNode * _Nonnull obj, const NSUInteger __unused idx, BOOL * const __unused stop) {
        VLCLibrarySegment * const childSegment = (VLCLibrarySegment *)obj;
        return childSegment.segmentType == selectedSegment.segmentType;
    }];
    
    // Hide triangle when collapsing would be disabled
    return childNodeIndex != NSNotFound;
}

- (void)selectRowIndexes:(NSIndexSet *)indexes byExtendingSelection:(BOOL)extend
{
    [super selectRowIndexes:indexes byExtendingSelection:extend];
    [self refreshDisclosureCaret];
}

- (void)refreshDisclosureCaret
{
    const NSInteger rowCount = self.numberOfRows;
    for (NSInteger row = 0; row < rowCount; row++) {
        const id item = [self itemAtRow:row];
        if ([self isExpandable:item]) {
            [self reloadItem:item reloadChildren:NO];
        }
    }
    
    [self setNeedsDisplay:YES];
    [self displayIfNeeded];
}

- (NSMenu *)menuForEvent:(NSEvent *)event
{
    const NSPoint location = [self convertPoint:event.locationInWindow fromView:nil];
    const NSInteger row = [self rowAtPoint:location];
    NSTreeNode * const node = [self itemAtRow:row];
    VLCLibrarySegment * const segment = node.representedObject;
    
    if ([segment.representedObject isKindOfClass:NSNumber.class]) {
        return nil;
    }

    if ([segment.representedObject isKindOfClass:VLCLibrarySegmentBookmarkedLocation.class]) {
        VLCLibrarySegmentBookmarkedLocation * const descriptor = segment.representedObject;
        NSMenu * const bookmarkMenu = [[NSMenu alloc] initWithTitle:descriptor.name];
        NSMenuItem * const removeBookmarkItem = 
            [[NSMenuItem alloc] initWithTitle:_NS("Remove Bookmark")
                                       action:@selector(removeBookmark:)
                                keyEquivalent:@""];
        removeBookmarkItem.representedObject = descriptor;
        [bookmarkMenu addItem:removeBookmarkItem];
        return bookmarkMenu;
    }

    return nil;
}

- (void)removeBookmark:(id)sender
{
    NSMenuItem * const menuItem = sender;
    NSParameterAssert(menuItem != nil);

    VLCLibrarySegmentBookmarkedLocation * const descriptor = menuItem.representedObject;
    NSParameterAssert(descriptor != nil);

    NSString * const descriptorMrl = descriptor.mrl;
    NSParameterAssert(descriptorMrl != nil);

    NSUserDefaults * const defaults = NSUserDefaults.standardUserDefaults;
    NSMutableArray<NSString *> * const bookmarkedLocations =
        [defaults stringArrayForKey:VLCLibraryBookmarkedLocationsKey].mutableCopy;
    [bookmarkedLocations removeObject:descriptorMrl];
    [defaults setObject:bookmarkedLocations forKey:VLCLibraryBookmarkedLocationsKey];

    NSNotificationCenter * const defaultCenter = NSNotificationCenter.defaultCenter;
    [defaultCenter postNotificationName:VLCLibraryBookmarkedLocationsChanged object:descriptor];
}

@end
