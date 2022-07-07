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
    enum vlc_ml_parent_type _currentRepresentedType;
    VLCMediaLibraryMediaItem *_representedMediaItem;
    VLCMediaLibraryAlbum *_representedAlbum;
    VLCMediaLibraryArtist *_representedArtist;
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
    if (_representedMediaItem != nil) {
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
- (void)addToPlaylist:(BOOL)playImmediately
{
    void (^mediaItemPlaylistHandler)(VLCMediaLibraryMediaItem*) = ^(VLCMediaLibraryMediaItem* mediaItem) {
        [[[VLCMain sharedInstance] libraryController] appendItemToPlaylist:mediaItem playImmediately:playImmediately];
    };

    switch(_currentRepresentedType) {
        case VLC_ML_PARENT_ARTIST:
        {
            [_representedArtist iterateMediaItemsWithBlock:mediaItemPlaylistHandler];
            break;
        }
        case VLC_ML_PARENT_ALBUM:
        {
            [_representedAlbum iterateMediaItemsWithBlock:mediaItemPlaylistHandler];
            break;
        }
        case VLC_ML_PARENT_UNKNOWN:
        {
            mediaItemPlaylistHandler(_representedMediaItem);
            break;
        }
        default:
            NSLog(@"No represented media type, cannot append nothing to playlist.");
    }
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
    switch(_currentRepresentedType) {
        case VLC_ML_PARENT_ARTIST:
        {
            [[[VLCMain sharedInstance] libraryController] showItemInFinder:_representedArtist.albums.firstObject.tracksAsMediaItems.firstObject];
            break;
        }
        case VLC_ML_PARENT_ALBUM:
        {
            [[[VLCMain sharedInstance] libraryController] showItemInFinder:_representedAlbum.tracksAsMediaItems.firstObject];
            break;
        }
        case VLC_ML_PARENT_UNKNOWN:
        {
            [[[VLCMain sharedInstance] libraryController] showItemInFinder:_representedMediaItem];
            break;
        }
        default:
            NSLog(@"No represented media type, nothing to show in Finder.");
    }
}

- (void)moveToTrash:(id)sender
{
    NSFileManager *fileManager = [NSFileManager defaultManager];

    void (^mediaItemTrashHandler)(VLCMediaLibraryMediaItem*) = ^(VLCMediaLibraryMediaItem* mediaItem) {
        for (VLCMediaLibraryFile *fileToTrash in mediaItem.files) {
            [fileManager trashItemAtURL:fileToTrash.fileURL resultingItemURL:nil error:nil];
        }
    };

    switch(_currentRepresentedType) {
        case VLC_ML_PARENT_ARTIST:
        {
            [_representedArtist iterateMediaItemsWithBlock:mediaItemTrashHandler];
            break;
        }
        case VLC_ML_PARENT_ALBUM:
        {
            [_representedAlbum iterateMediaItemsWithBlock:mediaItemTrashHandler];
            break;
        }
        case VLC_ML_PARENT_UNKNOWN:
        {
            mediaItemTrashHandler(_representedMediaItem);
            break;
        }
        default:
            NSLog(@"No represented media type, not moving anything to trash.");
            return;
    }
}

- (void)showInformation:(id)sender
{
    if (!_informationPanel) {
        _informationPanel = [[VLCLibraryInformationPanel alloc] initWithWindowNibName:@"VLCLibraryInformationPanel"];
    }

    if(_currentRepresentedType == VLC_ML_PARENT_UNKNOWN) {
        [_informationPanel setRepresentedMediaItem:_representedMediaItem];
        [_informationPanel showWindow:self];
    }
    
}

- (void)clearRepresentedMedia
{
    _representedMediaItem = nil;
    _representedAlbum = nil;
    _representedArtist = nil;
    _currentRepresentedType = VLC_ML_PARENT_UNKNOWN;
}

- (void)setRepresentedMediaItem:(VLCMediaLibraryMediaItem *)mediaItem
{
    [self clearRepresentedMedia];
    _representedMediaItem = mediaItem;
    _currentRepresentedType = VLC_ML_PARENT_UNKNOWN;
}

- (void)setRepresentedAlbum:(VLCMediaLibraryAlbum *)album
{
    [self clearRepresentedMedia];
    _representedAlbum = album;
    _currentRepresentedType = VLC_ML_PARENT_ALBUM;
}

- (void)setRepresentedArtist:(VLCMediaLibraryArtist *)artist
{
    [self clearRepresentedMedia];
    _representedArtist = artist;
    _currentRepresentedType = VLC_ML_PARENT_ARTIST;
}

@end
