/*****************************************************************************
 * VLCLibrarySortingMenuController.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2019 VLC authors and VideoLAN
 *
 * Authors: Felix Paul Kühne <fkuehne # videolan -dot- org>
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

#import "VLCLibrarySortingMenuController.h"

#import "library/VLCLibraryController.h"
#import "extensions/NSString+Helpers.h"
#import "main/VLCMain.h"

#import <vlc_media_library.h>

@interface VLCLibrarySortingMenuController () <NSMenuDelegate>
{
    VLCLibraryController *_libraryController;
    NSMenu *_librarySortingMenu;
}
@end

@implementation VLCLibrarySortingMenuController

- (instancetype)init
{
    self = [super init];
    if (self) {
        [self createMenu];
        _libraryController = [[VLCMain sharedInstance] libraryController];
    }
    return self;
}

- (void)createMenu
{
    _librarySortingMenu = [[NSMenu alloc] init];

    NSArray *titles = @[_NS("Default"),
                        _NS("Alphabetically"),
                        _NS("Duration"),
                        _NS("Insertion Date"),
                        _NS("Last Modification Date"),
                        _NS("Release Date"),
                        _NS("File Size"),
                        _NS("Artist"),
                        _NS("Play Count"),
                        _NS("Album"),
                        _NS("Filename"),
                        _NS("Track Number")];
    NSUInteger count = titles.count;
    for (NSUInteger x = 0; x < count; x++) {
        NSMenuItem *menuItem = [[NSMenuItem alloc] initWithTitle:titles[x] action:@selector(selectSortKey:) keyEquivalent:@""];
        menuItem.target = self;
        menuItem.tag = x;
        [_librarySortingMenu addItem:menuItem];
    }

    [_librarySortingMenu addItem:[NSMenuItem separatorItem]];

    titles = @[_NS("Ascending"),
               _NS("Descending")];
    count = titles.count;
    for (NSUInteger x = 0; x < count; x++) {
        NSMenuItem *menuItem = [[NSMenuItem alloc] initWithTitle:titles[x] action:@selector(selectSortOrder:) keyEquivalent:@""];
        menuItem.target = self;
        menuItem.tag = x + 100;
        [_librarySortingMenu addItem:menuItem];
    }

    _librarySortingMenu.delegate = self;
}

- (void)menuNeedsUpdate:(NSMenu *)menu
{
    if (_libraryController.unsorted) {
        return;
    }
    NSInteger count = _librarySortingMenu.numberOfItems;
    for (NSInteger x = 0; x < count; x++) {
        NSMenuItem *menuItem = [_librarySortingMenu itemAtIndex:x];
        menuItem.state = NSOffState;
    }

    NSMenuItem *menuItem = [_librarySortingMenu itemWithTag:_libraryController.lastSortingCriteria];
    menuItem.state = NSOnState;

    menuItem = [_librarySortingMenu itemWithTag:_libraryController.descendingLibrarySorting + 100];
    menuItem.state = NSOnState;
}

- (void)selectSortKey:(id)sender
{
    enum vlc_ml_sorting_criteria_t sortCriteria = (enum vlc_ml_sorting_criteria_t)[sender tag];
    bool descending;
    if (_libraryController.unsorted) {
        /* we don't have an order and the user can only do a single selection - pick the most popular */
        descending = NO;
    } else {
        descending = YES;
    }

    [_libraryController sortByCriteria:sortCriteria andDescending:descending];
}

- (void)selectSortOrder:(id)sender
{
    enum vlc_ml_sorting_criteria_t sortCriteria = (enum vlc_ml_sorting_criteria_t)[sender tag];
    bool descending = ([sender tag] - 100);
    if (_libraryController.unsorted) {
        /* we don't have a key and the user can only do a single selection - pick the most popular */
        sortCriteria = VLC_ML_SORTING_DEFAULT;
    } else {
        sortCriteria = _libraryController.lastSortingCriteria;
    }

    [_libraryController sortByCriteria:sortCriteria andDescending:descending];
}
@end
