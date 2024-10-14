/*****************************************************************************
 * VLCLibraryWindowAbstractSidebarViewController.m: MacOS X interface module
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

#import "VLCLibraryWindowAbstractSidebarViewController.h"

#import "extensions/NSColor+VLCAdditions.h"
#import "extensions/NSFont+VLCAdditions.h"

#import "library/VLCLibraryUIUnits.h"
#import "library/VLCLibraryWindow.h"

@implementation VLCLibraryWindowAbstractSidebarViewController

@synthesize supportsItemCount = _supportsItemCount;

- (instancetype)initWithLibraryWindow:(VLCLibraryWindow *)libraryWindow
                              nibName:(NSString *)nibName
{
    self = [super initWithNibName:nibName bundle:nil];
    if (self) {
        _libraryWindow = libraryWindow;
    }
    return self;
}

- (void)viewDidLoad
{
    [super viewDidLoad];
    [self updateColorsBasedOnAppearance:self.view.effectiveAppearance];

    if (@available(macOS 10.14, *)) {
        [NSApplication.sharedApplication addObserver:self
                                          forKeyPath:@"effectiveAppearance"
                                             options:NSKeyValueObservingOptionNew
                                             context:nil];
    }
}

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
        self.titleSeparator.borderColor = NSColor.VLClibrarySeparatorDarkColor;
    } else {
        self.titleSeparator.borderColor = NSColor.VLClibrarySeparatorLightColor;
    }
}

@end
