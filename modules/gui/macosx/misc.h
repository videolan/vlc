/*****************************************************************************
 * misc.h: code not specific to vlc
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: misc.h,v 1.1 2003/01/21 00:47:43 jlj Exp $
 *
 * Authors: Jon Lech Johansen <jon-vl@nanocrew.net>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

/*****************************************************************************
 * MPSlider
 *****************************************************************************/

@interface MPSlider : NSSlider
{

}

@end

/*****************************************************************************
 * MPSliderCell
 *****************************************************************************/

@interface MPSliderCell : NSSliderCell
{
    NSColor * _bgColor;
    NSColor * _knobColor;
    float _knobThickness;
}

- (void)setBackgroundColor:(NSColor *)newColor;
- (NSColor *)backgroundColor;

- (void)setKnobColor:(NSColor *)newColor;
- (NSColor *)knobColor;

@end