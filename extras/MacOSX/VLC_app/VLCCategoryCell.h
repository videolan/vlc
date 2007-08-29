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
#import <Cocoa/Cocoa.h>


@interface VLCCategoryCell : NSImageCell {
	@private
	NSImage *_statusImage;
	NSImage *_altImage;
	NSString *_mainText;
	NSString *_infoText;
	NSLineBreakMode _lineBreakMode;
	unsigned _statusNumber;
	unsigned _importantStatusNumber;
	BOOL _boldAndWhiteOnHighlight;
	BOOL _selectable;
}
- (void) setStatusImage:(NSImage *) image;
- (NSImage *) statusImage;

- (void) setHighlightedImage:(NSImage *) image;
- (NSImage *) highlightedImage;

- (void) setMainText:(NSString *) text;
- (NSString *) mainText;

- (void) setInformationText:(NSString *) text;
- (NSString *) informationText;

- (void) setLineBreakMode:(NSLineBreakMode) mode;
- (NSLineBreakMode) lineBreakMode;

- (void) setBoldAndWhiteOnHighlight:(BOOL) boldAndWhite;
- (BOOL) boldAndWhiteOnHighlight;

- (void) setStatusNumber:(unsigned) number;
- (unsigned) statusNumber;

- (void) setImportantStatusNumber:(unsigned) number;
- (unsigned) importantStatusNumber;
@end
