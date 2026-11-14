/*****************************************************************************
 * VLCLibraryMusicCompactCarouselCollectionViewItem.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2026 VLC authors and VideoLAN
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

#import "VLCLibraryMusicCompactCarouselCollectionViewItem.h"

#import "extensions/NSFont+VLCAdditions.h"
#import "extensions/NSImage+VLCAdditions.h"
#import "extensions/NSString+Helpers.h"
#import "library/VLCInputItem.h"
#import "library/VLCLibraryDataTypes.h"
#import "library/VLCLibraryImageCache.h"
#import "library/VLCLibraryNameCache.h"
#import "library/VLCLibraryRepresentedItem.h"
#import "views/VLCImageView.h"
#import "views/VLCUIUnits.h"

NSString * const VLCLibraryMusicCompactCarouselCollectionViewItemIdentifier =
    @"VLCLibraryMusicCompactCarouselCollectionViewItemIdentifier";

@interface VLCLibraryMusicCompactCarouselItemView : NSView
@end

@implementation VLCLibraryMusicCompactCarouselItemView

- (NSView *)hitTest:(NSPoint)point
{
    // Keep the artwork and labels from consuming the carousel's mouse events.
    return [super hitTest:point] != nil ? self : nil;
}

@end

@implementation VLCLibraryMusicCompactCarouselCollectionViewItem
{
    NSTextField *_durationTextField;
}

+ (NSSize)itemSize
{
    return NSMakeSize(220., 54.);
}

- (void)loadView
{
    const NSSize itemSize = VLCLibraryMusicCompactCarouselCollectionViewItem.itemSize;
    NSView * const view = [[VLCLibraryMusicCompactCarouselItemView alloc]
        initWithFrame:NSMakeRect(0., 0., itemSize.width, itemSize.height)];
    view.accessibilityElement = YES;
    view.accessibilityRole = NSAccessibilityGroupRole;
    NSAccessibilityCustomAction * const playAction = [[NSAccessibilityCustomAction alloc] initWithName:_NS("Play")
                                                                                                target:self
                                                                                              selector:@selector(accessibilityPlayItem)];
    view.accessibilityCustomActions = @[playAction];

    VLCImageView * const coverImageView = [[VLCImageView alloc] initWithFrame:NSZeroRect];
    coverImageView.translatesAutoresizingMaskIntoConstraints = NO;
    coverImageView.accessibilityElement = NO;

    NSTextField * const titleTextField = [NSTextField labelWithString:@""];
    titleTextField.font = NSFont.VLCLibraryCompactSubsectionSubheaderFont;
    titleTextField.lineBreakMode = NSLineBreakByTruncatingTail;
    titleTextField.translatesAutoresizingMaskIntoConstraints = NO;
    titleTextField.accessibilityElement = NO;

    NSTextField * const detailTextField = [NSTextField labelWithString:@""];
    detailTextField.font = NSFont.VLCLibrarySmallSubsectionSubheaderFont;
    detailTextField.textColor = NSColor.secondaryLabelColor;
    detailTextField.lineBreakMode = NSLineBreakByTruncatingTail;
    detailTextField.translatesAutoresizingMaskIntoConstraints = NO;
    detailTextField.accessibilityElement = NO;

    _durationTextField = [NSTextField labelWithString:@""];
    const CGFloat durationFontSize = NSFont.VLCLibrarySmallSubsectionSubheaderFont.pointSize;
    _durationTextField.font = [NSFont monospacedDigitSystemFontOfSize:durationFontSize weight:NSFontWeightMedium];
    _durationTextField.textColor = NSColor.secondaryLabelColor;
    _durationTextField.translatesAutoresizingMaskIntoConstraints = NO;
    _durationTextField.accessibilityElement = NO;
    [_durationTextField setContentCompressionResistancePriority:NSLayoutPriorityRequired
                                                 forOrientation:NSLayoutConstraintOrientationHorizontal];
    [_durationTextField setContentHuggingPriority:NSLayoutPriorityRequired
                                   forOrientation:NSLayoutConstraintOrientationHorizontal];

    [view addSubview:coverImageView];
    [view addSubview:titleTextField];
    [view addSubview:detailTextField];
    [view addSubview:_durationTextField];

    const CGFloat artworkSize = 42.;
    [NSLayoutConstraint activateConstraints:@[
        [coverImageView.leadingAnchor constraintEqualToAnchor:view.leadingAnchor],
        [coverImageView.centerYAnchor constraintEqualToAnchor:view.centerYAnchor],
        [coverImageView.widthAnchor constraintEqualToConstant:artworkSize],
        [coverImageView.heightAnchor constraintEqualToConstant:artworkSize],
        [titleTextField.leadingAnchor constraintEqualToAnchor:coverImageView.trailingAnchor constant:VLCUIUnits.mediumSpacing],
        [titleTextField.trailingAnchor constraintEqualToAnchor:_durationTextField.leadingAnchor constant:-VLCUIUnits.mediumSpacing],
        [titleTextField.bottomAnchor constraintEqualToAnchor:view.centerYAnchor],
        [detailTextField.leadingAnchor constraintEqualToAnchor:titleTextField.leadingAnchor],
        [detailTextField.trailingAnchor constraintEqualToAnchor:titleTextField.trailingAnchor],
        [detailTextField.topAnchor constraintEqualToAnchor:titleTextField.bottomAnchor],
        [_durationTextField.trailingAnchor constraintEqualToAnchor:view.trailingAnchor constant:-VLCUIUnits.smallSpacing],
        [_durationTextField.centerYAnchor constraintEqualToAnchor:view.centerYAnchor],
    ]];

    self.coverImageView = coverImageView;
    self.titleTextField = titleTextField;
    self.detailTextField = detailTextField;
    self.view = view;
}

- (void)prepareForReuse
{
    [super prepareForReuse];
    self.representedItem = nil;
}

- (void)updateRepresentation
{
    NSView * const view = self.view;
    id<VLCMediaLibraryItemProtocol> const item = self.representedItem.item;
    if (item == nil) {
        self.titleTextField.stringValue = @"";
        self.detailTextField.stringValue = @"";
        _durationTextField.stringValue = @"";
        _durationTextField.hidden = YES;
        self.coverImageView.image = nil;
        view.accessibilityLabel = nil;
        return;
    }

    NSString *detail;
    NSString *duration = @"";
    if ([item isKindOfClass:VLCMediaLibraryMediaItem.class]) {
        VLCMediaLibraryMediaItem * const track = (VLCMediaLibraryMediaItem *)item;
        detail = [VLCLibraryNameCache.sharedInstance artistNameForID:track.artistID];
        if (detail.length == 0) {
            detail = track.inputItem.artist;
        }
        if (detail.length == 0) {
            detail = _NS("Unknown Artist");
        }
        duration = track.durationString;
    } else {
        detail = item.primaryDetailString;
    }

    self.titleTextField.stringValue = item.displayString;
    self.detailTextField.stringValue = detail;
    _durationTextField.stringValue = duration;
    _durationTextField.hidden = duration.length == 0;
    NSArray<NSString *> * const labels = _durationTextField.hidden
        ? @[item.displayString, detail]
        : @[item.displayString, detail, duration];
    view.accessibilityLabel = [labels componentsJoinedByString:@", "];

    self.coverImageView.image = NSImage.VLCNoArtImage;
    __weak typeof(self) weakSelf = self;
    [VLCLibraryImageCache thumbnailForLibraryItem:item
                                   withCompletion:^(NSImage * const thumbnail) {
        if (weakSelf.representedItem.item == item) {
            weakSelf.coverImageView.image = thumbnail;
        }
    }];
}

- (BOOL)accessibilityPlayItem
{
    if (self.representedItem == nil) {
        return NO;
    }
    [self playRepresentedItem];
    return YES;
}

@end
