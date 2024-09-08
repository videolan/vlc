/*****************************************************************************
 * VLCLibraryAbstractSegmentViewController.m: MacOS X interface module
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

#import "VLCLibraryAbstractSegmentViewController.h"

#import "library/VLCLibraryDataSource.h"
#import "library/VLCLibraryWindow.h"

@implementation VLCLibraryAbstractSegmentViewController

- (instancetype)initWithLibraryWindow:(VLCLibraryWindow *)libraryWindow
{
    NSParameterAssert(libraryWindow);
    self = [super init];
    if (self) {
        _libraryWindow = libraryWindow;
        _libraryTargetView = libraryWindow.libraryTargetView;
        _emptyLibraryView = libraryWindow.emptyLibraryView;
        _placeholderImageView = libraryWindow.placeholderImageView;
        _placeholderLabel = libraryWindow.placeholderLabel;

        NSNotificationCenter * const notificationCenter = NSNotificationCenter.defaultCenter;
        [notificationCenter addObserver:self
                               selector:@selector(libraryWindowPresentedVideoView:)
                                   name:VLCLibraryWindowEmbeddedVideoViewPresentedNotification
                                 object:nil];
        [notificationCenter addObserver:self
                               selector:@selector(libraryWindowDismissedVideoView:)
                                   name:VLCLibraryWindowEmbeddedVideoViewDismissedNotification
                                 object:nil];
    }
    return self;
}

- (id<VLCLibraryDataSource>)currentDataSource
{
    [self doesNotRecognizeSelector:_cmd];
    return nil;
}

- (void)libraryWindowPresentedVideoView:(NSNotification *)notification
{
    [self.currentDataSource disconnect];
}

- (void)libraryWindowDismissedVideoView:(NSNotification *)notification
{
    [self.currentDataSource connect];
}

@end
