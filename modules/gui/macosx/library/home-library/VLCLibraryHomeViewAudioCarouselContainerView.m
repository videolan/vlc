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
#import "library/VLCLibraryModel.h"

#import "library/audio-library/VLCLibraryAudioDataSource.h"

#import "main/VLCMain.h"

@implementation VLCLibraryHomeViewAudioCarouselContainerView

@synthesize dataSource = _dataSource;

- (void)setup
{
    [super setup];
    [self setupDataSource];
}

- (void)setupDataSource
{
    VLCLibraryAudioDataSource * const audioDataSource = [[VLCLibraryAudioDataSource alloc] init];
    self.carouselView.dataSource = audioDataSource;
    audioDataSource.libraryModel = VLCMain.sharedInstance.libraryController.libraryModel;
    audioDataSource.carouselView = self.carouselView;
    [audioDataSource setup];
    _dataSource = audioDataSource;
}

- (VLCAudioLibrarySegment)audioLibrarySegment
{
    return ((VLCLibraryAudioDataSource *)self.dataSource).audioLibrarySegment;
}

- (void)setAudioLibrarySegment:(VLCAudioLibrarySegment)audioLibrarySegment
{
    if (audioLibrarySegment == self.audioLibrarySegment) {
        return;
    }

    switch (audioLibrarySegment) {
        case VLCAudioLibraryRecentsSegment:
            self.titleView.stringValue = _NS("Recent music");
            break;
        case VLCAudioLibraryArtistsSegment:
            self.titleView.stringValue = _NS("Artists");
            break;
        case VLCAudioLibraryAlbumsSegment:
            self.titleView.stringValue = _NS("Albums");
            break;
        case VLCAudioLibrarySongsSegment:
            self.titleView.stringValue = _NS("Songs");
            break;
        case VLCAudioLibraryGenresSegment:
            self.titleView.stringValue = _NS("Genres");
            break;
        case VLCAudioLibraryUnknownSegment:
        default:
            self.titleView.stringValue = _NS("Unknown");
            break;
    }

    ((VLCLibraryAudioDataSource *)self.dataSource).audioLibrarySegment = audioLibrarySegment;
    [self.carouselView reloadData];
}

@end
