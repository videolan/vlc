/*****************************************************************************
 * VLCLibrarySongTableCellView.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2022 VLC authors and VideoLAN
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

#import "VLCLibrarySongTableCellView.h"

#import "extensions/NSColor+VLCAdditions.h"
#import "extensions/NSFont+VLCAdditions.h"
#import "extensions/NSString+Helpers.h"
#import "extensions/NSView+VLCAdditions.h"

#import "views/VLCImageView.h"
#import "views/VLCTrackingView.h"

#import "main/VLCMain.h"

#import "library/VLCLibraryController.h"
#import "library/VLCLibraryDataTypes.h"
#import "library/VLCInputItem.h"
#import "library/VLCLibraryItemInternalMediaItemsDataSource.h"
#import "library/VLCLibraryTableCellView.h"
#import "library/VLCLibraryRepresentedItem.h"

#import "playqueue/VLCPlayerController.h"
#import "playqueue/VLCPlayQueueController.h"

NSString *VLCAudioLibrarySongCellIdentifier = @"VLCAudioLibrarySongCellIdentifier";

@interface VLCLibrarySongTableCellView ()
{
    VLCLibraryController *_libraryController;
}
@end

@implementation VLCLibrarySongTableCellView

+ (instancetype)fromNibWithOwner:(id)owner
{
    return (VLCLibrarySongTableCellView*)[NSView fromNibNamed:@"VLCLibrarySongTableCellView"
                                                     withClass:[VLCLibrarySongTableCellView class]
                                                     withOwner:owner];
}

- (void)awakeFromNib
{
    self.playInstantlyButton.target = self;
    self.playInstantlyButton.action = @selector(playInstantly:);

    self.trackingView.viewToHide = self.playInstantlyButton;
    self.trackingView.viewToShow = self.trackNumberTextField;

    if (@available(macOS 10.14, *)) {
        self.playInstantlyButton.contentTintColor = NSColor.VLCAccentColor;
    }

    NSNotificationCenter * const notificationCenter = NSNotificationCenter.defaultCenter;
    [notificationCenter addObserver:self
                           selector:@selector(playStateOrItemChanged:)
                               name:VLCPlayerCurrentMediaItemChanged
                             object:nil];
    [notificationCenter addObserver:self
                           selector:@selector(playStateOrItemChanged:)
                               name:VLCPlayerStateChanged
                             object:nil];

    [self prepareForReuse];
}

- (void)playStateOrItemChanged:(NSNotification *)notification
{
    const BOOL isCurrentItemAndIsPlaying =
        [self.representedItem.item.firstMediaItem.inputItem.MRL isEqualToString:VLCMain.sharedInstance.playQueueController.currentlyPlayingInputItem.MRL];
    NSFont * const fontToUse = isCurrentItemAndIsPlaying
        ? [NSFont boldSystemFontOfSize:NSFont.systemFontSize]
        : [NSFont systemFontOfSize:NSFont.systemFontSize];
    self.songNameTextField.font = fontToUse;
    self.durationTextField.font = fontToUse;
    self.trackNumberTextField.font = fontToUse;
}

- (void)prepareForReuse
{
    [super prepareForReuse];
    self.songNameTextField.stringValue = @"";
    self.durationTextField.stringValue = @"";
    self.trackNumberTextField.stringValue = @"";
    self.playInstantlyButton.hidden = YES;
    self.trackNumberTextField.hidden = NO;
    [self playStateOrItemChanged:nil];
}

- (IBAction)playInstantly:(id)sender
{
    [self.representedItem play];
}

- (void)setRepresentedItem:(VLCLibraryRepresentedItem *)representedItem
{
    if (representedItem == nil) {
        NSLog(@"Represented item is nil, cannot set in song table cell view");
        return;
    }

    _representedItem = representedItem;

    VLCMediaLibraryMediaItem * const mediaItem = (VLCMediaLibraryMediaItem *)representedItem.item;
    NSAssert(mediaItem != nil, @"Represented item should be a medialibrarymediaitem!");

    self.songNameTextField.stringValue = mediaItem.displayString;
    self.durationTextField.stringValue = mediaItem.durationString;

    if (mediaItem.trackNumber == 0) {
        self.trackNumberTextField.stringValue = @"—";
    } else {
        self.trackNumberTextField.stringValue = [NSString stringWithFormat:@"%d", mediaItem.trackNumber];
    }

    [self playStateOrItemChanged:nil];
}

@end
