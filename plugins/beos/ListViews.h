/*****************************************************************************
 * ListViews.h: BeOS interface list view class prototype
 *****************************************************************************
 * Copyright (C) 1999, 2000, 2001 VideoLAN
 * $Id: ListViews.h,v 1.1.2.1 2002/09/29 12:06:08 titer Exp $
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

#ifndef LIST_VIEWS_H
#define LIST_VIEWS_H

#include <ListItem.h>
#include <ListView.h>

class InterfaceWindow;

// PlaylistItem
class PlaylistItem : public BStringItem
{
 public:
							PlaylistItem( const char* name );
		virtual				~PlaylistItem();

		virtual	void		Draw( BView* owner, BRect frame,
								  bool tintedLine,
								  bool active = false,
								  bool playing = false );

};

// DragSortableListView
class DragSortableListView : public BListView
{
 public:
							DragSortableListView( BRect frame,
												  const char* name,
												  list_view_type type
														= B_SINGLE_SELECTION_LIST,
												  uint32 resizingMode
														= B_FOLLOW_LEFT
														  | B_FOLLOW_TOP,
												  uint32 flags
														= B_WILL_DRAW
														  | B_NAVIGABLE
														  | B_FRAME_EVENTS );
	virtual					~DragSortableListView();

							// BListView
	virtual	void			Draw( BRect updateRect );
	virtual	bool			InitiateDrag( BPoint point, int32 index,
										  bool wasSelected );
	virtual void			MessageReceived( BMessage* message );
	virtual void			MouseMoved( BPoint where, uint32 transit,
										const BMessage* dragMessage );
	virtual void			MouseUp( BPoint where );
	virtual	void			WindowActivated( bool active );
	virtual void			DrawItem( BListItem *item, BRect itemFrame,
									  bool complete = false);

							// DragSortableListView
	virtual	BListItem*		CloneItem( int32 atIndex ) const = 0;
	virtual	void			DrawListItem( BView* owner, int32 index,
										  BRect itemFrame ) const = 0;
	virtual	void			MakeDragMessage( BMessage* message ) const = 0;

 private:
	int32			fDropIndex;
};

// PlaylistView
class PlaylistView : public DragSortableListView
{
 public:
							PlaylistView( BRect frame,
										  InterfaceWindow* mainWindow );
							~PlaylistView();

							// BListView
	virtual	void			AttachedToWindow();
	virtual void			MouseDown( BPoint where );
	virtual	void			KeyDown( const char* bytes, int32 numBytes );
	virtual	void			Pulse();

							// DragSortableListView
	virtual	BListItem*		CloneItem( int32 atIndex ) const;
	virtual	void			DrawListItem( BView* owner, int32 index,
										  BRect itemFrame ) const;
	virtual	void			MakeDragMessage( BMessage* message ) const;

							// PlaylistView
			void			SetCurrent( int32 index );
			void			SetPlaying( bool playing );

 private:
	int32					fCurrentIndex;
	bool					fPlaying;
	InterfaceWindow*		fMainWindow;
};

#endif // LIST_VIEWS_H
