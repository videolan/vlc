/*****************************************************************************
 * VLCLibraryWindowSidebarViewController.m: MacOS X interface module
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

#import "VLCLibraryWindowSidebarViewController.h"

#import "library/VLCLibraryWindowChaptersSidebarViewController.h"
#import "library/VLCLibraryWindowPlaylistSidebarViewController.h"

@implementation VLCLibraryWindowSidebarViewController

- (instancetype)initWithLibraryWindow:(VLCLibraryWindow *)libraryWindow
{
    self = [super initWithNibName:@"VLCLibraryWindowSidebarView" bundle:nil];
    if (self) {
        _libraryWindow = libraryWindow;
    }
    return self;
}

- (void)viewDidLoad
{
    [super viewDidLoad];

    _playlistSidebarViewController =
        [[VLCLibraryWindowPlaylistSidebarViewController alloc] initWithLibraryWindow:self.libraryWindow];
    _chaptersSidebarViewController =
        [[VLCLibraryWindowChaptersSidebarViewController alloc] initWithLibraryWindow:self.libraryWindow];

    [self viewSelectorAction:self.viewSelector];
}

- (IBAction)viewSelectorAction:(id)sender
{
    NSParameterAssert(sender == self.viewSelector);
    const NSInteger selectedSegment = self.viewSelector.selectedSegment;
    if (selectedSegment == 0) {
        self.targetView.subviews = @[self.playlistSidebarViewController.view];
    } else if (selectedSegment == 1) {
        self.targetView.subviews = @[self.chaptersSidebarViewController.view];
    }
    NSAssert(NO, @"Invalid or unknown segment selected for sidebar!");
}

@end
