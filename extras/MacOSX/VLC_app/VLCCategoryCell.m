/*****************************************************************************
 * VLCCategoryCell.h: VLC.app custom cell
 * Most of the code here from Colloquy (GPL v2)
 *****************************************************************************
 * Copyright (C) 2007 Pierre d'Herbemont
 * Copyright (C) 2007 the VideoLAN team
 * $Id$
 *
 * Authors: Pierre d'Herbemont <pdherbemont # videolan.org>
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
#import "VLCCategoryCell.h"

@implementation VLCCategoryCell
- (id) init {
	if( ( self = [super init] ) ) {
		_altImage = nil;
		_statusImage = nil;
		_mainText = nil;
		_infoText = nil;
        _boldAndWhiteOnHighlight = YES;
        _selectable = YES;

		[self setImageAlignment:NSImageAlignLeft];
		[self setImageScaling:NSScaleProportionally];
		[self setImageFrameStyle:NSImageFrameNone];
		[self setLineBreakMode:NSLineBreakByTruncatingTail];
	}

	return self;
}

- (id) copyWithZone:(NSZone *) zone {
	VLCCategoryCell *cell = (VLCCategoryCell *)[super copyWithZone:zone];
	cell -> _statusImage = [_statusImage retain];
	cell -> _altImage = [_altImage retain];
	cell -> _mainText = [_mainText copyWithZone:zone];
	cell -> _infoText = [_infoText copyWithZone:zone];
	cell -> _lineBreakMode = _lineBreakMode;
	return cell;
}

- (void) dealloc {
	[_altImage release];
	[_statusImage release];
	[_mainText release];
	[_infoText release];

	_altImage = nil;
	_statusImage = nil;
	_mainText = nil;
	_infoText = nil;

	[super dealloc];
}

#pragma mark -

- (void) setStatusImage:(NSImage *) image {
	[_statusImage autorelease];
	_statusImage = [image retain];
}

- (NSImage *) statusImage {
	return _statusImage;
}

#pragma mark -

- (void) setStatusNumber:(unsigned) number {
	_statusNumber = number;
}

- (unsigned) statusNumber {
	return _statusNumber;
}

#pragma mark -

- (void) setImportantStatusNumber:(unsigned) number {
	_importantStatusNumber = number;
}

- (unsigned) importantStatusNumber {
	return _importantStatusNumber;
}

#pragma mark -

- (void) setHighlightedImage:(NSImage *) image {
	[_altImage autorelease];
	_altImage = [image retain];
}

- (NSImage *) highlightedImage {
	return _altImage;
}

#pragma mark -
- (void) setSelectable:(BOOL) selectable {
	_selectable = selectable;
}

- (BOOL) selectable {
	return _selectable;
}

#pragma mark -

- (void) setMainText:(NSString *) text {
	[_mainText autorelease];
	_mainText = [text copy];
}

- (NSString *) mainText {
	return _mainText;
}

#pragma mark -

- (void) setInformationText:(NSString *) text {
	[_infoText autorelease];
	_infoText = [text copy];
}

- (NSString *) informationText {
	return _infoText;
}

#pragma mark -

- (void) setBoldAndWhiteOnHighlight:(BOOL) boldAndWhite {
	_boldAndWhiteOnHighlight = boldAndWhite;
}

- (BOOL) boldAndWhiteOnHighlight {
	return _boldAndWhiteOnHighlight;
}

#pragma mark -

- (void) drawWithFrame:(NSRect) cellFrame inView:(NSView *) controlView {
	float imageWidth = 0.;
	BOOL highlighted = ( [self isHighlighted] && [[controlView window] firstResponder] == controlView && [[controlView window] isKeyWindow] && [[NSApplication sharedApplication] isActive] );

	NSMutableParagraphStyle *paraStyle = [[[NSParagraphStyle defaultParagraphStyle] mutableCopy] autorelease];
	[paraStyle setLineBreakMode:_lineBreakMode];
	[paraStyle setAlignment:[self alignment]];

	NSMutableDictionary *attributes = [NSMutableDictionary dictionaryWithObjectsAndKeys:[self font], NSFontAttributeName, paraStyle, NSParagraphStyleAttributeName, ( [self isEnabled] ? ( highlighted ? [NSColor alternateSelectedControlTextColor] : [NSColor controlTextColor] ) : ( highlighted ? [NSColor alternateSelectedControlTextColor] : [[NSColor controlTextColor] colorWithAlphaComponent:0.50] ) ), NSForegroundColorAttributeName, nil];
	NSMutableDictionary *subAttributes = [NSMutableDictionary dictionaryWithObjectsAndKeys:[NSFont toolTipsFontOfSize:9.], NSFontAttributeName, paraStyle, NSParagraphStyleAttributeName, ( [self isEnabled] ? ( highlighted ? [NSColor alternateSelectedControlTextColor] : [[NSColor controlTextColor] colorWithAlphaComponent:0.75] ) : ( highlighted ? [NSColor alternateSelectedControlTextColor] : [[NSColor controlTextColor] colorWithAlphaComponent:0.40] ) ), NSForegroundColorAttributeName, nil];
	NSImage *mainImage = nil, *curImage = nil;
	NSSize mainStringSize = [_mainText sizeWithAttributes:attributes];
	NSSize subStringSize = [_infoText sizeWithAttributes:subAttributes];

	if( _boldAndWhiteOnHighlight && [self isHighlighted] ) {
		NSFont *boldFont = [[NSFontManager sharedFontManager] fontWithFamily:@"Lucida Grande" traits:0 weight:15 size:11.];
		NSShadow *shadow = [[NSShadow allocWithZone:nil] init];
		NSColor *whiteColor = [NSColor whiteColor];
		if( ! [self isEnabled] ) whiteColor = [whiteColor colorWithAlphaComponent:0.5];

        [shadow setShadowOffset:NSMakeSize( 0, -1 )];
		[shadow setShadowBlurRadius:0.1];
		[shadow setShadowColor:[[NSColor shadowColor] colorWithAlphaComponent:0.2]];

		[attributes setObject:boldFont forKey:NSFontAttributeName];
		[attributes setObject:whiteColor forKey:NSForegroundColorAttributeName];
		[attributes setObject:shadow forKey:NSShadowAttributeName];

		boldFont = [[NSFontManager sharedFontManager] fontWithFamily:@"Lucida Grande" traits:0 weight:15 size:9.];
		[subAttributes setObject:boldFont forKey:NSFontAttributeName];
		[subAttributes setObject:whiteColor forKey:NSForegroundColorAttributeName];
		[subAttributes setObject:shadow forKey:NSShadowAttributeName];
		[shadow release];
	}
    else /*if( [self selectable] )
    {
        [self setMainText: [_mainText uppercaseString]];
		[attributes setObject:[NSColor grayColor] forKey:NSForegroundColorAttributeName];
    }
    else*/
    {
        NSFont *font = [[NSFontManager sharedFontManager] fontWithFamily:@"Lucida Grande" traits:0 weight:1 size:11.];
		[attributes setObject:font forKey:NSFontAttributeName];
    }

	if( highlighted && _altImage ) {
		mainImage = [[self image] retain];
		[self setImage:_altImage];
	}

	if( ! [self isEnabled] && [self image] ) {
		NSImage *fadedImage = [[[NSImage alloc] initWithSize:[[self image] size]] autorelease];
		[fadedImage lockFocus];
		[[self image] dissolveToPoint:NSMakePoint( 0., 0. ) fraction:0.5];
		[fadedImage unlockFocus];
		curImage = [[self image] retain];
		[self setImage:fadedImage];
	}

	cellFrame = NSMakeRect( cellFrame.origin.x + 1., cellFrame.origin.y, cellFrame.size.width - 1., cellFrame.size.height );
	[super drawWithFrame:cellFrame inView:controlView];

	if( ! [self isEnabled] ) {
		[self setImage:curImage];
		[curImage autorelease];
	}

	if( highlighted && mainImage ) {
		[self setImage:mainImage];
		[mainImage autorelease];
	}

	if( [self image] ) {
		switch( [self imageScaling] ) {
		case NSScaleProportionally:
			if( NSHeight( cellFrame ) < [[self image] size].height )
				imageWidth = ( NSHeight( cellFrame ) / [[self image] size].height ) * [[self image] size].width;
			else imageWidth = [[self image] size].width;
			break;
		default:
		case NSScaleNone:
			imageWidth = [[self image] size].width;
			break;
		case NSScaleToFit:
			imageWidth = [[self image] size].width;
			break;
		}
	}

#define JVDetailCellLabelPadding 3.
#define JVDetailCellImageLabelPadding 5.
#define JVDetailCellTextLeading 3.
#define JVDetailCellStatusImageLeftPadding 2.
#define JVDetailCellStatusImageRightPadding JVDetailCellStatusImageLeftPadding

	float statusWidth = ( _statusImage ? [_statusImage size].width + JVDetailCellStatusImageRightPadding : 0. );
	if( ! _statusImage && ( _statusNumber || _importantStatusNumber ) ) {
		NSColor *textColor = [NSColor whiteColor];
		NSColor *backgroundColor = [NSColor colorWithCalibratedRed:0.6 green:0.6705882352941176 blue:0.7725490196078431 alpha:1.];
		NSColor *importantColor = [NSColor colorWithCalibratedRed:0.831372549019608 green:0.572549019607843 blue:0.541176470588235 alpha:1.];

		if( ! _statusNumber && _importantStatusNumber )
			backgroundColor = importantColor;

		if( [self isHighlighted] ) {
			textColor = [backgroundColor shadowWithLevel:0.2];
			backgroundColor = [backgroundColor highlightWithLevel:0.7];
		}

		NSFont *font = [[NSFontManager sharedFontManager] fontWithFamily:@"Lucida Grande" traits:NSBoldFontMask weight:9 size:11.];
		NSMutableParagraphStyle *numberParaStyle = [[[NSParagraphStyle defaultParagraphStyle] mutableCopy] autorelease];
		[numberParaStyle setAlignment:NSCenterTextAlignment];

		NSDictionary *statusNumberAttributes = [NSDictionary dictionaryWithObjectsAndKeys:font, NSFontAttributeName, numberParaStyle, NSParagraphStyleAttributeName, textColor, NSForegroundColorAttributeName, [NSNumber numberWithFloat:1.0], NSKernAttributeName, nil];

		NSString *statusText = [NSString stringWithFormat:@"%d", ( _statusNumber ? _statusNumber : _importantStatusNumber )];
		NSSize numberSize = [statusText sizeWithAttributes:statusNumberAttributes];
		statusWidth = numberSize.width + 12.;

		if( imageWidth + ( imageWidth ? JVDetailCellImageLabelPadding : JVDetailCellLabelPadding ) + statusWidth < NSWidth( cellFrame ) ) {
			float radius = ( _importantStatusNumber ? 8. : 7. );
			NSRect mainRect = NSMakeRect( NSMinX( cellFrame ) + NSWidth( cellFrame ) - statusWidth - 2., NSMinY( cellFrame ) + ( ( NSHeight( cellFrame ) / 2 ) - radius ), statusWidth, radius * 2 );
			NSRect pathRect = NSInsetRect( mainRect, radius, radius );

			NSBezierPath *mainPath = [NSBezierPath bezierPath];
			[mainPath appendBezierPathWithArcWithCenter:NSMakePoint( NSMinX( pathRect ), NSMinY( pathRect ) ) radius:radius startAngle:180. endAngle:270.];
			[mainPath appendBezierPathWithArcWithCenter:NSMakePoint( NSMaxX( pathRect ), NSMinY( pathRect ) ) radius:radius startAngle:270. endAngle:360.];
			[mainPath appendBezierPathWithArcWithCenter:NSMakePoint( NSMaxX( pathRect ), NSMaxY( pathRect ) ) radius:radius startAngle:0. endAngle:90.];
			[mainPath appendBezierPathWithArcWithCenter:NSMakePoint( NSMinX( pathRect ), NSMaxY( pathRect ) ) radius:radius startAngle:90. endAngle:180.];
			[mainPath closePath];

			if( _importantStatusNumber ) {
				NSString *importantStatusText = [NSString stringWithFormat:@"%d", _importantStatusNumber];
				numberSize = [importantStatusText sizeWithAttributes:statusNumberAttributes];
				float mainStatusWidth = statusWidth;
				statusWidth += numberSize.width + 10.;
				radius = 7.;

				NSRect rect = NSMakeRect( NSMinX( cellFrame ) + NSWidth( cellFrame ) - statusWidth - 2., NSMinY( cellFrame ) + ( ( NSHeight( cellFrame ) / 2 ) - radius ), statusWidth - mainStatusWidth + 10., radius * 2 );
				pathRect = NSInsetRect( rect, radius, radius );

				NSBezierPath *path = [NSBezierPath bezierPath];
				[path appendBezierPathWithArcWithCenter:NSMakePoint( NSMinX( pathRect ), NSMinY( pathRect ) ) radius:radius startAngle:180. endAngle:270.];
				[path appendBezierPathWithArcWithCenter:NSMakePoint( NSMaxX( pathRect ), NSMinY( pathRect ) ) radius:radius startAngle:270. endAngle:360.];
				[path appendBezierPathWithArcWithCenter:NSMakePoint( NSMaxX( pathRect ), NSMaxY( pathRect ) ) radius:radius startAngle:0. endAngle:90.];
				[path appendBezierPathWithArcWithCenter:NSMakePoint( NSMinX( pathRect ), NSMaxY( pathRect ) ) radius:radius startAngle:90. endAngle:180.];
				[path closePath];

				if( [self isHighlighted] ) [[NSColor whiteColor] set];
				else [[NSColor colorWithCalibratedRed:0.92156862745098 green:0.231372549019608 blue:0.243137254901961 alpha:0.85] set];
				[path fill];

				rect.origin.x -= 3.;
				[importantStatusText drawInRect:rect withAttributes:statusNumberAttributes];
			}

			[backgroundColor set];
			[mainPath fill];

			if( _importantStatusNumber ) {
				if( [self isHighlighted] ) [[NSColor colorWithCalibratedRed:0.5803921568627451 green:0.6705882352941176 blue:0.7882352941176471 alpha:1.] set];
				else [[NSColor whiteColor] set];

				[mainPath setLineWidth:1.25];
				[mainPath stroke];
			}

			if( _importantStatusNumber ) mainRect.origin.y += 1.;
			[statusText drawInRect:mainRect withAttributes:statusNumberAttributes];

			statusWidth += JVDetailCellStatusImageRightPadding + 3.;

		} else statusWidth = 0.;
	}

	if( ( ! [_infoText length] && [_mainText length] ) || ( ( subStringSize.height + mainStringSize.height ) >= NSHeight( cellFrame ) - 2. ) ) {
		float mainYLocation = 0.;

		if( NSHeight( cellFrame ) >= mainStringSize.height ) {
			mainYLocation = NSMinY( cellFrame ) + ( NSHeight( cellFrame ) / 2 ) - ( mainStringSize.height / 2 );
			[_mainText drawInRect:NSMakeRect( NSMinX( cellFrame ) + imageWidth + ( imageWidth ? JVDetailCellImageLabelPadding : JVDetailCellLabelPadding ), mainYLocation, NSWidth( cellFrame ) - imageWidth - ( JVDetailCellImageLabelPadding * 1. ) - statusWidth, [_mainText sizeWithAttributes:attributes].height ) withAttributes:attributes];
		}
	} else if( [_infoText length] && [_mainText length] ) {
		float mainYLocation = 0., subYLocation = 0.;

		if( NSHeight( cellFrame ) >= mainStringSize.height ) {
			mainYLocation = NSMinY( cellFrame ) + ( NSHeight( cellFrame ) / 2 ) - mainStringSize.height + ( JVDetailCellTextLeading / 2. );
			[_mainText drawInRect:NSMakeRect( cellFrame.origin.x + imageWidth + ( imageWidth ? JVDetailCellImageLabelPadding : JVDetailCellLabelPadding ), mainYLocation, NSWidth( cellFrame ) - imageWidth - ( JVDetailCellImageLabelPadding * 1. ) - statusWidth, [_mainText sizeWithAttributes:attributes].height ) withAttributes:attributes];

			subYLocation = NSMinY( cellFrame ) + ( NSHeight( cellFrame ) / 2 ) + subStringSize.height - mainStringSize.height + ( JVDetailCellTextLeading / 2. );
			[_infoText drawInRect:NSMakeRect( NSMinX( cellFrame ) + imageWidth + ( imageWidth ? JVDetailCellImageLabelPadding : JVDetailCellLabelPadding ), subYLocation, NSWidth( cellFrame ) - imageWidth - ( JVDetailCellImageLabelPadding * 1. ) - statusWidth, [_infoText sizeWithAttributes:subAttributes].height ) withAttributes:subAttributes];
		}
	}

	if( _statusImage && NSHeight( cellFrame ) >= [_statusImage size].height ) {
		[_statusImage compositeToPoint:NSMakePoint( NSMinX( cellFrame ) + NSWidth( cellFrame ) - statusWidth, NSMaxY( cellFrame ) - ( ( NSHeight( cellFrame ) / 2 ) - ( [_statusImage size].height / 2 ) ) ) operation:NSCompositeSourceAtop fraction:( [self isEnabled] ? 1. : 0.5)];
	}
}

#pragma mark -

- (void) setImageScaling:(NSImageScaling) newScaling {
	[super setImageScaling:( newScaling == NSScaleProportionally || newScaling == NSScaleNone ? newScaling : NSScaleProportionally )];
}

- (void) setImageAlignment:(NSImageAlignment) newAlign {
	[super setImageAlignment:NSImageAlignLeft];
}

- (void) setLineBreakMode:(NSLineBreakMode) mode {
	_lineBreakMode = mode;
}

- (NSLineBreakMode) lineBreakMode {
	return _lineBreakMode;
}

- (void) setStringValue:(NSString *) string {
	[self setMainText:string];
}

- (void) setObjectValue:(id <NSCopying>) obj {
	if( ! obj || [(NSObject *)obj isKindOfClass:[NSImage class]] ) {
		[super setObjectValue:obj];
	} else if( [(NSObject *)obj isKindOfClass:[NSString class]] ) {
		[self setMainText:(NSString *)obj];
	}
}

- (NSString *) stringValue {
	return _mainText;
}
@end
