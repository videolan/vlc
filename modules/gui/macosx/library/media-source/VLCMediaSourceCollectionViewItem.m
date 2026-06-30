/*****************************************************************************
 * VLCMediaSourceCollectionViewItem.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2026 VLC authors and VideoLAN
 *
 * Authors: Felix Paul Kühne <fkuehne # videolan -dot- org>
 *          Claudio Cambra <developer@claudiocambra.com>
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

#import "VLCMediaSourceCollectionViewItem.h"

#import "extensions/NSString+Helpers.h"

#import "library/VLCInputItem.h"
#import "library/VLCLibraryImageCache.h"
#import "library/VLCLibraryMenuController.h"

#import "library/media-source/VLCMediaSourceDataSource.h"

#import "main/VLCMain.h"

#import "playqueue/VLCPlayQueueController.h"

#import "views/VLCImageView.h"
#import "views/VLCMediaItemCollectionViewItem.h"

NSString *VLCMediaSourceCollectionViewItemIdentifier = @"VLCMediaSourceCollectionViewItemIdentifier";

@interface VLCMediaSourceCollectionViewItem ()
{
    VLCLibraryMenuController *_menuController;
}
@end

@implementation VLCMediaSourceCollectionViewItem

- (void)setRepresentedItem:(VLCInputItem *)representedItem
{
    if (_representedItem == representedItem) {
        return;
    }
    _representedItem = representedItem;
    [self updateRepresentation];
}

- (void)updateRepresentation
{
    VLCInputItem * const inputItem = self.representedItem;
    if (inputItem == nil) {
        return;
    }

    self.mediaTitleTextField.stringValue = inputItem.name;

    switch (inputItem.inputType) {
        case ITEM_TYPE_STREAM:
            self.annotationTextField.stringValue = _NS("Stream");
            self.annotationTextField.hidden = NO;
            break;
        case ITEM_TYPE_PLAYLIST:
            self.annotationTextField.stringValue = _NS("Playlist");
            self.annotationTextField.hidden = NO;
            break;
        case ITEM_TYPE_DISC:
            self.annotationTextField.stringValue = _NS("Disk");
            self.annotationTextField.hidden = NO;
            break;
        default:
            self.annotationTextField.hidden = YES;
            break;
    }

    __weak typeof(self) weakSelf = self;
    [VLCLibraryImageCache thumbnailForInputItem:inputItem
                                 withCompletion:^(NSImage * const thumbnail) {
        __strong typeof(weakSelf) strongSelf = weakSelf;
        if (!strongSelf || strongSelf.representedItem != inputItem) {
            return;
        }
        strongSelf.mediaImageView.image = thumbnail;
    }];
}

#pragma mark - actions

- (IBAction)playInstantly:(id)sender
{
    [VLCMain.sharedInstance.playQueueController
        addInputItem:self.representedItem.vlcInputItem
           atPosition:-1
        startPlayback:YES];
}

- (IBAction)addToPlayQueue:(id)sender
{
    [VLCMain.sharedInstance.playQueueController
        addInputItem:self.representedItem.vlcInputItem
           atPosition:-1
        startPlayback:NO];
}

- (void)openContextMenu:(NSEvent *)event
{
    if (!_menuController) {
        _menuController = [[VLCLibraryMenuController alloc] init];
    }

    NSCollectionView * const collectionView = self.collectionView;
    VLCMediaSourceDataSource * const dataSource =
        (VLCMediaSourceDataSource *)collectionView.dataSource;
    if (dataSource == nil) {
        return;
    }
    NSSet<NSIndexPath *> * const indexPaths = collectionView.selectionIndexPaths;
    NSArray<VLCInputItem *> * const selectedInputItems =
        [dataSource mediaSourceInputItemsAtIndexPaths:indexPaths];
    const NSInteger representedItemIndex = [selectedInputItems indexOfObjectPassingTest:^BOOL(
        VLCInputItem * const inputItem, const NSUInteger __unused idx, BOOL * const __unused stop
    ) {
        return [inputItem.MRL isEqualToString:self.representedItem.MRL];
    }];
    NSArray<VLCInputItem *> *items = nil;
    if (representedItemIndex == NSNotFound) {
        items = @[self.representedItem];
    } else {
        items = selectedInputItems;
    }
    _menuController.representedInputItems = items;
    [_menuController popupMenuWithEvent:event forView:self.view];
}

- (void)mouseDown:(NSEvent *)event
{
    if (event.modifierFlags & NSEventModifierFlagControl) {
        [self openContextMenu:event];
        return;
    }
    if (event.modifierFlags & (NSEventModifierFlagShift | NSEventModifierFlagCommand)) {
        self.selected = !self.selected;
        return;
    }
    [super mouseDown:event];
}

@end
