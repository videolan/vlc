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

#import "extensions/NSString+Helpers.h"

#import "library/VLCLibraryWindow.h"
#import "library/VLCLibraryWindowPlaylistSidebarViewController.h"
#import "library/VLCLibraryWindowSplitViewController.h"

#import "main/VLCMain.h"

#import "menus/VLCMainMenu.h"
#import "menus/renderers/VLCRendererMenuController.h"

NSString * const VLCLibraryWindowTrackingSeparatorToolbarItemIdentifier = @"VLCLibraryWindowTrackingSeparatorToolbarItemIdentifier";

@implementation VLCLibraryWindowToolbarDelegate

#pragma mark - XIB handling

- (void)awakeFromNib
{
    self.toolbar.allowsUserCustomization = NO;
    
    if (@available(macOS 11.0, *)) {
        const NSInteger navSidebarToggleToolbarItemIndex = 
            [self.toolbar.items indexOfObject:self.toggleNavSidebarToolbarItem];

        NSAssert(navSidebarToggleToolbarItemIndex != NSNotFound,
                 @"Could not find navigation sidebar toggle toolbar item!");

        const NSInteger trackingSeparatorItemIndex = navSidebarToggleToolbarItemIndex + 1;
        [self.toolbar 
            insertItemWithItemIdentifier:VLCLibraryWindowTrackingSeparatorToolbarItemIdentifier
                                 atIndex:trackingSeparatorItemIndex];
        _trackingSeparatorToolbarItem =
            [self.toolbar.items objectAtIndex:trackingSeparatorItemIndex];
    }

    NSNotificationCenter * const notificationCenter = NSNotificationCenter.defaultCenter;
    [notificationCenter addObserver:self
                           selector:@selector(renderersChanged:)
                               name:VLCRendererAddedNotification
                             object:nil];
    [notificationCenter addObserver:self
                           selector:@selector(renderersChanged:)
                               name:VLCRendererRemovedNotification
                             object:nil];

    self.libraryViewModeToolbarItem.toolTip = _NS("Grid View or List View");
    self.sortOrderToolbarItem.toolTip = _NS("Select Sorting Mode");
    self.togglePlaylistToolbarItem.toolTip = _NS("Toggle Playqueue");

    // Hide renderers toolbar item at first. Start discoveries and wait for notifications about
    // renderers being added or removed to keep hidden or show depending on outcome
    [self hideToolbarItem:self.renderersToolbarItem];
    [VLCMain.sharedInstance.mainMenu.rendererMenuController startRendererDiscoveries];

    [self updatePlayqueueToggleState];
}

#pragma mark - toolbar delegate methods

- (NSToolbarItem *)toolbar:(NSToolbar *)toolbar
     itemForItemIdentifier:(NSToolbarItemIdentifier)itemIdentifier
 willBeInsertedIntoToolbar:(BOOL)flag
{
    if ([itemIdentifier isEqualToString:VLCLibraryWindowTrackingSeparatorToolbarItemIdentifier]) {
        if (@available(macOS 11.0, *)) {
            return [NSTrackingSeparatorToolbarItem
                    trackingSeparatorToolbarItemWithIdentifier:itemIdentifier
                    splitView:self.libraryWindow.mainSplitView
                    dividerIndex:VLCLibraryWindowNavigationSidebarSplitViewDividerIndex];
        }
    }

    return nil;
}

#pragma mark - renderers toolbar item handling

- (IBAction)rendererControlAction:(id)sender
{
    [NSMenu popUpContextMenu:VLCMain.sharedInstance.mainMenu.rendererMenu
                   withEvent:NSApp.currentEvent
                     forView:sender];
}

- (void)renderersChanged:(NSNotification *)notification
{
    const NSUInteger rendererCount =
        VLCMain.sharedInstance.mainMenu.rendererMenuController.rendererItems.count;
    const BOOL rendererToolbarItemVisible =
        [self.toolbar.items containsObject:self.renderersToolbarItem];

    if (rendererCount > 0 && !rendererToolbarItemVisible) {
        [self insertToolbarItem:self.renderersToolbarItem
                      inFrontOf:@[self.sortOrderToolbarItem,
                                  self.libraryViewModeToolbarItem,
                                  self.forwardsToolbarItem,
                                  self.backwardsToolbarItem]];
    } else if (rendererCount == 0 && rendererToolbarItemVisible) {
        [self hideToolbarItem:self.renderersToolbarItem];
    }
}

#pragma mark - play queue toggle toolbar item handling

- (void)updatePlayqueueToggleState
{
    NSView * const playlistView =
        self.libraryWindow.splitViewController.playlistSidebarViewController.view;
    self.libraryWindow.playQueueToggle.state =
        [self.libraryWindow.mainSplitView isSubviewCollapsed:playlistView] ?
            NSControlStateValueOff : NSControlStateValueOn;
}

#pragma mark - convenience method for configuration of toolbar items layout

- (void)layoutForSegment:(VLCLibrarySegmentType)segment
{
    switch(segment) {
        case VLCLibraryLowSentinelSegment:
            vlc_assert_unreachable();
        case VLCLibraryHomeSegment:
            [self setForwardsBackwardsToolbarItemsVisible:NO];
            [self setSortOrderToolbarItemVisible:NO];
            [self setLibrarySearchToolbarItemVisible:NO];
            [self setViewModeToolbarItemVisible:NO];
            break;
        case VLCLibraryVideoSegment:
            [self setForwardsBackwardsToolbarItemsVisible:NO];
            [self setSortOrderToolbarItemVisible:YES];
            [self setLibrarySearchToolbarItemVisible:YES];
            [self setViewModeToolbarItemVisible:YES];
            break;
        case VLCLibraryMusicSegment:
        case VLCLibraryArtistsMusicSubSegment:
        case VLCLibraryAlbumsMusicSubSegment:
        case VLCLibrarySongsMusicSubSegment:
        case VLCLibraryGenresMusicSubSegment:
            [self setForwardsBackwardsToolbarItemsVisible:NO];
            [self setSortOrderToolbarItemVisible:YES];
            [self setLibrarySearchToolbarItemVisible:YES];
            [self setViewModeToolbarItemVisible:YES];
            break;
        case VLCLibraryBrowseSegment:
        case VLCLibraryStreamsSegment:
            [self setForwardsBackwardsToolbarItemsVisible:YES];
            [self setSortOrderToolbarItemVisible:NO];
            [self setLibrarySearchToolbarItemVisible:NO];
            [self setViewModeToolbarItemVisible:YES];
            break;
        case VLCLibraryHighSentinelSegment:
            vlc_assert_unreachable();
    }
}

#pragma mark - item visibility handling

- (void)hideToolbarItem:(NSToolbarItem *)toolbarItem
{
    const NSInteger toolbarItemIndex = [self.toolbar.items indexOfObject:toolbarItem];
    if (toolbarItemIndex != NSNotFound) {
        [self.toolbar removeItemAtIndex:toolbarItemIndex];
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

    const NSInteger toolbarItemIndex = [self.toolbar.items indexOfObject:toolbarItem];
    if (toolbarItemIndex != NSNotFound) {
        return;
    }

    for (NSToolbarItem * const item in items) {
        const NSInteger itemIndex = [self.toolbar.items indexOfObject:item];

        if (itemIndex != NSNotFound) {
            [self.toolbar insertItemWithItemIdentifier:toolbarItem.itemIdentifier
                                               atIndex:itemIndex + 1];
            return;
        }
    }

    [self.toolbar insertItemWithItemIdentifier:toolbarItem.itemIdentifier atIndex:0];
}

#pragma mark - convenience methods for hiding/showing and positioning certain toolbar items

- (void)setForwardsBackwardsToolbarItemsVisible:(BOOL)visible
{
    if (!visible) {
        [self hideToolbarItem:self.forwardsToolbarItem];
        [self hideToolbarItem:self.backwardsToolbarItem];
        return;
    }

    [self insertToolbarItem:self.backwardsToolbarItem 
                  inFrontOf:@[self.trackingSeparatorToolbarItem,
                              self.toggleNavSidebarToolbarItem]];

    [self insertToolbarItem:self.forwardsToolbarItem
                  inFrontOf:@[self.backwardsToolbarItem,
                              self.trackingSeparatorToolbarItem,
                              self.toggleNavSidebarToolbarItem]];
}

- (void)setSortOrderToolbarItemVisible:(BOOL)visible
{
    if (!visible) {
        [self hideToolbarItem:self.sortOrderToolbarItem];
        return;
    }

    [self insertToolbarItem:self.sortOrderToolbarItem
                  inFrontOf:@[self.libraryViewModeToolbarItem,
                              self.forwardsToolbarItem,
                              self.backwardsToolbarItem,
                              self.trackingSeparatorToolbarItem,
                              self.toggleNavSidebarToolbarItem]];
}

- (void)setLibrarySearchToolbarItemVisible:(BOOL)visible
{
    if (!visible) {
        [self hideToolbarItem:self.librarySearchToolbarItem];
        [self.libraryWindow clearFilterString];
        return;
    }

    // Display as far to the right as possible, but not in front of the playlist toggle button
    NSMutableArray<NSToolbarItem *> * const currentToolbarItems =
        [NSMutableArray arrayWithArray:self.toolbar.items];
    if (currentToolbarItems.lastObject == self.togglePlaylistToolbarItem) {
        [currentToolbarItems removeLastObject];
    }

    NSArray * const reversedCurrentToolbarItems =
        currentToolbarItems.reverseObjectEnumerator.allObjects;
    [self insertToolbarItem:self.librarySearchToolbarItem
                  inFrontOf:reversedCurrentToolbarItems];
}

- (void)setViewModeToolbarItemVisible:(BOOL)visible
{
    if (!visible) {
        [self hideToolbarItem:self.libraryViewModeToolbarItem];
        return;
    }

    [self insertToolbarItem:self.libraryViewModeToolbarItem
                  inFrontOf:@[self.toggleNavSidebarToolbarItem,
                              self.trackingSeparatorToolbarItem,
                              self.forwardsToolbarItem,
                              self.backwardsToolbarItem]];
}

@end
