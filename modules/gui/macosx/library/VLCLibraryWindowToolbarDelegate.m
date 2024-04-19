/*****************************************************************************
 * VLCLibraryWindowToolbarDelegate.m: MacOS X interface module
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

#import "VLCLibraryWindowToolbarDelegate.h"

#import "library/VLCLibraryWindow.h"
#import "library/VLCLibraryWindowPlaylistSidebarViewController.h"
#import "library/VLCLibraryWindowSplitViewController.h"

NSString * const VLCLibraryWindowTrackingSeparatorToolbarItemIdentifier = @"VLCLibraryWindowTrackingSeparatorToolbarItemIdentifier";

@implementation VLCLibraryWindowToolbarDelegate

- (void)awakeFromNib
{
    self.toolbar.allowsUserCustomization = NO;
}

- (NSToolbarItem *)toolbar:(NSToolbar *)toolbar
     itemForItemIdentifier:(NSToolbarItemIdentifier)itemIdentifier
 willBeInsertedIntoToolbar:(BOOL)flag
{
    if ([itemIdentifier isEqualToString:VLCLibraryWindowTrackingSeparatorToolbarItemIdentifier]) {
        if (@available(macOS 11.0, *)) {
            return [NSTrackingSeparatorToolbarItem trackingSeparatorToolbarItemWithIdentifier:itemIdentifier splitView:self.libraryWindow.mainSplitView dividerIndex:VLCLibraryWindowNavigationSidebarSplitViewDividerIndex];
        }
    }

    return nil;
}

- (void)hideToolbarItem:(NSToolbarItem *)toolbarItem
{
    const NSInteger toolbarItemIndex = [self.libraryWindow.toolbar.items indexOfObject:toolbarItem];
    if (toolbarItemIndex != NSNotFound) {
        [self.libraryWindow.toolbar removeItemAtIndex:toolbarItemIndex];
    }
}

/*
 * Try to insert the toolbar item ahead of a group of possible toolbar items.
 * "items" should contain items sorted from the trailing edge of the toolbar to leading edge.
 * "toolbarItem" will be inserted as close to the trailing edge as possible.
 *
 * If you have: | item1 | item2 | item3 | item4 |
 * and the "items" parameter is an array containing @[item6, item5, item2, item1]
 * then the "toolbarItem" provided to this function will place toolbarItem thus:
 * | item1 | item2 | toolbarItem | item3 | item4 |
*/

- (void)insertToolbarItem:(NSToolbarItem *)toolbarItem inFrontOf:(NSArray<NSToolbarItem *> *)items
{
    NSParameterAssert(toolbarItem != nil && items != nil && toolbarItem.itemIdentifier.length > 0);

    const NSInteger toolbarItemIndex = [self.libraryWindow.toolbar.items indexOfObject:toolbarItem];
    if (toolbarItemIndex != NSNotFound) {
        return;
    }

    for (NSToolbarItem * const item in items) {
        const NSInteger itemIndex = [self.libraryWindow.toolbar.items indexOfObject:item];

        if (itemIndex != NSNotFound) {
            [self.libraryWindow.toolbar insertItemWithItemIdentifier:toolbarItem.itemIdentifier
                                                             atIndex:itemIndex + 1];
            return;
        }
    }

    [self.libraryWindow.toolbar insertItemWithItemIdentifier:toolbarItem.itemIdentifier atIndex:0];
}

- (void)setForwardsBackwardsToolbarItemsVisible:(BOOL)visible
{
    if (!visible) {
        [self hideToolbarItem:self.libraryWindow.forwardsToolbarItem];
        [self hideToolbarItem:self.libraryWindow.backwardsToolbarItem];
        return;
    }

    [self insertToolbarItem:self.libraryWindow.backwardsToolbarItem 
                  inFrontOf:@[
                    self.libraryWindow.trackingSeparatorToolbarItem,
                    self.libraryWindow.toggleNavSidebarToolbarItem]];

    [self insertToolbarItem:self.libraryWindow.forwardsToolbarItem
                  inFrontOf:@[self.libraryWindow.backwardsToolbarItem,
                              self.libraryWindow.trackingSeparatorToolbarItem,
                              self.libraryWindow.toggleNavSidebarToolbarItem]];
}

- (void)setSortOrderToolbarItemVisible:(BOOL)visible
{
    if (!visible) {
        [self hideToolbarItem:self.libraryWindow.sortOrderToolbarItem];
        return;
    }

    [self insertToolbarItem:self.libraryWindow.sortOrderToolbarItem
                  inFrontOf:@[self.libraryWindow.libraryViewModeToolbarItem,
                              self.libraryWindow.forwardsToolbarItem,
                              self.libraryWindow.backwardsToolbarItem,
                              self.libraryWindow.trackingSeparatorToolbarItem,
                              self.libraryWindow.toggleNavSidebarToolbarItem]];
}

- (void)setLibrarySearchToolbarItemVisible:(BOOL)visible
{
    if (!visible) {
        [self hideToolbarItem:self.libraryWindow.librarySearchToolbarItem];
        [self.libraryWindow clearFilterString];
        return;
    }

    // Display as far to the right as possible, but not in front of the playlist toggle button
    NSMutableArray<NSToolbarItem *> * const currentToolbarItems =
        [NSMutableArray arrayWithArray:self.libraryWindow.toolbar.items];
    if (currentToolbarItems.lastObject == self.libraryWindow.togglePlaylistToolbarItem) {
        [currentToolbarItems removeLastObject];
    }

    NSArray * const reversedCurrentToolbarItems =
        currentToolbarItems.reverseObjectEnumerator.allObjects;
    [self insertToolbarItem:self.libraryWindow.librarySearchToolbarItem
                  inFrontOf:reversedCurrentToolbarItems];
}

- (void)setViewModeToolbarItemVisible:(BOOL)visible
{
    if (!visible) {
        [self hideToolbarItem:self.libraryWindow.libraryViewModeToolbarItem];
        return;
    }

    [self insertToolbarItem:self.libraryWindow.libraryViewModeToolbarItem
                  inFrontOf:@[self.libraryWindow.toggleNavSidebarToolbarItem,
                              self.libraryWindow.trackingSeparatorToolbarItem,
                              self.libraryWindow.forwardsToolbarItem,
                              self.libraryWindow.backwardsToolbarItem]];
}

- (void)updatePlayqueueToggleState
{
    NSView * const playlistView =
        self.libraryWindow.splitViewController.playlistSidebarViewController.view;
    self.libraryWindow.playQueueToggle.state =
        [self.libraryWindow.mainSplitView isSubviewCollapsed:playlistView] ?
            NSControlStateValueOff : NSControlStateValueOn;
}

@end
