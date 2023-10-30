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
    if(self.clickedRow < 0 || self.dataSource == nil || !_vlcDataSourceConforming) {
        return;
    }

    if([self.dataSource conformsToProtocol:@protocol(VLCLibraryTableViewDataSource)]) {
        enum vlc_ml_parent_type parentType = VLC_ML_PARENT_UNKNOWN;

        if ([self.dataSource isKindOfClass:VLCLibraryAudioDataSource.class]) {
            VLCLibraryAudioDataSource * const audioDataSource = (VLCLibraryAudioDataSource*)self.dataSource;
            parentType = audioDataSource.currentParentType;
        } else if ([self.dataSource isKindOfClass:VLCLibraryAudioGroupDataSource.class]) {
            VLCLibraryAudioGroupDataSource * const audioGroupDataSource = (VLCLibraryAudioGroupDataSource*)self.dataSource;
            parentType = audioGroupDataSource.currentParentType;
        }

        const id<VLCLibraryTableViewDataSource> vlcLibraryDataSource = (id<VLCLibraryTableViewDataSource>)self.dataSource;
        const id<VLCMediaLibraryItemProtocol> mediaLibraryItem = [vlcLibraryDataSource libraryItemAtRow:self.clickedRow
                                                                                           forTableView:self];
        VLCLibraryRepresentedItem * const representedItem = [[VLCLibraryRepresentedItem alloc] initWithItem:mediaLibraryItem parentType:parentType];
        [_menuController setRepresentedItem:representedItem];
    } else if (self.dataSource.class == VLCMediaSourceDataSource.class) {
        VLCMediaSourceDataSource *mediaSourceDataSource = (VLCMediaSourceDataSource*)self.dataSource;
        NSAssert(mediaSourceDataSource != nil, @"This should be a valid pointer");
        VLCInputItem *mediaSourceInputItem = [mediaSourceDataSource mediaSourceInputItemAtRow:self.clickedRow];
        [_menuController setRepresentedInputItem:mediaSourceInputItem];
    }
}

@end
