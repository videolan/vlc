/*****************************************************************************
 * VLCLibraryWindowPlayQueueSidebarViewController.m: MacOS X interface module
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

#import "VLCLibraryWindowPlayQueueSidebarViewController.h"

#import <QuartzCore/QuartzCore.h>

#import "extensions/NSColor+VLCAdditions.h"
#import "extensions/NSFont+VLCAdditions.h"
#import "extensions/NSImage+VLCAdditions.h"
#import "extensions/NSString+Helpers.h"
#import "extensions/NSWindow+VLCAdditions.h"
#import "library/VLCLibraryUIUnits.h"
#import "library/VLCLibraryWindow.h"
#import "main/VLCMain.h"
#import "playqueue/VLCPlayQueueController.h"
#import "playqueue/VLCPlayQueueDataSource.h"
#import "playqueue/VLCPlayQueueSortingMenuController.h"
#import "views/VLCDragDropView.h"
#import "views/VLCRoundedCornerTextField.h"
#import "windows/VLCOpenWindowController.h"
#import "extensions/NSView+VLCAdditions.h"

@implementation VLCLibraryWindowPlayQueueSidebarViewController
{
    CAGradientLayer *_scrollViewGradientMask;
}

@synthesize counterLabel = _counterLabel;

- (instancetype)initWithLibraryWindow:(VLCLibraryWindow *)libraryWindow
{
    return [super initWithLibraryWindow:libraryWindow
                                nibName:@"VLCLibraryWindowPlayQueueView"];
}

- (void)viewDidLoad
{
    [super viewDidLoad];

    self.dragDropView.dropTarget = self.libraryWindow;

    _playQueueController = VLCMain.sharedInstance.playQueueController;
    _dataSource = [[VLCPlayQueueDataSource alloc] init];
    self.dataSource.playQueueController = self.playQueueController;
    self.dataSource.tableView = self.tableView;
    self.dataSource.dragDropView = self.dragDropView;
    self.dataSource.counterTextField = self.counterLabel;
    [self.dataSource prepareForUse];
    self.playQueueController.playQueueDataSource = self.dataSource;

    self.tableView.dataSource = self.dataSource;
    self.tableView.delegate = self.dataSource;
    self.tableView.rowHeight = VLCLibraryUIUnits.mediumTableViewRowHeight;
    [self.tableView reloadData];

    self.openMediaButton.title = _NS("Open media...");
    self.dragDropImageBackgroundBox.fillColor = NSColor.VLClibrarySeparatorLightColor;

    self.shuffleButton.toolTip = _NS("Shuffle");
    self.repeatButton.toolTip = _NS("Repeat");
    self.sortButton.toolTip = _NS("Sort Play Queue");
    self.clearButton.toolTip = _NS("Clear Play Queue");

    [self repeatStateUpdated:nil];
    [self shuffleStateUpdated:nil];

    NSNotificationCenter * const notificationCenter = NSNotificationCenter.defaultCenter;
    [notificationCenter addObserver:self
                           selector:@selector(shuffleStateUpdated:)
                               name:VLCPlaybackOrderChanged
                             object:nil];
    [notificationCenter addObserver:self
                           selector:@selector(repeatStateUpdated:)
                               name:VLCPlaybackRepeatChanged
                             object:nil];

    [self setupBlurredHeaderFooter];
}

- (void)setupScrollViewGradientMask
{
    const CGFloat gradientHeight = VLCLibraryUIUnits.mediumSpacing;

    _scrollViewGradientMask = [CAGradientLayer layer];
    _scrollViewGradientMask.colors = @[
        (__bridge id)[NSColor colorWithWhite:1.0 alpha:0.0].CGColor,
        (__bridge id)[NSColor colorWithWhite:1.0 alpha:1.0].CGColor
    ];
    _scrollViewGradientMask.startPoint = CGPointMake(0.5, 0.0);
    _scrollViewGradientMask.endPoint = CGPointMake(0.5, 1.0);
    _scrollViewGradientMask.locations = @[@0.0, @(gradientHeight / self.scrollView.bounds.size.height)];

    self.scrollView.wantsLayer = YES;
    self.scrollView.layer.mask = _scrollViewGradientMask;
    _scrollViewGradientMask.frame = self.scrollView.bounds;

    [NSNotificationCenter.defaultCenter addObserver:self
                                           selector:@selector(scrollViewFrameDidChange:)
                                               name:NSViewFrameDidChangeNotification
                                             object:self.scrollView];
}

- (void)scrollViewFrameDidChange:(NSNotification *)notification
{
    if (!_scrollViewGradientMask) {
        return;
    }
    _scrollViewGradientMask.frame = self.scrollView.bounds;
}

- (void)setupBlurredHeaderFooter
{
    const CGFloat footerHeight = self.footerContainerView.frame.size.height;
    [self setupScrollViewGradientMask];

    if (@available(macOS 26.0, *)) {
#if __MAC_OS_X_VERSION_MAX_ALLOWED >= 260000
        [self.bottomButtonsSeparator removeFromSuperview];

        NSGlassEffectView * const glassFooterView = [[NSGlassEffectView alloc] init];
        glassFooterView.translatesAutoresizingMaskIntoConstraints = NO;
        glassFooterView.contentView = self.buttonStack;
        glassFooterView.cornerRadius = CGFLOAT_MAX;

        self.footerContainerView.subviews = @[glassFooterView];
        self.footerContainerView.clipsToBounds = NO;
        [glassFooterView applyConstraintsToFillSuperview];

        self.buttonStack.edgeInsets = NSEdgeInsetsMake(
            VLCLibraryUIUnits.mediumSpacing,
            VLCLibraryUIUnits.largeSpacing,
            VLCLibraryUIUnits.mediumSpacing,
            VLCLibraryUIUnits.largeSpacing
        );

        self.scrollViewDefaultBottomConstraint.active = NO;
        self.footerContainerViewDefaultBottomConstraint.active = NO;
        self.footerContainerViewLeadingConstraint.constant = VLCLibraryUIUnits.largeSpacing;
        self.footerContainerViewTrailingConstraint.constant = VLCLibraryUIUnits.largeSpacing;

        NSLayoutConstraint * const footerBottomConstraint =
            [self.buttonStack.bottomAnchor constraintEqualToAnchor:self.view.bottomAnchor
                                                          constant:-VLCLibraryUIUnits.largeSpacing];

        [NSLayoutConstraint activateConstraints:@[
            [self.scrollView.bottomAnchor constraintEqualToAnchor:self.view.bottomAnchor],
            footerBottomConstraint
        ]];

        const CGFloat footerTopLine = footerHeight + -footerBottomConstraint.constant + VLCLibraryUIUnits.smallSpacing;
        self.scrollView.automaticallyAdjustsContentInsets = NO;
        self.scrollView.contentInsets = NSEdgeInsetsMake(0, 0, footerTopLine, 0);
#endif
    } else if (@available(macOS 10.14, *)) {
        NSVisualEffectView * const footerBlurView = [[NSVisualEffectView alloc] initWithFrame:self.footerContainerView.bounds];
        footerBlurView.translatesAutoresizingMaskIntoConstraints = NO;
        footerBlurView.material = NSVisualEffectMaterialHeaderView;
        footerBlurView.blendingMode = NSVisualEffectBlendingModeWithinWindow;

        [self.footerContainerView addSubview:footerBlurView];
        [footerBlurView applyConstraintsToFillSuperview];

        self.scrollViewDefaultBottomConstraint.active = NO;

        [NSLayoutConstraint activateConstraints:@[
            [self.scrollView.bottomAnchor constraintEqualToAnchor:self.footerContainerView.bottomAnchor]
        ]];

        self.scrollView.automaticallyAdjustsContentInsets = NO;
        self.scrollView.contentInsets = NSEdgeInsetsMake(0, 0, footerHeight, 0);
    }
}

- (NSString *)title
{
    return _NS("Play Queue");
}

- (BOOL)supportsItemCount
{
    return YES;
}

- (void)setCounterLabel:(NSTextField *)counterLabel
{
    _counterLabel = counterLabel;
    self.dataSource.counterTextField = counterLabel;
}

#pragma mark - appearance setters

- (void)updateColorsBasedOnAppearance:(NSAppearance *)appearance
{
    [super updateColorsBasedOnAppearance:appearance];

    BOOL isDark = NO;
    if (@available(macOS 10.14, *)) {
        isDark = [appearance.name isEqualToString:NSAppearanceNameDarkAqua] || 
                 [appearance.name isEqualToString:NSAppearanceNameVibrantDark];
    }

    // If we try to pull the view's effectiveAppearance we are going to get the previous 
    // appearance's name despite responding to the effectiveAppearance change (???) so it is a
    // better idea to pull from the general system theme preference, which is always up-to-date
    if (isDark) {
        self.bottomButtonsSeparator.borderColor = NSColor.VLClibrarySeparatorDarkColor;
        self.dragDropImageBackgroundBox.hidden = NO;
    } else {
        self.bottomButtonsSeparator.borderColor = NSColor.VLClibrarySeparatorLightColor;
        self.dragDropImageBackgroundBox.hidden = YES;
    }
}

#pragma mark - table view interaction

- (IBAction)tableDoubleClickAction:(id)sender
{
    const NSInteger selectedRow = self.tableView.selectedRow;
    if (selectedRow == -1) {
        return;
    }
    [VLCMain.sharedInstance.playQueueController playItemAtIndex:selectedRow];
}

#pragma mark - open media handling

- (IBAction)openMedia:(id)sender
{
    [VLCMain.sharedInstance.open openFileGeneric];
}

#pragma mark - playmode state display and interaction

- (IBAction)shuffleAction:(id)sender
{
    if (_playQueueController.playbackOrder == VLC_PLAYLIST_PLAYBACK_ORDER_NORMAL) {
        _playQueueController.playbackOrder = VLC_PLAYLIST_PLAYBACK_ORDER_RANDOM;
    } else {
        _playQueueController.playbackOrder = VLC_PLAYLIST_PLAYBACK_ORDER_NORMAL;
    }
}

- (void)shuffleStateUpdated:(NSNotification *)aNotification
{
    if (@available(macOS 11.0, *)) {
        self.shuffleButton.image = [NSImage imageWithSystemSymbolName:@"shuffle"
                                             accessibilityDescription:@"Shuffle"];
        self.shuffleButton.contentTintColor =
            self.playQueueController.playbackOrder == VLC_PLAYLIST_PLAYBACK_ORDER_NORMAL ?
                nil : NSColor.VLCAccentColor;
    } else {
        self.shuffleButton.image =
            self.playQueueController.playbackOrder == VLC_PLAYLIST_PLAYBACK_ORDER_NORMAL ?
                [NSImage imageNamed:@"shuffleOff"] :
                [[NSImage imageNamed:@"shuffleOn"] imageTintedWithColor:NSColor.VLCAccentColor];
    }
}

- (IBAction)repeatAction:(id)sender
{
    const enum vlc_playlist_playback_repeat repeatState = self.playQueueController.playbackRepeat;
    switch (repeatState) {
        case VLC_PLAYLIST_PLAYBACK_REPEAT_ALL:
            self.playQueueController.playbackRepeat = VLC_PLAYLIST_PLAYBACK_REPEAT_NONE;
            break;
        case VLC_PLAYLIST_PLAYBACK_REPEAT_CURRENT:
            self.playQueueController.playbackRepeat = VLC_PLAYLIST_PLAYBACK_REPEAT_ALL;
            break;
        default:
            self.playQueueController.playbackRepeat = VLC_PLAYLIST_PLAYBACK_REPEAT_CURRENT;
            break;
    }
}

- (void)repeatStateUpdated:(NSNotification *)aNotification
{
    const enum vlc_playlist_playback_repeat repeatState = self.playQueueController.playbackRepeat;

    if (@available(macOS 11.0, *)) {
        switch (repeatState) {
            case VLC_PLAYLIST_PLAYBACK_REPEAT_CURRENT:
                self.repeatButton.image = [NSImage imageWithSystemSymbolName:@"repeat.1"
                                                    accessibilityDescription:@"Repeat current"];
                self.repeatButton.contentTintColor = NSColor.VLCAccentColor;
                break;
            case VLC_PLAYLIST_PLAYBACK_REPEAT_ALL:
                self.repeatButton.image = [NSImage imageWithSystemSymbolName:@"repeat"
                                                    accessibilityDescription:@"Repeat"];
                self.repeatButton.contentTintColor = NSColor.VLCAccentColor;
                break;
            default:
                self.repeatButton.image = [NSImage imageWithSystemSymbolName:@"repeat"
                                                    accessibilityDescription:@"Repeat"];
                self.repeatButton.contentTintColor = nil;
                break;
        }
    } else {
        switch (repeatState) {
            case VLC_PLAYLIST_PLAYBACK_REPEAT_ALL:
                self.repeatButton.image =
                    [[NSImage imageNamed:@"repeatAll"] imageTintedWithColor:NSColor.VLCAccentColor];
                break;
            case VLC_PLAYLIST_PLAYBACK_REPEAT_CURRENT:
                self.repeatButton.image =
                    [[NSImage imageNamed:@"repeatOne"] imageTintedWithColor:NSColor.VLCAccentColor];
                break;
            default:
                self.repeatButton.image = [NSImage imageNamed:@"repeatOff"];
                break;
        }
    }
}

- (IBAction)sortPlayQueue:(id)sender
{
    if (!self.sortingMenuController) {
        _sortingMenuController = [[VLCPlayQueueSortingMenuController alloc] init];
    }
    [NSMenu popUpContextMenu:self.sortingMenuController.playQueueSortingMenu
                   withEvent:NSApp.currentEvent
                     forView:sender];
}

- (IBAction)clearPlayQueue:(id)sender
{
    [self.playQueueController clearPlayQueue];
}

@end
