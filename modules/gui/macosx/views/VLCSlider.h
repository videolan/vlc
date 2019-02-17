/*****************************************************************************
 * VLCSlider.h
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

@interface VLCSlider : NSSlider

@property (nonatomic, getter=getIndefinite,setter=setIndefinite:) BOOL indefinite;
@property (nonatomic, getter=getKnobHidden,setter=setKnobHidden:) BOOL isKnobHidden;

/* Indicates if the slider is scrollable with the mouse or trackpad scrollwheel. */
@property (readwrite) BOOL isScrollable;

- (void)setSliderStyleLight;
- (void)setSliderStyleDark;

@end
