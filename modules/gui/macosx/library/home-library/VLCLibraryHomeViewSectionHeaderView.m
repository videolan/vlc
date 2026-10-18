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

#import "views/VLCUIUnits.h"

@interface VLCLibraryHomeViewSectionHeaderButtonCell : NSButtonCell
@end

@implementation VLCLibraryHomeViewSectionHeaderButtonCell

- (void)setHighlighted:(BOOL)highlighted
{
    [super setHighlighted:highlighted];
    self.controlView.alphaValue = highlighted ? 0.5 : 1.0;
}

@end

@interface VLCLibraryHomeViewSectionHeaderView ()

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
    _countField = [self labelWithFont:NSFont.VLCLibrarySubsectionSubheaderFont textColor:NSColor.secondaryLabelColor];
    _libraryButton = [NSButton buttonWithTitle:@"" target:nil action:nil];
    self.libraryButton.cell = [[VLCLibraryHomeViewSectionHeaderButtonCell alloc] initTextCell:@""];
    [self.libraryButton setButtonType:NSButtonTypeMomentaryPushIn];
    self.libraryButton.translatesAutoresizingMaskIntoConstraints = NO;
    self.libraryButton.bordered = NO;
    if (@available(macOS 10.14, *)) {
        self.libraryButton.contentTintColor = NSColor.labelColor;
    }
    self.libraryButton.font = NSFont.VLClibrarySectionHeaderFont;
    self.libraryButton.alignment = NSTextAlignmentNatural;
    self.libraryButton.lineBreakMode = NSLineBreakByTruncatingTail;
    self.libraryButton.imagePosition = NSImageTrailing;
    self.libraryButton.imageScaling = NSImageScaleProportionallyDown;
    NSImage *chevron = nil;
    if (@available(macOS 11.0, *)) {
        chevron = [NSImage imageWithSystemSymbolName:@"chevron.forward" accessibilityDescription:nil];
    }
    if (chevron == nil) {
        chevron = [NSImage imageNamed:self.userInterfaceLayoutDirection == NSUserInterfaceLayoutDirectionRightToLeft
                    ? NSImageNameGoLeftTemplate : NSImageNameGoRightTemplate];
    }
    self.libraryButton.image = chevron;
    [self.libraryButton setContentHuggingPriority:NSLayoutPriorityDefaultHigh
                                 forOrientation:NSLayoutConstraintOrientationHorizontal];
    [self.libraryButton setContentCompressionResistancePriority:NSLayoutPriorityDefaultLow
                                               forOrientation:NSLayoutConstraintOrientationHorizontal];
    [self.countField setContentCompressionResistancePriority:NSLayoutPriorityRequired
                                             forOrientation:NSLayoutConstraintOrientationHorizontal];
    [self addSubview:self.libraryButton];
    [self addSubview:self.countField];

    [NSLayoutConstraint activateConstraints:@[
        [self.libraryButton.leadingAnchor constraintEqualToAnchor:self.leadingAnchor],
        [self.libraryButton.trailingAnchor constraintLessThanOrEqualToAnchor:self.countField.leadingAnchor constant:-VLCUIUnits.largeSpacing],
        [self.libraryButton.topAnchor constraintEqualToAnchor:self.topAnchor],
        [self.libraryButton.bottomAnchor constraintEqualToAnchor:self.bottomAnchor],
        [self.countField.trailingAnchor constraintEqualToAnchor:self.trailingAnchor],
        [self.countField.centerYAnchor constraintEqualToAnchor:self.libraryButton.centerYAnchor],
        [self.countField.topAnchor constraintGreaterThanOrEqualToAnchor:self.topAnchor],
        [self.countField.bottomAnchor constraintLessThanOrEqualToAnchor:self.bottomAnchor],
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
    self.libraryButton.title = title;
    self.libraryButton.accessibilityLabel = [NSString stringWithFormat:_NS("Open %@ in the library"), title];
    self.libraryButton.toolTip = self.libraryButton.accessibilityLabel;
}

- (void)setItemCount:(NSUInteger)itemCount
{
    _itemCount = itemCount;
    self.countField.stringValue = _NPS("%lu item", "%lu items", (unsigned long)itemCount);
}

@end
