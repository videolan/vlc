/*****************************************************************************
 * VLCLibraryWindowSplitViewManager.h: MacOS X interface module
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

#import "VLCLibraryWindowSplitViewController.h"

#import "library/VLCLibraryWindow.h"
#import "main/VLCMain.h"
#import "windows/video/VLCMainVideoViewController.h"

// Make sure these match the identifiers in the XIB
static NSString * const VLCLibraryWindowNavigationSidebarIdentifier = @"VLCLibraryWindowNavigationSidebarIdentifier";
static NSString * const VLCLibraryWindowPlaylistSidebarIdentifier = @"VLCLibraryWindowPlaylistSidebarIdentifier";

@implementation VLCLibraryWindowSplitViewController

- (void)viewDidLoad
{
    [super viewDidLoad];

    self.splitView.wantsLayer = YES;

    NSSplitViewItem * const navSidebarItem = [NSSplitViewItem sidebarWithViewController:self.navSidebarViewController];
    NSSplitViewItem * const libraryTargetViewItem = [NSSplitViewItem splitViewItemWithViewController:self.libraryTargetViewController];
    NSSplitViewItem * const playlistSidebarItem = [NSSplitViewItem sidebarWithViewController:self.playlistSidebarViewController];

    if (@available(macOS 11.0, *)) {
        navSidebarItem.allowsFullHeightLayout = YES;
    }

    self.splitViewItems = @[navSidebarItem, libraryTargetViewItem, playlistSidebarItem];
}

- (BOOL)splitView:(NSSplitView *)splitView shouldHideDividerAtIndex:(NSInteger)dividerIndex
{
    [super splitView:splitView shouldHideDividerAtIndex:dividerIndex];
    return dividerIndex == VLCLibraryWindowPlaylistSidebarSplitViewDividerIndex ||
           (dividerIndex == VLCLibraryWindowNavigationSidebarSplitViewDividerIndex &&
            !VLCMain.sharedInstance.libraryWindow.videoViewController.view.hidden);
}

@end
