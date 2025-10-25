/*****************************************************************************
 * VLCLibraryTableCellView.m: MacOS X interface module
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

#import "VLCLibraryTableCellView.h"

#import "extensions/NSFont+VLCAdditions.h"
#import "extensions/NSString+Helpers.h"
#import "extensions/NSView+VLCAdditions.h"

#import "library/VLCInputItem.h"
#import "library/VLCLibraryController.h"
#import "library/VLCLibraryDataTypes.h"
#import "library/VLCLibraryImageCache.h"
#import "library/VLCLibraryModel.h"
#import "library/VLCLibraryRepresentedItem.h"
#import "library/VLCLibraryUIUnits.h"

#import "library/video-library/VLCLibraryVideoGroupDescriptor.h"

#import "main/VLCMain.h"

#import "playqueue/VLCPlayQueueController.h"

#import "views/VLCImageView.h"
#import "views/VLCTrackingView.h"

NSString * const VLCLibraryTableCellViewIdentifier = @"VLCLibraryTableCellViewIdentifier";

@implementation VLCLibraryTableCellView

+ (instancetype)fromNibWithOwner:(id)owner
{
    return (VLCLibraryTableCellView*)[NSView fromNibNamed:NSStringFromClass(VLCLibraryTableCellView.class)
                                                withClass:[VLCLibraryTableCellView class]
                                                withOwner:owner];
}

- (void)awakeFromNib
{
    [self prepareForReuse];

    if (@available(macOS 26.0, *)) {
#if __MAC_OS_X_VERSION_MAX_ALLOWED >= 260000
        self.playInstantlyButton.bordered = YES;
        self.playInstantlyButton.bezelStyle = NSBezelStyleGlass;
        self.playInstantlyButton.borderShape = NSControlBorderShapeCircle;
        self.playInstantlyButton.image = [NSImage imageWithSystemSymbolName:@"play.fill" accessibilityDescription:nil];
        self.playInstantlyButton.imageScaling = NSImageScaleProportionallyUpOrDown;
        self.playInstantlyButton.controlSize = NSControlSizeExtraLarge;
#endif
    }
}

- (void)prepareForReuse
{
    [super prepareForReuse];
    self.representedImageView.image = nil;
    self.primaryTitleTextField.hidden = YES;
    self.secondaryTitleTextField.hidden = YES;
    self.singlePrimaryTitleTextField.hidden = YES;
    self.trackingView.viewToHide = nil;
    self.playInstantlyButton.hidden = YES;
}

- (void)setRepresentedItem:(VLCLibraryRepresentedItem *)representedItem
{
    _representedItem = representedItem;
    _representedInputItem = nil; // Reset and ensure the change is obvious

    id<VLCMediaLibraryItemProtocol> const actualItem = representedItem.item;
    NSAssert(actualItem != nil, @"Should not update nil represented item!");

    self.trackingView.viewToHide = self.playInstantlyButton;
    self.playInstantlyButton.action = @selector(playMediaItemInstantly:);
    self.playInstantlyButton.target = self;

    __weak typeof(self) weakSelf = self;
    [VLCLibraryImageCache thumbnailForLibraryItem:actualItem withCompletion:^(NSImage * const thumbnail) {
        if (!weakSelf || weakSelf.representedItem.item != actualItem) {
            return;
        }
        weakSelf.representedImageView.image = thumbnail;
    }];

    if(actualItem.primaryDetailString.length > 0) {
        self.primaryTitleTextField.hidden = NO;
        self.primaryTitleTextField.stringValue = actualItem.displayString;
        self.secondaryTitleTextField.hidden = NO;
        self.secondaryTitleTextField.stringValue = actualItem.primaryDetailString;
    } else {
        self.singlePrimaryTitleTextField.hidden = NO;
        self.singlePrimaryTitleTextField.stringValue = actualItem.displayString;
    }
}

- (void)setRepresentedInputItem:(VLCInputItem *)representedInputItem
{
    _representedInputItem = representedInputItem;
    _representedItem = nil; // Reset and ensure the change is obvious

    self.singlePrimaryTitleTextField.hidden = NO;
    self.singlePrimaryTitleTextField.stringValue = _representedInputItem.name;

    __weak typeof(self) weakSelf = self;
    [VLCLibraryImageCache thumbnailForInputItem:self->_representedInputItem withCompletion:^(NSImage * const thumbnail) {
        VLCLibraryTableCellView * const strongSelf = weakSelf;
        if (!strongSelf || representedInputItem != strongSelf->_representedInputItem) {
            return;
        }
        strongSelf->_representedImageView.image = thumbnail;
    }];

    self.trackingView.viewToHide = self.playInstantlyButton;
    self.playInstantlyButton.action = @selector(playInputItemInstantly:);
    self.playInstantlyButton.target = self;
}

- (void)setRepresentedVideoLibrarySection:(NSUInteger)section
{
    // We need to adjust the selected row value to match the backing enum.
    // Additionally, we hide recents when there are no recent media items.
    VLCLibraryModel * const model = VLCMain.sharedInstance.libraryController.libraryModel;
    const BOOL anyRecents = model.numberOfRecentMedia > 0;
    const NSUInteger sectionBase = VLCMediaLibraryParentGroupTypeRecentVideos;
    const NSUInteger sectionAdjustment = anyRecents ? sectionBase : sectionBase + 1;

    NSString *sectionString = @"";
    switch(section + sectionAdjustment) { // Group 0 is Invalid, so add one
        case VLCMediaLibraryParentGroupTypeRecentVideos:
            sectionString = _NS("Recents");
            break;
        case VLCMediaLibraryParentGroupTypeVideoLibrary:
            sectionString = _NS("Library");
            break;
        default:
            NSAssert(1, @"Reached unreachable case for video library section");
            break;
    }

    self.singlePrimaryTitleTextField.hidden = NO;
    self.singlePrimaryTitleTextField.stringValue = sectionString;
    self.representedImageView.image = [NSImage imageNamed: @"noart.png"];

    _representedVideoLibrarySection = section;
}

- (void)playMediaItemInstantly:(id)sender
{
    [self.representedItem play];
}

- (void)playInputItemInstantly:(id)sender
{
    [VLCMain.sharedInstance.playQueueController addInputItem:_representedInputItem.vlcInputItem atPosition:-1 startPlayback:YES];
}

@end
