/*****************************************************************************
 * VLCDefaultValueSlider.h: Custom NSSlider which allows a defaultValue
 *****************************************************************************
 * Copyright (C) 2016 VLC authors and VideoLAN
 *
 * Authors: Marvin Scholz <epirat07 -at- gmail -dot- com>
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

/**
 \c VLCDefaultValueSlider is a NSSlider subclass that allows setting
 a \c defaultValue which gets a tickmark and the knob snaps to that
 default value. Additionally a VLCDefaultValueSlider can be adjusted
 with the mouse scroll wheel, if enabled.
 */
@interface VLCDefaultValueSlider : NSSlider

/**
 Indicates if the slider is scrollable with the mouse or trackpad scrollwheel.
 */
@property (readwrite) BOOL isScrollable;

/**
 Sets the default value to which the slider will snap and draw a tickmark for.
 To unset the defaultValue, set it to \c DBL_MAX

 \note value must be in the value range of the slider and be smaller than \c DBL_MAX

 \param value The default value
 */
- (void)setDefaultValue:(double)value;

/**
 Get the default value

 \note It the returned value is \c DBL_MAX, there is no defaultValue set.
 */
- (double)defaultValue;

@end
