/*****************************************************************************
 * VLCLibraryWindowSidebarRootViewController.m: MacOS X interface module
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

#import "VLCLibraryWindowSidebarRootViewController.h"

#import "extensions/NSFont+VLCAdditions.h"
#import "extensions/NSString+Helpers.h"
#import "extensions/NSWindow+VLCAdditions.h"

#import "main/VLCMain.h"

#import "library/VLCLibraryUIUnits.h"
#import "library/VLCLibraryWindow.h"
#import "library/VLCLibraryWindowChaptersSidebarViewController.h"
#import "library/VLCLibraryWindowPlaylistSidebarViewController.h"
#import "library/VLCLibraryWindowSidebarChildViewController.h"
#import "library/VLCLibraryWindowTitlesSidebarViewController.h"

#import "playlist/VLCPlayerController.h"
#import "playlist/VLCPlaylistController.h"

#include "views/VLCRoundedCornerTextField.h"

const NSInteger VLCLibraryWindowSidebarViewPlaylistSegment = 0;
const NSInteger VLCLibraryWindowSidebarViewTitlesSegment = 1;
const NSInteger VLCLibraryWindowSidebarViewChaptersSegment = 2;

@interface VLCLibraryWindowSidebarRootViewController ()

@property (readwrite) NSViewController<VLCLibraryWindowSidebarChildViewController> *currentChildVc;

@end

@implementation VLCLibraryWindowSidebarRootViewController

- (instancetype)initWithLibraryWindow:(VLCLibraryWindow *)libraryWindow
{
    self = [super initWithNibName:@"VLCLibraryWindowSidebarRootView" bundle:nil];
    if (self) {
        _libraryWindow = libraryWindow;
    }
    return self;
}

- (void)viewDidLoad
{
    [super viewDidLoad];
    [self setupPlaylistTitle];
    [self setupCounterLabel];

    self.mainVideoModeEnabled = NO;

    _playlistSidebarViewController =
        [[VLCLibraryWindowPlaylistSidebarViewController alloc] initWithLibraryWindow:self.libraryWindow];
    _chaptersSidebarViewController =
        [[VLCLibraryWindowChaptersSidebarViewController alloc] initWithLibraryWindow:self.libraryWindow];
    _titlesSidebarViewController =
        [[VLCLibraryWindowTitlesSidebarViewController alloc] initWithLibraryWindow:self.libraryWindow];

    self.targetView.translatesAutoresizingMaskIntoConstraints = NO;

    [self setupViewSelector];
    [self updateViewSelectorState];
    [self viewSelectorAction:self.viewSelector];

    NSNotificationCenter * const notificationCenter = NSNotificationCenter.defaultCenter;
    [notificationCenter addObserver:self
                           selector:@selector(titleListChanged:)
                               name:VLCPlayerTitleListChanged
                             object:nil];
}

- (void)setupPlaylistTitle
{
    _playlistHeaderLabel = [[NSTextField alloc] init];
    self.playlistHeaderLabel.translatesAutoresizingMaskIntoConstraints = NO;
    self.playlistHeaderLabel.font = NSFont.VLClibrarySectionHeaderFont;
    self.playlistHeaderLabel.editable = NO;
    self.playlistHeaderLabel.bezeled = NO;
    self.playlistHeaderLabel.drawsBackground = NO;
    self.playlistHeaderLabel.textColor = NSColor.headerTextColor;

    [self.view addSubview:self.playlistHeaderLabel];
    _playlistHeaderTopConstraint = 
        [self.playlistHeaderLabel.topAnchor constraintEqualToAnchor:self.view.topAnchor
                                                           constant:VLCLibraryUIUnits.smallSpacing];
    [NSLayoutConstraint activateConstraints:@[
        self.playlistHeaderTopConstraint,
        [self.playlistHeaderLabel.bottomAnchor constraintEqualToAnchor:self.targetView.topAnchor
                                                              constant:-VLCLibraryUIUnits.smallSpacing],
        [self.playlistHeaderLabel.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor
                                                               constant:VLCLibraryUIUnits.largeSpacing] 
    ]];
}

- (void)setupViewSelector
{
    self.viewSelector.segmentCount = 3;
    [self.viewSelector setLabel:self.playlistSidebarViewController.title
                     forSegment:VLCLibraryWindowSidebarViewPlaylistSegment];
    [self.viewSelector setLabel:self.titlesSidebarViewController.title
                     forSegment:VLCLibraryWindowSidebarViewTitlesSegment];
    [self.viewSelector setLabel:self.chaptersSidebarViewController.title
                     forSegment:VLCLibraryWindowSidebarViewChaptersSegment];
    self.viewSelector.selectedSegment = VLCLibraryWindowSidebarViewPlaylistSegment;
}

- (void)setupCounterLabel
{
    _counterLabel = [[VLCRoundedCornerTextField alloc] init];
    self.counterLabel.useStrongRounding = YES;
    self.counterLabel.translatesAutoresizingMaskIntoConstraints = NO;
    [self.counterLabel setContentCompressionResistancePriority:NSLayoutPriorityRequired
                                                 forOrientation:NSLayoutConstraintOrientationHorizontal];
    [self.counterLabel setContentCompressionResistancePriority:NSLayoutPriorityRequired
                                                 forOrientation:NSLayoutConstraintOrientationVertical];

    [self.view addSubview:self.counterLabel];

    _counterLabelInHeaderConstraint = 
        [self.counterLabel.centerYAnchor constraintEqualToAnchor:self.playlistHeaderLabel.centerYAnchor];
    _counterLabelInChildViewConstraint =
        [self.counterLabel.topAnchor constraintEqualToAnchor:self.targetView.topAnchor
                                                    constant:VLCLibraryUIUnits.smallSpacing];
    
    [NSLayoutConstraint activateConstraints:@[
        self.counterLabelInHeaderConstraint,
        [self.counterLabel.trailingAnchor constraintEqualToAnchor:self.view.trailingAnchor
                                                         constant:-VLCLibraryUIUnits.largeSpacing]
    ]];
}

- (void)titleListChanged:(NSNotification *)notification
{
    [self updateViewSelectorState];
}

- (void)updateViewSelectorState
{
    VLCPlaylistController * const playlistController = VLCMain.sharedInstance.playlistController;
    VLCPlayerController * const playerController = playlistController.playerController;
    const BOOL titlesEnabled = playerController.numberOfTitlesOfCurrentMedia > 0;
    const BOOL chaptersEnabled = playerController.numberOfChaptersForCurrentTitle > 0;
    
    self.viewSelector.hidden = !chaptersEnabled && !titlesEnabled;
    self.topInternalConstraint.active = !self.viewSelector.hidden;

    const NSLayoutPriority playlistCompressionPriority =
        self.viewSelector.hidden ? NSLayoutPriorityDefaultLow : NSLayoutPriorityRequired;
    self.playlistHeaderLabel.hidden = chaptersEnabled;
    [self.playlistHeaderLabel setContentCompressionResistancePriority:playlistCompressionPriority
                                                       forOrientation:NSLayoutConstraintOrientationVertical];
    self.playlistHeaderTopConstraint.active = !self.playlistHeaderLabel.hidden;

    NSLayoutConstraint * const counterLabelConstraintToActivate = self.viewSelector.hidden
        ? self.counterLabelInHeaderConstraint
        : self.counterLabelInChildViewConstraint;
    NSLayoutConstraint * const counterLabelConstraintToDeactivate = self.viewSelector.hidden
        ? self.counterLabelInChildViewConstraint
        : self.counterLabelInHeaderConstraint;
    counterLabelConstraintToActivate.active = YES;
    counterLabelConstraintToDeactivate.active = NO;
    
    NSString * const selectedSegmentLabel = [self.viewSelector labelForSegment:self.viewSelector.selectedSegment];
    if ((!chaptersEnabled && [selectedSegmentLabel isEqualToString:self.chaptersSidebarViewController.title]) ||
        (!titlesEnabled && [selectedSegmentLabel isEqualToString:self.titlesSidebarViewController.title])) {
        self.viewSelector.selectedSegment = 0;
        [self viewSelectorAction:self.viewSelector];
    }
}

- (IBAction)viewSelectorAction:(id)sender
{
    NSParameterAssert(sender == self.viewSelector);
    const NSInteger selectedSegment = self.viewSelector.selectedSegment;
    NSString * const selectedSegmentLabel = [self.viewSelector labelForSegment:selectedSegment];
    if ([selectedSegmentLabel isEqualToString:self.playlistSidebarViewController.title]) {
        [self setChildViewController:self.playlistSidebarViewController];
    } else if ([selectedSegmentLabel isEqualToString:self.titlesSidebarViewController.title]) {
        [self setChildViewController:self.titlesSidebarViewController];
    } else if ([selectedSegmentLabel isEqualToString:self.chaptersSidebarViewController.title]) {
        [self setChildViewController:self.chaptersSidebarViewController];
    } else {
        NSAssert(NO, @"Invalid or unknown segment selected for sidebar!");
    }
}

- (void)setChildViewController:(NSViewController<VLCLibraryWindowSidebarChildViewController> *)viewController
{
    self.currentChildVc.counterLabel = nil; // Stop old vc manipulating the counter label
    self.currentChildVc = viewController;
    
    self.counterLabel.hidden = !viewController.supportsItemCount;
    if (viewController.supportsItemCount) {
        viewController.counterLabel = self.counterLabel;
    }

    self.playlistHeaderLabel.stringValue = viewController.title;

    NSView * const view = viewController.view;
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
    [self updateTopConstraints];
}

- (void)updateTopConstraints
{
    CGFloat internalTopConstraintConstant = VLCLibraryUIUnits.smallSpacing;
    if (!self.mainVideoModeEnabled && self.libraryWindow.styleMask & NSFullSizeContentViewWindowMask) {
        // Compensate for full content view window's titlebar height, prevent top being cut off
        internalTopConstraintConstant += self.libraryWindow.titlebarHeight;
    }
    self.topInternalConstraint.constant = internalTopConstraintConstant;
    self.playlistHeaderTopConstraint.constant = internalTopConstraintConstant;
}

@end
