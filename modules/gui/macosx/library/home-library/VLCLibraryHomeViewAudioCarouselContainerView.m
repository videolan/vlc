/*****************************************************************************
 * VLCLibraryHomeViewAudioCarouselContainerView.m: MacOS X interface module
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

#import "VLCLibraryHomeViewAudioCarouselContainerView.h"

#import "extensions/NSString+Helpers.h"

#import "library/VLCLibraryController.h"
#import "library/VLCLibraryCarouselCollectionViewItem.h"
#import "library/VLCLibraryCollectionView.h"
#import "library/VLCLibraryModel.h"
#import "library/VLCLibrarySegment.h"
#import "library/VLCLibraryWindow.h"
#import "library/VLCLibraryWindowSplitViewController.h"
#import "library/VLCLibraryWindowNavigationSidebarViewController.h"

#import "library/audio-library/VLCLibraryAudioDataSource.h"

#import "main/VLCMain.h"

@implementation VLCLibraryHomeViewAudioCarouselContainerView

@synthesize dataSource = _dataSource;

- (NSNotificationName)displayedCollectionChangedNotification
{
    return VLCLibraryAudioDataSourceDisplayedCollectionChangedNotification;
}

- (void)setupDataSource
{
    VLCLibraryAudioDataSource * const audioDataSource = [[VLCLibraryAudioDataSource alloc] init];
    audioDataSource.collectionViewItemIdentifier = VLCLibraryCarouselCollectionViewItemIdentifier;
    self.collectionView.dataSource = audioDataSource;
    audioDataSource.libraryModel = VLCMain.sharedInstance.libraryController.libraryModel;
    audioDataSource.collectionView = self.collectionView;
    [audioDataSource setup];
    _dataSource = audioDataSource;
}

- (VLCAudioLibrarySegment)audioLibrarySegment
{
    return ((VLCLibraryAudioDataSource *)self.dataSource).audioLibrarySegment;
}

- (void)connect
{
    [super connect];
    NSNotificationCenter * const notificationCenter = NSNotificationCenter.defaultCenter;
    for (NSString * const name in @[VLCLibraryModelRecentAudioMediaListReset,
                                   VLCLibraryModelRecentAudioMediaItemDeleted,
                                   VLCLibraryModelAudioMediaItemDeleted]) {
        [notificationCenter addObserver:self selector:@selector(reloadRecentMusic:) name:name object:nil];
    }
}

- (void)disconnect
{
    [super disconnect];
    NSNotificationCenter * const notificationCenter = NSNotificationCenter.defaultCenter;
    for (NSString * const name in @[VLCLibraryModelRecentAudioMediaListReset,
                                   VLCLibraryModelRecentAudioMediaItemDeleted,
                                   VLCLibraryModelAudioMediaItemDeleted]) {
        [notificationCenter removeObserver:self name:name object:nil];
    }
}

- (void)reloadRecentMusic:(NSNotification *)notification
{
    if (self.audioLibrarySegment == VLCAudioLibraryRecentsSegment) {
        [self.dataSource reloadData];
    }
}

- (void)openLibrary:(id)sender
{
    VLCLibrarySegmentType segmentType;
    switch (self.audioLibrarySegment) {
        case VLCAudioLibraryRecentsSegment:
            segmentType = VLCLibraryMusicSegmentType;
            break;
        case VLCAudioLibraryArtistsSegment:
            segmentType = VLCLibraryArtistsMusicSubSegmentType;
            break;
        case VLCAudioLibraryAlbumsSegment:
            segmentType = VLCLibraryAlbumsMusicSubSegmentType;
            break;
        case VLCAudioLibrarySongsSegment:
            segmentType = VLCLibrarySongsMusicSubSegmentType;
            break;
        case VLCAudioLibraryGenresSegment:
            segmentType = VLCLibraryGenresMusicSubSegmentType;
            break;
        default:
            return;
    }
    [VLCMain.sharedInstance.libraryWindow.splitViewController.navSidebarViewController selectSegment:segmentType];
}

- (void)setAudioLibrarySegment:(VLCAudioLibrarySegment)audioLibrarySegment
{
    if (audioLibrarySegment == self.audioLibrarySegment) {
        return;
    }

    switch (audioLibrarySegment) {
        case VLCAudioLibraryRecentsSegment:
            self.headerView.title = _NS("Recent music");
            break;
        case VLCAudioLibraryArtistsSegment:
            self.headerView.title = _NS("Artists");
            break;
        case VLCAudioLibraryAlbumsSegment:
            self.headerView.title = _NS("Albums");
            break;
        case VLCAudioLibrarySongsSegment:
            self.headerView.title = _NS("Songs");
            break;
        case VLCAudioLibraryGenresSegment:
            self.headerView.title = _NS("Genres");
            break;
        case VLCAudioLibraryUnknownSegment:
        default:
            self.headerView.title = _NS("Unknown");
            break;
    }

    ((VLCLibraryAudioDataSource *)self.dataSource).audioLibrarySegment = audioLibrarySegment;
    self.headerView.libraryButton.enabled = audioLibrarySegment != VLCAudioLibraryUnknownSegment;
    [self.collectionView reloadData];
}

@end
