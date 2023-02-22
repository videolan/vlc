/*****************************************************************************
 * VLCLibraryWindow.h: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2019 VLC authors and VideoLAN
 *
 * Authors: Felix Paul Kühne <fkuehne # videolan -dot- org>
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

#import "windows/video/VLCFullVideoViewWindow.h"

NS_ASSUME_NONNULL_BEGIN

@class VLCDragDropView;
@class VLCRoundedCornerTextField;
@class VLCInputNodePathControl;
@class VLCLibraryNavigationStack;
@class VLCLibraryAudioViewController;
@class VLCLibraryMediaSourceViewController;
@class VLCLibraryVideoViewController;
@class VLCLibrarySortingMenuController;
@class VLCPlaylistDataSource;
@class VLCPlaylistController;
@class VLCPlaylistSortingMenuController;
@class VLCCustomEmptyLibraryBrowseButton;

typedef NS_ENUM(NSUInteger, VLCLibrarySegment) {
    VLCLibraryVideoSegment = 0,
    VLCLibraryMusicSegment,
    VLCLibraryBrowseSegment,
    VLCLibraryStreamsSegment
};

typedef NS_ENUM(NSUInteger, VLCViewModeSegment) {
    VLCGridViewModeSegment = 0,
    VLCListViewModeSegment
};

@interface VLCLibraryWindow : VLCFullVideoViewWindow<NSUserInterfaceItemIdentification>

extern const CGFloat VLCLibraryWindowMinimalWidth;
extern const CGFloat VLCLibraryWindowMinimalHeight;
extern const NSUserInterfaceItemIdentifier VLCLibraryWindowIdentifier;

@property (readwrite, weak) IBOutlet NSSegmentedControl *segmentedTitleControl;
@property (readwrite, weak) IBOutlet NSSegmentedControl *gridVsListSegmentedControl;
@property (readwrite, weak) IBOutlet NSSplitView *mainSplitView;
@property (readwrite, strong) IBOutlet NSView *playlistView;
@property (readwrite, weak) IBOutlet NSView *videoLibraryView;
@property (readwrite, weak) IBOutlet NSSplitView *videoLibrarySplitView;
@property (readwrite, weak) IBOutlet NSScrollView *videoLibraryCollectionViewsStackViewScrollView;
@property (readwrite, weak) IBOutlet NSStackView *videoLibraryCollectionViewsStackView;
@property (readwrite, weak) IBOutlet NSScrollView *videoLibraryGroupSelectionTableViewScrollView;
@property (readwrite, weak) IBOutlet NSTableView *videoLibraryGroupSelectionTableView;
@property (readwrite, weak) IBOutlet NSScrollView *videoLibraryGroupsTableViewScrollView;
@property (readwrite, weak) IBOutlet NSTableView *videoLibraryGroupsTableView;
@property (readwrite, weak) IBOutlet NSCollectionView *mediaSourceCollectionView;
@property (readwrite, weak) IBOutlet NSView *audioLibraryView;
@property (readwrite, weak) IBOutlet NSSplitView *audioLibrarySplitView;
@property (readwrite, weak) IBOutlet NSScrollView *audioCollectionSelectionTableViewScrollView;
@property (readwrite, weak) IBOutlet NSTableView *audioCollectionSelectionTableView;
@property (readwrite, weak) IBOutlet NSScrollView *audioGroupSelectionTableViewScrollView;
@property (readwrite, weak) IBOutlet NSTableView *audioGroupSelectionTableView;
@property (readwrite, weak) IBOutlet NSScrollView *audioLibrarySongsTableViewScrollView;
@property (readwrite, weak) IBOutlet NSTableView *audioLibrarySongsTableView;
@property (readwrite, weak) IBOutlet NSScrollView *audioCollectionViewScrollView;
@property (readwrite, weak) IBOutlet NSCollectionView *audioLibraryCollectionView;
@property (readwrite, weak) IBOutlet NSSplitView *audioLibraryGridModeSplitView;
@property (readwrite, weak) IBOutlet NSScrollView *audioLibraryGridModeSplitViewListTableViewScrollView;
@property (readwrite, weak) IBOutlet NSTableView *audioLibraryGridModeSplitViewListTableView;
@property (readwrite, weak) IBOutlet NSScrollView *audioLibraryGridModeSplitViewListSelectionCollectionViewScrollView;
@property (readwrite, weak) IBOutlet NSCollectionView *audioLibraryGridModeSplitViewListSelectionCollectionView;
@property (readwrite, weak) IBOutlet NSVisualEffectView *optionBarView;
@property (readwrite, weak) IBOutlet NSSegmentedControl *audioSegmentedControl;
@property (readwrite, weak) IBOutlet NSView *mediaSourceView;
@property (readwrite, weak) IBOutlet NSButton *mediaSourceHomeButton;
@property (readwrite, weak) IBOutlet VLCInputNodePathControl *mediaSourcePathControl;
@property (readwrite, weak) IBOutlet NSScrollView *mediaSourceTableViewScrollView;
@property (readwrite, weak) IBOutlet NSTableView *mediaSourceTableView;
@property (readwrite, weak) IBOutlet NSScrollView *mediaSourceCollectionViewScrollView;
@property (readwrite, weak) IBOutlet NSView *libraryTargetView;
@property (readwrite, weak) IBOutlet NSTableView *playlistTableView;
@property (readwrite, weak) IBOutlet NSView *mediaOptionBar;
@property (readwrite, weak) IBOutlet NSToolbar *mediaToolBar;
@property (readwrite, weak) IBOutlet NSTextField *upNextLabel;
@property (readwrite, weak) IBOutlet VLCDragDropView *playlistDragDropView;
@property (readwrite, weak) IBOutlet NSButton *openMediaButton;
@property (readwrite, weak) IBOutlet NSBox *dragDropImageBackgroundBox;
@property (readwrite, weak) IBOutlet NSBox *upNextSeparator;
@property (readwrite, weak) IBOutlet NSButton *clearPlaylistButton;
@property (readwrite, weak) IBOutlet NSBox *clearPlaylistSeparator;
@property (readwrite, weak) IBOutlet NSButton *repeatPlaylistButton;
@property (readwrite, weak) IBOutlet NSButton *shufflePlaylistButton;
@property (readwrite, weak) IBOutlet VLCRoundedCornerTextField *playlistCounterTextField;
@property (readwrite, weak) IBOutlet NSButton *librarySortButton;
@property (readwrite, weak) IBOutlet NSSearchField *librarySearchField;
@property (readwrite, weak) IBOutlet NSButton *playQueueToggle;
@property (readwrite, weak) IBOutlet NSButton *backwardsNavigationButton;
@property (readwrite, weak) IBOutlet NSButton *forwardsNavigationButton;
@property (readwrite, weak) IBOutlet NSButton *artworkButton;
@property (readwrite, weak) IBOutlet NSToolbarItem *backwardsToolbarItem;
@property (readwrite, weak) IBOutlet NSToolbarItem *forwardsToolbarItem;
@property (readwrite, weak) IBOutlet NSToolbarItem *libraryViewModeToolbarItem;
@property (readwrite, weak) IBOutlet NSToolbarItem *sortOrderToolbarItem;
@property (readwrite, weak) IBOutlet NSToolbarItem *flexibleSpaceToolbarItem;
@property (readwrite, weak) IBOutlet NSToolbarItem *segmentedTitleControlToolbarItem;
@property (readwrite, weak) IBOutlet NSToolbarItem *librarySearchToolbarItem;
@property (readwrite, weak) IBOutlet NSToolbarItem *togglePlaylistToolbarItem;
@property (readwrite, weak) IBOutlet NSLayoutConstraint *splitViewBottomConstraintToBottomBar;
@property (readwrite, weak) IBOutlet NSLayoutConstraint *splitViewBottomConstraintToSuperView;

@property (nonatomic, readwrite, strong) IBOutlet NSView *emptyLibraryView;
@property (nonatomic, readwrite, strong) IBOutlet NSImageView *placeholderImageView;
@property (nonatomic, readwrite, strong) IBOutlet NSTextField *placeholderLabel;
@property (nonatomic, readwrite, strong) IBOutlet VLCCustomEmptyLibraryBrowseButton *placeholderGoToBrowseButton;

@property (readwrite) BOOL nonembedded;
@property (readwrite) VLCLibraryNavigationStack *navigationStack;
@property (readonly) VLCLibraryAudioViewController *libraryAudioViewController;
@property (readonly) VLCLibraryMediaSourceViewController *libraryMediaSourceViewController;
@property (readonly) VLCLibraryVideoViewController *libraryVideoViewController;
@property (readonly) VLCLibrarySortingMenuController *librarySortingMenuController;
@property (readonly) VLCPlaylistDataSource *playlistDataSource;
@property (readonly) VLCPlaylistSortingMenuController *playlistSortingMenuController;
@property (readonly) VLCPlaylistController *playlistController;

- (void)reopenVideoView;
- (void)disableVideoPlaybackAppearance;
- (void)togglePlaylist;

- (IBAction)playlistDoubleClickAction:(id)sender;
- (IBAction)shuffleAction:(id)sender;
- (IBAction)repeatAction:(id)sender;
- (IBAction)clearPlaylist:(id)sender;
- (IBAction)sortPlaylist:(id)sender;
- (IBAction)sortLibrary:(id)sender;
- (IBAction)filterLibrary:(id)sender;
- (IBAction)openMedia:(id)sender;
- (IBAction)showAndHidePlaylist:(id)sender;
- (IBAction)backwardsNavigationAction:(id)sender;
- (IBAction)forwardsNavigationAction:(id)sender;
- (IBAction)segmentedControlAction:(id)sender;
@end

NS_ASSUME_NONNULL_END
