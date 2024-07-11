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

#import "extensions/NSMenu+VLCAdditions.h"
#import "extensions/NSString+Helpers.h"

#import "library/VLCInputItem.h"
#import "library/VLCLibraryController.h"
#import "library/VLCLibraryRepresentedItem.h"
#import "library/VLCLibrarySegment.h"

#import "main/VLCMain.h"

#import "panels/VLCInformationWindowController.h"

#import "playlist/VLCPlaylistController.h"

#import <vlc_input.h>
#import <vlc_url.h>

@interface VLCLibraryMenuController ()
{
    VLCInformationWindowController *_informationWindowController;

    NSHashTable<NSMenuItem*> *_mediaItemRequiringMenuItems;
    NSHashTable<NSMenuItem*> *_inputItemRequiringMenuItems;
    NSHashTable<NSMenuItem*> *_localInputItemRequiringMenuItems;
    NSHashTable<NSMenuItem*> *_folderInputItemRequiringMenuItems;
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

    NSMenuItem * const bookmarkItem = [[NSMenuItem alloc] initWithTitle:_NS("Toggle Bookmark")
                                                                 action:@selector(toggleBookmark:)
                                                          keyEquivalent:@""];
    bookmarkItem.target = self;

    _libraryMenu = [[NSMenu alloc] initWithTitle:@""];
    [_libraryMenu addMenuItemsFromArray:@[
        playItem,
        appendItem,
        bookmarkItem,
        revealItem,
        deleteItem,
        informationItem,
        [NSMenuItem separatorItem], 
        addItem
    ]];

    _mediaItemRequiringMenuItems = [NSHashTable weakObjectsHashTable];
    [_mediaItemRequiringMenuItems addObject:playItem];
    [_mediaItemRequiringMenuItems addObject:appendItem];
    [_mediaItemRequiringMenuItems addObject:revealItem];
    [_mediaItemRequiringMenuItems addObject:deleteItem];
    [_mediaItemRequiringMenuItems addObject:informationItem];

    _inputItemRequiringMenuItems = [NSHashTable weakObjectsHashTable];
    [_inputItemRequiringMenuItems addObject:playItem];
    [_inputItemRequiringMenuItems addObject:appendItem];

    _localInputItemRequiringMenuItems = [NSHashTable weakObjectsHashTable];
    [_localInputItemRequiringMenuItems addObject:revealItem];
    [_localInputItemRequiringMenuItems addObject:deleteItem];

    _folderInputItemRequiringMenuItems = [NSHashTable weakObjectsHashTable];
    [_folderInputItemRequiringMenuItems addObject:bookmarkItem];
}

- (void)menuItems:(NSHashTable<NSMenuItem*>*)menuItems
        setHidden:(BOOL)hidden
{
    for (NSMenuItem * const menuItem in menuItems) {
        menuItem.hidden = hidden;
    }
}

- (void)updateMenuItems
{
    if (self.representedItems != nil && self.representedItems.count > 0) {
        [self menuItems:_inputItemRequiringMenuItems setHidden:YES];
        [self menuItems:_localInputItemRequiringMenuItems setHidden:YES];
        [self menuItems:_folderInputItemRequiringMenuItems setHidden:YES];
        [self menuItems:_mediaItemRequiringMenuItems setHidden:NO];
    } else if (_representedInputItems != nil && self.representedInputItems.count > 0) {
        [self menuItems:_mediaItemRequiringMenuItems setHidden:YES];
        [self menuItems:_inputItemRequiringMenuItems setHidden:NO];

        BOOL anyStream = NO;
        for (VLCInputItem * const inputItem in self.representedInputItems) {
            if (inputItem.isStream) {
                anyStream = YES;
                break;
            }
        }

        const BOOL bookmarkable =
            self.representedInputItems.count == 1 &&
            self.representedInputItems.firstObject.inputType == ITEM_TYPE_DIRECTORY;

        [self menuItems:_localInputItemRequiringMenuItems setHidden:anyStream];
        [self menuItems:_folderInputItemRequiringMenuItems setHidden:!bookmarkable];
   }
}

- (void)popupMenuWithEvent:(NSEvent *)theEvent forView:(NSView *)theView
{
    [NSMenu popUpContextMenu:_libraryMenu withEvent:theEvent forView:theView];
}

#pragma mark - actions

- (void)addInputItemToPlaylist:(VLCInputItem*)inputItem
               playImmediately:(BOOL)playImmediately
{
    NSParameterAssert(inputItem);
    [VLCMain.sharedInstance.playlistController addInputItem:_representedInputItems.firstObject.vlcInputItem
                                                 atPosition:-1
                                              startPlayback:playImmediately];
}

- (void)play:(id)sender
{
    if (self.representedItems != nil && self.representedItems.count > 0) {
        [self.representedItems.firstObject play];

        if (self.representedItems.count > 1) {
            for (NSUInteger i = 1; i < self.representedItems.count; i++) {
                [self.representedItems[i] queue];
            }
        }

    } else if (self.representedInputItems != nil && self.representedInputItems.count > 0) {
        [self addInputItemToPlaylist:self.representedInputItems.firstObject
                     playImmediately:YES];

        if (self.representedInputItems.count > 1) {
            for (NSUInteger i = 1; i < self.representedInputItems.count; i++) {
                [self addInputItemToPlaylist:self.representedInputItems[i]
                             playImmediately:NO];
            }
        }
    }
}

- (void)appendToPlaylist:(id)sender
{
    if (self.representedItems != nil && self.representedItems.count > 0) {
        for (VLCLibraryRepresentedItem * const item in self.representedItems) {
            [item queue];
        }
    } else if (self.representedInputItems != nil && self.representedInputItems.count > 0) {
        for (VLCInputItem * const inputItem in self.representedInputItems) {
            [self addInputItemToPlaylist:inputItem playImmediately:NO];
        }
    }
}

- (void)addMedia:(id)sender
{
    NSOpenPanel *openPanel = [NSOpenPanel openPanel];
    [openPanel setCanChooseFiles: NO];
    [openPanel setCanChooseDirectories: YES];
    [openPanel setAllowsMultipleSelection: YES];

    NSModalResponse modalResponse = [openPanel runModal];

    if (modalResponse == NSModalResponseOK) {
        VLCLibraryController *libraryController = VLCMain.sharedInstance.libraryController;
        for (NSURL *url in [openPanel URLs]) {
            [libraryController addFolderWithFileURL:url];
        }
    }
}

- (void)revealInFinder:(id)sender
{
    if (self.representedItems != nil && self.representedItems.count > 0) {
        [self.representedItems.firstObject revealInFinder];
    } else if (self.representedInputItems != nil && self.representedInputItems.count > 0) {
        [self.representedInputItems.firstObject revealInFinder];
    }
}

- (void)moveToTrash:(id)sender
{
    if (self.representedItems != nil && self.representedItems.count > 0) {
        for (VLCLibraryRepresentedItem * const item in self.representedItems) {
            [item moveToTrash];
        }
    } else if (self.representedInputItems != nil && self.representedInputItems.count > 0) {
        for (VLCInputItem * const inputItem in self.representedInputItems) {
            [inputItem moveToTrash];
        }
    }
}

- (void)showInformation:(id)sender
{
    if (!_informationWindowController) {
        _informationWindowController = [[VLCInformationWindowController alloc] init];
    }

    if (self.representedItems != nil && self.representedItems.count > 0) {
        [_informationWindowController setRepresentedMediaLibraryItems:self.representedItems];
    } else if (self.representedInputItems != nil && self.representedInputItems.count > 0) {
        _informationWindowController.representedInputItems = self.representedInputItems;
    }

    [_informationWindowController toggleWindow:sender];
}

- (void)toggleBookmark:(id)sender
{
    if (self.representedInputItems == nil || 
        self.representedInputItems.count != 1 ||
        self.representedInputItems.firstObject.inputType != ITEM_TYPE_DIRECTORY) {
        return;
    }

    VLCInputItem * const inputItem = self.representedInputItems.firstObject;
    NSString * const inputItemMRL = inputItem.MRL;
    NSUserDefaults * const defaults = NSUserDefaults.standardUserDefaults;
    NSMutableArray<NSString *> * const bookmarkedLocations =
        [defaults stringArrayForKey:VLCLibraryBookmarkedLocationsKey].mutableCopy;
    NSNotificationCenter * const defaultCenter = NSNotificationCenter.defaultCenter;

    if ([bookmarkedLocations containsObject:inputItemMRL]) {
        [bookmarkedLocations removeObject:inputItemMRL];
    } else {
        [bookmarkedLocations addObject:inputItemMRL];
    }
    [defaults setObject:bookmarkedLocations forKey:VLCLibraryBookmarkedLocationsKey];
    [defaultCenter postNotificationName:VLCLibraryBookmarkedLocationsChanged object:inputItemMRL];
}

- (void)setRepresentedItems:(NSArray<VLCLibraryRepresentedItem *> *)items
{
    _representedItems = items;
    _representedInputItems = nil;
    [self updateMenuItems];
}

- (void)setRepresentedInputItems:(NSArray<VLCInputItem *> *)representedInputItems
{
    _representedInputItems = representedInputItems;
    _representedItems = nil;
    [self updateMenuItems];
}

@end
