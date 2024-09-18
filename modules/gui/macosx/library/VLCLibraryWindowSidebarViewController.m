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

#import "extensions/NSString+Helpers.h"
#import "extensions/NSWindow+VLCAdditions.h"

#import "library/VLCLibraryUIUnits.h"
#import "library/VLCLibraryWindow.h"
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

    self.mainVideoModeEnabled = NO;

    _playlistSidebarViewController =
        [[VLCLibraryWindowPlaylistSidebarViewController alloc] initWithLibraryWindow:self.libraryWindow];
    _chaptersSidebarViewController =
        [[VLCLibraryWindowChaptersSidebarViewController alloc] initWithLibraryWindow:self.libraryWindow];

    self.targetView.translatesAutoresizingMaskIntoConstraints = NO;

    self.viewSelector.segmentCount = 2;
    [self.viewSelector setLabel:_NS("Playlist") forSegment:0];
    [self.viewSelector setLabel:_NS("Chapters") forSegment:1];
    self.viewSelector.selectedSegment = 0;

    [self viewSelectorAction:self.viewSelector];
}

- (IBAction)viewSelectorAction:(id)sender
{
    NSParameterAssert(sender == self.viewSelector);
    const NSInteger selectedSegment = self.viewSelector.selectedSegment;
    if (selectedSegment == 0) {
        [self setViewOnSidebarTargetView:self.playlistSidebarViewController.view];
    } else if (selectedSegment == 1) {
        [self setViewOnSidebarTargetView:self.chaptersSidebarViewController.view];
    } else {
        NSAssert(NO, @"Invalid or unknown segment selected for sidebar!");
    }
}

- (void)setViewOnSidebarTargetView:(NSView *)view
{
    self.targetView.subviews = @[view];
    view.translatesAutoresizingMaskIntoConstraints = NO;
    [NSLayoutConstraint activateConstraints:@[
        [view.topAnchor constraintEqualToAnchor:self.targetView.topAnchor],
        [view.bottomAnchor constraintEqualToAnchor:self.targetView.bottomAnchor],
        [view.leadingAnchor constraintEqualToAnchor:self.targetView.leadingAnchor],
        [view.trailingAnchor constraintEqualToAnchor:self.targetView.trailingAnchor]
    ]];
}

- (void)setMainVideoModeEnabled:(BOOL)mainVideoModeEnabled
{
    _mainVideoModeEnabled = mainVideoModeEnabled;
    CGFloat internalTopConstraintConstant = VLCLibraryUIUnits.smallSpacing;
    if (!mainVideoModeEnabled && self.libraryWindow.styleMask & NSFullSizeContentViewWindowMask) {
        // Compensate for full content view window's titlebar height, prevent top being cut off
        internalTopConstraintConstant += self.libraryWindow.titlebarHeight;
    }
    self.topInternalConstraint.constant = internalTopConstraintConstant;
}

@end
