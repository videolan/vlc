/*****************************************************************************
 * VLCMediaItemCollectionViewItem.m: MacOS X interface module
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

#import "VLCMediaItemCollectionViewItem.h"
#include "vlc_media_library.h"

#import "extensions/NSColor+VLCAdditions.h"
#import "extensions/NSFont+VLCAdditions.h"
#import "extensions/NSString+Helpers.h"
#import "extensions/NSView+VLCAdditions.h"

#import "library/VLCInputItem.h"
#import "library/VLCLibraryCollectionViewDataSource.h"
#import "library/VLCLibraryCollectionViewFlowLayout.h"
#import "library/VLCLibraryDataTypes.h"
#import "library/VLCLibraryMenuController.h"
#import "library/VLCLibraryModel.h"
#import "library/VLCLibraryRepresentedItem.h"
#import "library/VLCLibraryUIUnits.h"

#import "library/media-source/VLCMediaSourceDataSource.h"

#import "views/VLCImageView.h"
#import "views/VLCLinearProgressIndicator.h"
#import "views/VLCTrackingView.h"

NSString *VLCMediaItemCollectionViewItemIdentifier = @"VLCMediaItemCollectionViewItemIdentifier";

const CGFloat VLCMediaItemCollectionViewItemMinimalDisplayedProgress = 0.05;
const CGFloat VLCMediaItemCollectionViewItemMaximumDisplayedProgress = 0.95;

@interface VLCMediaItemCollectionViewItem ()
{
    VLCLibraryMenuController *_menuController;
    NSLayoutConstraint *_videoImageViewAspectRatioConstraint;
}
@end

@implementation VLCMediaItemCollectionViewItem

+ (const NSSize)defaultSize
{
    const CGFloat width = VLCMediaItemCollectionViewItem.defaultWidth;
    return CGSizeMake(width, width + self.bottomTextViewsHeight);
}

+ (const NSSize)defaultVideoItemSize
{
    const CGFloat width = VLCMediaItemCollectionViewItem.defaultWidth;
    const CGFloat imageViewHeight = width * VLCMediaItemCollectionViewItem.videoHeightAspectRatioMultiplier;
    return CGSizeMake(width, imageViewHeight + self.bottomTextViewsHeight);
}

+ (const CGFloat)defaultWidth
{
    return 214.;
}

+ (const CGFloat)bottomTextViewsHeight
{
    return VLCLibraryUIUnits.smallSpacing +
           16 +
           VLCLibraryUIUnits.smallSpacing +
           16 +
           VLCLibraryUIUnits.smallSpacing;
}

+ (const CGFloat)videoHeightAspectRatioMultiplier
{
    return 10. / 16.;
}

- (instancetype)initWithNibName:(NSNibName)nibNameOrNil bundle:(NSBundle *)nibBundleOrNil
{
    self = [super initWithNibName:nibNameOrNil bundle:nibBundleOrNil];
    if (self) {
        NSNotificationCenter *notificationCenter = NSNotificationCenter.defaultCenter;
        [notificationCenter addObserver:self
                               selector:@selector(mediaItemThumbnailGenerated:)
                                   name:VLCLibraryModelMediaItemThumbnailGenerated
                                 object:nil];
    }
    return self;
}

- (void)dealloc
{
    [NSNotificationCenter.defaultCenter removeObserver:self];
}

- (void)awakeFromNib
{
    _deselectWhenClickedIfSelected = YES;
    _videoImageViewAspectRatioConstraint =
        [NSLayoutConstraint constraintWithItem:_mediaImageView
                                     attribute:NSLayoutAttributeHeight
                                     relatedBy:NSLayoutRelationEqual
                                        toItem:_mediaImageView
                                     attribute:NSLayoutAttributeWidth
                                    multiplier:VLCMediaItemCollectionViewItem.videoHeightAspectRatioMultiplier
                                      constant:1];
    _videoImageViewAspectRatioConstraint.priority = NSLayoutPriorityRequired;
    _videoImageViewAspectRatioConstraint.active = NO;

    if (@available(macOS 26.0, *)) {
#if __MAC_OS_X_VERSION_MAX_ALLOWED >= 260000
        self.playInstantlyButton.bordered = YES;
        self.playInstantlyButton.bezelStyle = NSBezelStyleGlass;
        self.playInstantlyButton.borderShape = NSControlBorderShapeCircle;
        self.playInstantlyButton.image = [NSImage imageWithSystemSymbolName:@"play.fill"
                                                   accessibilityDescription:nil];
        self.playInstantlyButton.imageScaling = NSImageScaleProportionallyUpOrDown;
        self.playInstantlyButton.controlSize = NSControlSizeExtraLarge;

        const CGFloat playButtonSize = VLCLibraryUIUnits.mediumPlaybackControlButtonSize;
        [NSLayoutConstraint activateConstraints:@[
            [self.playInstantlyButton.widthAnchor constraintEqualToConstant:playButtonSize],
            [self.playInstantlyButton.heightAnchor constraintEqualToConstant:playButtonSize],
        ]];

        self.addToPlayQueueButton.bordered = YES;
        self.addToPlayQueueButton.bezelStyle = NSBezelStyleGlass;
        self.addToPlayQueueButton.borderShape = NSControlBorderShapeCapsule;
        self.addToPlayQueueButton.image = [NSImage imageWithSystemSymbolName:@"ellipsis"
                                                    accessibilityDescription:nil];
        self.addToPlayQueueButton.imageScaling = NSImageScaleProportionallyUpOrDown;
        self.addToPlayQueueButton.controlSize = NSControlSizeSmall;
#endif
    }

    NSArray * const viewsToHide = @[self.playInstantlyButton, self.addToPlayQueueButton];
    [(VLCTrackingView *)self.view setViewsToHide:viewsToHide];

    self.secondaryInfoTextField.textColor = NSColor.VLClibrarySubtitleColor;
    self.annotationTextField.font = NSFont.VLCLibraryItemAnnotationFont;
    self.annotationTextField.textColor = NSColor.VLClibraryAnnotationColor;
    self.annotationTextField.backgroundColor = NSColor.VLClibraryAnnotationBackgroundColor;
    self.unplayedIndicatorTextField.stringValue = _NS("NEW");
    self.unplayedIndicatorTextField.font = [NSFont systemFontOfSize:NSFont.systemFontSize
                                                             weight:NSFontWeightBold];
    self.highlightBox.borderColor = NSColor.VLCAccentColor;
    self.unplayedIndicatorTextField.textColor = NSColor.VLCAccentColor;

    [self updateColoredAppearance:self.view.effectiveAppearance];
    [self prepareForReuse];
}

#pragma mark - dynamic appearance

- (void)viewDidChangeEffectiveAppearance
{
    [self updateColoredAppearance:self.view.effectiveAppearance];
}

- (void)updateColoredAppearance:(NSAppearance*)appearance
{
    NSParameterAssert(appearance);
    BOOL isDark = NO;
    if (@available(macOS 10.14, *)) {
        isDark = 
            [appearance.name isEqualToString:NSAppearanceNameDarkAqua] ||
            [appearance.name isEqualToString:NSAppearanceNameVibrantDark];
    }

    self.mediaTitleTextField.textColor = isDark 
        ? NSColor.VLClibraryDarkTitleColor
        : NSColor.VLClibraryLightTitleColor;
}

#pragma mark - view representation

- (void)prepareForReuse
{
    [super prepareForReuse];
    self.playInstantlyButton.hidden = YES;
    self.addToPlayQueueButton.hidden = YES;
    _mediaTitleTextField.stringValue = @"";
    _secondaryInfoTextField.stringValue = [NSString stringWithTime:0];
    _mediaImageView.image = nil;
    _annotationTextField.hidden = YES;
    _secondaryInfoTextField.hidden = YES;
    _progressIndicator.hidden = YES;
    _highlightBox.hidden = YES;

    [self setUnplayedIndicatorHidden:YES];
}

- (void)setRepresentedItem:(id<VLCMediaItemRepresentable>)representedItem
{
    _representedItem = representedItem;
    [self updateRepresentation];
}

- (void)setSelected:(BOOL)selected
{
    [super setSelected:selected];
    _highlightBox.hidden = !selected;
}

- (void)mediaItemThumbnailGenerated:(NSNotification *)aNotification
{
    VLCMediaLibraryMediaItem *updatedMediaItem = aNotification.object;
    if (updatedMediaItem == nil ||
        _representedItem == nil ||
        ![_representedItem isKindOfClass:VLCLibraryRepresentedItem.class]) {
        return;
    }

    VLCLibraryRepresentedItem * const representedItem =
        (VLCLibraryRepresentedItem *)self.representedItem;
    if (![representedItem.item isKindOfClass:VLCMediaLibraryMediaItem.class]) {
        return;
    }

    VLCMediaLibraryMediaItem * const mediaItem = (VLCMediaLibraryMediaItem *)representedItem.item;
    if (updatedMediaItem.libraryID == mediaItem.libraryID) {
        [self updateRepresentation];
    }
}

- (void)updateRepresentation
{
    const id<VLCMediaItemRepresentable> item = self.representedItem;
    if (item == nil) {
        return;
    }

    self.mediaTitleTextField.stringValue = item.displayTitle;

    NSString * const annotation = item.annotation;
    if (annotation.length > 0) {
        self.annotationTextField.stringValue = annotation;
        self.annotationTextField.hidden = NO;
    } else {
        self.annotationTextField.hidden = YES;
    }

    __weak typeof(self) weakSelf = self;
    [item requestThumbnailWithSize:NSMakeSize(0, 0)
                        completion:^(NSImage * _Nullable const thumbnail) {
        __strong typeof(weakSelf) strongSelf = weakSelf;
        if (!strongSelf || strongSelf.representedItem != item) {
            return;
        }
        strongSelf.mediaImageView.image = thumbnail;
    }];

    if (![item isKindOfClass:VLCLibraryRepresentedItem.class]) {
        // In media-source mode (VLCInputItem) library-only fields stay hidden; the
        // prepareForReuse reset already hid them and updateRepresentation does not
        // re-show them.
        return;
    }

    VLCLibraryRepresentedItem * const libraryItem = (VLCLibraryRepresentedItem *)item;
    const id<VLCMediaLibraryItemProtocol> actualItem = libraryItem.item;
    self.secondaryInfoTextField.stringValue = actualItem.primaryDetailString;
    self.secondaryInfoTextField.hidden = NO;

    if ([actualItem isKindOfClass:VLCMediaLibraryMediaItem.class]) {
        VLCMediaLibraryMediaItem * const mediaItem = (VLCMediaLibraryMediaItem *)actualItem;
        const vlc_ml_media_type_t mediaType = mediaItem.mediaType;

        if (mediaType == VLC_ML_MEDIA_TYPE_VIDEO || mediaType == VLC_ML_MEDIA_TYPE_UNKNOWN) {
            self.imageViewAspectRatioConstraint.active = NO;
            _videoImageViewAspectRatioConstraint.active = YES;
        } else {
            _videoImageViewAspectRatioConstraint.active = NO;
            self.imageViewAspectRatioConstraint.active = YES;
        }

        const CGFloat position = mediaItem.progress;
        if (position > VLCMediaItemCollectionViewItemMinimalDisplayedProgress &&
            position < VLCMediaItemCollectionViewItemMaximumDisplayedProgress) {
            self.progressIndicator.progress = position;
            self.progressIndicator.hidden = NO;
        }

        if (mediaItem.playCount == 0) {
            [self setUnplayedIndicatorHidden:NO];
        }
    } else {
        self.progressIndicator.hidden = YES;
        _videoImageViewAspectRatioConstraint.active = NO;
        self.imageViewAspectRatioConstraint.active = YES;
    }
}

- (void)setUnplayedIndicatorHidden:(BOOL)indicatorHidden
{
    if (self.unplayedIndicatorTextField.hidden == indicatorHidden) {
        return;
    }

    self.unplayedIndicatorTextField.hidden = indicatorHidden;
    self.trailingSecondaryTextToLeadingUnplayedIndicatorConstraint.active = !indicatorHidden;
    self.trailingSecondaryTextToTrailingSuperviewConstraint.active = indicatorHidden;
}

#pragma mark - actions

- (IBAction)playInstantly:(id)sender
{
    [self.representedItem play];
}

- (IBAction)addToPlayQueue:(id)sender
{
    [self.representedItem queue];
}

- (void)openContextMenu:(NSEvent *)event
{
    if (!_menuController) {
        _menuController = [VLCLibraryMenuController new];
    }

    NSCollectionView * const collectionView = self.collectionView;
    const id<NSCollectionViewDataSource> dataSource = collectionView.dataSource;
    NSSet<NSIndexPath *> * const indexPaths = collectionView.selectionIndexPaths;

    if ([self.representedItem isKindOfClass:VLCInputItem.class]) {
        if (![dataSource isKindOfClass:VLCMediaSourceDataSource.class]) {
            return;
        }
        VLCMediaSourceDataSource * const mediaSourceDataSource = (VLCMediaSourceDataSource *)dataSource;
        NSArray<VLCInputItem *> * const selectedInputItems =
            [mediaSourceDataSource mediaSourceInputItemsAtIndexPaths:indexPaths];
        const NSInteger representedItemIndex = [selectedInputItems indexOfObjectPassingTest:^BOOL(
            VLCInputItem * const inputItem, const NSUInteger __unused idx, BOOL * const __unused stop
        ) {
            return [inputItem.MRL isEqualToString:((VLCInputItem *)self.representedItem).MRL];
        }];
        NSArray<VLCInputItem *> *items = nil;

        if (representedItemIndex == NSNotFound) {
            items = @[(VLCInputItem *)self.representedItem];
        } else {
            items = selectedInputItems;
        }

        _menuController.representedInputItems = items;
    } else {
        if (![dataSource conformsToProtocol:@protocol(VLCLibraryCollectionViewDataSource)]) {
            return;
        }

        NSObject<VLCLibraryCollectionViewDataSource> * const libraryDataSource =
            (NSObject<VLCLibraryCollectionViewDataSource> *)dataSource;
        NSArray<VLCLibraryRepresentedItem *> * const selectedItems =
            [libraryDataSource representedItemsAtIndexPaths:indexPaths
                                          forCollectionView:collectionView];
        const NSInteger representedItemIndex = [selectedItems indexOfObjectPassingTest:^BOOL(
            VLCLibraryRepresentedItem * const repItem,
            const NSUInteger __unused idx,
            BOOL * const __unused stop
        ) {
            return [repItem isEqual:self.representedItem];
        }];
        NSArray<VLCLibraryRepresentedItem *> *items = nil;

        if (representedItemIndex == NSNotFound) {
            items = @[(VLCLibraryRepresentedItem *)self.representedItem];
        } else {
            items = selectedItems;
        }

        _menuController.representedItems = items;
    }

    [_menuController popupMenuWithEvent:event forView:self.view];
}

- (void)mouseDown:(NSEvent *)event
{
    if (event.modifierFlags & NSEventModifierFlagControl) {
        [self openContextMenu:event];
        return;
    }

    if (event.modifierFlags & (NSEventModifierFlagShift | NSEventModifierFlagCommand) &&
        [self.representedItem isKindOfClass:[VLCInputItem class]]) {
        self.selected = !self.selected;
        return;
    }

    const id<NSCollectionViewDataSource> dataSource = self.collectionView.dataSource;
    if (self.deselectWhenClickedIfSelected &&
        self.selected &&
        [dataSource conformsToProtocol:@protocol(VLCLibraryCollectionViewDataSource)]) {

        NSObject<VLCLibraryCollectionViewDataSource> * const libraryDataSource =
            (NSObject<VLCLibraryCollectionViewDataSource> *)self.collectionView.dataSource;
        VLCLibraryRepresentedItem * const representedItem =
            (VLCLibraryRepresentedItem *)self.representedItem;
        NSIndexPath * const indexPath =
            [libraryDataSource indexPathForLibraryItem:representedItem.item];
        if (indexPath == nil) {
            NSLog(@"Received nil indexPath for item %@!", representedItem.item.displayString);
            [super mouseDown:event];
            return;
        }

        NSSet<NSIndexPath *> * const indexPathSet = [NSSet setWithObject:indexPath];
        [self.collectionView deselectItemsAtIndexPaths:indexPathSet];

        NSCollectionViewFlowLayout * const collectionViewFlowLayout = 
            self.collectionView.collectionViewLayout;
        if ([collectionViewFlowLayout isKindOfClass:VLCLibraryCollectionViewFlowLayout.class]) {
            VLCLibraryCollectionViewFlowLayout * const flowLayout =
                (VLCLibraryCollectionViewFlowLayout *)collectionViewFlowLayout;
            [flowLayout collapseDetailSectionAtIndex:indexPath];
        }
    }

    [super mouseDown:event];
}

- (void)rightMouseDown:(NSEvent *)event
{
    [self openContextMenu:event];
    [super rightMouseDown:event];
}

@end
