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

#import "windows/video/VLCVideoWindowCommon.h"

NS_ASSUME_NONNULL_BEGIN

@interface VLCLibraryWindowController : NSWindowController

- (instancetype)initWithLibraryWindow;

@end

@interface VLCLibraryCollectionView : NSCollectionView
@end

@interface VLCLibraryWindow : VLCVideoWindowCommon

@property (readwrite, weak) IBOutlet NSSegmentedControl *segmentedTitleControl;
@property (readwrite, weak) IBOutlet VLCLibraryCollectionView *libraryCollectionView;
@property (readwrite, weak) IBOutlet NSCollectionView *mediaSourceCollectionView;
@property (readwrite, weak) IBOutlet NSScrollView *mediaSourceScrollView;
@property (readwrite, weak) IBOutlet NSTableView *playlistTableView;
@property (readwrite, weak) IBOutlet NSTextField *upNextLabel;
@property (readwrite, weak) IBOutlet NSBox *upNextSeparator;
@property (readwrite, weak) IBOutlet NSButton *clearPlaylistButton;
@property (readwrite, weak) IBOutlet NSBox *clearPlaylistSeparator;
@property (readwrite, weak) IBOutlet NSButton *repeatPlaylistButton;
@property (readwrite, weak) IBOutlet NSButton *shufflePlaylistButton;

@property (readonly) BOOL nativeFullscreenMode;
@property (readwrite) BOOL nonembedded;

- (void)videoPlaybackWillBeStarted;
- (void)enableVideoPlaybackAppearance;
- (void)disableVideoPlaybackAppearance;

- (IBAction)playlistDoubleClickAction:(id)sender;

@end

NS_ASSUME_NONNULL_END
