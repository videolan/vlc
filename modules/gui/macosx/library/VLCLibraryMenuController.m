/*****************************************************************************
 * VLCLibraryMenuController.m: MacOS X interface module
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

#import "VLCLibraryMenuController.h"

#import "main/VLCMain.h"
#import "library/VLCLibraryController.h"
#import "library/VLCLibraryDataTypes.h"
#import "library/VLCLibraryInformationPanel.h"

#import "extensions/NSString+Helpers.h"
#import "extensions/NSMenu+VLCAdditions.h"

@interface VLCLibraryMenuController ()
{
    NSMenu *_libraryMenu;
    VLCLibraryInformationPanel *_informationPanel;
}
@end

@implementation VLCLibraryMenuController

- (instancetype)init
{
    self = [super init];
    if (self) {
        [self createMenu];
    }
    return self;
}

- (void)createMenu
{
    NSMenuItem *playItem = [[NSMenuItem alloc] initWithTitle:_NS("Play") action:@selector(play:) keyEquivalent:@""];
    playItem.target = self;
    NSMenuItem *appendItem = [[NSMenuItem alloc] initWithTitle:_NS("Append to Playlist") action:@selector(appendToPlaylist:) keyEquivalent:@""];
    appendItem.target = self;
    NSMenuItem *addItem = [[NSMenuItem alloc] initWithTitle:_NS("Add Media Folder...") action:@selector(addMedia:) keyEquivalent:@""];
    addItem.target = self;
    NSMenuItem *revealItem = [[NSMenuItem alloc] initWithTitle:_NS("Reveal in Finder") action:@selector(revealInFinder:) keyEquivalent:@""];
    revealItem.target = self;
    NSMenuItem *deleteItem = [[NSMenuItem alloc] initWithTitle:_NS("Delete from Library") action:@selector(moveToTrash:) keyEquivalent:@""];
    deleteItem.target = self;
    NSMenuItem *informationItem = [[NSMenuItem alloc] initWithTitle:_NS("Information...") action:@selector(showInformation:) keyEquivalent:@""];
    informationItem.target = self;

    _libraryMenu = [[NSMenu alloc] initWithTitle:@""];
    [_libraryMenu addMenuItemsFromArray:@[playItem, appendItem, revealItem, deleteItem, informationItem, [NSMenuItem separatorItem], addItem]];
}

- (void)popupMenuWithEvent:(NSEvent *)theEvent forView:(NSView *)theView
{
    if (self.representedMediaItem != nil) {
        [NSMenu popUpContextMenu:_libraryMenu withEvent:theEvent forView:theView];
    } else {
        NSMenu *minimalMenu = [[NSMenu alloc] initWithTitle:@""];
        NSMenuItem *addItem = [[NSMenuItem alloc] initWithTitle:_NS("Add Media Folder...") action:@selector(addMedia:) keyEquivalent:@""];
        addItem.target = self;
        [minimalMenu addItem:addItem];
        [NSMenu popUpContextMenu:minimalMenu withEvent:theEvent forView:theView];
    }
}

#pragma mark - actions

- (void)play:(id)sender
{
    [[[VLCMain sharedInstance] libraryController] appendItemToPlaylist:self.representedMediaItem playImmediately:YES];
}

- (void)appendToPlaylist:(id)sender
{
    [[[VLCMain sharedInstance] libraryController] appendItemToPlaylist:self.representedMediaItem playImmediately:NO];
}

- (void)addMedia:(id)sender
{
    NSOpenPanel *openPanel = [NSOpenPanel openPanel];
    [openPanel setCanChooseFiles: NO];
    [openPanel setCanChooseDirectories: YES];
    [openPanel setAllowsMultipleSelection: YES];

    NSModalResponse modalResponse = [openPanel runModal];

    if (modalResponse == NSModalResponseOK) {
        NSArray *URLs = [openPanel URLs];
        NSUInteger count = [URLs count];
        VLCLibraryController *libraryController = [[VLCMain sharedInstance] libraryController];
        for (NSUInteger i = 0; i < count ; i++) {
            NSURL *url = URLs[i];
            [libraryController addFolderWithFileURL:url];
        }
    }
}

- (void)revealInFinder:(id)sender
{
    [[[VLCMain sharedInstance] libraryController] showItemInFinder:self.representedMediaItem];
}

- (void)moveToTrash:(id)sender
{
    NSArray *filesToTrash = self.representedMediaItem.files;
    NSUInteger trashCount = filesToTrash.count;
    NSFileManager *fileManager = [NSFileManager defaultManager];

    for (NSUInteger x = 0; x < trashCount; x++) {
        VLCMediaLibraryFile *fileToTrash = filesToTrash[x];
        [fileManager trashItemAtURL:fileToTrash.fileURL resultingItemURL:nil error:nil];
    }
}

- (void)showInformation:(id)sender
{
    if (!_informationPanel) {
        _informationPanel = [[VLCLibraryInformationPanel alloc] initWithWindowNibName:@"VLCLibraryInformationPanel"];
    }

    [_informationPanel setRepresentedMediaItem:self.representedMediaItem];
    [_informationPanel showWindow:self];
}

@end
