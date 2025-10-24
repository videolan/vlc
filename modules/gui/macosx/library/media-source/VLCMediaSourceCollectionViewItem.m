/*****************************************************************************
 * VLCMediaSourceCollectionViewItem.m: MacOS X interface module
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

#import "VLCMediaSourceCollectionViewItem.h"

#import "extensions/NSColor+VLCAdditions.h"
#import "extensions/NSFont+VLCAdditions.h"
#import "extensions/NSString+Helpers.h"
#import "extensions/NSView+VLCAdditions.h"

#import "library/VLCInputItem.h"
#import "library/VLCLibraryMenuController.h"
#import "library/VLCLibraryImageCache.h"
#import "library/VLCLibraryUIUnits.h"

#import "library/media-source/VLCMediaSourceDataSource.h"

#import "main/VLCMain.h"

#import "playqueue/VLCPlayQueueController.h"

#import "views/VLCImageView.h"
#import "views/VLCTrackingView.h"

#import <vlc_configuration.h>

NSString *VLCMediaSourceCellIdentifier = @"VLCLibraryCellIdentifier";

@interface VLCMediaSourceCollectionViewItem()
{
    VLCLibraryMenuController *_menuController;
}
@end

@implementation VLCMediaSourceCollectionViewItem

- (void)dealloc
{
    [NSNotificationCenter.defaultCenter removeObserver:self];
    if (@available(macOS 10.14, *)) {
        [NSApplication.sharedApplication removeObserver:self forKeyPath:@"effectiveAppearance"];
    }
}

- (void)awakeFromNib
{
    [(VLCTrackingView *)self.view setViewToHide:self.playInstantlyButton];
    self.annotationTextField.font = [NSFont systemFontOfSize:NSFont.systemFontSize weight:NSFontWeightBold];
    self.annotationTextField.textColor = NSColor.VLClibraryAnnotationColor;
    self.annotationTextField.backgroundColor = NSColor.VLClibraryAnnotationBackgroundColor;
    self.highlightBox.borderColor = NSColor.VLCAccentColor;

        if (@available(macOS 26.0, *)) {
#if __MAC_OS_X_VERSION_MAX_ALLOWED >= 260000
        self.playInstantlyButton.bordered = YES;
        self.playInstantlyButton.bezelStyle = NSBezelStyleGlass;
        self.playInstantlyButton.borderShape = NSControlBorderShapeCircle;
        self.playInstantlyButton.image = [NSImage imageWithSystemSymbolName:@"play.fill" accessibilityDescription:nil];
        self.playInstantlyButton.imageScaling = NSImageScaleProportionallyUpOrDown;
        self.playInstantlyButton.controlSize = NSControlSizeExtraLarge;
        [NSLayoutConstraint activateConstraints:@[
            [self.playInstantlyButton.widthAnchor constraintEqualToConstant:VLCLibraryUIUnits.mediumPlaybackControlButtonSize],
            [self.playInstantlyButton.heightAnchor constraintEqualToConstant:VLCLibraryUIUnits.mediumPlaybackControlButtonSize],
        ]];
#endif
    }

    if (@available(macOS 10.14, *)) {
        [NSApplication.sharedApplication addObserver:self
                                            forKeyPath:@"effectiveAppearance"
                                               options:NSKeyValueObservingOptionNew
                                               context:nil];
    }

    [self updateColoredAppearance:self.view.effectiveAppearance];
    [self prepareForReuse];
}

#pragma mark - dynamic appearance

- (void)observeValueForKeyPath:(NSString *)keyPath
                      ofObject:(id)object
                        change:(NSDictionary<NSKeyValueChangeKey,id> *)change
                       context:(void *)context
{
    if ([keyPath isEqualToString:@"effectiveAppearance"]) {
        NSAppearance *effectiveAppearance = change[NSKeyValueChangeNewKey];
        [self updateColoredAppearance:effectiveAppearance];
    }
}

- (void)updateColoredAppearance:(NSAppearance*)appearance
{
    NSParameterAssert(appearance);
    BOOL isDark = NO;
    if (@available(macOS 10.14, *)) {
        isDark = [appearance.name isEqualToString:NSAppearanceNameDarkAqua] || [appearance.name isEqualToString:NSAppearanceNameVibrantDark];
    }

    self.mediaTitleTextField.textColor = isDark ? NSColor.VLClibraryDarkTitleColor : NSColor.VLClibraryLightTitleColor;
}

#pragma mark - view representation

- (void)prepareForReuse
{
    [super prepareForReuse];
    _playInstantlyButton.hidden = YES;
    _mediaTitleTextField.stringValue = @"";
    _annotationTextField.hidden = YES;
    _mediaImageView.image = nil;
    _addToPlayQueueButton.hidden = NO;
    _highlightBox.hidden = YES;
}

- (void)setRepresentedInputItem:(VLCInputItem *)representedInputItem
{
    _representedInputItem = representedInputItem;
    [self updateRepresentation];
}

- (void)setSelected:(BOOL)selected
{
    super.selected = selected;
    _highlightBox.hidden = !selected;
}

- (void)updateRepresentation
{
    if (_representedInputItem == nil) {
        NSAssert(1, @"no input item assigned for collection view item", nil);
        return;
    }

    VLCInputItem * const inputItem = _representedInputItem;
    _mediaTitleTextField.stringValue = inputItem.name;

    __weak typeof(self) weakSelf = self;
    [VLCLibraryImageCache thumbnailForInputItem:inputItem
                                 withCompletion:^(NSImage * const thumbnail) {
        VLCMediaSourceCollectionViewItem * const strongSelf = weakSelf;
        if (!strongSelf || strongSelf->_representedInputItem != inputItem) {
            return;
        }
        strongSelf->_mediaImageView.image = thumbnail;
    }];


    switch (_representedInputItem.inputType) {
        case ITEM_TYPE_STREAM:
            _annotationTextField.stringValue = _NS("Stream");
            _annotationTextField.hidden = NO;
            break;

        case ITEM_TYPE_PLAYLIST:
            _annotationTextField.stringValue = _NS("Playlist");
            _annotationTextField.hidden = NO;
            break;

        case ITEM_TYPE_DISC:
            _annotationTextField.stringValue = _NS("Disk");
            _annotationTextField.hidden = NO;
            break;

        default:
            break;
    }
}

#pragma mark - actions

- (IBAction)playInstantly:(id)sender
{
    [VLCMain.sharedInstance.playQueueController addInputItem:_representedInputItem.vlcInputItem atPosition:-1 startPlayback:YES];
}

- (IBAction)addToPlayQueue:(id)sender
{
    [VLCMain.sharedInstance.playQueueController addInputItem:_representedInputItem.vlcInputItem atPosition:-1 startPlayback:NO];
}

- (void)openContextMenu:(NSEvent *)event
{
    if (!_menuController) {
        _menuController = [[VLCLibraryMenuController alloc] init];
    }

    NSCollectionView * const collectionView = self.collectionView;
    VLCMediaSourceDataSource * const dataSource =
        (VLCMediaSourceDataSource *)collectionView.dataSource;
    NSParameterAssert(dataSource != nil);
    NSSet<NSIndexPath *> * const indexPaths = collectionView.selectionIndexPaths;
    NSArray<VLCInputItem *> * const selectedInputItems =
        [dataSource mediaSourceInputItemsAtIndexPaths:indexPaths];
    const NSInteger mediaSourceItemIndex = [selectedInputItems indexOfObjectPassingTest:^BOOL(
        VLCInputItem * const inputItem, const NSUInteger __unused idx, BOOL * const __unused stop
    ) {
        return [inputItem.MRL isEqualToString:_representedInputItem.MRL];
    }];
    NSArray<VLCInputItem *> *items = nil;

    if (mediaSourceItemIndex == NSNotFound) {
        items = @[_representedInputItem];
    } else {
        items = selectedInputItems;
    }

    _menuController.representedInputItems = items;
    [_menuController popupMenuWithEvent:event forView:self.view];
}

- (void)mouseDown:(NSEvent *)event
{
    if (event.modifierFlags & NSControlKeyMask) {
        [self openContextMenu:event];
    } else if (event.modifierFlags & (NSShiftKeyMask | NSCommandKeyMask)) {
        self.selected = !self.selected;
    } else {
        [super mouseDown:event];
    }
}

- (void)rightMouseDown:(NSEvent *)event
{
    [self openContextMenu:event];
    [super rightMouseDown:event];
}

@end
