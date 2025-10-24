/*****************************************************************************
 * VLCLibraryHeroView.m: MacOS X interface module
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

#import "VLCLibraryHeroView.h"

#import "extensions/NSString+Helpers.h"
#import "extensions/NSView+VLCAdditions.h"

#import "library/VLCLibraryController.h"
#import "library/VLCLibraryDataTypes.h"
#import "library/VLCLibraryImageCache.h"
#import "library/VLCLibraryMenuController.h"
#import "library/VLCLibraryModel.h"
#import "library/VLCLibraryRepresentedItem.h"

#import "main/VLCMain.h"

#import "views/VLCImageView.h"

@interface VLCLibraryHeroView ()

@property (readonly) VLCLibraryMenuController *menuController;

@end

@implementation VLCLibraryHeroView

+ (instancetype)fromNibWithOwner:(id)owner
{
    return (VLCLibraryHeroView*)[NSView fromNibNamed:@"VLCLibraryHeroView"
                                                     withClass:VLCLibraryHeroView.class
                                                     withOwner:owner];
}

- (void)awakeFromNib
{
    self.largeImageView.contentGravity = VLCImageViewContentGravityResizeAspectFill;
    self.titleTextField.maximumNumberOfLines = 3;
    self.detailTextField.maximumNumberOfLines = 1;
    [self connectItemUpdaters];
}

- (void)updateRepresentedItem
{
    NSAssert(self.representedItem != nil, @"Should not update nil represented item!");
    const id<VLCMediaLibraryItemProtocol> actualItem = self.representedItem.item;
    self.titleTextField.stringValue = actualItem.displayString;
    self.detailTextField.stringValue = actualItem.primaryDetailString;

    __weak typeof(self) weakSelf = self;
    [VLCLibraryImageCache thumbnailForLibraryItem:actualItem withCompletion:^(NSImage * const image) {
        if (!weakSelf || weakSelf.representedItem.item != actualItem) {
            return;
        }
        weakSelf.largeImageView.image = image;
    }];
}

- (void)setRepresentedItem:(VLCLibraryRepresentedItem *)representedItem
{
    NSParameterAssert(representedItem != nil);
    if (representedItem == self.representedItem) {
        return;
    }

    _representedItem = representedItem;
    [self updateRepresentedItem];
}

- (VLCMediaLibraryMediaItem *)randomItem
{
    VLCLibraryModel * const libraryModel = VLCMain.sharedInstance.libraryController.libraryModel;
    NSArray * const videos = libraryModel.listOfVideoMedia;
    const NSInteger videoCount = videos.count;
    if (videoCount == 0) {
        return nil;
    }
    const uint32_t randIdx = arc4random_uniform((uint32_t)(videoCount - 1));
    return [videos objectAtIndex:randIdx];
}

- (VLCMediaLibraryMediaItem *)latestPartiallyPlayedItem
{
    VLCLibraryModel * const libraryModel = VLCMain.sharedInstance.libraryController.libraryModel;
    NSArray<VLCMediaLibraryMediaItem *> * const recentMedia = libraryModel.listOfRecentMedia;
    const NSUInteger firstPartialPlayItemIdx = [recentMedia indexOfObjectPassingTest:^BOOL(VLCMediaLibraryMediaItem *testedItem, NSUInteger __unused idx, BOOL * const __unused stop) {
        const float playProgress = testedItem.progress;
        return playProgress > 0 && playProgress < 100;
    }];

    if (firstPartialPlayItemIdx == NSNotFound) {
        return nil;
    }

    return [recentMedia objectAtIndex:firstPartialPlayItemIdx];
}

- (void)setOptimalRepresentedItem
{
    VLCMediaLibraryMediaItem * const latestPartialPlayItem = self.latestPartiallyPlayedItem;
    if (latestPartialPlayItem != nil) {
        const BOOL isVideo = latestPartialPlayItem.mediaType == VLC_ML_MEDIA_TYPE_VIDEO;
        const VLCMediaLibraryParentGroupType parentType = isVideo ? VLCMediaLibraryParentGroupTypeRecentVideos : VLCMediaLibraryParentGroupTypeAudioLibrary;
        VLCLibraryRepresentedItem * const representedItem = [[VLCLibraryRepresentedItem alloc] initWithItem:latestPartialPlayItem parentType:parentType];

        self.representedItem = representedItem;
        self.explanationTextField.stringValue = _NS("Last watched");
        self.playButton.title = _NS("Resume playing");
        return;
    }

    VLCMediaLibraryMediaItem * const randomItem = self.randomItem;
    if (randomItem != nil) {
        const BOOL isVideo = randomItem.mediaType == VLC_ML_MEDIA_TYPE_VIDEO;
        const VLCMediaLibraryParentGroupType parentType = isVideo ? VLCMediaLibraryParentGroupTypeVideoLibrary : VLCMediaLibraryParentGroupTypeAudioLibrary;
        VLCLibraryRepresentedItem * const representedItem = [[VLCLibraryRepresentedItem alloc] initWithItem:randomItem parentType:parentType];

        self.representedItem = representedItem;
        self.explanationTextField.stringValue = _NS("From your library");
        self.playButton.title = _NS("Play now");
        return;
    }

    NSLog(@"Could not find a good media item for hero view!");
    [self connectForNewVideo];
}

- (IBAction)playRepresentedItem:(id)sender
{
    [self.representedItem play];
}

- (void)connectForNewVideo
{
    NSNotificationCenter * const notificationCenter = NSNotificationCenter.defaultCenter;
    [notificationCenter addObserver:self
                           selector:@selector(newVideosAvailable:)
                               name:VLCLibraryModelVideoMediaListReset
                             object:nil];
    [notificationCenter addObserver:self
                           selector:@selector(newVideosAvailable:)
                               name:VLCLibraryModelRecentsMediaListReset
                             object:nil];
}

- (void)disconnectForNewVideo
{
    NSNotificationCenter * const notificationCenter = NSNotificationCenter.defaultCenter;
    [notificationCenter removeObserver:self name:VLCLibraryModelVideoMediaListReset object:nil];
    [notificationCenter removeObserver:self name:VLCLibraryModelRecentsMediaListReset object:nil];
}

- (void)newVideosAvailable:(NSNotification *)notification
{
    [self setOptimalRepresentedItem];
    [self disconnectForNewVideo];
}

- (void)connectItemUpdaters
{
    NSNotificationCenter * const notificationCenter = NSNotificationCenter.defaultCenter;
    [notificationCenter addObserver:self
                           selector:@selector(itemUpdated:)
                               name:VLCLibraryModelVideoMediaItemUpdated
                             object:nil];
    [notificationCenter addObserver:self
                           selector:@selector(itemUpdated:)
                               name:VLCLibraryModelAudioMediaItemUpdated
                             object:nil];
    [notificationCenter addObserver:self
                           selector:@selector(itemDeleted:)
                               name:VLCLibraryModelVideoMediaItemDeleted
                             object:nil];
    [notificationCenter addObserver:self
                           selector:@selector(itemDeleted:)
                               name:VLCLibraryModelAudioMediaItemDeleted
                             object:nil];
}

- (void)itemUpdated:(NSNotification *)notification
{
    VLCMediaLibraryMediaItem * const mediaItem = notification.object;
    NSAssert(mediaItem != nil, @"Notification should contain a media item!");
    if (mediaItem.libraryID != self.representedItem.item.libraryID) {
        return;
    }

    VLCLibraryRepresentedItem * const item =
        [[VLCLibraryRepresentedItem alloc] initWithItem:mediaItem
                                             parentType:self.representedItem.parentType];
    self.representedItem = item;
}

- (void)itemDeleted:(NSNotification *)notification
{
    VLCMediaLibraryMediaItem * const mediaItem = notification.object;
    NSAssert(mediaItem != nil, @"Notification should contain a media item!");
    if (mediaItem.libraryID != self.representedItem.item.libraryID) {
        return;
    }

    [self setOptimalRepresentedItem];
}

- (void)openContextMenu:(NSEvent *)event
{
    if (self.menuController == nil) {
        _menuController = [[VLCLibraryMenuController alloc] init];
    }

    self.menuController.representedItems = @[self.representedItem];
    [self.menuController popupMenuWithEvent:event forView:self];
}

- (void)mouseDown:(NSEvent *)event
{
    if (event.modifierFlags & NSEventModifierFlagControl) {
        [self openContextMenu:event];
    }

    [super mouseDown:event];
}

- (void)rightMouseDown:(NSEvent *)event
{
    [self openContextMenu:event];
    [super rightMouseDown:event];
}

@end
