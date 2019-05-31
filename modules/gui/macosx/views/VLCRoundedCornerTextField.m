/*****************************************************************************
 * VLCRoundedCornerTextField.m: MacOS X interface module
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

#import "VLCRoundedCornerTextField.h"
#import "extensions/NSColor+VLCAdditions.h"

const CGFloat VLCRoundedCornerTextFieldLightCornerRadius = 3.;
const CGFloat VLCRoundedCornerTextFieldStrongCornerRadius = 10.;

@implementation VLCRoundedCornerTextField

- (instancetype)initWithCoder:(NSCoder *)coder
{
    self = [super initWithCoder:coder];
    if (self) {
        [self setupCustomAppearance];
    }
    return self;
}

- (instancetype)initWithFrame:(NSRect)frameRect
{
    self = [super initWithFrame:frameRect];
    if (self) {
        [self setupCustomAppearance];
    }
    return self;
}

- (void)setupCustomAppearance
{
    self.wantsLayer = YES;
    self.layer.cornerRadius = VLCRoundedCornerTextFieldLightCornerRadius;
    self.layer.masksToBounds = YES;
    self.layer.backgroundColor = [NSColor VLClibraryAnnotationBackgroundColor].CGColor;
}

- (void)setBackgroundColor:(NSColor *)backgroundColor
{
    self.layer.backgroundColor = backgroundColor.CGColor;
}

- (void)setUseStrongRounding:(BOOL)useStrongRounding
{
    _useStrongRounding = useStrongRounding;
    if (_useStrongRounding) {
        self.layer.cornerRadius = VLCRoundedCornerTextFieldStrongCornerRadius;
    } else {
        self.layer.cornerRadius = VLCRoundedCornerTextFieldLightCornerRadius;
    }
}

- (void)setStringValue:(NSString *)stringValue
{
    if (stringValue != nil) {
        [super setStringValue:[NSString stringWithFormat:@" %@ ", stringValue]];
    } else {
        [super setStringValue:@""];
    }
}

@end
