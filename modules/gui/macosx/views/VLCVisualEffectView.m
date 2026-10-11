/*****************************************************************************
 * VLCVisualEffectView.m: MacOS X interface module
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

#import "VLCVisualEffectView.h"

#import "extensions/NSView+VLCAdditions.h"
#import "main/CompatibilityFixes.h"

@interface VLCVisualEffectView ()

@property (readonly) NSView *effectView;
@property (readonly, nullable) NSVisualEffectView *visualEffectView;

@end

@implementation VLCVisualEffectView

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
    _contentView = [[NSView alloc] initWithFrame:self.bounds];
    self.contentView.translatesAutoresizingMaskIntoConstraints = NO;
    self.wantsLayer = YES;
    self.layer.masksToBounds = NO;
    if (@available(macOS 14.0, *)) {
        self.clipsToBounds = NO;
    }

    if (@available(macOS 10.14, *)) {
        _material = NSVisualEffectMaterialPopover;
    } else {
        _material = NSVisualEffectMaterialSidebar;
    }
    _blendingMode = NSVisualEffectBlendingModeWithinWindow;

#if __MAC_OS_X_VERSION_MAX_ALLOWED >= 260000
    if (@available(macOS 26.0, *)) {
        NSGlassEffectView * const glassView = [[NSGlassEffectView alloc] initWithFrame:self.bounds];
        glassView.contentView = self.contentView;
        _cornerRadius = glassView.cornerRadius;
        _effectView = glassView;
        _usesGlassEffect = YES;
    } else
#endif
    {
        _visualEffectView = [[NSVisualEffectView alloc] initWithFrame:self.bounds];
        self.visualEffectView.material = self.material;
        self.visualEffectView.blendingMode = self.blendingMode;
        self.visualEffectView.wantsLayer = YES;
        [self.visualEffectView addSubview:self.contentView];
        [self.contentView applyConstraintsToFillSuperview];
        _effectView = self.visualEffectView;
    }

    self.effectView.translatesAutoresizingMaskIntoConstraints = NO;
    [self addSubview:self.effectView];
    [self.effectView applyConstraintsToFillSuperview];
}

- (void)setMaterial:(NSVisualEffectMaterial)material
{
    _material = material;
    self.visualEffectView.material = material;
}

- (void)setBlendingMode:(NSVisualEffectBlendingMode)blendingMode
{
    _blendingMode = blendingMode;
    self.visualEffectView.blendingMode = blendingMode;
}

- (void)setCornerRadius:(CGFloat)cornerRadius
{
    _cornerRadius = cornerRadius;
#if __MAC_OS_X_VERSION_MAX_ALLOWED >= 260000
    if (@available(macOS 26.0, *)) {
        ((NSGlassEffectView *)self.effectView).cornerRadius = cornerRadius;
        return;
    }
#endif
    self.visualEffectView.layer.cornerRadius = cornerRadius;
    self.visualEffectView.layer.masksToBounds = YES;
    self.layer.cornerRadius = cornerRadius;
}

@end
