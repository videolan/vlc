/*****************************************************************************
 * VLCMediaLibraryFolderManagementController.m: Media Library folder settings
 *****************************************************************************
 * Copyright (C) 2008-2019 VLC authors and VideoLAN
 *
 * Authors: Felix Paul Kühne <fkuehne # videolan dot org>
 *          David Fuhrmann <dfuhrmann # videolan dot org>
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#import "VLCMediaLibraryFolderManagementController.h"

#import "extensions/NSString+Helpers.h"
#import "main/VLCMain.h"
#import "library/VLCLibraryController.h"
#import "library/VLCLibraryModel.h"
#import "library/VLCLibraryDataTypes.h"

@interface VLCMediaLibraryFolderManagementController ()
{
    VLCLibraryController *_libraryController;
    VLCLibraryModel *_libraryModel;
    NSArray<VLCMediaLibraryEntryPoint *> *_cachedFolderList;
}

@end

@implementation VLCMediaLibraryFolderManagementController

- (instancetype)init
{
    self = [super init];
    if (self) {
        _libraryController = VLCMain.sharedInstance.libraryController;
        _libraryModel = _libraryController.libraryModel;
        _cachedFolderList = _libraryModel.listOfMonitoredFolders ?: @[];

        NSNotificationCenter *notificationCenter = NSNotificationCenter.defaultCenter;
        [notificationCenter addObserver:self
                               selector:@selector(listOfMonitoredFoldersUpdated:)
                                   name:VLCLibraryModelListOfMonitoredFoldersUpdated
                                 object:nil];
    }
    return self;
}

- (void)dealloc
{
    [NSNotificationCenter.defaultCenter removeObserver:self];
}

- (void)listOfMonitoredFoldersUpdated:(NSNotification *)aNotification
{
    _cachedFolderList = _libraryModel.listOfMonitoredFolders ?: @[];
    [self.libraryFolderTableView reloadData];
    [self updateFolderButtonState];
}

- (nullable VLCMediaLibraryEntryPoint *)selectedEntryPoint
{
    const NSInteger selectedRow = self.libraryFolderTableView.selectedRow;
    if (selectedRow < 0 || (NSUInteger)selectedRow >= _cachedFolderList.count) {
        return nil;
    }
    return _cachedFolderList[(NSUInteger)selectedRow];
}

- (void)updateFolderButtonState
{
    VLCMediaLibraryEntryPoint * const entryPoint = [self selectedEntryPoint];
    if (entryPoint == nil) {
        self.banFolderButton.enabled = self.removeFolderButton.enabled = self.reloadFolderButton.enabled = NO;
        return;
    }
    self.banFolderButton.enabled = self.removeFolderButton.enabled = self.reloadFolderButton.enabled = YES;
    [self.banFolderButton setTitle:entryPoint.isBanned ? _NS("Unban Folder") : _NS("Ban Folder")];
}

- (IBAction)addFolder:(id)sender
{
    NSOpenPanel *openPanel = [NSOpenPanel openPanel];
    [openPanel setTitle:_NS("Add Folder")];
    [openPanel setCanChooseFiles:NO];
    [openPanel setCanChooseDirectories:YES];
    [openPanel setAllowsMultipleSelection:YES];

    NSModalResponse returnValue = [openPanel runModal];

    if (returnValue == NSModalResponseOK) {
        NSArray *URLs = [openPanel URLs];
        NSUInteger count = [URLs count];
        for (NSUInteger i = 0; i < count ; i++) {
            NSURL *url = URLs[i];
            [_libraryController addFolderWithFileURL:url];
        }
    }
}

- (IBAction)banFolder:(id)sender
{
    VLCMediaLibraryEntryPoint * const entryPoint = [self selectedEntryPoint];
    if (entryPoint == nil) {
        return;
    }
    if (entryPoint.isBanned) {
        [_libraryController unbanFolderWithFileURL:[NSURL URLWithString:entryPoint.MRL]];
    } else {
        [_libraryController banFolderWithFileURL:[NSURL URLWithString:entryPoint.MRL]];
    }
}

- (IBAction)removeFolder:(id)sender
{
    VLCMediaLibraryEntryPoint * const entryPoint = [self selectedEntryPoint];
    if (entryPoint == nil) {
        return;
    }
    [_libraryController removeFolderWithFileURL:[NSURL URLWithString:entryPoint.MRL]];
}

- (IBAction)reloadFolder:(id)sender
{
    VLCMediaLibraryEntryPoint * const entryPoint = [self selectedEntryPoint];
    if (entryPoint == nil) {
        return;
    }
    [_libraryController reloadFolderWithFileURL:[NSURL URLWithString:entryPoint.MRL]];
}

- (NSInteger)numberOfRowsInTableView:(NSTableView *)tableView
{
    return _cachedFolderList.count;
}

- (id)tableView:(NSTableView *)tableView objectValueForTableColumn:(NSTableColumn *)tableColumn row:(NSInteger)row
{
    if (row < 0 || (NSUInteger)row >= _cachedFolderList.count) {
        return @"";
    }

    VLCMediaLibraryEntryPoint * const entryPoint = _cachedFolderList[(NSUInteger)row];
    if (tableColumn == self.nameTableColumn) {
        return [entryPoint.decodedMRL lastPathComponent];
    } else if (tableColumn == self.presentTableColumn) {
        return entryPoint.isPresent ? @"✔" : @"✘";
    } else if (tableColumn == self.bannedTableColumn) {
        return entryPoint.isBanned ? @"✔" : @"✘";
    } else {
        return entryPoint.decodedMRL;
    }
}

- (void)tableViewSelectionDidChange:(NSNotification *)notification
{
    [self updateFolderButtonState];
}

@end
