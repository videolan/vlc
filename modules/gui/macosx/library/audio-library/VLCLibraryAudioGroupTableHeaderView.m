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
    self.wantsLayer = YES;

    if (@available(macOS 10.14, *)) {
        NSVisualEffectView * const visualEffectView = [[NSVisualEffectView alloc] initWithFrame:self.bounds];
        visualEffectView.translatesAutoresizingMaskIntoConstraints = NO;
        visualEffectView.material = NSVisualEffectMaterialHeaderView;
        visualEffectView.blendingMode = NSVisualEffectBlendingModeWithinWindow;
        visualEffectView.state = NSVisualEffectStateFollowsWindowActiveState;
        [self addSubview:visualEffectView];
        self.backgroundView = visualEffectView;
    } else {
        NSView * const fallbackBackgroundView = [[NSView alloc] initWithFrame:self.bounds];
        fallbackBackgroundView.translatesAutoresizingMaskIntoConstraints = NO;
        fallbackBackgroundView.wantsLayer = YES;
        fallbackBackgroundView.layer.backgroundColor = NSColor.windowBackgroundColor.CGColor;
        [self addSubview:fallbackBackgroundView];
        self.backgroundView = fallbackBackgroundView;
    }

    self.titleField = [self buildLabelWithFont:NSFont.VLClibrarySectionHeaderFont
                                      textColor:NSColor.labelColor
                                      alignment:NSTextAlignmentLeft];
    self.detailField = [self buildLabelWithFont:NSFont.VLCLibrarySubsectionSubheaderFont
                                       textColor:NSColor.secondaryLabelColor
                                       alignment:NSTextAlignmentLeft];
    self.playButton = [self buildActionButtonWithTitle:_NS("Play") action:@selector(play:)];
    self.queueButton = [self buildActionButtonWithTitle:_NS("Queue") action:@selector(enqueue:)];

    NSStackView *labelsStack = [NSStackView stackViewWithViews:@[self.titleField, self.detailField]];
    labelsStack.translatesAutoresizingMaskIntoConstraints = NO;
    labelsStack.orientation = NSUserInterfaceLayoutOrientationVertical;
    labelsStack.alignment = NSLayoutAttributeLeading;
    labelsStack.spacing = VLCLibraryUIUnits.smallSpacing;
    [labelsStack setContentHuggingPriority:NSLayoutPriorityDefaultLow forOrientation:NSLayoutConstraintOrientationHorizontal];
    [labelsStack setContentCompressionResistancePriority:NSLayoutPriorityDefaultLow forOrientation:NSLayoutConstraintOrientationHorizontal];
    self.labelsStackView = labelsStack;

    NSStackView *buttonsStack = [NSStackView stackViewWithViews:@[self.playButton, self.queueButton]];
    buttonsStack.translatesAutoresizingMaskIntoConstraints = NO;
    buttonsStack.orientation = NSUserInterfaceLayoutOrientationHorizontal;
    buttonsStack.alignment = NSLayoutAttributeCenterY;
    buttonsStack.spacing = VLCLibraryUIUnits.smallSpacing;
    [buttonsStack setContentHuggingPriority:NSLayoutPriorityRequired forOrientation:NSLayoutConstraintOrientationHorizontal];
    [buttonsStack setContentCompressionResistancePriority:NSLayoutPriorityRequired forOrientation:NSLayoutConstraintOrientationHorizontal];
    self.buttonsStackView = buttonsStack;

    NSStackView *rootStack = [NSStackView new];
    rootStack.translatesAutoresizingMaskIntoConstraints = NO;
    rootStack.orientation = NSUserInterfaceLayoutOrientationHorizontal;
    rootStack.alignment = NSLayoutAttributeCenterY;
    rootStack.spacing = VLCLibraryUIUnits.largeSpacing;
    [rootStack addArrangedSubview:labelsStack];
    [rootStack addArrangedSubview:buttonsStack];
    self.rootStackView = rootStack;

    [self addSubview:rootStack];

    [NSLayoutConstraint activateConstraints:@[
        [self.backgroundView.topAnchor constraintEqualToAnchor:self.topAnchor],
        [self.backgroundView.leadingAnchor constraintEqualToAnchor:self.leadingAnchor],
        [self.backgroundView.trailingAnchor constraintEqualToAnchor:self.trailingAnchor],
        [self.backgroundView.bottomAnchor constraintEqualToAnchor:self.bottomAnchor],

        [rootStack.leadingAnchor constraintEqualToAnchor:self.leadingAnchor constant:VLCLibraryUIUnits.largeSpacing],
        [self.trailingAnchor constraintEqualToAnchor:rootStack.trailingAnchor constant:VLCLibraryUIUnits.largeSpacing],
        [rootStack.topAnchor constraintEqualToAnchor:self.topAnchor constant:VLCLibraryUIUnits.mediumSpacing],
        [self.bottomAnchor constraintEqualToAnchor:rootStack.bottomAnchor constant:VLCLibraryUIUnits.mediumSpacing],
    ]];

    self.layer.cornerRadius = VLCLibraryUIUnits.smallSpacing;
    self.layer.masksToBounds = YES;
    self.layer.borderWidth = VLCLibraryUIUnits.borderThickness;
    [self updateAppearance];
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

- (void)updateAppearance
{
    if (@available(macOS 10.14, *)) {
        NSAppearance *appearance = self.effectiveAppearance;
        BOOL isDark = NO;
        if ([appearance.name isEqualToString:NSAppearanceNameDarkAqua] ||
            [appearance.name isEqualToString:NSAppearanceNameVibrantDark]) {
            isDark = YES;
        }
        self.layer.borderColor = (isDark ? NSColor.VLCDarkSubtleBorderColor : NSColor.VLCLightSubtleBorderColor).CGColor;
    } else {
        self.layer.borderColor = NSColor.VLCLightSubtleBorderColor.CGColor;
    }
}

- (void)viewDidChangeEffectiveAppearance
{
    [super viewDidChangeEffectiveAppearance];
    [self updateAppearance];
}

- (void)layout
{
    [super layout];
    self.backgroundView.frame = self.bounds;
}

- (NSSize)intrinsicContentSize
{
    return NSMakeSize(NSViewNoIntrinsicMetric, VLCLibraryAudioGroupTableHeaderViewHeight);
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
