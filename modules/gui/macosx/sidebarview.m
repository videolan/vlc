/*****************************************************************************
 * sidebarview.m: MacOS X interface module
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

#import "sidebarview.h"
#import "intf.h"
#import "playlist.h"

@implementation sidebarview
- (void) resetCursorRects 
{
	if( ! [self isPaneSplitter] )
		[super resetCursorRects];
}

- (id) initWithCoder:(NSCoder *) decoder {
	if( ( self = [super initWithCoder:decoder] ) )
		_mainSubviewIndex = 1;
	return self;
}

- (CGFloat) dividerThickness
{
	return 1.0;
}

- (BOOL) isVertical
{
    return YES;
}

- (void) drawDividerInRect:(NSRect) rect
{
	[[NSColor colorWithCalibratedWhite:0.65 alpha:1.] set];
	NSRectFill( rect );
}

- (void) adjustSubviews
{
	if( _mainSubviewIndex == -1 || [[self subviews] count] != 2 ) {
		[super adjustSubviews];
		return;
	}
    
	float dividerThickness = [self dividerThickness];
	NSRect newFrame = [self frame];
    
	NSView *mainView = [[self subviews] objectAtIndex:_mainSubviewIndex];
	NSView *otherView = ( _mainSubviewIndex ? [[self subviews] objectAtIndex:0] : [[self subviews] objectAtIndex:1] );
    
	NSRect mainFrame = [mainView frame];
	NSRect otherFrame = [otherView frame];
    

	mainFrame.size.width = NSWidth( newFrame ) - dividerThickness - NSWidth( otherFrame );
	mainFrame.size.height = NSHeight( newFrame );
	mainFrame.origin.x = ( _mainSubviewIndex ? NSWidth( otherFrame ) + dividerThickness : 0. );
	mainFrame.origin.y = 0.;

	otherFrame.size.width = NSWidth( otherFrame );
	otherFrame.size.height = NSHeight( newFrame );
	otherFrame.origin.x = ( _mainSubviewIndex ? 0. : NSWidth( mainFrame ) + dividerThickness );
	otherFrame.origin.y = 0.;
    
	[mainView setFrame:mainFrame];
	[otherView setFrame:otherFrame];
    
	[self setNeedsDisplay:YES];
}
@end

/*****************************************************************************
 * VLCPlaylist implementation
 *****************************************************************************/
@implementation VLCSidebar

- (void)awakeFromNib
{
    [o_outline_view setTarget: self];
    [o_outline_view setDelegate: self];
    [o_outline_view setDataSource: self];
    [o_outline_view setAllowsEmptySelection: NO];
}

- (NSOutlineView *)outlineView
{
    return o_outline_view;
}

- (void)outlineView:(NSOutlineView *)outlineView
    willDisplayCell:(id)cell
     forTableColumn:(NSTableColumn *)tableColumn
               item:(id)item
{
    if ( ![outlineView isExpandable:item] )
    {
        [cell setFont: [NSFont systemFontOfSize: 12]];
        [cell setTextColor:[NSColor blackColor]];
    }
    else
    {
        [cell setFont: [NSFont boldSystemFontOfSize: 10]];
        [cell setTextColor:[NSColor colorWithCalibratedWhite:0.365 alpha:1.0]];
    }
}

- (void)updateSidebar:(id)item
{
    int i_row = -1;
    [o_outline_view reloadData];
    i_row = [o_outline_view rowForItem:item];
    if( i_row > -1 )
    {
		[o_outline_view selectRowIndexes:[NSIndexSet indexSetWithIndex:i_row] byExtendingSelection:NO];
        [o_outline_view scrollRowToVisible: i_row];
    }
}

- (CGFloat)outlineView:(NSOutlineView *)outlineView heightOfRowByItem:(id)item
{
    if( [outlineView isExpandable:item] )
        return 12.;
    else
        return 20.;
}

- (BOOL)outlineView:(NSOutlineView *)outlineView shouldSelectItem:(id)item
{
    if( [outlineView isExpandable:item] )
        return NO;
    else
    {
        if( ![[o_playlist playingItem] isEqual: item] )
            [o_playlist playSidebarItem:item];
        return YES;
    }
}

- (void)outlineViewItemDidExpand:(NSNotification *)notification
{
    int i_row = -1;
    i_row = [o_outline_view rowForItem:[o_playlist playingItem]];
    if( i_row > -1 )
    {
		[o_outline_view selectRowIndexes:[NSIndexSet indexSetWithIndex:i_row] byExtendingSelection:NO];
        [o_outline_view scrollRowToVisible: i_row];
    }
}

@end

@implementation VLCSidebar (NSOutlineViewDataSource)

/* return the number of children for Obj-C pointer item */ /* DONE */
- (NSInteger)outlineView:(NSOutlineView *)outlineView numberOfChildrenOfItem:(id)item
{
    return [o_playlist outlineView:outlineView numberOfChildrenOfItem:item];
}

/* return the child at index for the Obj-C pointer item */ /* DONE */
- (id)outlineView:(NSOutlineView *)outlineView child:(NSInteger)index ofItem:(id)item
{
    return [o_playlist outlineView:outlineView child:index ofItem:item];
}

/* is the item expandable */
- (BOOL)outlineView:(NSOutlineView *)outlineView isItemExpandable:(id)item
{
    return [o_playlist outlineView:outlineView isItemExpandable:item];
}

/* retrieve the string values for the cells */
- (id)outlineView:(NSOutlineView *)outlineView objectValueForTableColumn:(NSTableColumn *)o_tc byItem:(id)item
{
    if( [outlineView isExpandable:item] )
        return [[o_playlist outlineView:outlineView objectValueForTableColumn:o_tc byItem:item] uppercaseString];
    else
        return [o_playlist outlineView:outlineView objectValueForTableColumn:o_tc byItem:item];
}

@end
