/*****************************************************************************
 * VLCLibraryMasterDetailViewTableViewDelegate.m MacOS X interface module
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

#import "VLCLibraryMasterDetailViewTableViewDelegate.h"

#import "library/VLCLibraryMasterDetailViewTableViewDataSource.h"
#import "library/VLCLibraryRepresentedItem.h"

#import "library/audio-library/VLCLibraryAudioGroupTableHeaderView.h"

@implementation VLCLibraryMasterDetailViewTableViewDelegate

- (void)tableViewSelectionDidChange:(NSNotification *)notification
{
    NSParameterAssert(notification);
    NSTableView * const tableView = (NSTableView *)notification.object;
    NSAssert(tableView, @"Must be a valid table view");

    if (![tableView.dataSource conformsToProtocol:@protocol(VLCLibraryMasterDetailViewTableViewDataSource)]) {
        return;
    }

    NSObject<VLCLibraryMasterDetailViewTableViewDataSource> * const masterDetailViewDataSource =
        (NSObject<VLCLibraryMasterDetailViewTableViewDataSource> *)tableView.dataSource;

    if (tableView == masterDetailViewDataSource.masterTableView) {
        [masterDetailViewDataSource.detailTableView reloadData];
        [masterDetailViewDataSource updateHeaderInTableView:masterDetailViewDataSource.detailTableView forMasterSelection:tableView];
    }
}

#pragma mark - VLCLibraryGroupHeaderDelegate

- (void)updateHeaderForTableView:(NSTableView *)tableView
             withRepresentedItem:(nullable VLCLibraryRepresentedItem *)representedItem
                   fallbackTitle:(NSString *)fallbackTitle
                  fallbackDetail:(NSString *)fallbackDetail
{
    if (![tableView.dataSource conformsToProtocol:@protocol(VLCLibraryMasterDetailViewTableViewDataSource)]) {
        return;
    }

    NSObject<VLCLibraryMasterDetailViewTableViewDataSource> * const masterDetailViewDataSource =
        (NSObject<VLCLibraryMasterDetailViewTableViewDataSource> *)tableView.dataSource;

    if (tableView != masterDetailViewDataSource.detailTableView) {
        return;
    }

    [self.detailTableHeaderView updateWithRepresentedItem:representedItem
                                            fallbackTitle:fallbackTitle
                                           fallbackDetail:fallbackDetail];
}


@end
