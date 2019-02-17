/*****************************************************************************
 * VLCDefaultValueSliderCell.h: SliderCell subclass for VLCDefaultValueSlider
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
 \c VLCDefaultValueSliderCell is the cell use by the
 \c VLCDefaultValueSlider class.
 */
@interface VLCDefaultValueSliderCell : NSSliderCell

/**
 Indicates if a tickmark should be drawn for the \c defaultValue
 */
@property (readwrite) BOOL drawTickMarkForDefault;

/**
 Indicates if the slider knob should snap to the \c defaultValue
 */
@property (readwrite) BOOL snapsToDefault;

/**
 The default value of the slider
 
 \note It may not be equal to \c DBL_MAX, as this is the value
       that it should be set to, if no defaultValue is desired.
 */
@property (getter=defaultValue, setter=setDefaultValue:) double defaultValue;

/**
 Color of the default tick mark
 */
@property (getter=defaultTickMarkColor, setter=setDefaultTickMarkColor:) NSColor *defaultTickMarkColor;

/**
 Draws the tick mark for the \c defaultValue in the
 given rect.
 
 \note Override this in a subclass if you need to customize the
 tickmark that is drawn for the \c defaultValue
 
 \param rect The rect in which the tickMark should be drawn
 */
- (void)drawDefaultTickMarkWithFrame:(NSRect)rect;

@end
