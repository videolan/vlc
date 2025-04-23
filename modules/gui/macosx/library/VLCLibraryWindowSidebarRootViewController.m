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
#import "extensions/NSTextField+VLCAdditions.h"
#import "extensions/NSView+VLCAdditions.h"
#import "extensions/NSWindow+VLCAdditions.h"

#import "main/VLCMain.h"

#import "library/VLCLibraryUIUnits.h"
#import "library/VLCLibraryWindow.h"
#import "library/VLCLibraryWindowChaptersSidebarViewController.h"
#import "library/VLCLibraryWindowPlayQueueSidebarViewController.h"
#import "library/VLCLibraryWindowSidebarChildViewController.h"
#import "library/VLCLibraryWindowTitlesSidebarViewController.h"

#import "playqueue/VLCPlayerController.h"
#import "playqueue/VLCPlayQueueController.h"

#include "views/VLCRoundedCornerTextField.h"

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
    [self setupPlayQueueTitle];
    [self setupCounterLabel];

    self.mainVideoModeEnabled = NO;

    _playQueueSidebarViewController =
        [[VLCLibraryWindowPlayQueueSidebarViewController alloc] initWithLibraryWindow:self.libraryWindow];
    _chaptersSidebarViewController =
        [[VLCLibraryWindowChaptersSidebarViewController alloc] initWithLibraryWindow:self.libraryWindow];
    _titlesSidebarViewController =
        [[VLCLibraryWindowTitlesSidebarViewController alloc] initWithLibraryWindow:self.libraryWindow];

    self.targetView.translatesAutoresizingMaskIntoConstraints = NO;

    if (@available(macOS 10.13, *)) {
        self.viewSelector.segmentDistribution = NSSegmentDistributionFillEqually;
    }

    [self setupViewSelectorSegments];
    [self updateViewSelectorState];
    [self viewSelectorAction:self.viewSelector];

    NSNotificationCenter * const notificationCenter = NSNotificationCenter.defaultCenter;
    [notificationCenter addObserver:self
                           selector:@selector(titleListChanged:)
                               name:VLCPlayerTitleListChanged
                             object:nil];
}

- (void)setupPlayQueueTitle
{
    _playQueueHeaderLabel = [NSTextField defaultLabelWithString:@""];
    self.playQueueHeaderLabel.font = NSFont.VLCLibrarySubsectionHeaderFont;
    self.playQueueHeaderLabel.textColor = NSColor.headerTextColor;

    [self.view addSubview:self.playQueueHeaderLabel];
    _playQueueHeaderTopConstraint = 
        [self.playQueueHeaderLabel.topAnchor constraintEqualToAnchor:self.view.topAnchor
                                                            constant:VLCLibraryUIUnits.smallSpacing];
    [NSLayoutConstraint activateConstraints:@[
        self.playQueueHeaderTopConstraint,
        [self.playQueueHeaderLabel.bottomAnchor constraintEqualToAnchor:self.targetView.topAnchor
                                                               constant:-VLCLibraryUIUnits.smallSpacing],
        [self.playQueueHeaderLabel.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor
                                                                constant:VLCLibraryUIUnits.largeSpacing] 
    ]];
}

- (void)setupViewSelectorSegments
{
    self.viewSelector.segmentCount = 1;
    [self.viewSelector setLabel:self.playQueueSidebarViewController.title
                     forSegment:self.viewSelector.segmentCount - 1];

    VLCPlayQueueController * const playQueueController = VLCMain.sharedInstance.playQueueController;
    VLCPlayerController * const playerController = playQueueController.playerController;
    if (playerController.numberOfTitlesOfCurrentMedia > 0) {
        self.viewSelector.segmentCount++; 
        [self.viewSelector setLabel:self.titlesSidebarViewController.title
                         forSegment:self.viewSelector.segmentCount - 1];
    }
    if (playerController.numberOfChaptersForCurrentTitle > 0) {
        self.viewSelector.segmentCount++;
        [self.viewSelector setLabel:self.chaptersSidebarViewController.title
                         forSegment:self.viewSelector.segmentCount - 1];
    }
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
        [self.counterLabel.centerYAnchor constraintEqualToAnchor:self.playQueueHeaderLabel.centerYAnchor];
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
    [self setupViewSelectorSegments];

    VLCPlayQueueController * const playQueueController = VLCMain.sharedInstance.playQueueController;
    VLCPlayerController * const playerController = playQueueController.playerController;
    const BOOL titlesEnabled = playerController.numberOfTitlesOfCurrentMedia > 0;
    const BOOL chaptersEnabled = playerController.numberOfChaptersForCurrentTitle > 0;
    
    self.viewSelector.hidden = !chaptersEnabled && !titlesEnabled;
    self.topInternalConstraint.active = !self.viewSelector.hidden;

    const NSLayoutPriority playQueueCompressionPriority =
        self.viewSelector.hidden ? NSLayoutPriorityDefaultLow : NSLayoutPriorityRequired;
    self.playQueueHeaderLabel.hidden = chaptersEnabled;
    [self.playQueueHeaderLabel setContentCompressionResistancePriority:playQueueCompressionPriority
                                                        forOrientation:NSLayoutConstraintOrientationVertical];
    self.playQueueHeaderTopConstraint.active = !self.playQueueHeaderLabel.hidden;

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
    if ([selectedSegmentLabel isEqualToString:self.playQueueSidebarViewController.title]) {
        [self setChildViewController:self.playQueueSidebarViewController];
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

    self.playQueueHeaderLabel.stringValue = viewController.title;

    NSView * const view = viewController.view;
    self.targetView.subviews = @[view];
    view.translatesAutoresizingMaskIntoConstraints = NO;
    [view applyConstraintsToFillSuperview];
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
    self.playQueueHeaderTopConstraint.constant = internalTopConstraintConstant;
}

@end
