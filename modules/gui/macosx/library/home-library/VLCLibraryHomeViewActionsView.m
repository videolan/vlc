/*****************************************************************************
 * VLCLibraryHomeViewActionsViewController.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2024 VLC authors and VideoLAN
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

#import "VLCLibraryHomeViewActionsView.h"

#import "extensions/NSColor+VLCAdditions.h"
#import "extensions/NSFont+VLCAdditions.h"
#import "extensions/NSImage+VLCAdditions.h"
#import "extensions/NSString+Helpers.h"
#import "main/VLCMain.h"
#import "menus/VLCMainMenu.h"
#import "views/VLCUIUnits.h"

@interface VLCLibraryHomeViewActionsView ()

@property (nonatomic) BOOL dragHighlighted;
@property (nonatomic, weak) IBOutlet NSStackView *introductionStackView;
@property (nonatomic, weak) IBOutlet NSStackView *actionGridStackView;

@end

@implementation VLCLibraryHomeViewActionsView

- (void)dealloc
{
    [NSNotificationCenter.defaultCenter removeObserver:self];
}

- (void)awakeFromNib
{
    [super awakeFromNib];

    [self setupLayout];

    self.dropTarget = self;
    self.logoImageView.image = NSImage.VLCAppIconImage;
    self.dropTitleTextField.stringValue = _NS("Drop anything here");
    [self updateDropTitleFont];
    self.dropDetailTextField.stringValue = _NS("Files, folders, discs and links.");
    self.dropHintTextField.stringValue = _NS("Your content will be added to the play queue.");
    self.dropDetailTextField.font = [NSFont systemFontOfSize:NSFont.systemFontSize];
    self.dropHintTextField.font = [NSFont systemFontOfSize:NSFont.systemFontSize];
    self.dropDetailTextField.textColor = NSColor.secondaryLabelColor;
    self.dropHintTextField.textColor = NSColor.secondaryLabelColor;
    for (NSTextField * const textField in @[self.dropTitleTextField,
                                           self.dropDetailTextField,
                                           self.dropHintTextField]) {
        textField.maximumNumberOfLines = 0;
        textField.postsFrameChangedNotifications = YES;
        [NSNotificationCenter.defaultCenter addObserver:self
                                               selector:@selector(dropTextFieldFrameChanged:)
                                                   name:NSViewFrameDidChangeNotification
                                                 object:textField];
    }

    self.openFileButton.title = _NS("Open file");
    self.openDiscButton.title = _NS("Open disc");
    self.openUrlButton.title = _NS("Open stream");
    self.openCaptureDeviceButton.title = _NS("Open capture device");
    for (NSButton * const button in @[self.openFileButton,
                                      self.openDiscButton,
                                      self.openUrlButton,
                                      self.openCaptureDeviceButton]) {
        button.accessibilityLabel = button.title;
        button.toolTip = button.title;
    }

    self.accessibilityLabel = self.dropTitleTextField.stringValue;
    self.accessibilityHelp = self.dropDetailTextField.stringValue;
}

- (void)setupLayout
{
    const CGFloat horizontalInset = VLCUIUnits.extraLargeSpacing;
    const CGFloat verticalInset = VLCUIUnits.largeSpacing;
    const CGFloat minimumHeight = VLCUIUnits.libraryHomeActionsMinimumHeight;
    NSLayoutConstraint * const preferredHeight = [self.heightAnchor constraintEqualToConstant:minimumHeight];
    preferredHeight.priority = NSLayoutPriorityDefaultLow;

    // Prefer the logo's natural size without allowing it to enlarge the window.
    [self.logoImageView setContentCompressionResistancePriority:NSLayoutPriorityDragThatCannotResizeWindow
                                                 forOrientation:NSLayoutConstraintOrientationHorizontal];
    [self.logoImageView setContentCompressionResistancePriority:NSLayoutPriorityDragThatCannotResizeWindow
                                                 forOrientation:NSLayoutConstraintOrientationVertical];

    // Wrapped text can grow taller than the square logo.
    [self.introductionStackView setHuggingPriority:NSLayoutPriorityDefaultLow
                                    forOrientation:NSLayoutConstraintOrientationVertical];

    NSLayoutConstraint * const logoMaximumWidth =
        [self.logoImageView.widthAnchor constraintLessThanOrEqualToConstant:VLCUIUnits.largeIllustrationSize];

    [NSLayoutConstraint activateConstraints:@[
        [self.heightAnchor constraintGreaterThanOrEqualToConstant:minimumHeight],
        preferredHeight,
        logoMaximumWidth,
        [self.logoImageView.widthAnchor constraintLessThanOrEqualToAnchor:self.introductionStackView.widthAnchor
                                                               multiplier:VLCUIUnits.libraryHomeActionsLogoWidthMultiplier],
        [self.introductionStackView.leadingAnchor constraintEqualToAnchor:self.leadingAnchor constant:horizontalInset],
        [self.actionGridStackView.trailingAnchor constraintEqualToAnchor:self.trailingAnchor constant:-horizontalInset],
        [self.actionGridStackView.leadingAnchor constraintEqualToAnchor:self.introductionStackView.trailingAnchor
                                                               constant:horizontalInset],
        [self.actionGridStackView.widthAnchor constraintEqualToAnchor:self.widthAnchor
                                                           multiplier:VLCUIUnits.libraryHomeActionsGridWidthMultiplier
                                                             constant:-horizontalInset],
    ]];

    for (NSStackView * const stackView in @[self.introductionStackView, self.actionGridStackView]) {
        [NSLayoutConstraint activateConstraints:@[
            [stackView.centerYAnchor constraintEqualToAnchor:self.centerYAnchor],
            [stackView.topAnchor constraintGreaterThanOrEqualToAnchor:self.topAnchor constant:verticalInset],
            [stackView.bottomAnchor constraintLessThanOrEqualToAnchor:self.bottomAnchor constant:-verticalInset],
        ]];
    }
    for (NSStackView * const row in self.actionGridStackView.arrangedSubviews) {
        row.spacing = VLCUIUnits.mediumSpacing + VLCUIUnits.smallSpacing;
        for (NSView * const button in row.arrangedSubviews) {
            [button.heightAnchor constraintEqualToConstant:VLCUIUnits.largeActionButtonHeight].active = YES;
        }
    }
}

- (void)updateDropTitleFont
{
    NSFont * const titleFont = NSWidth(self.bounds) < VLCUIUnits.libraryHomeActionsNarrowWidthThreshold
        ? NSFont.VLCLibraryCompactSectionHeaderFont
        : NSFont.VLClibrarySectionHeaderFont;
    if (![self.dropTitleTextField.font isEqual:titleFont]) {
        self.dropTitleTextField.font = titleFont;
    }
}

- (void)dropTextFieldFrameChanged:(NSNotification *)notification
{
    [self updateDropTitleFont];

    // Ancestor stack layout can resize labels without laying out this view again.
    NSTextField * const textField = notification.object;
    const CGFloat width = NSWidth([textField alignmentRectForFrame:textField.frame]);
    if (width > 0 && textField.preferredMaxLayoutWidth != width) {
        textField.preferredMaxLayoutWidth = width;
        [textField invalidateIntrinsicContentSize];
    }
}

- (void)drawRect:(NSRect)dirtyRect
{
    [super drawRect:dirtyRect];

    NSBezierPath * const border =
        [NSBezierPath bezierPathWithRoundedRect:NSInsetRect(self.bounds, VLCUIUnits.borderThickness,
                                                                        VLCUIUnits.borderThickness)
                                        xRadius:VLCUIUnits.largeSpacing
                                        yRadius:VLCUIUnits.largeSpacing];
    NSColor * const fillColor = self.dragHighlighted
        ? NSColor.VLCDropTargetHighlightColor
        : NSColor.VLCDropTargetBackgroundColor;
    [fillColor setFill];
    [border fill];

    NSColor * const borderColor = self.dragHighlighted
        ? NSColor.VLCAccentColor
        : NSColor.VLCDropTargetBorderColor;
    [borderColor setStroke];
    border.lineWidth = self.dragHighlighted ? VLCUIUnits.borderThickness * 2. : VLCUIUnits.borderThickness;
    const CGFloat dashPattern[] = {VLCUIUnits.dashedBorderDashLength,
                                   VLCUIUnits.dashedBorderDashSpacing};
    [border setLineDash:dashPattern count:sizeof(dashPattern) / sizeof(dashPattern[0]) phase:0.];
    [border stroke];
}

- (void)setDragHighlighted:(BOOL)dragHighlighted
{
    if (_dragHighlighted == dragHighlighted) {
        return;
    }
    _dragHighlighted = dragHighlighted;
    self.needsDisplay = YES;
}

- (NSDragOperation)draggingEntered:(id<NSDraggingInfo>)sender
{
    const NSDragOperation operation = [super draggingEntered:sender];
    self.dragHighlighted = operation != NSDragOperationNone;
    return operation;
}

- (NSDragOperation)draggingUpdated:(id<NSDraggingInfo>)sender
{
    return [self draggingEntered:sender];
}

- (void)draggingExited:(id<NSDraggingInfo>)sender
{
    self.dragHighlighted = NO;
}

- (void)draggingEnded:(id<NSDraggingInfo>)sender
{
    self.dragHighlighted = NO;
}

- (void)concludeDragOperation:(id<NSDraggingInfo>)sender
{
    self.dragHighlighted = NO;
}

- (BOOL)performDragOperation:(id<NSDraggingInfo>)sender
{
    self.dragHighlighted = NO;
    return [super performDragOperation:sender];
}

- (BOOL)handlePasteBoardFromDragSession:(NSPasteboard *)pasteboard
{
    return [VLCFileDragRecognisingView handlePasteboardFromDragSessionAsPlayQueueItems:pasteboard];
}

- (void)viewDidChangeEffectiveAppearance
{
    [super viewDidChangeEffectiveAppearance];
    self.needsDisplay = YES;
}

- (IBAction)openFileAction:(id)sender
{
    [VLCMain.sharedInstance.mainMenu intfOpenFile:self];
}

- (IBAction)openDiscAction:(id)sender
{
    [VLCMain.sharedInstance.mainMenu intfOpenDisc:self];
}

- (IBAction)openUrlAction:(id)sender
{
    [VLCMain.sharedInstance.mainMenu intfOpenNet:self];
}

- (IBAction)openCaptureDeviceAction:(id)sender
{
    [VLCMain.sharedInstance.mainMenu intfOpenCapture:self];
}

@end
