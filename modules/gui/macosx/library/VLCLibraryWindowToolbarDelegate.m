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
#import "library/VLCLibraryWindowSidebarRootViewController.h"
#import "library/VLCLibraryWindowSplitViewController.h"

#import "main/VLCMain.h"

#import "menus/VLCMainMenu.h"
#import "menus/renderers/VLCRendererMenuController.h"

NSString * const VLCLibraryWindowTrackingSeparatorToolbarItemIdentifier = 
    @"VLCLibraryWindowTrackingSeparatorToolbarItemIdentifier";

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
    self.togglePlayQueueToolbarItem.toolTip = _NS("Toggle Play Queue");

    self.vlcIconToolbarItem.minSize = NSMakeSize(18, 18);
    self.vlcIconToolbarItem.maxSize = NSMakeSize(18, 18);

    NSImageView * const vlcIconImageView = [[NSImageView alloc] initWithFrame:NSZeroRect];
    vlcIconImageView.image = NSApp.applicationIconImage;
    self.vlcIconToolbarItem.view = vlcIconImageView;;

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
    NSView * const multifunctionSidebar =
        self.libraryWindow.splitViewController.multifunctionSidebarViewController.view;
    NSSplitView * const sv = self.libraryWindow.mainSplitView;
    self.libraryWindow.playQueueToggle.state =
        ![sv.arrangedSubviews containsObject:multifunctionSidebar] ||
        [sv isSubviewCollapsed:multifunctionSidebar]
            ? NSControlStateValueOff
            : NSControlStateValueOn;
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
    NSParameterAssert(toolbarItem != nil);
    NSParameterAssert(items != nil);
    NSParameterAssert(toolbarItem.itemIdentifier.length > 0);

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
                              self.toggleNavSidebarToolbarItem,
                              self.vlcIconToolbarItem]];

    [self insertToolbarItem:self.forwardsToolbarItem
                  inFrontOf:@[self.backwardsToolbarItem,
                              self.trackingSeparatorToolbarItem,
                              self.toggleNavSidebarToolbarItem,
                              self.vlcIconToolbarItem]];
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
                              self.toggleNavSidebarToolbarItem,
                              self.vlcIconToolbarItem]];
}

- (void)setLibrarySearchToolbarItemVisible:(BOOL)visible
{
    if (!visible) {
        [self hideToolbarItem:self.librarySearchToolbarItem];
        [self.libraryWindow clearFilterString];
        return;
    }

    // Display as far to the right as possible, but not in front of the multifunc bar toggle button
    NSMutableArray<NSToolbarItem *> * const currentToolbarItems =
        [NSMutableArray arrayWithArray:self.toolbar.items];
    if (currentToolbarItems.lastObject == self.togglePlayQueueToolbarItem) {
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
                  inFrontOf:@[self.forwardsToolbarItem,
                              self.backwardsToolbarItem,
                              self.trackingSeparatorToolbarItem,
                              self.toggleNavSidebarToolbarItem,
                              self.vlcIconToolbarItem]];
}

- (void)applyVisiblityFlags:(VLCLibraryWindowToolbarDisplayFlags)flags
{
    [self setForwardsBackwardsToolbarItemsVisible:flags & VLCLibraryWindowToolbarDisplayFlagNavigationButtons];
    [self setSortOrderToolbarItemVisible:flags & VLCLibraryWindowToolbarDisplayFlagSortOrderButton];
    [self setLibrarySearchToolbarItemVisible:flags & VLCLibraryWindowToolbarDisplayFlagLibrarySearchBar];
    [self setViewModeToolbarItemVisible:flags & VLCLibraryWindowToolbarDisplayFlagToggleViewModeSegmentButton];
}

@end
