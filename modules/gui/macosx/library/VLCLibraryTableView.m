/*****************************************************************************
 * VLCLibraryTableView.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2022 VLC authors and VideoLAN
 *
 * Authors: Claudio Cambra <claudio.cambra@gmail.com>
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

#import "VLCLibraryTableView.h"
#import <library/VLCLibraryDataTypes.h>
#import "library/VLCLibraryMenuController.h"

@interface VLCLibraryTableView ()
{
    VLCLibraryMenuController *_menuController;
    BOOL _vlcDataSourceConforming;
}
@end

@implementation VLCLibraryTableView

- (void)setupMenu
{
    if(_menuController == nil) {
        _menuController = [[VLCLibraryMenuController alloc] init];
        _menuController.libraryMenu.delegate = self;
    }

    self.menu = _menuController.libraryMenu;
}

- (void)setDataSource:(id<NSTableViewDataSource>)dataSource
{
    [super setDataSource:dataSource];

    if([self.dataSource conformsToProtocol:@protocol(VLCLibraryTableViewDataSource)]) {
        _vlcDataSourceConforming = YES;
        [self setupMenu];
    } else {
        _vlcDataSourceConforming = NO;
        self.menu = nil;
        _menuController = nil;
    }
}

#pragma mark tableview menu delegates

- (void)menuNeedsUpdate:(NSMenu *)menu
{
    if(self.clickedRow < 0 || self.dataSource == nil || !_vlcDataSourceConforming) {
        return;
    }

    [_menuController setRepresentedItem:[(id<VLCLibraryTableViewDataSource>)self.dataSource libraryItemAtRow:self.clickedRow forTableView:self]];
}

@end
