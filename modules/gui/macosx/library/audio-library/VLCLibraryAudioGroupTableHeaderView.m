/*****************************************************************************
 * VLCLibraryAudioGroupTableHeaderView.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2025 VLC authors and VideoLAN
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

#import "VLCLibraryAudioGroupTableHeaderView.h"

#import "library/VLCLibraryRepresentedItem.h"
#import "library/VLCLibraryUIUnits.h"

#import "extensions/NSColor+VLCAdditions.h"
#import "extensions/NSFont+VLCAdditions.h"
#import "extensions/NSString+Helpers.h"

const CGFloat VLCLibraryAudioGroupTableHeaderViewHeight = 86.f;

@interface VLCLibraryAudioGroupTableHeaderView ()

@property NSView *backgroundView;
@property NSStackView *rootStackView;
@property NSStackView *labelsStackView;
@property NSStackView *buttonsStackView;
@property NSTextField *titleField;
@property NSTextField *detailField;
@property NSButton *playButton;
@property NSButton *queueButton;

@end

@implementation VLCLibraryAudioGroupTableHeaderView

- (instancetype)initWithFrame:(NSRect)frameRect
{
    self = [super initWithFrame:frameRect];
    if (self) {
        [self commonInit];
    }
    return self;
}

- (instancetype)initWithCoder:(NSCoder *)coder
{
    self = [super initWithCoder:coder];
    if (self) {
        [self commonInit];
    }
    return self;
}


- (void)commonInit
{
    NSView *contentHostView = self;
    const CGFloat backgroundTopInset = VLCLibraryUIUnits.largeSpacing + VLCLibraryUIUnits.mediumSpacing;
    CGFloat backgroundBottomInset = 0.f;

#if __MAC_OS_X_VERSION_MAX_ALLOWED >= 260000
    if (@available(macOS 26.0, *)) {
        NSGlassEffectView * const glassView = [[NSGlassEffectView alloc] initWithFrame:self.bounds];
        glassView.translatesAutoresizingMaskIntoConstraints = NO;
        NSView * const glassContentView = [[NSView alloc] initWithFrame:glassView.bounds];
        glassContentView.translatesAutoresizingMaskIntoConstraints = NO;
        glassView.contentView = glassContentView;
        self.backgroundView = glassView;
        contentHostView = glassContentView;
        backgroundBottomInset = VLCLibraryUIUnits.largeSpacing + VLCLibraryUIUnits.mediumSpacing + VLCLibraryUIUnits.smallSpacing;
    } else
#endif
    if (@available(macOS 10.14, *)) {
        NSVisualEffectView * const visualEffectView = [[NSVisualEffectView alloc] initWithFrame:self.bounds];
        visualEffectView.translatesAutoresizingMaskIntoConstraints = NO;
        visualEffectView.wantsLayer = YES;
        visualEffectView.material = NSVisualEffectMaterialPopover;
        visualEffectView.blendingMode = NSVisualEffectBlendingModeWithinWindow;
        self.backgroundView = visualEffectView;
        contentHostView = visualEffectView;
    } else {
        NSView * const fallbackBackgroundView = [[NSView alloc] initWithFrame:self.bounds];
        fallbackBackgroundView.translatesAutoresizingMaskIntoConstraints = NO;
        fallbackBackgroundView.wantsLayer = YES;
        fallbackBackgroundView.layer.backgroundColor = NSColor.windowBackgroundColor.CGColor;
        self.backgroundView = fallbackBackgroundView;
        contentHostView = fallbackBackgroundView;
    }

    [self addSubview:self.backgroundView];

    self.titleField = [self buildLabelWithFont:NSFont.VLClibrarySectionHeaderFont
                                      textColor:NSColor.labelColor
                                      alignment:NSTextAlignmentLeft];
    self.detailField = [self buildLabelWithFont:NSFont.VLCLibrarySubsectionSubheaderFont
                                       textColor:NSColor.secondaryLabelColor
                                       alignment:NSTextAlignmentLeft];
    self.playButton = [self buildActionButtonWithTitle:_NS("Play") action:@selector(play:)];
    self.queueButton = [self buildActionButtonWithTitle:_NS("Queue") action:@selector(enqueue:)];

    NSStackView * const labelsStack = [NSStackView stackViewWithViews:@[self.titleField, self.detailField]];
    labelsStack.translatesAutoresizingMaskIntoConstraints = NO;
    labelsStack.orientation = NSUserInterfaceLayoutOrientationVertical;
    labelsStack.alignment = NSLayoutAttributeLeading;
    labelsStack.spacing = VLCLibraryUIUnits.smallSpacing;
    [labelsStack setContentHuggingPriority:NSLayoutPriorityDefaultLow forOrientation:NSLayoutConstraintOrientationHorizontal];
    [labelsStack setContentCompressionResistancePriority:NSLayoutPriorityRequired forOrientation:NSLayoutConstraintOrientationHorizontal];
    self.labelsStackView = labelsStack;

    NSStackView * const buttonsStack = [NSStackView stackViewWithViews:@[self.playButton, self.queueButton]];
    buttonsStack.translatesAutoresizingMaskIntoConstraints = NO;
    buttonsStack.orientation = NSUserInterfaceLayoutOrientationHorizontal;
    buttonsStack.alignment = NSLayoutAttributeCenterY;
    buttonsStack.spacing = VLCLibraryUIUnits.smallSpacing;
    [buttonsStack setContentHuggingPriority:NSLayoutPriorityRequired forOrientation:NSLayoutConstraintOrientationHorizontal];
    [buttonsStack setContentCompressionResistancePriority:NSLayoutPriorityRequired forOrientation:NSLayoutConstraintOrientationHorizontal];
    self.buttonsStackView = buttonsStack;

    [contentHostView addSubview:labelsStack];
    [contentHostView addSubview:buttonsStack];

    const CGFloat horizontalContentInset = VLCLibraryUIUnits.mediumSpacing;

    [NSLayoutConstraint activateConstraints:@[
        [self.backgroundView.topAnchor constraintEqualToAnchor:self.topAnchor constant:backgroundTopInset],
        [self.backgroundView.leadingAnchor constraintEqualToAnchor:self.leadingAnchor],
        [self.backgroundView.trailingAnchor constraintEqualToAnchor:self.trailingAnchor],
        [self.backgroundView.bottomAnchor constraintEqualToAnchor:self.bottomAnchor constant:-backgroundBottomInset],
        [labelsStack.leadingAnchor constraintEqualToAnchor:contentHostView.leadingAnchor constant:horizontalContentInset],
        [labelsStack.centerYAnchor constraintEqualToAnchor:contentHostView.centerYAnchor],
        [contentHostView.trailingAnchor constraintEqualToAnchor:buttonsStack.trailingAnchor constant:horizontalContentInset],
        [buttonsStack.centerYAnchor constraintEqualToAnchor:contentHostView.centerYAnchor],
        [buttonsStack.leadingAnchor constraintGreaterThanOrEqualToAnchor:labelsStack.trailingAnchor constant:VLCLibraryUIUnits.largeSpacing],
    ]];

    if (@available(macOS 26.0, *)) {
    } else {
        self.backgroundView.layer.borderColor = NSColor.VLCSubtleBorderColor.CGColor;
        self.backgroundView.layer.borderWidth = VLCLibraryUIUnits.borderThickness;
        self.backgroundView.layer.cornerRadius = VLCLibraryUIUnits.cornerRadius;
        self.backgroundView.layer.masksToBounds = YES;

        if (@available(macOS 10.14, *)) {
            [NSApplication.sharedApplication addObserver:self
                                            forKeyPath:@"effectiveAppearance"
                                                options:NSKeyValueObservingOptionNew
                                                context:nil];
        }
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
        if (@available(macOS 10.14, *))  {
            NSAppearance * const appearance = change[NSKeyValueChangeNewKey];
            const BOOL isDark = [appearance.name isEqualToString:NSAppearanceNameDarkAqua] ||
                                [appearance.name isEqualToString:NSAppearanceNameVibrantDark];
            self.backgroundView.layer.borderColor = isDark ?
                NSColor.VLCDarkSubtleBorderColor.CGColor : NSColor.VLCLightSubtleBorderColor.CGColor;
        }
    }
}

- (NSTextField *)buildLabelWithFont:(NSFont *)font textColor:(NSColor *)color alignment:(NSTextAlignment)alignment
{
    NSTextField *label;
    if (@available(macOS 10.12, *)) {
        label = [NSTextField labelWithString:@""];
    } else {
        label = [[NSTextField alloc] initWithFrame:NSZeroRect];
        label.editable = NO;
        label.bezeled = NO;
        label.drawsBackground = NO;
        label.selectable = NO;
    }
    label.font = font;
    label.textColor = color;
    label.alignment = alignment;
    label.lineBreakMode = NSLineBreakByTruncatingTail;
    label.translatesAutoresizingMaskIntoConstraints = NO;
    return label;
}

- (NSButton *)buildActionButtonWithTitle:(NSString *)title action:(SEL)selector
{
    NSButton *button;
    if (@available(macOS 10.12, *)) {
        button = [NSButton buttonWithTitle:title target:self action:selector];
    } else {
        button = [[NSButton alloc] initWithFrame:NSZeroRect];
        button.title = title;
        button.target = self;
        button.action = selector;
    }
    button.bezelStyle = NSBezelStyleRounded;
    button.translatesAutoresizingMaskIntoConstraints = NO;
    if (@available(macOS 10.14, *))
        button.contentTintColor = NSColor.VLCAccentColor;
    return button;
}

- (void)setRepresentedItem:(VLCLibraryRepresentedItem *)representedItem
{
    _representedItem = representedItem;
    [self applyRepresentedItemWithFallbackTitle:nil fallbackDetail:nil];
}

- (void)applyRepresentedItemWithFallbackTitle:(NSString *)fallbackTitle fallbackDetail:(NSString *)fallbackDetail
{
    id<VLCMediaLibraryItemProtocol> const item = self.representedItem.item;
    if (item == nil) {
        self.titleField.stringValue = fallbackTitle ?: @"";
        self.detailField.stringValue = fallbackDetail ?: @"";
        self.playButton.enabled = NO;
        self.queueButton.enabled = NO;
        return;
    }

    self.titleField.stringValue = item.displayString ?: fallbackTitle ?: @"";
    self.detailField.stringValue = item.primaryDetailString ?: fallbackDetail ?: @"";
    self.playButton.enabled = YES;
    self.queueButton.enabled = YES;
}

- (void)updateWithRepresentedItem:(VLCLibraryRepresentedItem *)representedItem
                    fallbackTitle:(NSString *)fallbackTitle
                   fallbackDetail:(NSString *)fallbackDetail
{
    self.representedItem = representedItem;
    [self applyRepresentedItemWithFallbackTitle:fallbackTitle fallbackDetail:fallbackDetail];
}

#pragma mark - Actions

- (void)play:(id)sender
{
    [self.representedItem play];
}

- (void)enqueue:(id)sender
{
    [self.representedItem queue];
}

@end
