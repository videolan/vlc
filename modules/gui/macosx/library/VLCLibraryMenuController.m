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
    VLCLibraryInformationPanel *_informationPanel;

    NSHashTable<NSMenuItem*> *_mediaItemRequiringMenuItems;
}
@end

@implementation VLCLibraryMenuController

- (instancetype)init
{
    self = [super init];
    if (self) {
        [self createLibraryMenu];
    }
    return self;
}

- (void)createLibraryMenu
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
    
    _mediaItemRequiringMenuItems = [NSHashTable weakObjectsHashTable];
    [_mediaItemRequiringMenuItems addObject:playItem];
    [_mediaItemRequiringMenuItems addObject:appendItem];
    [_mediaItemRequiringMenuItems addObject:revealItem];
    [_mediaItemRequiringMenuItems addObject:deleteItem];
    [_mediaItemRequiringMenuItems addObject:informationItem];
}

- (void)setMediaItemRequiringMenuItemsHidden:(BOOL)hidden
{
    for (NSMenuItem *menuItem in _mediaItemRequiringMenuItems) {
        menuItem.hidden = hidden;
    }
}

- (void)updateMenuItems
{
    BOOL hideMediaItemMenuItems = _representedItem == nil;
    [self setMediaItemRequiringMenuItemsHidden:hideMediaItemMenuItems];
}

- (void)popupMenuWithEvent:(NSEvent *)theEvent forView:(NSView *)theView
{
    [NSMenu popUpContextMenu:_libraryMenu withEvent:theEvent forView:theView];
}

#pragma mark - actions
- (void)addToPlaylist:(BOOL)playImmediately
{
    if(_representedItem == nil) {
        return;
    }

    // We want to add all the tracks to the playlist but only play the first one immediately,
    // otherwise we will skip straight to the last track of the last album from the artist
    __block BOOL beginPlayImmediately = playImmediately;

    [_representedItem iterateMediaItemsWithBlock:^(VLCMediaLibraryMediaItem* mediaItem) {
        [[[VLCMain sharedInstance] libraryController] appendItemToPlaylist:mediaItem playImmediately:beginPlayImmediately];

        if(beginPlayImmediately) {
            beginPlayImmediately = NO;
        }
    }];
}

- (void)play:(id)sender
{
    [self addToPlaylist:YES];
}

- (void)appendToPlaylist:(id)sender
{
    [self addToPlaylist:NO];
}

- (void)addMedia:(id)sender
{
    NSOpenPanel *openPanel = [NSOpenPanel openPanel];
    [openPanel setCanChooseFiles: NO];
    [openPanel setCanChooseDirectories: YES];
    [openPanel setAllowsMultipleSelection: YES];

    NSModalResponse modalResponse = [openPanel runModal];

    if (modalResponse == NSModalResponseOK) {
        VLCLibraryController *libraryController = [[VLCMain sharedInstance] libraryController];
        for (NSURL *url in [openPanel URLs]) {
            [libraryController addFolderWithFileURL:url];
        }
    }
}

- (void)revealInFinder:(id)sender
{
    if(_representedItem == nil) {
        return;
    }

    VLCMediaLibraryMediaItem *firstMediaItem = _representedItem.firstMediaItem;

    if(firstMediaItem) {
        [[[VLCMain sharedInstance] libraryController] showItemInFinder:firstMediaItem];
    }
}

- (void)moveToTrash:(id)sender
{
    if(_representedItem == nil) {
        return;
    }

    NSFileManager *fileManager = [NSFileManager defaultManager];
    [_representedItem iterateMediaItemsWithBlock:^(VLCMediaLibraryMediaItem* mediaItem) {
        for (VLCMediaLibraryFile *fileToTrash in mediaItem.files) {
            [fileManager trashItemAtURL:fileToTrash.fileURL resultingItemURL:nil error:nil];
        }
    }];
}

- (void)showInformation:(id)sender
{
    if (!_informationPanel) {
        _informationPanel = [[VLCLibraryInformationPanel alloc] initWithWindowNibName:@"VLCLibraryInformationPanel"];
    }

    [_informationPanel setRepresentedItem:_representedItem];
    [_informationPanel showWindow:self];
    
}

- (void)setRepresentedItem:(id<VLCMediaLibraryItemProtocol>)item
{
    _representedItem = item;
    _representedInputItem = nil;
    [self updateMenuItems];
}

- (void)setRepresentedInputItem:(VLCInputItem *)representedInputItem
{
    _representedInputItem = representedInputItem;
    _representedItem = nil;
    [self updateMenuItems];
}

@end
