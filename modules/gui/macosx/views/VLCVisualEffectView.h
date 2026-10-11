/*****************************************************************************
 * VLCVisualEffectView.h: MacOS X interface module
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

#import <Cocoa/Cocoa.h>

NS_ASSUME_NONNULL_BEGIN

// A glass background on macOS 26 and later, with a visual-effect fallback.
// Add controls to contentView; its constraints determine this view's fitting size.
@interface VLCVisualEffectView : NSView

@property (readonly) NSView *contentView;
@property (readonly) BOOL usesGlassEffect;
// These configure the visual-effect fallback; glass uses the system material.
@property (nonatomic) NSVisualEffectMaterial material;
@property (nonatomic) NSVisualEffectBlendingMode blendingMode;

@property (nonatomic) CGFloat cornerRadius;

@end

NS_ASSUME_NONNULL_END
