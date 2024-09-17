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

#import "library/VLCLibraryUIUnits.h"
#import "library/VLCLibraryWindow.h"
#import "library/VLCLibraryWindowNavigationSidebarViewController.h"
#import "library/VLCLibraryWindowSidebarViewController.h"

#import "main/VLCMain.h"

#import "windows/video/VLCMainVideoViewController.h"

@implementation VLCLibraryWindowSplitViewController

- (void)viewDidLoad
{
    [super viewDidLoad];

    [VLCMain.sharedInstance.libraryWindow.videoViewController.view addObserver:self
                                                                    forKeyPath:@"hidden"
                                                                       options:0
                                                                       context:nil];

    self.splitView.wantsLayer = YES;

    _navSidebarViewController = [[VLCLibraryWindowNavigationSidebarViewController alloc] initWithLibraryWindow:VLCMain.sharedInstance.libraryWindow];
    _libraryTargetViewController = [[NSViewController alloc] init];
    _sidebarViewController = [[VLCLibraryWindowSidebarViewController alloc] initWithLibraryWindow:VLCMain.sharedInstance.libraryWindow];

    self.libraryTargetViewController.view = self.libraryWindow.libraryTargetView;

    _navSidebarItem = [NSSplitViewItem sidebarWithViewController:self.navSidebarViewController];
    _libraryTargetViewItem = [NSSplitViewItem splitViewItemWithViewController:self.libraryTargetViewController];
    _sidebarItem = [NSSplitViewItem sidebarWithViewController:self.sidebarViewController];

    if (@available(macOS 11.0, *)) {
        _navSidebarItem.allowsFullHeightLayout = YES;
    }

    _navSidebarItem.preferredThicknessFraction = 0.2;
    _navSidebarItem.maximumThickness = VLCLibraryUIUnits.libraryWindowNavSidebarMaxWidth;

    self.sidebarItem.preferredThicknessFraction = 0.2;
    self.sidebarItem.maximumThickness = VLCLibraryUIUnits.libraryWindowPlaylistSidebarMaxWidth;
    self.sidebarItem.canCollapse = YES;
    self.sidebarItem.collapseBehavior = NSSplitViewItemCollapseBehaviorPreferResizingSiblingsWithFixedSplitView;

    self.splitViewItems = @[_navSidebarItem, _libraryTargetViewItem, self.sidebarItem];
}

- (void)observeValueForKeyPath:(NSString *)keyPath
                      ofObject:(id)object
                        change:(NSDictionary *)change
                       context:(void *)context

{
    if([keyPath isEqualToString:@"hidden"]) {
        VLCLibraryWindow * const libraryWindow = VLCMain.sharedInstance.libraryWindow;
        const BOOL videoViewClosed = libraryWindow.videoViewController.view.hidden;
        _navSidebarItem.collapsed = !videoViewClosed;
    }
}

- (IBAction)toggleNavigationSidebar:(id)sender
{
    const BOOL navigationSidebarCollapsed = self.navSidebarItem.isCollapsed;
    self.navSidebarItem.animator.collapsed = !navigationSidebarCollapsed;
}

- (IBAction)togglePlaylistSidebar:(id)sender
{
    const BOOL sidebarCollapsed = self.sidebarItem.isCollapsed;
    self.sidebarItem.animator.collapsed = !sidebarCollapsed;

    const NSControlStateValue controlState = self.sidebarItem.isCollapsed ? NSControlStateValueOff : NSControlStateValueOn;
    self.libraryWindow.playQueueToggle.state = controlState;
    self.libraryWindow.videoViewController.playlistButton.state = controlState;
}

@end
