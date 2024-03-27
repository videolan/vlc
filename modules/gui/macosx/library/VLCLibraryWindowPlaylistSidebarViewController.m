/*****************************************************************************
 * VLCLibraryWindowPlaylistSidebarViewController.m: MacOS X interface module
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

#import "VLCLibraryWindowPlaylistSidebarViewController.h"

#import "extensions/NSColor+VLCAdditions.h"
#import "extensions/NSFont+VLCAdditions.h"
#import "extensions/NSString+Helpers.h"
#import "extensions/NSWindow+VLCAdditions.h"
#import "library/VLCLibraryUIUnits.h"
#import "library/VLCLibraryWindow.h"
#import "main/VLCMain.h"
#import "playlist/VLCPlaylistController.h"
#import "playlist/VLCPlaylistDataSource.h"
#import "views/VLCDragDropView.h"
#import "views/VLCRoundedCornerTextField.h"

@implementation VLCLibraryWindowPlaylistSidebarViewController

- (instancetype)initWithLibraryWindow:(VLCLibraryWindow *)libraryWindow
{
    self = [super initWithNibName:@"VLCLibraryWindowPlaylistView" bundle:nil];
    if (self) {
        _libraryWindow = libraryWindow;
    }
    return self;
}

- (void)viewDidLoad
{
    if (self.libraryWindow.styleMask & NSFullSizeContentViewWindowMask) {
        // Compensate for full content view window's titlebar height, prevent top being cut off
        self.topInternalConstraint.constant =
            self.libraryWindow.titlebarHeight + VLCLibraryUIUnits.mediumSpacing;
    }

    self.dragDropView.dropTarget = self.libraryWindow;
    self.counterTextField.useStrongRounding = YES;
    self.counterTextField.font = [NSFont boldSystemFontOfSize:NSFont.systemFontSize];
    self.counterTextField.textColor = NSColor.VLClibraryAnnotationColor;
    self.counterTextField.hidden = YES;

    _playlistController = VLCMain.sharedInstance.playlistController;
    _dataSource = [[VLCPlaylistDataSource alloc] init];
    self.dataSource.playlistController = self.playlistController;
    self.dataSource.tableView = self.tableView;
    self.dataSource.dragDropView = self.dragDropView;
    self.dataSource.counterTextField = self.counterTextField;
    [self.dataSource prepareForUse];
    self.playlistController.playlistDataSource = self.dataSource;

    self.tableView.dataSource = self.dataSource;
    self.tableView.delegate = self.dataSource;
    self.tableView.rowHeight = VLCLibraryUIUnits.mediumTableViewRowHeight;
    [self.tableView reloadData];

    self.titleLabel.font = NSFont.VLClibrarySectionHeaderFont;
    self.titleLabel.stringValue = _NS("Playlist");
    self.openMediaButton.title = _NS("Open media...");
    self.dragDropImageBackgroundBox.fillColor = NSColor.VLClibrarySeparatorLightColor;

    [self updateColorsBasedOnAppearance:self.view.effectiveAppearance];

    if (@available(macOS 10.14, *)) {
        [NSApplication.sharedApplication addObserver:self
                                          forKeyPath:@"effectiveAppearance"
                                             options:NSKeyValueObservingOptionNew
                                             context:nil];
    }
}

#pragma mark - appearance setters

- (void)observeValueForKeyPath:(NSString *)keyPath
                      ofObject:(id)object
                        change:(NSDictionary<NSKeyValueChangeKey,id> *)change
                       context:(void *)context
{
    if ([keyPath isEqualToString:@"effectiveAppearance"]) {
        NSAppearance * const effectiveAppearance = change[NSKeyValueChangeNewKey];
        [self updateColorsBasedOnAppearance:effectiveAppearance];
    }
}

- (void)updateColorsBasedOnAppearance:(NSAppearance *)appearance
{
    NSParameterAssert(appearance);
    BOOL isDark = NO;
    if (@available(macOS 10.14, *)) {
        isDark = [appearance.name isEqualToString:NSAppearanceNameDarkAqua] || 
                 [appearance.name isEqualToString:NSAppearanceNameVibrantDark];
    }

    // If we try to pull the view's effectiveAppearance we are going to get the previous 
    // appearance's name despite responding to the effectiveAppearance change (???) so it is a
    // better idea to pull from the general system theme preference, which is always up-to-date
    if (isDark) {
        self.titleLabel.textColor = NSColor.VLClibraryDarkTitleColor;
        self.titleSeparator.borderColor = NSColor.VLClibrarySeparatorDarkColor;
        self.bottomButtonsSeparator.borderColor = NSColor.VLClibrarySeparatorDarkColor;
        self.dragDropImageBackgroundBox.hidden = NO;
    } else {
        self.titleLabel.textColor = NSColor.VLClibraryLightTitleColor;
        self.titleSeparator.borderColor = NSColor.VLClibrarySeparatorLightColor;
        self.bottomButtonsSeparator.borderColor = NSColor.VLClibrarySeparatorLightColor;
        self.dragDropImageBackgroundBox.hidden = YES;
    }
}

@end
