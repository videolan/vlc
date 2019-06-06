/*****************************************************************************
 * VLCSliderCell.h
 *****************************************************************************
 * Copyright (C) 2017 VLC authors and VideoLAN
 *
 * Authors: Marvin Scholz <epirat07 at gmail dot com>
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
#import <QuartzCore/QuartzCore.h>

@interface VLCSliderCell : NSSliderCell

// Colors
@property NSColor *gradientColor;
@property NSColor *gradientColor2;
@property NSColor *trackStrokeColor;
@property NSColor *filledTrackColor;
@property NSColor *knobFillColor;
@property NSColor *activeKnobFillColor;
@property NSColor *shadowColor;
@property NSColor *knobStrokeColor;
@property NSColor *highlightBackground;

// Gradients
@property NSGradient *trackGradient;
@property NSGradient *highlightGradient;

// Shadows
@property NSShadow *knobShadow;


@property NSInteger animationWidth;

@property (nonatomic, setter=setIndefinite:) BOOL indefinite;
@property (nonatomic, setter=setKnobHidden:) BOOL isKnobHidden;

- (void)setSliderStyleLight;
- (void)setSliderStyleDark;

@end
