/*****************************************************************************
 * VLCLibraryHomeViewSectionHeaderView.m: MacOS X interface module
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

#import "VLCLibraryHomeViewSectionHeaderView.h"

#import "extensions/NSFont+VLCAdditions.h"
#import "extensions/NSString+Helpers.h"
#import "extensions/NSView+VLCAdditions.h"

#import "views/VLCUIUnits.h"
#import "views/VLCVisualEffectView.h"

@interface VLCLibraryHomeViewSectionHeaderView ()

@property (readonly) NSTextField *titleField;
@property (readonly) NSTextField *countField;

@end

@implementation VLCLibraryHomeViewSectionHeaderView

- (instancetype)initWithFrame:(NSRect)frameRect
{
    self = [super initWithFrame:frameRect];
    if (self) {
        [self setupView];
    }
    return self;
}

- (instancetype)initWithCoder:(NSCoder *)coder
{
    self = [super initWithCoder:coder];
    if (self) {
        [self setupView];
    }
    return self;
}

- (void)setupView
{
    VLCVisualEffectView * const backgroundView = [[VLCVisualEffectView alloc] initWithFrame:self.bounds];
    if (!backgroundView.usesGlassEffect) {
        backgroundView.cornerRadius = VLCUIUnits.cornerRadius;
    }
    backgroundView.translatesAutoresizingMaskIntoConstraints = NO;
    [self addSubview:backgroundView];
    [backgroundView applyConstraintsToFillSuperview];
    NSView * const contentView = backgroundView.contentView;

    _titleField = [self labelWithFont:NSFont.VLClibrarySectionHeaderFont textColor:NSColor.labelColor];
    _countField = [self labelWithFont:NSFont.VLCLibrarySubsectionSubheaderFont textColor:NSColor.secondaryLabelColor];
    _libraryButton = [NSButton buttonWithTitle:_NS("Show all") target:nil action:nil];
    self.libraryButton.translatesAutoresizingMaskIntoConstraints = NO;
    self.libraryButton.bezelStyle = NSBezelStyleRounded;
    [self.libraryButton setContentHuggingPriority:NSLayoutPriorityRequired
                                 forOrientation:NSLayoutConstraintOrientationHorizontal];
    [self.libraryButton setContentCompressionResistancePriority:NSLayoutPriorityRequired
                                               forOrientation:NSLayoutConstraintOrientationHorizontal];

    NSStackView * const labelsView = [NSStackView stackViewWithViews:@[self.titleField, self.countField]];
    labelsView.translatesAutoresizingMaskIntoConstraints = NO;
    labelsView.orientation = NSUserInterfaceLayoutOrientationVertical;
    labelsView.alignment = NSLayoutAttributeLeading;
    labelsView.spacing = VLCUIUnits.smallSpacing;
    [labelsView setContentHuggingPriority:NSLayoutPriorityDefaultLow
                         forOrientation:NSLayoutConstraintOrientationHorizontal];
    [labelsView setContentCompressionResistancePriority:NSLayoutPriorityDefaultLow
                                       forOrientation:NSLayoutConstraintOrientationHorizontal];
    [contentView addSubview:labelsView];
    [contentView addSubview:self.libraryButton];

    [NSLayoutConstraint activateConstraints:@[
        [labelsView.leadingAnchor constraintEqualToAnchor:contentView.leadingAnchor constant:VLCUIUnits.mediumSpacing],
        [labelsView.topAnchor constraintEqualToAnchor:contentView.topAnchor constant:VLCUIUnits.smallSpacing],
        [labelsView.bottomAnchor constraintEqualToAnchor:contentView.bottomAnchor constant:-VLCUIUnits.smallSpacing],
        [self.libraryButton.leadingAnchor constraintGreaterThanOrEqualToAnchor:labelsView.trailingAnchor constant:VLCUIUnits.largeSpacing],
        [self.libraryButton.trailingAnchor constraintEqualToAnchor:contentView.trailingAnchor constant:-VLCUIUnits.mediumSpacing],
        [self.libraryButton.centerYAnchor constraintEqualToAnchor:contentView.centerYAnchor],
        [self.libraryButton.topAnchor constraintGreaterThanOrEqualToAnchor:contentView.topAnchor constant:VLCUIUnits.smallSpacing],
        [self.libraryButton.bottomAnchor constraintLessThanOrEqualToAnchor:contentView.bottomAnchor constant:-VLCUIUnits.smallSpacing],
    ]];
}

- (NSTextField *)labelWithFont:(NSFont *)font textColor:(NSColor *)textColor
{
    NSTextField * const label = [NSTextField labelWithString:@""];
    label.translatesAutoresizingMaskIntoConstraints = NO;
    label.font = font;
    label.textColor = textColor;
    label.maximumNumberOfLines = 1;
    label.lineBreakMode = NSLineBreakByTruncatingTail;
    [label setContentCompressionResistancePriority:NSLayoutPriorityDefaultLow
                                    forOrientation:NSLayoutConstraintOrientationHorizontal];
    return label;
}

- (void)setTitle:(NSString *)title
{
    _title = [title copy];
    self.titleField.stringValue = title;
    self.titleField.toolTip = title;
    self.libraryButton.accessibilityLabel = [NSString stringWithFormat:_NS("Open %@ in the library"), title];
    self.libraryButton.toolTip = self.libraryButton.accessibilityLabel;
}

- (void)setItemCount:(NSUInteger)itemCount
{
    _itemCount = itemCount;
    self.countField.stringValue = _NPS("%lu item", "%lu items", (unsigned long)itemCount);
}

@end
