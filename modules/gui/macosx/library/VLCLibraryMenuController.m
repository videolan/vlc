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
#import "library/VLCLibraryModel.h"
#import "library/VLCLibraryRepresentedItem.h"
#import "library/VLCLibrarySegment.h"

#import "main/VLCMain.h"

#import "panels/VLCInformationWindowController.h"

#import "playqueue/VLCPlayQueueController.h"

#import <vlc_input.h>
#import <vlc_url.h>
#import <vlc_common.h>

@interface VLCLibraryMenuController ()
{
    VLCInformationWindowController *_informationWindowController;

    NSHashTable<NSMenuItem*> *_mediaItemRequiringMenuItems;
    NSHashTable<NSMenuItem*> *_recentsMediaItemRequiringMenuItems;
    NSHashTable<NSMenuItem*> *_inputItemRequiringMenuItems;
    NSHashTable<NSMenuItem*> *_localInputItemRequiringMenuItems;
    NSHashTable<NSMenuItem*> *_folderInputItemRequiringMenuItems;
    
    NSMenuItem *_deleteItem;
}

@property (readwrite) NSMenuItem *favoriteItem;

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

    NSMenuItem *appendItem = [[NSMenuItem alloc] initWithTitle:_NS("Append to Play Queue") action:@selector(appendToPlayQueue:) keyEquivalent:@""];
    appendItem.target = self;

    NSMenuItem *addItem = [[NSMenuItem alloc] initWithTitle:_NS("Add Media Folder...") action:@selector(addMedia:) keyEquivalent:@""];
    addItem.target = self;

    NSMenuItem *revealItem = [[NSMenuItem alloc] initWithTitle:_NS("Reveal in Finder") action:@selector(revealInFinder:) keyEquivalent:@""];
    revealItem.target = self;

    _deleteItem = [[NSMenuItem alloc] initWithTitle:_NS("Move to Trash") action:@selector(moveToTrash:) keyEquivalent:@""];
    _deleteItem.target = self;

    NSMenuItem *markUnseenItem = [[NSMenuItem alloc] initWithTitle:_NS("Mark as Unseen") action:@selector(markUnseen:) keyEquivalent:@""];
    markUnseenItem.target = self;

    NSMenuItem *informationItem = [[NSMenuItem alloc] initWithTitle:_NS("Information...") action:@selector(showInformation:) keyEquivalent:@""];
    informationItem.target = self;

    NSMenuItem * const bookmarkItem = [[NSMenuItem alloc] initWithTitle:_NS("Toggle Bookmark")
                                                                 action:@selector(toggleBookmark:)
                                                          keyEquivalent:@""];
    bookmarkItem.target = self;

    NSMenuItem * const addToLibraryItem = [[NSMenuItem alloc] initWithTitle:_NS("Add to Media Library")
                                                                      action:@selector(addToMediaLibrary:)
                                                               keyEquivalent:@""];
    addToLibraryItem.target = self;

    _favoriteItem = [[NSMenuItem alloc] initWithTitle:_NS("Toggle Favorite") action:@selector(toggleFavorite:) keyEquivalent:@""];
    self.favoriteItem.target = self;

    NSMenuItem *createPlaylistItem = [[NSMenuItem alloc] initWithTitle:_NS("Create Playlist from Selection") action:@selector(createPlaylistFromSelection:) keyEquivalent:@""];
    createPlaylistItem.target = self;

    _libraryMenu = [[NSMenu alloc] initWithTitle:@""];
    [_libraryMenu addMenuItemsFromArray:@[
        playItem,
        appendItem,
        createPlaylistItem,
        self.favoriteItem,
        bookmarkItem,
        addToLibraryItem,
        revealItem,
        _deleteItem,
        markUnseenItem,
        informationItem,
        [NSMenuItem separatorItem], 
        addItem
    ]];

    _mediaItemRequiringMenuItems = [NSHashTable weakObjectsHashTable];
    [_mediaItemRequiringMenuItems addObject:playItem];
    [_mediaItemRequiringMenuItems addObject:appendItem];
    [_mediaItemRequiringMenuItems addObject:createPlaylistItem];
    [_mediaItemRequiringMenuItems addObject:self.favoriteItem];
    [_mediaItemRequiringMenuItems addObject:revealItem];
    [_mediaItemRequiringMenuItems addObject:_deleteItem];
    [_mediaItemRequiringMenuItems addObject:informationItem];

    _recentsMediaItemRequiringMenuItems = [NSHashTable weakObjectsHashTable];
    [_recentsMediaItemRequiringMenuItems addObject:markUnseenItem];

    _inputItemRequiringMenuItems = [NSHashTable weakObjectsHashTable];
    [_inputItemRequiringMenuItems addObject:playItem];
    [_inputItemRequiringMenuItems addObject:appendItem];

    _localInputItemRequiringMenuItems = [NSHashTable weakObjectsHashTable];
    [_localInputItemRequiringMenuItems addObject:revealItem];
    [_localInputItemRequiringMenuItems addObject:_deleteItem];

    _folderInputItemRequiringMenuItems = [NSHashTable weakObjectsHashTable];
    [_folderInputItemRequiringMenuItems addObject:bookmarkItem];
    [_folderInputItemRequiringMenuItems addObject:addToLibraryItem];
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
    VLCLibraryModel * const libraryModel = VLCMain.sharedInstance.libraryController.libraryModel;
    NSArray<VLCMediaLibraryMediaItem *> * const recents = libraryModel.listOfRecentMedia;

    if (self.representedItems != nil && self.representedItems.count > 0) {
        [self menuItems:_inputItemRequiringMenuItems setHidden:YES];
        [self menuItems:_localInputItemRequiringMenuItems setHidden:YES];
        [self menuItems:_folderInputItemRequiringMenuItems setHidden:YES];
        [self menuItems:_mediaItemRequiringMenuItems setHidden:NO];

        BOOL anyNonRecent = NO;
        for (VLCLibraryRepresentedItem * const item in self.representedItems) {
            if ([recents indexOfObjectPassingTest:^BOOL(VLCMediaLibraryMediaItem * _Nonnull obj, NSUInteger __unused idx, BOOL * __unused stop) {
                return obj.libraryID == item.item.libraryID;
            }] == NSNotFound) {
                anyNonRecent = YES;
                break;
            }
        }
        [self menuItems:_recentsMediaItemRequiringMenuItems setHidden:anyNonRecent];
        
        BOOL anyUnfavorited = NO;
        for (VLCLibraryRepresentedItem * const item in self.representedItems) {
            if (!item.item.favorited) {
                anyUnfavorited = YES;
                break;
            }
        }
        self.favoriteItem.title = anyUnfavorited ? _NS("Add to Favorites") : _NS("Remove from Favorites");
        self.favoriteItem.action = anyUnfavorited ? @selector(addFavorite:) : @selector(removeFavorite:);
        
        // Update delete menu item title based on whether items are file-backed
        BOOL hasFileBacked = NO;
        BOOL hasNonFileBacked = NO;
        
        for (VLCLibraryRepresentedItem * const item in self.representedItems) {
            if (item.item.isFileBacked) {
                hasFileBacked = YES;
            } else {
                hasNonFileBacked = YES;
            }
            if (hasFileBacked && hasNonFileBacked) {
                break;
            }
        }
        
        if (hasFileBacked && hasNonFileBacked) {
            _deleteItem.title = _NS("Move to Trash / Delete from Library");
        } else if (hasFileBacked) {
            _deleteItem.title = _NS("Move to Trash");
        } else {
            _deleteItem.title = _NS("Delete from Library");
        }

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

- (void)addInputItemToPlayQueue:(VLCInputItem*)inputItem
                playImmediately:(BOOL)playImmediately
{
    NSParameterAssert(inputItem);
    [VLCMain.sharedInstance.playQueueController addInputItem:inputItem.vlcInputItem
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
        [self addInputItemToPlayQueue:self.representedInputItems.firstObject
                      playImmediately:YES];

        if (self.representedInputItems.count > 1) {
            for (NSUInteger i = 1; i < self.representedInputItems.count; i++) {
                [self addInputItemToPlayQueue:self.representedInputItems[i]
                              playImmediately:NO];
            }
        }
    }
}

- (void)appendToPlayQueue:(id)sender
{
    if (self.representedItems != nil && self.representedItems.count > 0) {
        for (VLCLibraryRepresentedItem * const item in self.representedItems) {
            [item queue];
        }
    } else if (self.representedInputItems != nil && self.representedInputItems.count > 0) {
        for (VLCInputItem * const inputItem in self.representedInputItems) {
            [self addInputItemToPlayQueue:inputItem playImmediately:NO];
        }
    }
}

- (void)createPlaylistFromSelection:(id)sender
{
    if (self.representedItems == nil || self.representedItems.count == 0) {
        return;
    }
    
    NSMutableArray<VLCMediaLibraryMediaItem *> * const mediaItems = [NSMutableArray arrayWithCapacity:self.representedItems.count];
    for (VLCLibraryRepresentedItem * const representedItem in self.representedItems) {
        [mediaItems addObjectsFromArray:representedItem.item.mediaItems];
    }
    
    if (mediaItems.count > 0) {
        [VLCMain.sharedInstance.libraryController showCreatePlaylistDialogForMediaItems:mediaItems];
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

- (void)addFavorite:(id)sender
{
    [self setItemsFavorite:YES];
}

- (void)removeFavorite:(id)sender
{
    [self setItemsFavorite:NO];
}

- (void)setItemsFavorite:(BOOL)favorite
{
    for (VLCLibraryRepresentedItem * const item in self.representedItems) {
        [item.item setFavorite:favorite];
    }
}

- (void)addToMediaLibrary:(id)sender
{
    if (self.representedInputItems == nil || self.representedInputItems.count == 0) {
        return;
    }
    
    VLCLibraryController * const libraryController = VLCMain.sharedInstance.libraryController;
    
    for (VLCInputItem * const inputItem in self.representedInputItems) {
        if (inputItem.inputType != ITEM_TYPE_DIRECTORY) {
            continue;
        }
        
        NSString * const inputItemMRL = inputItem.MRL;
        NSURL * const folderURL = [NSURL URLWithString:inputItemMRL];
        
        if (folderURL == nil) {
            msg_Warn(getIntf(), "Invalid URL for folder: %s", inputItemMRL.UTF8String);
            continue;
        }
        
        const int result = [libraryController addFolderWithFileURL:folderURL];
        if (result == VLC_SUCCESS) {
            msg_Info(getIntf(), "Added folder to media library: %s", inputItemMRL.UTF8String);
        } else {
            msg_Warn(getIntf(), "Failed to add folder to media library: %s (error %d)", inputItemMRL.UTF8String, result);
        }
    }
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

- (void)markUnseen:(id)sender
{
    vlc_medialibrary_t * const p_ml = vlc_ml_instance_get(getIntf());
    for (VLCLibraryRepresentedItem * const item in self.representedItems) {
        vlc_ml_media_set_played(p_ml, item.item.libraryID, false);
    }
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
