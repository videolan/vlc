/*****************************************************************************
 * ListViews.h: BeOS interface list view class implementation
 *****************************************************************************
 * Copyright (C) 1999, 2000, 2001 VideoLAN
 * $Id: ListViews.cpp,v 1.1.2.1 2002/09/29 12:06:08 titer Exp $
 *
 * Authors: Stephan Aßmus <stippi@yellowbites.com>
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

#include <stdio.h>

#include <Bitmap.h>
#include <String.h>

extern "C"
{
#include <videolan/vlc.h>

#include "stream_control.h"

#include "interface.h"
#include "input_ext-intf.h"
}

#include "InterfaceWindow.h"
#include "ListViews.h"
#include "MsgVals.h"
#include "intf_vlc_wrapper.h"


#define MAX_DRAG_HEIGHT		200.0
#define ALPHA				170
#define TEXT_OFFSET			20.0

/*****************************************************************************
 * PlaylistItem class
 *****************************************************************************/
PlaylistItem::PlaylistItem( const char *name )
	: BStringItem( name )
{
}

PlaylistItem::~PlaylistItem()
{
}

/*****************************************************************************
 * PlaylistItem::DrawItem
 *****************************************************************************/
void
PlaylistItem::Draw( BView *owner, BRect frame, bool tintedLine,
					bool active, bool playing )
{
	rgb_color color = (rgb_color){ 255, 255, 255, 255 };
	if ( tintedLine )
		color = tint_color( color, 1.04 );
	// background
	if ( IsSelected() )
		color = tint_color( color, B_DARKEN_2_TINT );
	owner->SetLowColor( color );
	owner->FillRect( frame, B_SOLID_LOW );
	// label
	owner->SetHighColor( 0, 0, 0, 255 );
	font_height fh;
	owner->GetFontHeight( &fh );
	BString truncatedString( Text() );
	owner->TruncateString( &truncatedString, B_TRUNCATE_MIDDLE,
						   frame.Width() - TEXT_OFFSET - 4.0 );
	owner->DrawString( truncatedString.String(),
					   BPoint( frame.left + TEXT_OFFSET,
							   frame.top + fh.ascent + 1.0 ) );
	// playmark
	if ( active )
	{
		rgb_color black = (rgb_color){ 0, 0, 0, 255 };
		rgb_color green = (rgb_color){ 0, 255, 0, 255 };
		BRect r( 0.0, 0.0, 10.0, 10.0 );
		r.OffsetTo( frame.left + 4.0,
					ceilf( ( frame.top + frame.bottom ) / 2.0 ) - 5.0 );
		if ( !playing )
			green = tint_color( color, B_DARKEN_1_TINT );
		rgb_color lightGreen = tint_color( green, B_LIGHTEN_2_TINT );
		rgb_color darkGreen = tint_color( green, B_DARKEN_2_TINT );
		BPoint arrow[3];
		arrow[0] = r.LeftTop();
		arrow[1] = r.LeftBottom();
		arrow[2].x = r.right;
		arrow[2].y = ( r.top + r.bottom ) / 2.0;
		owner->BeginLineArray( 6 );
			// black outline
			owner->AddLine( arrow[0], arrow[1], black );
			owner->AddLine( BPoint( arrow[1].x + 1.0, arrow[1].y - 1.0 ),
							arrow[2], black );
			owner->AddLine( arrow[0], arrow[2], black );
			// inset arrow
			arrow[0].x += 1.0;
			arrow[0].y += 2.0;
			arrow[1].x += 1.0;
			arrow[1].y -= 2.0;
			arrow[2].x -= 2.0;
			// highlights and shadow
			owner->AddLine( arrow[1], arrow[2], darkGreen );
			owner->AddLine( arrow[0], arrow[2], lightGreen );
			owner->AddLine( arrow[0], arrow[1], lightGreen );
		owner->EndLineArray();
		// fill green
		arrow[0].x += 1.0;
		arrow[0].y += 1.0;
		arrow[1].x += 1.0;
		arrow[1].y -= 1.0;
		arrow[2].x -= 2.0;
		owner->SetHighColor( green );
		owner->FillPolygon( arrow, 3 );
	}
}

/*****************************************************************************
 * DragSortableListView class
 *****************************************************************************/
DragSortableListView::DragSortableListView( BRect frame, const char* name,
											list_view_type type, uint32 resizingMode,
											uint32 flags )
	: BListView( frame, name, type, resizingMode, flags ),
	  fDropIndex( -1 )
{
	SetViewColor( B_TRANSPARENT_32_BIT );
}

DragSortableListView::~DragSortableListView()
{
}

/*****************************************************************************
 * DragSortableListView::Draw
 *****************************************************************************/
void
DragSortableListView::Draw( BRect updateRect )
{
	int32 firstIndex = IndexOf( updateRect.LeftTop() );
	int32 lastIndex = IndexOf( updateRect.RightBottom() );
	if ( firstIndex >= 0 )
	{
		if ( lastIndex < firstIndex )
			lastIndex = CountItems() - 1;
		// update rect contains items
		BRect r( updateRect );
		for ( int32 i = firstIndex; i <= lastIndex; i++)
		{
			r = ItemFrame( i );
			DrawListItem( this, i, r );
		}
		updateRect.top = r.bottom + 1.0;
		if ( updateRect.IsValid() )
		{
			SetLowColor( 255, 255, 255, 255 );
			FillRect( updateRect, B_SOLID_LOW );
		}
	}
	else
	{
		SetLowColor( 255, 255, 255, 255 );
		FillRect( updateRect, B_SOLID_LOW );
	}
}

/*****************************************************************************
 * DragSortableListView::InitiateDrag
 *****************************************************************************/
bool
DragSortableListView::InitiateDrag( BPoint point, int32 index, bool )
{
return false;
	bool success = false;
	BListItem* item = ItemAt( CurrentSelection( 0 ) );
	if ( !item )
	{
		// workarround a timing problem
		Select( index );
		item = ItemAt( index );
	}
	if ( item )
	{
		// create drag message
		BMessage msg( B_SIMPLE_DATA );
		MakeDragMessage( &msg );
		// figure out drag rect
		float width = Bounds().Width();
		BRect dragRect(0.0, 0.0, width, -1.0);
		// figure out, how many items fit into our bitmap
		int32 numItems;
		bool fade = false;
		for (numItems = 0; BListItem* item = ItemAt( CurrentSelection( numItems ) ); numItems++) {
			dragRect.bottom += item->Height();
			if ( dragRect.Height() > MAX_DRAG_HEIGHT ) {
				fade = true;
				dragRect.bottom = MAX_DRAG_HEIGHT;
				numItems++;
				break;
			}
		}
		BBitmap* dragBitmap = new BBitmap( dragRect, B_RGB32, true );
		if ( dragBitmap && dragBitmap->IsValid() ) {
			if ( BView *v = new BView( dragBitmap->Bounds(), "helper", B_FOLLOW_NONE, B_WILL_DRAW ) ) {
				dragBitmap->AddChild( v );
				dragBitmap->Lock();
				BRect itemBounds( dragRect) ;
				itemBounds.bottom = 0.0;
				// let all selected items, that fit into our drag_bitmap, draw
				for ( int32 i = 0; i < numItems; i++ ) {
					int32 index = CurrentSelection( i );
					BListItem* item = ItemAt( index );
					itemBounds.bottom = itemBounds.top + item->Height() - 1.0;
					if ( itemBounds.bottom > dragRect.bottom )
						itemBounds.bottom = dragRect.bottom;
					DrawListItem( v, index, itemBounds );
					itemBounds.top = itemBounds.bottom + 1.0;
				}
				// make a black frame arround the edge
				v->SetHighColor( 0, 0, 0, 255 );
				v->StrokeRect( v->Bounds() );
				v->Sync();
	
				uint8 *bits = (uint8 *)dragBitmap->Bits();
				int32 height = (int32)dragBitmap->Bounds().Height() + 1;
				int32 width = (int32)dragBitmap->Bounds().Width() + 1;
				int32 bpr = dragBitmap->BytesPerRow();
	
				if (fade) {
					for ( int32 y = 0; y < height - ALPHA / 2; y++, bits += bpr ) {
						uint8 *line = bits + 3;
						for (uint8 *end = line + 4 * width; line < end; line += 4)
							*line = ALPHA;
					}
					for ( int32 y = height - ALPHA / 2; y < height; y++, bits += bpr ) {
						uint8 *line = bits + 3;
						for (uint8 *end = line + 4 * width; line < end; line += 4)
							*line = (height - y) << 1;
					}
				} else {
					for ( int32 y = 0; y < height; y++, bits += bpr ) {
						uint8 *line = bits + 3;
						for (uint8 *end = line + 4 * width; line < end; line += 4)
							*line = ALPHA;
					}
				}
				dragBitmap->Unlock();
				success = true;
			}
		}
		if (success)
			DragMessage( &msg, dragBitmap, B_OP_ALPHA, BPoint( 0.0, 0.0 ) );
		else {
			delete dragBitmap;
			DragMessage( &msg, dragRect.OffsetToCopy( point ), this );
		}
	}
	return success;
}

/*****************************************************************************
 * DragSortableListView::WindowActivated
 *****************************************************************************/
void
DragSortableListView::WindowActivated( bool active )
{
	// workarround for buggy focus indication of BScrollView
	if ( BView* view = Parent() )
		view->Invalidate();
}

/*****************************************************************************
 * DragSortableListView::MessageReceived
 *****************************************************************************/
void
DragSortableListView::MessageReceived(BMessage* message)
{
	BListItem *item = NULL;
	DragSortableListView *list = NULL;
	if ( message->FindPointer( "list", (void **)&list ) == B_OK
		 && list == this )
	{
		int32 count = CountItems();
		if ( fDropIndex < 0 || fDropIndex > count )
			fDropIndex = count;
		bool copy = ( modifiers() & B_SHIFT_KEY );
		for ( int32 i = 0; message->FindPointer( "item", i, (void **)&item ) == B_OK; i++ )
		{
			
			if ( HasItem( item ) )
			{
				BListItem* itemToAdd = NULL;
				int32 index = IndexOf( item );
				if ( copy )
				{
					// add cloned item
					itemToAdd = CloneItem( index );
					Deselect( IndexOf( item ) );
				}
				else
				{
					// drag sort
					if ( index < fDropIndex )
						fDropIndex--;
					if ( RemoveItem( item ) )
						itemToAdd = item;
				}
				if ( itemToAdd )
				{
					if ( AddItem( itemToAdd, fDropIndex ) )
						Select( IndexOf( itemToAdd ), true );
					else
						delete itemToAdd;
				}
			}
			fDropIndex++;
		}
		fDropIndex = -1;
	} else
		BListView::MessageReceived( message );
}

/*****************************************************************************
 * DragSortableListView::MouseMoved
 *****************************************************************************/
void
DragSortableListView::MouseMoved(BPoint where, uint32 transit, const BMessage *msg)
{
	if ( msg && msg->what == B_SIMPLE_DATA )
	{
		switch ( transit )
		{
			case B_ENTERED_VIEW:
			{
				// draw drop mark
				BRect r(ItemFrame(0L));
				where.y += r.Height() / 2.0;
				int32 count = CountItems();
				bool found = false;
				for (int32 index = 0; index <= count; index++)
				{
					r = ItemFrame(index);
					if (r.Contains(where))
					{
						SetHighColor(255, 0, 0, 255);
						StrokeLine(r.LeftTop(), r.RightTop(), B_SOLID_HIGH);
						r.top++;
						StrokeLine(r.LeftTop(), r.RightTop(), B_SOLID_HIGH);
						fDropIndex = index;
						found = true;
						break;
					}
				}
				if (found)
					break;
				// mouse is after last item
				fDropIndex = count;
				r = Bounds();
				if (count > 0)
					r.top = ItemFrame(count - 1).bottom + 1.0;
				SetHighColor(255, 0, 0, 255);
				StrokeLine(r.LeftTop(), r.RightTop(), B_SOLID_HIGH);
				r.top++;
				StrokeLine(r.LeftTop(), r.RightTop(), B_SOLID_HIGH);
				break;
			}
			case B_INSIDE_VIEW:
			{
				// draw drop mark and invalidate previous drop mark
				BRect r(ItemFrame(0L));
				where.y += r.Height() / 2.0;
				int32 count = CountItems();
				// mouse still after last item?
				if (fDropIndex == count)
				{
					r = Bounds();
					if (count > 0)
						r.top = ItemFrame(count - 1).bottom + 1.0;
					if (r.Contains(where))
						break;
					else
					{
						r.bottom = r.top + 2.0;
						Invalidate(r);
					}
				}
				// mouse still over same item?
				if (ItemFrame(fDropIndex).Contains(where))
					break;
				else
					InvalidateItem(fDropIndex);

				// mouse over new item
				bool found = false;
				for (int32 index = 0; index <= count; index++)
				{
					r = ItemFrame(index);
					if (r.Contains(where))
					{
						SetHighColor(255, 0, 0, 255);
						StrokeLine(r.LeftTop(), r.RightTop(), B_SOLID_HIGH);
						r.top++;
						StrokeLine(r.LeftTop(), r.RightTop(), B_SOLID_HIGH);
						fDropIndex = index;
						found = true;
						break;
					}
				}
				if (found)
					break;
				// mouse is after last item
				fDropIndex = count;
				r = Bounds();
				if (count > 0)
					r.top = ItemFrame(count - 1).bottom + 1.0;
				SetHighColor(255, 0, 0, 255);
				StrokeLine(r.LeftTop(), r.RightTop(), B_SOLID_HIGH);
				r.top++;
				StrokeLine(r.LeftTop(), r.RightTop(), B_SOLID_HIGH);
				break;
			}
			case B_EXITED_VIEW:
			{
				int32 count = CountItems();
				if (count > 0)
				{
					if (fDropIndex == count)
					{
						BRect r(Bounds());
						r.top = ItemFrame(count - 1).bottom + 1.0;
						r.bottom = r.top + 2.0;
						Invalidate(r);
					}
					else
						InvalidateItem(fDropIndex);
				}
				break;
			}
			case B_OUTSIDE_VIEW:
				break;
		}
	}
	else
		BListView::MouseMoved(where, transit, msg);
}

/*****************************************************************************
 * DragSortableListView::MouseUp
 *****************************************************************************/
void
DragSortableListView::MouseUp( BPoint where )
{
	// remove drop mark
	if ( fDropIndex >= 0 && fDropIndex < CountItems() )
		InvalidateItem( fDropIndex );
	BListView::MouseUp( where );
}

/*****************************************************************************
 * DragSortableListView::DrawItem
 *****************************************************************************/
void
DragSortableListView::DrawItem( BListItem *item, BRect itemFrame, bool complete )
{
	DrawListItem( this, IndexOf( item ), itemFrame );
}


/*****************************************************************************
 * PlaylistView class
 *****************************************************************************/
PlaylistView::PlaylistView( BRect frame, InterfaceWindow* mainWindow )
	: DragSortableListView( frame, "playlist listview",
							B_MULTIPLE_SELECTION_LIST, B_FOLLOW_ALL_SIDES,
							B_WILL_DRAW | B_NAVIGABLE | B_PULSE_NEEDED
							| B_FRAME_EVENTS | B_FULL_UPDATE_ON_RESIZE ),
	  fCurrentIndex( -1 ),
	  fPlaying( false ),
	  fMainWindow( mainWindow )
{
}

PlaylistView::~PlaylistView()
{
}

/*****************************************************************************
 * PlaylistView::AttachedToWindow
 *****************************************************************************/
void
PlaylistView::AttachedToWindow()
{
	// get pulse message every two frames
	Window()->SetPulseRate(80000);
}

/*****************************************************************************
 * PlaylistView::MouseDown
 *****************************************************************************/
void
PlaylistView::MouseDown( BPoint where )
{
	int32 clicks = 1;
	Window()->CurrentMessage()->FindInt32( "clicks", &clicks );
	bool handled = false;
	for ( int32 i = 0; PlaylistItem* item = (PlaylistItem*)ItemAt( i ); i++ )
	{
		BRect r = ItemFrame( i );
		if ( r.Contains( where ) )
		{
			if ( clicks == 2 )
			{
				Intf_VLCWrapper::playlistJumpTo( i );
				handled = true;
			}
			else if ( i == fCurrentIndex )
			{
				r.right = r.left + TEXT_OFFSET;
				if ( r.Contains ( where ) )
				{
					fMainWindow->PostMessage( PAUSE_PLAYBACK );
					InvalidateItem( i );
					handled = true;
				}
			}
			break;
		}
	}
	if ( !handled )
		DragSortableListView::MouseDown(where);
}

/*****************************************************************************
 * PlaylistView::KeyDown
 *****************************************************************************/
void
PlaylistView::KeyDown( const char* bytes, int32 numBytes )
{
	if (numBytes < 1)
		return;
		
	if ( ( bytes[0] == B_BACKSPACE ) || ( bytes[0] == B_DELETE ) )
	{
		int32 i = CurrentSelection();
		if ( BListItem *item = ItemAt( i ) )
		{
/*			if ( RemoveItem( item ) )
			{
				delete item;
				Select( i + 1 );
			}*/
		}
	}
	DragSortableListView::KeyDown( bytes, numBytes );
}

/*****************************************************************************
 * PlaylistView::Pulse
 *****************************************************************************/
void
PlaylistView::Pulse()
{
	if (fMainWindow->IsStopped())
		SetPlaying( false );
}

/*****************************************************************************
 * PlaylistView::CloneItem
 *****************************************************************************/
BListItem*
PlaylistView::CloneItem( int32 atIndex ) const
{
	BListItem* clone = NULL;
	if ( PlaylistItem* item = dynamic_cast<PlaylistItem*>( ItemAt( atIndex ) ) )
		clone = new PlaylistItem( item->Text() );
	return clone;
}

/*****************************************************************************
 * PlaylistView::DrawListItem
 *****************************************************************************/
void
PlaylistView::DrawListItem( BView* owner, int32 index, BRect frame ) const
{
	if ( PlaylistItem* item = dynamic_cast<PlaylistItem*>( ItemAt( index ) ) )
		item->Draw( owner,  frame, index % 2, index == fCurrentIndex, fPlaying );
}

/*****************************************************************************
 * PlaylistView::MakeDragMessage
 *****************************************************************************/
void
PlaylistView::MakeDragMessage( BMessage* message ) const
{
	if ( message )
	{
		message->AddPointer( "list", (void*)this );
		for ( int32 i = 0; BListItem* item = ItemAt( CurrentSelection( i ) ); i++ )
			message->AddPointer( "item", (void*)item );
	}
}

/*****************************************************************************
 * PlaylistView::SetCurrent
 *****************************************************************************/
void
PlaylistView::SetCurrent( int32 index )
{
	if ( fCurrentIndex != index )
	{
		InvalidateItem( fCurrentIndex );
		fCurrentIndex = index;
		InvalidateItem( fCurrentIndex );
	}
}

/*****************************************************************************
 * PlaylistView::SetPlaying
 *****************************************************************************/
void
PlaylistView::SetPlaying( bool playing )
{
	if ( fPlaying != playing )
	{
		fPlaying = playing;
		InvalidateItem( fCurrentIndex );
	}
}
