/*****************************************************************************
 * VLCBookmarksTableViewDataSource.m: MacOS X interface module bookmarking functionality
 *****************************************************************************
 * Copyright (C) 2023 VLC authors and VideoLAN
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

#import "VLCBookmarksTableViewDelegate.h"

#import "VLCBookmark.h"
#import "VLCBookmarksTableViewDataSource.h"

#import "extensions/NSString+Helpers.h"
#import "extensions/NSView+VLCAdditions.h"

#import "panels/VLCBookmarksWindowController.h"

@interface VLCBookmarksTableViewDelegate ()
{
    VLCBookmarksWindowController* _parentController;
}
@end

@implementation VLCBookmarksTableViewDelegate

- (instancetype)initWithBookmarksWindowController:(VLCBookmarksWindowController *)controller
{
    self = [super init];
    if (self) {
        _parentController = controller;
    }
    return self;
}

- (void)tableViewSelectionDidChange:(NSNotification *)notification
{
    NSParameterAssert(notification);
    NSTableView * const tableView = notification.object;
    NSParameterAssert(tableView);

    const BOOL enableRowDependentBookmarkWindowButtons = tableView.selectedRow >= 0;
    [_parentController toggleRowDependentButtonsEnabled:enableRowDependentBookmarkWindowButtons];
}

@end
