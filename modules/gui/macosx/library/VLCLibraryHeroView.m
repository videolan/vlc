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

#import "extensions/NSColor+VLCAdditions.h"
#import "extensions/NSFont+VLCAdditions.h"
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
#import "views/VLCCarouselView.h"

@interface VLCLibraryHeroView ()

@property (readonly) VLCLibraryMenuController *menuController;

@end

@implementation VLCLibraryHeroView

- (void)dealloc
{
    [NSNotificationCenter.defaultCenter removeObserver:self];
}

- (void)awakeFromNib
{
    [super awakeFromNib];
    self.largeImageView.contentGravity = VLCImageViewContentGravityResizeAspectFill;
    self.playButton.bezelColor = NSColor.VLCAccentColor;
    self.playButton.font = NSFont.VLCLibrarySubsectionSubheaderFont;
    if (@available(macOS 10.14, *)) {
        self.playButton.contentTintColor = NSColor.VLCAccentColor;
    }
#if __MAC_OS_X_VERSION_MAX_ALLOWED >= 260000
    if (@available(macOS 26.0, *)) {
        self.playButton.bordered = YES;
        self.playButton.bezelStyle = NSBezelStyleGlass;
        self.playButton.borderShape = NSControlBorderShapeCapsule;
    }
#endif
    self.titleTextField.font = NSFont.VLCLibrarySubsectionHeaderFont;
    self.titleTextField.maximumNumberOfLines = 3;
    self.detailTextField.font = NSFont.VLCLibrarySubsectionSubheaderFont;
    self.detailTextField.maximumNumberOfLines = 1;
    [self connectItemUpdaters];
}

- (void)updateRepresentedItem
{
    NSAssert(self.representedItem != nil, @"Should not update nil represented item!");
    const id<VLCMediaLibraryItemProtocol> actualItem = self.representedItem.item;
    self.titleTextField.stringValue = actualItem.displayString;
    self.detailTextField.stringValue = actualItem.primaryDetailString;
    self.explanationTextField.stringValue = self.selectionMode == VLCLibraryHeroViewSelectionModeMostRecentVideo
        ? _NS("Last watched")
        : _NS("From your library");
    VLCMediaLibraryMediaItem * const mediaItem = (id)actualItem;
    self.playButton.title = mediaItem.progress > 0. && mediaItem.progress < 1.
        ? _NS("Resume playing")
        : _NS("Play now");

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
    if (representedItem != nil && representedItem == self.representedItem) {
        return;
    }

    _representedItem = representedItem;
    self.hidden = representedItem == nil;
    self.playButton.enabled = representedItem != nil;
    if (representedItem == nil) {
        self.largeImageView.image = nil;
        self.titleTextField.stringValue = @"";
        self.detailTextField.stringValue = @"";
        self.explanationTextField.stringValue = @"";
        self.playButton.title = @"";
        self.menuController.representedItems = @[];
        return;
    }

    [self updateRepresentedItem];
}

- (void)setSelectionMode:(VLCLibraryHeroViewSelectionMode)selectionMode
{
    if (_selectionMode == selectionMode) {
        return;
    }
    _selectionMode = selectionMode;
    [self setOptimalRepresentedItem];
}

- (VLCMediaLibraryMediaItem *)randomItem
{
    VLCLibraryModel * const libraryModel = VLCMain.sharedInstance.libraryController.libraryModel;
    NSArray * const videos = libraryModel.listOfVideoMedia;
    const NSInteger videoCount = videos.count;
    if (videoCount == 0) {
        return nil;
    }
    const uint32_t randIdx = arc4random_uniform((uint32_t)videoCount);
    return [videos objectAtIndex:randIdx];
}

- (void)setOptimalRepresentedItem
{
    VLCMediaLibraryMediaItem * const mediaItem = self.selectionMode == VLCLibraryHeroViewSelectionModeMostRecentVideo
        ? VLCMain.sharedInstance.libraryController.libraryModel.listOfRecentMedia.firstObject
        : self.randomItem;
    if (mediaItem == nil) {
        self.representedItem = nil;
        return;
    }
    const VLCMediaLibraryParentGroupType parentType = self.selectionMode == VLCLibraryHeroViewSelectionModeMostRecentVideo
        ? VLCMediaLibraryParentGroupTypeRecentVideos
        : VLCMediaLibraryParentGroupTypeVideoLibrary;
    self.representedItem = [[VLCLibraryRepresentedItem alloc] initWithItem:mediaItem parentType:parentType];
}

- (IBAction)playRepresentedItem:(id)sender
{
    [self.representedItem play];
}

- (void)newVideosAvailable:(NSNotification *)notification
{
    if (self.selectionMode == VLCLibraryHeroViewSelectionModeMostRecentVideo || self.representedItem == nil) {
        [self setOptimalRepresentedItem];
        return;
    }
    if ([notification.name isEqualToString:VLCLibraryModelRecentsMediaListReset]) {
        return;
    }

    // Keep a random recommendation stable while the library refreshes. Replace
    // it only when it disappears, while still refreshing its metadata.
    const int64_t selectedID = self.representedItem.item.libraryID;
    NSArray<VLCMediaLibraryMediaItem *> * const videos = VLCMain.sharedInstance.libraryController.libraryModel.listOfVideoMedia;
    if (videos.count == 0) {
        self.representedItem = nil;
        return;
    }
    const NSUInteger selectedIndex = [videos indexOfObjectPassingTest:^BOOL(VLCMediaLibraryMediaItem *item, NSUInteger __unused index, BOOL * __unused stop) {
        return item.libraryID == selectedID;
    }];
    if (selectedIndex == NSNotFound) {
        [self setOptimalRepresentedItem];
    } else {
        self.representedItem = [[VLCLibraryRepresentedItem alloc] initWithItem:videos[selectedIndex]
                                                                    parentType:VLCMediaLibraryParentGroupTypeVideoLibrary];
    }
}

- (void)connectItemUpdaters
{
    NSNotificationCenter * const notificationCenter = NSNotificationCenter.defaultCenter;
    [notificationCenter removeObserver:self];
    for (NSNotificationName name in @[VLCLibraryModelVideoMediaListReset,
                                      VLCLibraryModelRecentsMediaListReset,
                                      VLCLibraryModelAllCachesDropped]) {
        [notificationCenter addObserver:self selector:@selector(newVideosAvailable:) name:name object:nil];
    }
    [notificationCenter addObserver:self
                           selector:@selector(itemUpdated:)
                               name:VLCLibraryModelVideoMediaItemUpdated
                             object:nil];
    [notificationCenter addObserver:self
                           selector:@selector(itemUpdated:)
                               name:VLCLibraryModelRecentsMediaItemUpdated
                             object:nil];
    [notificationCenter addObserver:self
                           selector:@selector(itemDeleted:)
                               name:VLCLibraryModelVideoMediaItemDeleted
                             object:nil];
    [notificationCenter addObserver:self
                           selector:@selector(itemDeleted:)
                               name:VLCLibraryModelRecentsMediaItemDeleted
                             object:nil];
}

- (void)itemUpdated:(NSNotification *)notification
{
    VLCMediaLibraryMediaItem * const mediaItem = notification.object;
    NSAssert(mediaItem != nil, @"Notification should contain a media item!");
    if (self.selectionMode == VLCLibraryHeroViewSelectionModeMostRecentVideo || self.representedItem == nil) {
        [self setOptimalRepresentedItem];
        return;
    }
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
    if (self.selectionMode != VLCLibraryHeroViewSelectionModeMostRecentVideo &&
        mediaItem.libraryID != self.representedItem.item.libraryID) {
        return;
    }

    [self setOptimalRepresentedItem];
}

- (void)openContextMenu:(NSEvent *)event
{
    VLCLibraryRepresentedItem * const representedItem = self.representedItem;
    if (representedItem == nil) {
        return;
    }
    if (self.menuController == nil) {
        _menuController = [[VLCLibraryMenuController alloc] init];
    }

    self.menuController.representedItems = @[representedItem];
    [self.menuController popupMenuWithEvent:event forView:self];
}

- (void)mouseDown:(NSEvent *)event
{
    if (event.modifierFlags & NSEventModifierFlagControl) {
        [self openContextMenu:event];
        return;
    }

    VLCLibraryRepresentedItem * const representedItem = self.representedItem;
    if (representedItem == nil) {
        return;
    }

    VLCCarouselView * const carouselView = self.carouselView;
    NSEvent *mouseUpEvent;
    if (carouselView != nil) {
        [self.window makeFirstResponder:carouselView];
        mouseUpEvent = [carouselView trackMouseDown:event];
    } else {
        mouseUpEvent = [self trackMouseDown:event dragHandler:nil];
    }
    if (mouseUpEvent == nil ||
        self.representedItem == nil ||
        self.representedItem.item.libraryID != representedItem.item.libraryID ||
        !NSPointInRect([self convertPoint:mouseUpEvent.locationInWindow fromView:nil], self.bounds)) {
        return;
    }
    if (mouseUpEvent.modifierFlags & NSEventModifierFlagControl) {
        [self openContextMenu:mouseUpEvent];
        return;
    }
    [self playRepresentedItem:self];
}

- (void)rightMouseDown:(NSEvent *)event
{
    [self openContextMenu:event];
}

@end
