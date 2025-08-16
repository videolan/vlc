/*****************************************************************************
 * VLCLibraryAudioGroupHeaderView.m: MacOS X interface module
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

#import "VLCLibraryAudioGroupHeaderView.h"

#import "extensions/NSColor+VLCAdditions.h"
#import "extensions/NSString+Helpers.h"
#import "extensions/NSView+VLCAdditions.h"

#import "main/VLCMain.h"

#import "library/VLCLibraryController.h"
#import "library/VLCLibraryDataTypes.h"
#import "library/VLCLibraryRepresentedItem.h"
#import "library/VLCLibraryUIUnits.h"

NSString * const VLCLibraryAudioGroupHeaderViewIdentifier = @"VLCLibraryAudioGroupHeaderViewIdentifier";

@interface VLCLibraryAudioGroupHeaderView ()

#if __MAC_OS_X_VERSION_MAX_ALLOWED >= 260000
@property NSGlassEffectView *glassBackgroundView API_AVAILABLE(macos(26.0));
#endif

@end

@implementation VLCLibraryAudioGroupHeaderView

+ (CGSize)defaultHeaderSize
{
    return CGSizeMake(690., 86.);
}

- (void)awakeFromNib
{
    [super awakeFromNib];

    if (@available(macOS 10.14, *))
        _playButton.bezelColor = NSColor.VLCAccentColor;

    if (@available(macOS 26.0, *)) {
#if __MAC_OS_X_VERSION_MAX_ALLOWED >= 260000
        self.glassBackgroundView = [[NSGlassEffectView alloc] initWithFrame:self.backgroundEffectView.frame];
        self.glassBackgroundView.translatesAutoresizingMaskIntoConstraints = NO;

        [self addSubview:self.glassBackgroundView
              positioned:NSWindowBelow
              relativeTo:self.backgroundEffectView];
        [self.glassBackgroundView applyConstraintsToFillSuperview];
        [self.backgroundEffectView removeFromSuperview];
        [self.stackView removeFromSuperview];
        self.glassBackgroundView.contentView = self.stackView;
#endif
    } else {
        if (@available(macOS 10.14, *)) {
            [NSApplication.sharedApplication addObserver:self
                                            forKeyPath:@"effectiveAppearance"
                                                options:NSKeyValueObservingOptionNew
                                                context:nil];
        }
        self.backgroundEffectView.wantsLayer = YES;
        self.backgroundEffectView.layer.cornerRadius = VLCLibraryUIUnits.smallSpacing;
        self.backgroundEffectView.layer.borderWidth = VLCLibraryUIUnits.borderThickness;
        [self updateColoredAppearance:self.effectiveAppearance];
    }
}

- (void)observeValueForKeyPath:(NSString *)keyPath
                      ofObject:(id)object
                        change:(NSDictionary<NSKeyValueChangeKey,id> *)change
                       context:(void *)context
{
    if (@available(macOS 26.0, *)) {
        return;
    } else if ([keyPath isEqualToString:@"effectiveAppearance"]) {
        NSAppearance * const effectiveAppearance = change[NSKeyValueChangeNewKey];
        [self updateColoredAppearance:effectiveAppearance];
    }
}

- (void)updateColoredAppearance:(NSAppearance *)appearance
{
    if (@available(macOS 26.0, *))
        return;

    NSParameterAssert(appearance);
    BOOL isDark = NO;
    if (@available(macOS 10.14, *)) {
        isDark = [appearance.name isEqualToString:NSAppearanceNameDarkAqua] || 
                 [appearance.name isEqualToString:NSAppearanceNameVibrantDark];
    }

    self.backgroundEffectView.layer.borderColor = isDark ?
        NSColor.VLCDarkSubtleBorderColor.CGColor : NSColor.VLCLightSubtleBorderColor.CGColor;
}


- (void)updateRepresentation
{
    const id<VLCMediaLibraryItemProtocol> actualItem = self.representedItem.item;
    if (actualItem == nil) {
        _titleTextField.stringValue = _NS("Unknown");
        _detailTextField.stringValue = _NS("Unknown");
        return;
    }

    _titleTextField.stringValue = actualItem.displayString;
    _detailTextField.stringValue = actualItem.primaryDetailString;
}

- (void)setRepresentedItem:(VLCLibraryRepresentedItem *)representedItem
{
    if (representedItem == _representedItem) {
        return;
    }

    _representedItem = representedItem;
    [self updateRepresentation];
}

- (IBAction)play:(id)sender
{
    [self.representedItem play];
}

- (IBAction)enqueue:(id)sender
{
    [self.representedItem queue];
}

@end
