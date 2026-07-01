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

#import "extensions/NSColor+VLCAdditions.h"
#import "extensions/NSFont+VLCAdditions.h"
#import "extensions/NSString+Helpers.h"

#import "library/VLCLibraryUIUnits.h"

#import "views/VLCImageView.h"
#import "views/VLCLinearProgressIndicator.h"
#import "views/VLCTrackingView.h"

NSString *VLCMediaItemCollectionViewItemIdentifier = @"VLCMediaItemCollectionViewItemIdentifier";

const CGFloat VLCMediaItemCollectionViewItemMinimalDisplayedProgress = 0.05;
const CGFloat VLCMediaItemCollectionViewItemMaximumDisplayedProgress = 0.95;

@implementation VLCMediaItemCollectionViewItem

- (instancetype)init
{
    return [self initWithNibName:nil bundle:nil];
}

- (instancetype)initWithNibName:(NSNibName)nibNameOrNil bundle:(NSBundle *)nibBundleOrNil
{
    self = [super initWithNibName:nibNameOrNil bundle:nibBundleOrNil];
    if (self) {
        if (nibNameOrNil == nil) {
            NSBundle * const bundle = [NSBundle bundleForClass:self.class];
            [bundle loadNibNamed:@"VLCMediaItemCollectionViewItem" owner:self topLevelObjects:nil];
        }
    }
    return self;
}

- (void)awakeFromNib
{
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

- (void)updateColoredAppearance:(NSAppearance *)appearance
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
    self.mediaTitleTextField.stringValue = @"";
    self.mediaImageView.image = nil;
    self.annotationTextField.hidden = YES;
    self.secondaryInfoTextField.stringValue = [NSString stringWithTime:0];
    self.secondaryInfoTextField.hidden = YES;
    self.unplayedIndicatorTextField.hidden = YES;
    self.progressIndicator.hidden = YES;
    self.highlightBox.hidden = YES;
}

- (void)setSelected:(BOOL)selected
{
    [super setSelected:selected];
    self.highlightBox.hidden = !selected;
}

- (void)rightMouseDown:(NSEvent *)event
{
    [self openContextMenu:event];
    [super rightMouseDown:event];
}

- (void)openContextMenu:(NSEvent *)event
{
    NSAssert(NO, @"Subclasses must override openContextMenu:");
}

@end
