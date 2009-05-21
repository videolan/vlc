/*****************************************************************************
 * sidestatusview.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2005-2008 the VideoLAN team
 * $Id$
 *
 * Authors: Eric Dudiak <dudiak at gmail dot com>
 *          Colloquy <http://colloquy.info/>
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

#import "sidestatusview.h"

@implementation sidestatusview
-(void)resetCursorRects
{
	[super resetCursorRects];
	if( ! splitView ) return;
    
	NSImage *resizeImage = [NSImage imageNamed:@"sidebarResizeWidget"];
	NSRect location;
	location.size = [resizeImage size];
	location.origin = NSMakePoint( NSWidth( [self bounds] ) - [resizeImage size].width, 0. );
	[self addCursorRect:location cursor:[NSCursor resizeLeftRightCursor]];
}

- (void)drawRect:(NSRect)rect
{
	NSImage *backgroundImage = [NSImage imageNamed:@"sidebarStatusAreaBackground"];
    [backgroundImage setSize:NSMakeSize(NSWidth( [self bounds] ), [backgroundImage size].height)];
    [backgroundImage setScalesWhenResized:YES];
    [backgroundImage compositeToPoint:NSMakePoint( 0., 0. ) operation:NSCompositeCopy];
    
	if( splitView ) {
		NSImage *resizeImage = [NSImage imageNamed:@"sidebarResizeWidget"];
		[resizeImage compositeToPoint:NSMakePoint( NSWidth( [self bounds] ) - [resizeImage size].width, 0. ) operation:NSCompositeCopy];
	}
}

- (void)mouseDown:(NSEvent *)event
{
	if( ! splitView ) return;
    NSPoint clickLocation = [self convertPoint:[event locationInWindow] fromView:nil];
    
	NSImage *resizeImage = [NSImage imageNamed:@"sidebarResizeWidget"];
	NSRect location;
	location.size = [resizeImage size];
	location.origin = NSMakePoint( NSWidth( [self bounds] ) - [resizeImage size].width, 0. );
    
	_insideResizeArea = ( NSPointInRect( clickLocation, location ) );
	if( ! _insideResizeArea ) return;
    
	clickLocation = [self convertPoint:[event locationInWindow] fromView:[self superview]];
	_clickOffset = NSWidth( [[self superview] frame] ) - clickLocation.x;
}

- (void)mouseDragged:(NSEvent *)event
{
	if( ! splitView || ! _insideResizeArea ) return;
    
	[[NSNotificationCenter defaultCenter] postNotificationName:NSSplitViewWillResizeSubviewsNotification object:splitView];
    
    NSPoint clickLocation = [self convertPoint:[event locationInWindow] fromView:[self superview]];
    
	NSRect newFrame = [[self superview] frame];
	newFrame.size.width = clickLocation.x + _clickOffset;
    
	id delegate = [splitView delegate];
	if( delegate && [delegate respondsToSelector:@selector( splitView:constrainSplitPosition:ofSubviewAt: )] ) {
		float new = [delegate splitView:splitView constrainSplitPosition:newFrame.size.width ofSubviewAt:0];
		newFrame.size.width = new;
	}
    
	if( delegate && [delegate respondsToSelector:@selector( splitView:constrainMinCoordinate:ofSubviewAt: )] ) {
		float min = [delegate splitView:splitView constrainMinCoordinate:0. ofSubviewAt:0];
		newFrame.size.width = MAX( min, newFrame.size.width );
	}
    
	if( delegate && [delegate respondsToSelector:@selector( splitView:constrainMaxCoordinate:ofSubviewAt: )] ) {
		float max = [delegate splitView:splitView constrainMaxCoordinate:0. ofSubviewAt:0];
		newFrame.size.width = MIN( max, newFrame.size.width );
	}
    
    if( delegate ) {
        [delegate setMinSize:NSMakeSize(newFrame.size.width + 551., 114.)];
    }
    
	[[self superview] setFrame:newFrame];
    
	[splitView adjustSubviews];
    
	[[NSNotificationCenter defaultCenter] postNotificationName:NSSplitViewDidResizeSubviewsNotification object:splitView];
}
@end
