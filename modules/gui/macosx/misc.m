/*****************************************************************************
 * misc.m: code not specific to vlc
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: misc.m,v 1.1 2003/01/21 00:47:43 jlj Exp $
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

#include <Cocoa/Cocoa.h>

#include "misc.h"

/*****************************************************************************
 * MPSlider
 *****************************************************************************/

@implementation MPSlider

+ (Class)cellClass
{
    return( [MPSliderCell class] );
}

@end

/*****************************************************************************
 * MPSliderCell
 *****************************************************************************/

@implementation MPSliderCell

- (id)init
{
    self = [super init];

    if( self != nil )
    {
        _bgColor = [[NSColor colorWithDeviceRed: 0.8627451
                                          green: 0.8784314
                                           blue: 0.7725490
                                          alpha: 1.0] retain];
        _knobColor = [[NSColor blackColor] retain];
    }

    return( self );
}

- (void)dealloc
{
    [_bgColor release];
    [_knobColor release];
    [super dealloc];
}

- (void)setBackgroundColor:(NSColor *)newColor
{
    [_bgColor release];  
    _bgColor = [newColor retain];
}

- (NSColor *)backgroundColor
{
    return( _bgColor );
}

- (void)setKnobColor:(NSColor *)newColor
{
    [_knobColor release];  
    _knobColor = [newColor retain];
}

- (NSColor *)knobColor
{
    return( _knobColor );
}

- (void)setKnobThickness:(float)f_value
{
    _knobThickness = f_value;
}

- (float)knobThickness
{
    return( _knobThickness );
}

- (NSSize)cellSizeForBounds:(NSRect)s_rc
{
    return( s_rc.size );
}

- (void)drawWithFrame:(NSRect)s_rc inView:(NSView *)o_view
{
    if( _scFlags.weAreVertical )
    {
        s_rc.origin.x = 1; s_rc.size.width -= 3;
        s_rc.origin.y = 2; s_rc.size.height -= 5;    
    }
    else
    {
        s_rc.origin.x = 2; s_rc.size.width -= 5;
        s_rc.origin.y = 1; s_rc.size.height -= 3;
    }

    [super drawWithFrame: s_rc inView: o_view]; 
}

- (void)drawBarInside:(NSRect)s_rc flipped:(BOOL)b_flipped
{
    NSRect s_arc;
 
    s_rc.size.width += (s_rc.origin.x * 2) + 1;
    s_rc.size.height += (s_rc.origin.y * 2) + 1;
    s_rc.origin.x = s_rc.origin.y = 0;

    [[NSGraphicsContext currentContext] setShouldAntialias: NO];

    [_bgColor set];
    NSRectFill( s_rc );

    s_arc = s_rc;
    s_arc.origin.x += 1.5;
    s_arc.origin.y += 1.5;
    s_arc.size.width -= s_arc.origin.x;
    s_arc.size.height -= s_arc.origin.y;
    [[_bgColor shadowWithLevel: 0.1] set];
    [NSBezierPath strokeRect: s_arc];

    s_arc.origin = s_rc.origin;
    [[NSColor blackColor] set];
    [NSBezierPath strokeRect: s_arc];

    [[NSGraphicsContext currentContext] setShouldAntialias: YES];
}

- (NSRect)knobRectFlipped:(BOOL)b_flipped
{
    NSSize s_size;
    NSPoint s_pto;
    float floatValue;

    floatValue = [self floatValue];

    if( _scFlags.weAreVertical && b_flipped )
    {
        floatValue = _maxValue + _minValue - floatValue;
    }

    floatValue = (floatValue - _minValue) / (_maxValue - _minValue);

    if( _scFlags.weAreVertical )
    {   
        s_size = NSMakeSize( _trackRect.size.width, _knobThickness ?
                             _knobThickness : _trackRect.size.width );
        s_pto = _trackRect.origin;
        s_pto.y += (_trackRect.size.height - s_size.height) * floatValue;
    }
    else
    {   
        s_size = NSMakeSize( _knobThickness ? _knobThickness :
                             _trackRect.size.height, _trackRect.size.height );
        s_pto = _trackRect.origin;
        s_pto.x += (_trackRect.size.width - s_size.width) * floatValue;
    }

    return NSMakeRect( s_pto.x, s_pto.y, s_size.width, s_size.height );
}

- (void)drawKnob:(NSRect)s_rc
{
    [[NSGraphicsContext currentContext] setShouldAntialias: NO];

    [_knobColor set];
    NSRectFill( s_rc );

    [[NSGraphicsContext currentContext] setShouldAntialias: YES];
}

@end
