/*****************************************************************************
 * VLCLibraryTableView.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2022 VLC authors and VideoLAN
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

#import "VLCLibraryTableView.h"

#import "library/VLCLibraryDataTypes.h"
#import "library/VLCLibraryMenuController.h"
#import "library/VLCLibraryRepresentedItem.h"

#import "library/audio-library/VLCLibraryAudioDataSource.h"
#import "library/audio-library/VLCLibraryAudioGroupDataSource.h"

#import "media-source/VLCMediaSourceDataSource.h"

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

    if([self.dataSource conformsToProtocol:@protocol(VLCLibraryTableViewDataSource)] ||
       self.dataSource.class == VLCMediaSourceDataSource.class) {
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
    NSIndexSet * const indices = self.selectedRowIndexes;
    if (indices.count == 0 || self.dataSource == nil || !_vlcDataSourceConforming) {
        return;
    }

    if([self.dataSource conformsToProtocol:@protocol(VLCLibraryTableViewDataSource)]) {
        NSMutableArray<VLCLibraryRepresentedItem *> * const representedItems = 
            NSMutableArray.array;
        const id<VLCLibraryTableViewDataSource> vlcLibraryDataSource = 
            (id<VLCLibraryTableViewDataSource>)self.dataSource;

        [indices enumerateIndexesUsingBlock:^(const NSUInteger index, BOOL * const stop) {
            const id<VLCMediaLibraryItemProtocol> mediaItem = 
                [vlcLibraryDataSource libraryItemAtRow:index forTableView:self];
            const VLCMediaLibraryParentGroupType parentType = 
                vlcLibraryDataSource.currentParentType;
            VLCLibraryRepresentedItem * const representedItem = 
                [[VLCLibraryRepresentedItem alloc] initWithItem:mediaItem parentType:parentType];
            [representedItems addObject:representedItem];
        }];
        _menuController.representedItems = representedItems;

    } else if (self.dataSource.class == VLCMediaSourceDataSource.class) {
        NSMutableArray<VLCInputItem *> * const mediaSourceInputItems = NSMutableArray.array;
        VLCMediaSourceDataSource * const mediaSourceDataSource = 
            (VLCMediaSourceDataSource*)self.dataSource;
        NSAssert(mediaSourceDataSource != nil, @"This should be a valid pointer");

        [indices enumerateIndexesUsingBlock:^(const NSUInteger index, BOOL * const stop) {
            VLCInputItem * const mediaSourceInputItem = 
                [mediaSourceDataSource mediaSourceInputItemAtRow:index];
            [mediaSourceInputItems addObject:mediaSourceInputItem];
        }];
        _menuController.representedInputItems = mediaSourceInputItems;
    }
}

@end
