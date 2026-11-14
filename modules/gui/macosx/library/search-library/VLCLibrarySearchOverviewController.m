/*****************************************************************************
 * VLCLibrarySearchOverviewController.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2026 VLC authors and VideoLAN
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

#import "VLCLibrarySearchOverviewController.h"

#import "VLCLibrarySearchViewController.h"

#import "library/VLCLibraryWindow.h"
#import "library/VLCLibraryWindowPersistentPreferences.h"

@implementation VLCLibrarySearchOverviewController
{
    VLCLibrarySearchViewController *_searchViewController;
}

- (instancetype)initWithLibraryWindow:(VLCLibraryWindow *)libraryWindow
{
    self = [super initWithLibraryWindow:libraryWindow];
    if (self) {
        [NSNotificationCenter.defaultCenter addObserver:self
                                               selector:@selector(librarySearchFieldDidChange:)
                                                   name:NSControlTextDidChangeNotification
                                                 object:nil];
    }
    return self;
}

- (void)dealloc
{
    [_searchViewController dismissFromContainer];
}

- (VLCLibrarySearchViewController *)newSearchViewController
{
    return [[VLCLibrarySearchViewController alloc] initWithLibraryWindow:self.libraryWindow];
}

- (void)librarySearchFieldDidChange:(NSNotification *)notification
{
    if (self.libraryWindow.librarySegmentViewController != self || self.libraryWindow.embeddedVideoPlaybackActive) {
        return;
    }

    NSSearchField * const searchField = self.libraryWindow.librarySearchField;
    if (notification.object == searchField || notification.object == searchField.currentEditor) {
        [self presentOverview];
    }
}

- (void)presentOverview
{
    NSString * const query = self.libraryWindow.librarySearchField.stringValue;
    const BOOL wasShowingSearch = self.isShowingSearchOverlay;
    if (query.length > 0) {
        if (_searchViewController == nil) {
            _searchViewController = [self newSearchViewController];
        }
        [_searchViewController presentInContainer:self.libraryTargetView];
        _isShowingSearchOverlay = YES;
        [_searchViewController searchForString:query];
    } else {
        [self dismissSearch];
        [self presentOverviewContent];
    }

    if (wasShowingSearch != self.isShowingSearchOverlay) {
        [self.libraryWindow updateGridVsListViewModeSegmentedControl];
        [self.libraryWindow updateToolbarDisplayFlags];
    }
}

- (void)presentOverviewContent
{
    [self doesNotRecognizeSelector:_cmd];
}

- (void)dismissSearch
{
    [_searchViewController dismissFromContainer];
    _isShowingSearchOverlay = NO;
}

- (void)disconnect
{
    [super disconnect];
    [self dismissSearch];
}

- (VLCLibraryWindowToolbarDisplayFlags)toolbarDisplayFlags
{
    VLCLibraryWindowToolbarDisplayFlags flags = VLCLibraryWindowToolbarDisplayFlagLibrarySearchBar;
    if (self.isShowingSearchOverlay) {
        flags |= VLCLibraryWindowToolbarDisplayFlagToggleViewModeSegmentButton;
    }
    return flags;
}

- (VLCLibraryViewModeSegment)viewMode
{
    return self.isShowingSearchOverlay
        ? VLCLibraryWindowPersistentPreferences.sharedInstance.searchLibraryViewMode
        : VLCLibrarySmallestSentinelViewModeSegment;
}

- (void)setViewMode:(VLCLibraryViewModeSegment)viewMode
{
    if (self.isShowingSearchOverlay) {
        VLCLibraryWindowPersistentPreferences.sharedInstance.searchLibraryViewMode = viewMode;
        [_searchViewController presentInContainer:self.libraryTargetView];
    }
}

@end
