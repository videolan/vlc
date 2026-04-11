/*****************************************************************************
 * VLCLibraryVideoTableViewDelegate.m MacOS X interface module
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

#import "VLCLibraryVideoTableViewDelegate.h"

#import "library/VLCLibraryDataTypes.h"
#import "library/VLCLibraryRepresentedItem.h"
#import "library/VLCLibrarySectionedTableViewDataSource.h"
#import "library/VLCLibraryTableCellView.h"
#import "library/VLCLibraryTableView.h"
#import "library/VLCLibraryUIUnits.h"

#import "library/audio-library/VLCLibraryAudioGroupTableHeaderView.h"

@implementation VLCLibraryVideoTableViewDelegate

- (instancetype)init
{
    self = [super init];
    if (self) {
        self.cellViewIdentifier = @"VLCVideoLibraryTableViewCellIdentifier";
        self.cellViewClass = [VLCLibraryTableCellView class];
    }
    return self;
}

#pragma mark - NSTableViewDelegate

- (NSView *)tableView:(NSTableView *)tableView
    viewForTableColumn:(NSTableColumn *)tableColumn
                   row:(NSInteger)row
{
    if (![tableView.dataSource conformsToProtocol:@protocol(VLCLibrarySectionedTableViewDataSource)]) {
        return [super tableView:tableView viewForTableColumn:tableColumn row:row];
    }

    NSObject<VLCLibrarySectionedTableViewDataSource> * const sectionedDataSource =
        (NSObject<VLCLibrarySectionedTableViewDataSource> *)tableView.dataSource;

    if ([sectionedDataSource isHeaderRow:row]) {
        VLCLibraryAudioGroupTableHeaderView *headerView =
            (VLCLibraryAudioGroupTableHeaderView *)[tableView makeViewWithIdentifier:VLCLibraryAudioGroupTableHeaderViewIdentifier
                                                                               owner:self];
        if (headerView == nil) {
            headerView = [[VLCLibraryAudioGroupTableHeaderView alloc] initWithFrame:NSZeroRect];
            headerView.identifier = VLCLibraryAudioGroupTableHeaderViewIdentifier;
        }

        NSString * const title = [sectionedDataSource titleForRow:row];
        [headerView updateWithRepresentedItem:nil
                                fallbackTitle:title
                               fallbackDetail:nil];
        return headerView;
    }

    return [super tableView:tableView viewForTableColumn:tableColumn row:row];
}

- (CGFloat)tableView:(NSTableView *)tableView heightOfRow:(NSInteger)row
{
    if ([tableView.dataSource conformsToProtocol:@protocol(VLCLibrarySectionedTableViewDataSource)]) {
        NSObject<VLCLibrarySectionedTableViewDataSource> * const sectionedDataSource =
            (NSObject<VLCLibrarySectionedTableViewDataSource> *)tableView.dataSource;
        if ([sectionedDataSource isHeaderRow:row]) {
            return VLCLibraryAudioGroupTableHeaderViewHeight;
        }
    }

    return VLCLibraryUIUnits.mediumTableViewRowHeight;
}

- (BOOL)tableView:(NSTableView *)tableView shouldSelectRow:(NSInteger)row
{
    if ([tableView.dataSource conformsToProtocol:@protocol(VLCLibrarySectionedTableViewDataSource)]) {
        NSObject<VLCLibrarySectionedTableViewDataSource> * const sectionedDataSource =
            (NSObject<VLCLibrarySectionedTableViewDataSource> *)tableView.dataSource;
        return ![sectionedDataSource isHeaderRow:row];
    }
    return YES;
}

- (BOOL)tableView:(NSTableView *)tableView isGroupRow:(NSInteger)row
{
    if ([tableView.dataSource conformsToProtocol:@protocol(VLCLibrarySectionedTableViewDataSource)]) {
        NSObject<VLCLibrarySectionedTableViewDataSource> * const sectionedDataSource =
            (NSObject<VLCLibrarySectionedTableViewDataSource> *)tableView.dataSource;
        return [sectionedDataSource isHeaderRow:row];
    }
    return NO;
}

@end
