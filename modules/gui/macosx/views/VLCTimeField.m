/*****************************************************************************
 * VLCTimeField.m: NSButton subclass for playback time fields
 *****************************************************************************
 * Copyright (C) 2003-2017 VLC authors and VideoLAN
 *
 * Authors: Jon Lech Johansen <jon-vl@nanocrew.net>
 *          Felix Paul Kühne <fkuehne at videolan dot org>
 *          Marvin Scholz <epirat07 at gmail dot com>
 *          Claudio Cambra <developer at claudiocambra dot com>
 *          Serhii Bykov <esphynox@gmail.com>
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

#import "VLCTimeField.h"

#import "main/VLCMain.h"
#import "menus/VLCMainMenu.h"

NSString * const VLCTimeFieldDisplayTimeAsElapsed = @"DisplayTimeAsTimeElapsed";
NSString * const VLCTimeFieldDisplayTimeAsRemaining = @"DisplayTimeAsTimeRemaining";

@interface VLCTimeField ()
{
    NSString *_cachedTime;
    NSString *_remainingTime;
    NSFont *_titleFont;
    NSColor *_titleColor;
}
@end

@implementation VLCTimeField

+ (void)initialize
{
    NSDictionary * const appDefaults = @{VLCTimeFieldDisplayTimeAsRemaining : @NO};
    [NSUserDefaults.standardUserDefaults registerDefaults:appDefaults];
}

- (instancetype)initWithFrame:(NSRect)frameRect
{
    self = [super initWithFrame:frameRect];
    if (self) {
        [self setup];
    }
    return self;
}

- (instancetype)initWithCoder:(NSCoder *)coder
{
    self = [super initWithCoder:coder];
    if (self) {
        [self setup];
    }
    return self;
}

- (void)setup
{
    NSDictionary<NSAttributedStringKey, id> * const initialAttributes =
        self.attributedTitle.length > 0 ? [self.attributedTitle attributesAtIndex:0 effectiveRange:nil] : nil;
    _titleFont = initialAttributes[NSFontAttributeName] ?: self.cell.font;
    _titleColor = initialAttributes[NSForegroundColorAttributeName] ?: NSColor.labelColor;

    self.bordered = NO;
    [self setButtonType:NSButtonTypeMomentaryChange];
    self.bezelStyle = NSBezelStyleRegularSquare;
    self.focusRingType = NSFocusRingTypeNone;
    self.imagePosition = NSNoImage;
    self.cell.lineBreakMode = NSLineBreakByClipping;
    self.title = @"";
    [self updateDisplayedTitle];
}

- (BOOL)acceptsFirstMouse:(NSEvent *)event
{
    return YES;
}

- (BOOL)mouseDownCanMoveWindow
{
    return NO;
}

- (void)setPreferencesIdentifier:(NSString *)preferencesIdentifier
{
    _preferencesIdentifier = preferencesIdentifier;
    self.isTimeRemaining = [NSUserDefaults.standardUserDefaults
                            boolForKey:self.preferencesIdentifier];
}

- (void)setIsTimeRemaining:(BOOL)isTimeRemaining
{
    _isTimeRemaining = isTimeRemaining;

    if (self.identifier) {
        [NSUserDefaults.standardUserDefaults setBool:_isTimeRemaining
                                              forKey:self.preferencesIdentifier];
    }

    [self updateDisplayedTitle];
}

- (NSTextAlignment)alignment
{
    return self.cell.alignment;
}

- (void)setAlignment:(NSTextAlignment)alignment
{
    self.cell.alignment = alignment;
    [self updateDisplayedTitle];
}

- (void)mouseDown:(NSEvent *)event
{
    if (event.clickCount > 1) {
        [VLCMain.sharedInstance.mainMenu goToSpecificTime:nil];
    } else {
        self.isTimeRemaining = !self.isTimeRemaining;
    }
}

- (void)setTime:(NSString *)time withRemainingTime:(NSString *)remainingTime
{
    _cachedTime = time;
    _remainingTime = remainingTime;

    [self updateDisplayedTitle];
}

- (void)updateDisplayedTitle
{
    NSString * const title =
        self.timeRemaining ? (_remainingTime ?: @"") : (_cachedTime ?: @"");

    NSMutableParagraphStyle * const paragraphStyle = [[NSMutableParagraphStyle alloc] init];
    paragraphStyle.alignment = self.alignment;
    paragraphStyle.lineBreakMode = NSLineBreakByClipping;

    NSDictionary<NSAttributedStringKey, id> * const attributes = @{
        NSFontAttributeName : _titleFont ?: [NSFont systemFontOfSize:NSFont.smallSystemFontSize],
        NSForegroundColorAttributeName : _titleColor ?: NSColor.labelColor,
        NSParagraphStyleAttributeName : paragraphStyle
    };

    self.attributedTitle = [[NSAttributedString alloc] initWithString:title
                                                           attributes:attributes];
}

- (BOOL)timeRemaining
{
    if (self.preferencesIdentifier) {
        return [NSUserDefaults.standardUserDefaults boolForKey:self.preferencesIdentifier];
    } else {
        return _isTimeRemaining;
    }
}

@end
