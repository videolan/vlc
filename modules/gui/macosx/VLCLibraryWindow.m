/*****************************************************************************
 * VLCLibraryWindow.m: MacOS X interface module
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

#import "VLCLibraryWindow.h"
#import "NSString+Helpers.h"
#import "VLCPlaylistTableCellView.h"
#import "VLCPlaylistController.h"
#import "VLCPlaylistDataSource.h"
#import "VLCLibraryCollectionViewItem.h"
#import "VLCMain.h"

static const float f_min_window_width = 604.;
static const float f_min_window_height = 307.;
static const float f_playlist_row_height = 40.;

static NSString *VLCLibraryCellIdentifier = @"VLCLibraryCellIdentifier";

@interface VLCLibraryWindow ()
{
    VLCPlaylistDataSource *_playlistDataSource;
    VLCLibraryDataSource *_libraryDataSource;
}
@end

@implementation VLCLibraryWindow

- (void)awakeFromNib
{
    _segmentedTitleControl.segmentCount = 3;
    [_segmentedTitleControl setTarget:self];
    [_segmentedTitleControl setAction:@selector(segmentedControlAction)];
    [_segmentedTitleControl setLabel:_NS("Music") forSegment:0];
    [_segmentedTitleControl setLabel:_NS("Video") forSegment:1];
    [_segmentedTitleControl setLabel:_NS("Network") forSegment:2];
    [_segmentedTitleControl sizeToFit];

    VLCPlaylistController *playlistController = [[VLCMain sharedInstance] playlistController];
    _playlistDataSource = [[VLCPlaylistDataSource alloc] init];
    _playlistDataSource.playlistController = playlistController;
    _playlistDataSource.tableView = _playlistTableView;
    playlistController.playlistDataSource = _playlistDataSource;

    _playlistTableView.dataSource = _playlistDataSource;
    _playlistTableView.delegate = _playlistDataSource;
    _playlistTableView.rowHeight = f_playlist_row_height;
    [_playlistTableView reloadData];

    _libraryDataSource = [[VLCLibraryDataSource alloc] init];
    _libraryCollectionView.dataSource = _libraryDataSource;
    _libraryCollectionView.delegate = _libraryDataSource;
    [_libraryCollectionView registerClass:[VLCLibraryCollectionViewItem class] forItemWithIdentifier:VLCLibraryCellIdentifier];
    [_libraryCollectionView reloadData];
}

- (void)segmentedControlAction
{
}

@end

@implementation VLCLibraryDataSource

- (NSInteger)collectionView:(NSCollectionView *)collectionView numberOfItemsInSection:(NSInteger)section
{
    return 2;
}

- (NSCollectionViewItem *)collectionView:(NSCollectionView *)collectionView itemForRepresentedObjectAtIndexPath:(NSIndexPath *)indexPath
{
    VLCLibraryCollectionViewItem *viewItem = [collectionView makeItemWithIdentifier:VLCLibraryCellIdentifier forIndexPath:indexPath];

    viewItem.mediaTitleTextField.stringValue = @"Custom Cell Label Text";
    viewItem.mediaImageView.image = [NSImage imageNamed: @"noart.png"];

    return viewItem;
}

- (void)collectionView:(NSCollectionView *)collectionView didSelectItemsAtIndexPaths:(NSSet<NSIndexPath *> *)indexPaths
{
    NSLog(@"library selection changed: %@", indexPaths);
}

@end

@implementation VLCLibraryWindowController

- (instancetype)initWithLibraryWindow
{
    self = [super initWithWindowNibName:@"VLCLibraryWindow"];
    return self;
}

- (void)windowDidLoad
{
    VLCLibraryWindow *window = (VLCLibraryWindow *)self.window;
    [window setRestorable:NO];
    [window setExcludedFromWindowsMenu:YES];
    [window setAcceptsMouseMovedEvents:YES];
    [window setContentMinSize:NSMakeSize(f_min_window_width, f_min_window_height)];
}

@end
