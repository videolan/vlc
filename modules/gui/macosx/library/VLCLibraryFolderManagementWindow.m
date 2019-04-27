/*****************************************************************************
 * VLCLibraryFolderManagementWindow.m: MacOS X interface module
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

#import "VLCLibraryFolderManagementWindow.h"

#import "main/VLCMain.h"
#import "library/VLCLibraryController.h"
#import "library/VLCLibraryModel.h"
#import "library/VLCLibraryDataTypes.h"
#import "extensions/NSString+Helpers.h"

@implementation VLCLibraryFolderManagementWindowController

- (void)windowDidLoad {
    [super windowDidLoad];

    VLCLibraryFolderManagementWindow *window = (VLCLibraryFolderManagementWindow *)self.window;
    [window setTitle:_NS("Media Library")];
    [window.addFolderButton setTitle:_NS("Add Folder...")];
    [window.banFolderButton setTitle:_NS("Ban Folder")];
    [window.removeFolderButton setTitle:_NS("Remove Folder")];
    [window.nameTableColumn setTitle:_NS("Name")];
    [window.presentTableColumn setTitle:_NS("Present")];
    [window.bannedTableColumn setTitle:_NS("Banned")];
    [window.pathTableColumn setTitle:_NS("Location")];
}

@end

@interface VLCLibraryFolderManagementWindow ()
{
    NSArray *_cachedFolderList;
    VLCLibraryController *_libraryController;
}
@end

@implementation VLCLibraryFolderManagementWindow

- (instancetype)initWithContentRect:(NSRect)contentRect styleMask:(NSWindowStyleMask)style backing:(NSBackingStoreType)backingStoreType defer:(BOOL)flag
{
    self = [super initWithContentRect:contentRect styleMask:style backing:backingStoreType defer:flag];
    if (self) {
        _libraryController = [[VLCMain sharedInstance] libraryController];
    }
    return self;
}

- (void)awakeFromNib
{
    self.banFolderButton.enabled = self.removeFolderButton.enabled = NO;
}

- (void)makeKeyAndOrderFront:(id)sender
{
    [super makeKeyAndOrderFront:sender];
    [self.libraryFolderTableView reloadData];
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

        [self.libraryFolderTableView reloadData];
    }
}

- (IBAction)banFolder:(id)sender
{
    VLCMediaLibraryEntryPoint *entryPoint = _cachedFolderList[self.libraryFolderTableView.selectedRow];
    if (entryPoint.isBanned) {
        [_libraryController unbanFolderWithFileURL:[NSURL URLWithString:entryPoint.MRL]];
    } else {
        [_libraryController banFolderWithFileURL:[NSURL URLWithString:entryPoint.MRL]];
    }

    [self.libraryFolderTableView reloadData];
}

- (IBAction)removeFolder:(id)sender
{
    VLCMediaLibraryEntryPoint *entryPoint = _cachedFolderList[self.libraryFolderTableView.selectedRow];
    [_libraryController removeFolderWithFileURL:[NSURL URLWithString:entryPoint.MRL]];

    [self.libraryFolderTableView reloadData];
}

- (NSInteger)numberOfRowsInTableView:(NSTableView *)tableView
{
    if (!_cachedFolderList) {
        _cachedFolderList = [[_libraryController libraryModel] listOfMonitoredFolders];
    }
    return _cachedFolderList.count;
}

- (id)tableView:(NSTableView *)tableView objectValueForTableColumn:(NSTableColumn *)tableColumn row:(NSInteger)row
{
    VLCMediaLibraryEntryPoint *entryPoint = _cachedFolderList[row];
    if (tableColumn == self.nameTableColumn) {
        return [entryPoint.MRL lastPathComponent];
    } else if (tableColumn == self.presentTableColumn) {
        return entryPoint.isPresent ? @"✔" : @"✘";
    } else if (tableColumn == self.bannedTableColumn) {
        return entryPoint.isBanned ? @"✔" : @"✘";
    } else {
        return entryPoint.MRL;
    }
}

- (void)tableViewSelectionDidChange:(NSNotification *)notification
{
    NSInteger selectedRow = self.libraryFolderTableView.selectedRow;
    if (selectedRow == -1) {
        self.banFolderButton.enabled = self.removeFolderButton.enabled = NO;
        return;
    }
    self.banFolderButton.enabled = self.removeFolderButton.enabled = YES;
    VLCMediaLibraryEntryPoint *entryPoint = _cachedFolderList[selectedRow];
    [self.banFolderButton setTitle:entryPoint.isBanned ? _NS("Unban Folder") : _NS("Ban Folder")];
}

@end
