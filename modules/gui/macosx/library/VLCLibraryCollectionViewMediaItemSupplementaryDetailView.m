/*****************************************************************************
 * VLCLibraryCollectionViewMediaItemSupplementaryDetailView.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2022 VLC authors and VideoLAN
 *
 * Authors: Claudio Cambra <claudio.cambra@gmail.com>
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

#import "VLCLibraryCollectionViewMediaItemSupplementaryDetailView.h"

#import "extensions/NSString+Helpers.h"
#import "extensions/NSFont+VLCAdditions.h"
#import "extensions/NSColor+VLCAdditions.h"
#import "extensions/NSView+VLCAdditions.h"

#import "main/VLCMain.h"

#import "library/VLCInputItem.h"
#import "library/VLCLibraryController.h"
#import "library/VLCLibraryDataTypes.h"
#import "library/VLCLibraryImageCache.h"
#import "library/VLCLibraryModel.h"
#import "library/VLCLibraryMenuController.h"
#import "library/VLCLibraryRepresentedItem.h"
#import "library/VLCLibraryWindow.h"

#import "views/VLCImageView.h"

NSString *const VLCLibraryCollectionViewMediaItemSupplementaryDetailViewIdentifier = @"VLCLibraryCollectionViewMediaItemSupplementaryDetailViewIdentifier";
NSCollectionViewSupplementaryElementKind const VLCLibraryCollectionViewMediaItemSupplementaryDetailViewKind = @"VLCLibraryCollectionViewMediaItemSupplementaryDetailViewIdentifier";

@implementation VLCLibraryCollectionViewMediaItemSupplementaryDetailView

- (void)awakeFromNib
{
    _mediaItemTitleTextField.font = NSFont.VLCLibrarySubsectionHeaderFont;
    _mediaItemPrimaryDetailButton.font = NSFont.VLCLibrarySubsectionSubheaderFont;
    _mediaItemSecondaryDetailButton.font = NSFont.VLCLibrarySubsectionSubheaderFont;

    if (@available(macOS 10.14, *)) {
        _mediaItemPrimaryDetailButton.contentTintColor = NSColor.VLCAccentColor;
        _mediaItemSecondaryDetailButton.contentTintColor = NSColor.secondaryLabelColor;
    }

    if(@available(macOS 10.12.2, *)) {
        _playMediaItemButton.bezelColor = NSColor.VLCAccentColor;
    }
}

- (NSString *)formattedYearAndDurationAndTypeString
{
    if (self.representedItem == nil) {
        return @"";
    }

    const VLCMediaLibraryMediaItem * const actualItem = self.representedItem.item;
    NSMutableArray<NSString *> * const strings = NSMutableArray.array;

    if (actualItem.year > 0) {
        NSDate *yearDate = [NSDate dateWithTimeIntervalSince1970:actualItem.year];
        NSCalendar *calendar = [NSCalendar currentCalendar];
        [strings addObject:[NSString stringWithFormat:@"%ld", [calendar component:NSCalendarUnitYear fromDate:yearDate]]];
    } else if (actualItem.files.count > 0) {
        VLCMediaLibraryFile * const firstFile = actualItem.files.firstObject;
        const time_t fileLastModTime = firstFile.lastModificationDate;

        if (fileLastModTime > 0) {
            NSDate * const lastModDate = [NSDate dateWithTimeIntervalSince1970:fileLastModTime];
            NSDateComponents * const components =
                [[NSCalendar currentCalendar] components:NSCalendarUnitYear fromDate:lastModDate];
            [strings addObject:[NSString stringWithFormat:@"%ld", components.year]];
        }
    }

    if (![actualItem.primaryDetailString isEqualToString:actualItem.durationString]) {
        [strings addObject:actualItem.durationString];
    }

    [strings addObjectsFromArray:@[actualItem.readableMediaType, actualItem.readableMediaSubType]];

    VLCInputItem * const inputItem = actualItem.inputItem;
    NSString * const discNumber = inputItem.discNumber;
    if (![discNumber isEqualToString:@""]) {
        [strings addObject:[NSString stringWithFormat:@"%@ %@", _NS("Disc"), discNumber]];
    }

    return [strings componentsJoinedByString:@" · "];
}

- (void)updateRepresentation
{
    NSAssert(self.representedItem, @"no represented item assigned for collection view item", nil);
    VLCMediaLibraryMediaItem * const actualItem = self.representedItem.item;
    NSAssert(actualItem != nil, @"represented item is not a media item", nil);

    _mediaItemTitleTextField.stringValue = actualItem.displayString;
    _mediaItemPrimaryDetailButton.title = actualItem.primaryDetailString;
    _mediaItemSecondaryDetailButton.title = actualItem.secondaryDetailString;
    _mediaItemYearAndDurationAndTypeTextField.stringValue = [self formattedYearAndDurationAndTypeString];
    _mediaItemFileNameTextField.stringValue = actualItem.inputItem.name;
    _mediaItemPathTextField.stringValue = actualItem.inputItem.decodedMRL;

    const BOOL primaryActionableDetail = actualItem.primaryActionableDetail;
    const BOOL secondaryActionableDetail = actualItem.secondaryActionableDetail;
    self.mediaItemPrimaryDetailButton.enabled = primaryActionableDetail;
    self.mediaItemSecondaryDetailButton.enabled = secondaryActionableDetail;
    if (@available(macOS 10.14, *)) {
        NSColor * const primaryDetailButtonColor = 
            primaryActionableDetail ? NSColor.VLCAccentColor : NSColor.labelColor;
        NSColor * const secondaryDetailButtonColor = 
            secondaryActionableDetail ? NSColor.VLCAccentColor : NSColor.labelColor;
        self.mediaItemPrimaryDetailButton.contentTintColor = primaryDetailButtonColor;
        self.mediaItemSecondaryDetailButton.contentTintColor = secondaryDetailButtonColor;
    }
    self.mediaItemFavoriteButton.state = actualItem.favorited ? NSOnState : NSOffState;

    self.mediaItemPrimaryDetailButton.target = self;
    self.mediaItemSecondaryDetailButton.target = self;
    self.mediaItemFavoriteButton.target = self;

    self.mediaItemPrimaryDetailButton.action = @selector(primaryDetailAction:);
    self.mediaItemSecondaryDetailButton.action = @selector(secondaryDetailAction:);
    self.mediaItemFavoriteButton.action = @selector(favoriteAction:);

    const double ratingControlMax = self.mediaItemRatingIndicator.maxValue;
    const double proportionOfMaxRating = 100 / ratingControlMax;
    self.mediaItemRatingIndicator.doubleValue = (double)actualItem.rating / proportionOfMaxRating;

    NSArray<NSString *> * const mediaItemLabels = actualItem.labels;
    self.mediaItemLabelsTextField.hidden = mediaItemLabels.count == 0;
    self.mediaItemLabelsTitleTextField.hidden = self.mediaItemLabelsTextField.hidden;
    if (!self.mediaItemLabelsTextField.hidden) {
        self.mediaItemLabelsTextField.stringValue = [mediaItemLabels componentsJoinedByString:@", "];
    }

    self.mediaItemLastPlayedTextField.hidden = actualItem.lastPlayedDate == 0;
    self.mediaItemLastPlayedTitleTextField.hidden = self.mediaItemLastPlayedTextField.hidden;
    if (actualItem.lastPlayedDate > 0) {
        NSDate * const lastPlayedDate =
            [NSDate dateWithTimeIntervalSince1970:actualItem.lastPlayedDate];
        NSDateFormatter * const formatter = [[NSDateFormatter alloc] init];
        formatter.dateStyle = NSDateFormatterFullStyle;
        formatter.timeStyle = NSDateFormatterFullStyle;
        NSString * const lastPlayedString = [formatter stringFromDate:lastPlayedDate];
        self.mediaItemLastPlayedTextField.stringValue = lastPlayedString;
    }

    NSString * const summary = actualItem.movie != nil
        ? actualItem.movie.summary : actualItem.showEpisode != nil
            ? actualItem.showEpisode.summary : @"";
    self.mediaItemSummaryStackView.hidden = summary == nil || [summary isEqualToString:@""];
    self.mediaItemSummarySeparator.hidden = self.mediaItemSummaryStackView.hidden;
    self.mediaItemSummaryTextField.stringValue = summary;

    VLCInputItem * const inputItem = actualItem.inputItem;

    NSString * const copyright = inputItem.copyright;
    self.mediaItemCopyrightTextField.hidden = [copyright isEqualToString:@""];
    self.mediaItemCopyrightTitleTextField.hidden = self.mediaItemCopyrightTextField.hidden;
    self.mediaItemCopyrightTextField.stringValue = copyright;

    NSString * const contentDescription = inputItem.contentDescription;
    self.mediaItemContentDescriptionTextField.hidden = [contentDescription isEqualToString:@""];
    self.mediaItemContentDescriptionTitleTextField.hidden = self.mediaItemContentDescriptionTextField.hidden;
    self.mediaItemContentDescriptionTextField.stringValue = contentDescription;

    NSString * const director = inputItem.director;
    self.mediaItemDirectorTextField.hidden = [inputItem.director isEqualToString:@""];
    self.mediaItemDirectorTitleTextField.hidden = self.mediaItemDirectorTextField.hidden;
    self.mediaItemDirectorTextField.stringValue = director;

    NSString * const actors = inputItem.actors;
    self.mediaItemActorsTextField.hidden = [inputItem.actors isEqualToString:@""];
    self.mediaItemActorsTitleTextField.hidden = self.mediaItemActorsTextField.hidden;
    self.mediaItemActorsTextField.stringValue = actors;

    __weak typeof(self) weakSelf = self;
    [VLCLibraryImageCache thumbnailForLibraryItem:actualItem withCompletion:^(NSImage * const thumbnail) {
        if (!weakSelf || weakSelf.representedItem.item != actualItem) {
            return;
        }
        VLCLibraryCollectionViewMediaItemSupplementaryDetailView * const strongSelf = weakSelf;
        strongSelf->_mediaItemArtworkImageView.image = thumbnail;
    }];
}

- (IBAction)playAction:(id)sender
{
    [self.representedItem play];
}

- (IBAction)enqueueAction:(id)sender
{
    [self.representedItem queue];
}

- (IBAction)primaryDetailAction:(id)sender
{
    VLCMediaLibraryMediaItem * const actualItem = self.representedItem.item;
    if (actualItem == nil || !actualItem.primaryActionableDetail) {
        return;
    }

    VLCLibraryWindow * const libraryWindow = VLCMain.sharedInstance.libraryWindow;
    const id<VLCMediaLibraryItemProtocol> libraryItem = actualItem.primaryActionableDetailLibraryItem;
    [libraryWindow presentLibraryItem:libraryItem];
}

- (IBAction)secondaryDetailAction:(id)sender
{
    VLCMediaLibraryMediaItem * const actualItem = self.representedItem.item;
    if (actualItem == nil || !actualItem.secondaryActionableDetail) {
        return;
    }

    VLCLibraryWindow * const libraryWindow = VLCMain.sharedInstance.libraryWindow;
    const id<VLCMediaLibraryItemProtocol> libraryItem = actualItem.secondaryActionableDetailLibraryItem;
    [libraryWindow presentLibraryItem:libraryItem];
}

- (IBAction)favoriteAction:(id)sender
{
    VLCMediaLibraryMediaItem * const mediaItem = self.representedItem.item;
    if ([mediaItem toggleFavorite] == VLC_SUCCESS) {
        VLCMediaLibraryMediaItem * const updatedItem =
            [VLCMediaLibraryMediaItem mediaItemForLibraryID:mediaItem.libraryID];
        self.representedItem =
            [[VLCLibraryRepresentedItem alloc] initWithItem:updatedItem
                                                 parentType:self.representedItem.parentType];
    }
}

- (IBAction)ratingAction:(id)sender
{
    NSLevelIndicator * const control = (NSLevelIndicator *)sender;
    if (control == nil) {
        return;
    }

    const double proportion = 100 / control.maxValue;
    const double rating = control.doubleValue * proportion;
    ((VLCMediaLibraryMediaItem *)self.representedItem.item).rating = (int)rating;
}

- (IBAction)openPathAction:(id)sender
{
    [self.representedItem revealInFinder];
}

@end
