/*****************************************************************************
 * VLCPlayQueueSortingMenuController.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2002-2019 VLC authors and VideoLAN
 *
 * Authors: Felix Paul Kühne <fkuehne # videolan dot org>
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

#import "VLCPlayQueueSortingMenuController.h"

#import "extensions/NSString+Helpers.h"
#import "main/VLCMain.h"
#import "playqueue/VLCPlayQueueController.h"

@interface VLCPlayQueueSortingMenuController () <NSMenuDelegate>
{
    VLCPlayQueueController *_playQueueController;
}
@end

@implementation VLCPlayQueueSortingMenuController

- (instancetype)init
{
    self = [super init];
    if (self) {
        [self createMenu];
        _playQueueController = VLCMain.sharedInstance.playQueueController;
    }
    return self;
}

- (void)createMenu
{
    _playQueueSortingMenu = [[NSMenu alloc] init];

    NSArray *titles = @[_NS("Title"),
                        _NS("Duration"),
                        _NS("Artist"),
                        _NS("Album"),
                        _NS("Album Artist"),
                        _NS("Genre"),
                        _NS("Date"),
                        _NS("Track Number"),
                        _NS("Disc Number"),
                        _NS("URL"),
                        _NS("Rating")];
    NSUInteger count = titles.count;
    for (NSUInteger x = 0; x < count; x++) {
        NSMenuItem *menuItem = [[NSMenuItem alloc] initWithTitle:titles[x] action:@selector(selectSortKey:) keyEquivalent:@""];
        menuItem.target = self;
        menuItem.tag = x;
        [_playQueueSortingMenu addItem:menuItem];
    }

    [_playQueueSortingMenu addItem:[NSMenuItem separatorItem]];

    titles = @[_NS("Ascending"),
               _NS("Descending")];
    count = titles.count;
    for (NSUInteger x = 0; x < count; x++) {
        NSMenuItem *menuItem = [[NSMenuItem alloc] initWithTitle:titles[x] action:@selector(selectSortOrder:) keyEquivalent:@""];
        menuItem.target = self;
        menuItem.tag = x + 100;
        [_playQueueSortingMenu addItem:menuItem];
    }

    _playQueueSortingMenu.delegate = self;
}

- (void)menuNeedsUpdate:(NSMenu *)menu
{
    if (_playQueueController.unsorted) {
        return;
    }
    NSInteger count = _playQueueSortingMenu.numberOfItems;
    for (NSInteger x = 0; x < count; x++) {
        NSMenuItem *menuItem = [_playQueueSortingMenu itemAtIndex:x];
        menuItem.state = NSOffState;
    }

    NSMenuItem *menuItem = [_playQueueSortingMenu itemWithTag:_playQueueController.lastSortKey];
    menuItem.state = NSOnState;

    menuItem = [_playQueueSortingMenu itemWithTag:_playQueueController.lastSortOrder + 100];
    menuItem.state = NSOnState;
}

- (void)selectSortKey:(id)sender
{
    enum vlc_playlist_sort_key sortKey = (enum vlc_playlist_sort_key)[sender tag];
    enum vlc_playlist_sort_order sortOrder;
    if (_playQueueController.unsorted) {
        /* we don't have an order and the user can only do a single selection - pick the most popular */
        sortOrder = VLC_PLAYLIST_SORT_ORDER_ASCENDING;
    } else {
        sortOrder = _playQueueController.lastSortOrder;
    }

    [_playQueueController sortByKey:sortKey andOrder:sortOrder];
}

- (void)selectSortOrder:(id)sender
{
    enum vlc_playlist_sort_key sortKey;
    enum vlc_playlist_sort_order sortOrder = (enum vlc_playlist_sort_order)([sender tag] - 100);
    if (_playQueueController.unsorted) {
        /* we don't have a key and the user can only do a single selection - pick the most popular */
        sortKey = VLC_PLAYLIST_SORT_KEY_TITLE;
    } else {
        sortKey = _playQueueController.lastSortKey;
    }

    [_playQueueController sortByKey:sortKey andOrder:sortOrder];
}

@end
