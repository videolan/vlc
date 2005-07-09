/*****************************************************************************
 * ListViews.h: BeOS interface list view class prototype
 *****************************************************************************
 * Copyright (C) 1999, 2000, 2001 the VideoLAN team
 * $Id$
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
#include <String.h>

enum
{
	DISPLAY_PATH	= 0,
	DISPLAY_NAME,
};

class InterfaceWindow;

// PlaylistItem
class PlaylistItem : public BStringItem
{
 public:
							PlaylistItem( const char* name );
		virtual				~PlaylistItem();

		virtual	void		Draw( BView* owner, BRect frame,
								  bool tintedLine,
								  uint32 mode,
								  bool active = false,
								  bool playing = false );

 private:
		BString				fName;	// additional to BStringItem::Text()

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
	virtual	void			ModifiersChanged();	// called by window

	virtual	void			MoveItems( BList& items, int32 toIndex );
	virtual	void			CopyItems( BList& items, int32 toIndex );
	virtual	void			RemoveItemList( BList& indices );
			void			RemoveSelected(); // uses RemoveItemList()
			int32			CountSelectedItems() const;

	virtual	BListItem*		CloneItem( int32 atIndex ) const = 0;
	virtual	void			DrawListItem( BView* owner, int32 index,
										  BRect itemFrame ) const = 0;
	virtual	void			MakeDragMessage( BMessage* message ) const = 0;

 private:
			void			_SetDropAnticipationRect( BRect r );
			void			_SetDropIndex( int32 index );
			void			_RemoveDropAnticipationRect();

	BRect					fDropRect;
	BMessage				fDragMessageCopy;

 protected:
	int32					fDropIndex;
};

// PlaylistView
class PlaylistView : public DragSortableListView
{
 public:
							PlaylistView( intf_thread_t * p_intf,
							              BRect frame,
										  InterfaceWindow* mainWindow,
										  BMessage* selectionChangeMessage = NULL );
							~PlaylistView();

							// BListView
	virtual	void			AttachedToWindow();
	virtual void			MessageReceived( BMessage* message );
	virtual void			MouseDown( BPoint where );
	virtual	void			KeyDown( const char* bytes, int32 numBytes );
	virtual	void			Pulse();
	virtual	void			SelectionChanged();

							// DragSortableListView
	virtual	void			MoveItems( BList& items, int32 toIndex );
	virtual	void			CopyItems( BList& items, int32 toIndex );
	virtual	void			RemoveItemList( BList& indices );

	virtual	BListItem*		CloneItem( int32 atIndex ) const;
	virtual	void			DrawListItem( BView* owner, int32 index,
										  BRect itemFrame ) const;
	virtual	void			MakeDragMessage( BMessage* message ) const;

							// PlaylistView
			void			SetCurrent( int32 index );
			void			SetPlaying( bool playing );
			void			RebuildList();

			void			SortReverse();
			void			SortByPath();
			void			SortByName();

			void			SetDisplayMode( uint32 mode );
			uint32			DisplayMode() const
								{ return fDisplayMode; }

 private:
			BListItem*		_PlayingItem() const;
			void			_SetPlayingIndex( BListItem* item );

    intf_thread_t * p_intf;

	int32					fCurrentIndex;
	bool					fPlaying;
	uint32					fDisplayMode;
	InterfaceWindow*		fMainWindow;
	BMessage*				fSelectionChangeMessage;
	PlaylistItem*			fLastClickedItem;
};

#endif // LIST_VIEWS_H
