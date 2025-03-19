/*****************************************************************************
 * VLCMainVideoViewAudioMediaDecorativeView.m: MacOS X interface module
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

#import "VLCMainVideoViewAudioMediaDecorativeView.h"

#import "extensions/NSView+VLCAdditions.h"

#import "main/VLCMain.h"

#import "library/VLCLibraryDataTypes.h"
#import "library/VLCLibraryImageCache.h"

#import "playqueue/VLCPlayQueueController.h"
#import "playqueue/VLCPlayerController.h"

@implementation VLCMainVideoViewAudioMediaDecorativeView

+ (instancetype)fromNibWithOwner:(id)owner
{
    return (VLCMainVideoViewAudioMediaDecorativeView*)[NSView fromNibNamed:@"VLCMainVideoViewAudioMediaDecorativeView"
                                                                 withClass:[VLCMainVideoViewAudioMediaDecorativeView class]
                                                                 withOwner:owner];
}

- (void)awakeFromNib
{
    NSNotificationCenter * const notificationCenter = NSNotificationCenter.defaultCenter;
    [notificationCenter addObserver:self
                           selector:@selector(playerCurrentMediaItemChanged:)
                               name:VLCPlayerCurrentMediaItemChanged
                             object:nil];
    [self updateCoverArt];
}

- (void)playerCurrentMediaItemChanged:(NSNotification *)notification
{
    [self updateCoverArt];
}

- (void)updateCoverArt
{
    VLCPlayerController * const controller =
        VLCMain.sharedInstance.playQueueController.playerController;
    VLCInputItem * const currentInputItem = controller.currentMedia;
    if (controller.currentMedia) {
        __weak typeof(self) weakSelf = self;
        [VLCLibraryImageCache thumbnailForInputItem:currentInputItem
                                     withCompletion:^(NSImage * const thumbnail) {
            if (currentInputItem != controller.currentMedia) {
                return;
            }
            [weakSelf setCoverArt:thumbnail];
        }];
    }
}

- (void)setCoverArt:(NSImage *)coverArtImage
{
    _backgroundCoverArtView.image = coverArtImage;
    _foregroundCoverArtView.image = coverArtImage;
}

@end
