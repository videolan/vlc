/*****************************************************************************
 * ListViews.h: BeOS interface list view class implementation
 *****************************************************************************
 * Copyright (C) 1999, 2000, 2001 VideoLAN (Centrale Réseaux) and its contributors
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

#include <stdio.h>
#include <malloc.h>

#include <Bitmap.h>
#include <Entry.h>
#include <String.h>

/* VLC headers */
#include <vlc/vlc.h>
#include <vlc/intf.h>

#include "InterfaceWindow.h"
#include "ListViews.h"
#include "MsgVals.h"

#define MAX_DRAG_HEIGHT        200.0
#define ALPHA                170
#define TEXT_OFFSET            20.0

/*****************************************************************************
 * PlaylistItem class
 *****************************************************************************/
PlaylistItem::PlaylistItem( const char *name )
    : BStringItem( name ),
      fName( "" )
{
    entry_ref ref;
    if ( get_ref_for_path( name, &ref) == B_OK )
        fName.SetTo( ref.name );
}

PlaylistItem::~PlaylistItem()
{
}

/*****************************************************************************
 * PlaylistItem::DrawItem
 *****************************************************************************/
void
PlaylistItem::Draw( BView *owner, BRect frame, bool tintedLine,
                    uint32 mode, bool active, bool playing )
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
    const char* text = Text();
    switch ( mode )
    {
        case DISPLAY_NAME:
            if ( fName.CountChars() > 0 )
                text = fName.String();
            break;
        case DISPLAY_PATH:
        default:
            break;
    }
    BString truncatedString( text );
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
      fDropRect( 0.0, 0.0, -1.0, -1.0 ),
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
    // drop anticipation indication
    if ( fDropRect.IsValid() )
    {
        SetHighColor( 255, 0, 0, 255 );
        StrokeRect( fDropRect );
    }
}

/*****************************************************************************
 * DragSortableListView::InitiateDrag
 *****************************************************************************/
bool
DragSortableListView::InitiateDrag( BPoint point, int32 index, bool )
{
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
            dragRect.bottom += ceilf( item->Height() ) + 1.0;
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
                    itemBounds.bottom = itemBounds.top + ceilf( item->Height() );
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
    switch ( message->what )
    {
        case B_MODIFIERS_CHANGED:
            ModifiersChanged();
            break;
        case B_SIMPLE_DATA:
        {
            DragSortableListView *list = NULL;
            if ( message->FindPointer( "list", (void **)&list ) == B_OK
                 && list == this )
            {
                int32 count = CountItems();
                if ( fDropIndex < 0 || fDropIndex > count )
                    fDropIndex = count;
                BList items;
                int32 index;
                for ( int32 i = 0; message->FindInt32( "index", i, &index ) == B_OK; i++ )
                    if ( BListItem* item = ItemAt(index) )
                        items.AddItem( (void*)item );
                if ( items.CountItems() > 0 )
                {
                    if ( modifiers() & B_SHIFT_KEY )
                        CopyItems( items, fDropIndex );
                    else
                        MoveItems( items, fDropIndex );
                }
                fDropIndex = -1;
            }
            break;
        }
        default:
            BListView::MessageReceived( message );
            break;
    }
}

/*****************************************************************************
 * DragSortableListView::MouseMoved
 *****************************************************************************/
void
DragSortableListView::MouseMoved(BPoint where, uint32 transit, const BMessage *msg)
{
    if ( msg && ( msg->what == B_SIMPLE_DATA || msg->what == MSG_SOUNDPLAY ) )
    {
        bool replaceAll = !msg->HasPointer("list") && !(modifiers() & B_SHIFT_KEY);
        switch ( transit )
        {
            case B_ENTERED_VIEW:
                // remember drag message
                // this is needed to react on modifier changes
                fDragMessageCopy = *msg;
            case B_INSIDE_VIEW:
            {
                if ( replaceAll )
                {
                    BRect r( Bounds() );
                    r.bottom--;    // compensate for scrollbar offset
                    _SetDropAnticipationRect( r );
                    fDropIndex = -1;
                }
                else
                {
                    // offset where by half of item height
                    BRect r( ItemFrame( 0 ) );
                    where.y += r.Height() / 2.0;
    
                    int32 index = IndexOf( where );
                    if ( index < 0 )
                        index = CountItems();
                    _SetDropIndex( index );
                }
                break;
            }
            case B_EXITED_VIEW:
                // forget drag message
                fDragMessageCopy.what = 0;
            case B_OUTSIDE_VIEW:
                _RemoveDropAnticipationRect();
                break;
        }
    }
    else
    {
        _RemoveDropAnticipationRect();
        BListView::MouseMoved(where, transit, msg);
        fDragMessageCopy.what = 0;
    }
}

/*****************************************************************************
 * DragSortableListView::MouseUp
 *****************************************************************************/
void
DragSortableListView::MouseUp( BPoint where )
{
    // remove drop mark
    _SetDropAnticipationRect( BRect( 0.0, 0.0, -1.0, -1.0 ) );
    // be sure to forget drag message
    fDragMessageCopy.what = 0;
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
 * DragSortableListView::ModifiersChaned
 *****************************************************************************/
void
DragSortableListView::ModifiersChanged()
{
    BPoint where;
    uint32 buttons;
    GetMouse( &where, &buttons, false );
    uint32 transit = Bounds().Contains( where ) ? B_INSIDE_VIEW : B_OUTSIDE_VIEW;
    MouseMoved( where, transit, &fDragMessageCopy );
}

/*****************************************************************************
 * DragSortableListView::MoveItems
 *****************************************************************************/
void
DragSortableListView::MoveItems( BList& items, int32 index )
{
    DeselectAll();
    // we remove the items while we look at them, the insertion index is decreased
    // when the items index is lower, so that we insert at the right spot after
    // removal
    BList removedItems;
    int32 count = items.CountItems();
    for ( int32 i = 0; i < count; i++ )
    {
        BListItem* item = (BListItem*)items.ItemAt( i );
        int32 removeIndex = IndexOf( item );
        if ( RemoveItem( item ) && removedItems.AddItem( (void*)item ) )
        {
            if ( removeIndex < index )
                index--;
        }
        // else ??? -> blow up
    }
    for ( int32 i = 0; BListItem* item = (BListItem*)removedItems.ItemAt( i ); i++ )
    {
        if ( AddItem( item, index ) )
        {
            // after we're done, the newly inserted items will be selected
            Select( index, true );
            // next items will be inserted after this one
            index++;
        }
        else
            delete item;
    }
}

/*****************************************************************************
 * DragSortableListView::CopyItems
 *****************************************************************************/
void
DragSortableListView::CopyItems( BList& items, int32 index )
{
    DeselectAll();
    // by inserting the items after we copied all items first, we avoid
    // cloning an item we already inserted and messing everything up
    // in other words, don't touch the list before we know which items
    // need to be cloned
    BList clonedItems;
    int32 count = items.CountItems();
    for ( int32 i = 0; i < count; i++ )
    {
        BListItem* item = CloneItem( IndexOf( (BListItem*)items.ItemAt( i ) ) );
        if ( item && !clonedItems.AddItem( (void*)item ) )
            delete item;
    }
    for ( int32 i = 0; BListItem* item = (BListItem*)clonedItems.ItemAt( i ); i++ )
    {
        if ( AddItem( item, index ) )
        {
            // after we're done, the newly inserted items will be selected
            Select( index, true );
            // next items will be inserted after this one
            index++;
        }
        else
            delete item;
    }
}

/*****************************************************************************
 * DragSortableListView::RemoveItemList
 *****************************************************************************/
void
DragSortableListView::RemoveItemList( BList& items )
{
    int32 count = items.CountItems();
    for ( int32 i = 0; i < count; i++ )
    {
        BListItem* item = (BListItem*)items.ItemAt( i );
        if ( RemoveItem( item ) )
            delete item;
    }
}

/*****************************************************************************
 * DragSortableListView::RemoveSelected
 *****************************************************************************/
void
DragSortableListView::RemoveSelected()
{
    BList items;
    for ( int32 i = 0; BListItem* item = ItemAt( CurrentSelection( i ) ); i++ )
        items.AddItem( (void*)item );
    RemoveItemList( items );
}

/*****************************************************************************
 * DragSortableListView::CountSelectedItems
 *****************************************************************************/
int32
DragSortableListView::CountSelectedItems() const
{
    int32 count = 0;
    while ( CurrentSelection( count ) >= 0 )
        count++;
    return count;
}

/*****************************************************************************
 * DragSortableListView::_SetDropAnticipationRect
 *****************************************************************************/
void
DragSortableListView::_SetDropAnticipationRect( BRect r )
{
    if ( fDropRect != r )
    {
        if ( fDropRect.IsValid() )
            Invalidate( fDropRect );
        fDropRect = r;
        if ( fDropRect.IsValid() )
            Invalidate( fDropRect );
    }
}

/*****************************************************************************
 * DragSortableListView::_SetDropAnticipationRect
 *****************************************************************************/
void
DragSortableListView::_SetDropIndex( int32 index )
{
    if ( fDropIndex != index )
    {
        fDropIndex = index;
        if ( fDropIndex == -1 )
            _SetDropAnticipationRect( BRect( 0.0, 0.0, -1.0, -1.0 ) );
        else
        {
            int32 count = CountItems();
            if ( fDropIndex == count )
            {
                BRect r;
                if ( BListItem* item = ItemAt( count - 1 ) )
                {
                    r = ItemFrame( count - 1 );
                    r.top = r.bottom + 1.0;
                    r.bottom = r.top + 1.0;
                }
                else
                {
                    r = Bounds();
                    r.bottom--;    // compensate for scrollbars moved slightly out of window
                }
                _SetDropAnticipationRect( r );
            }
            else
            {
                BRect r = ItemFrame( fDropIndex );
                r.bottom = r.top + 1.0;
                _SetDropAnticipationRect( r );
            }
        }
    }
}

/*****************************************************************************
 * DragSortableListView::_RemoveDropAnticipationRect
 *****************************************************************************/
void
DragSortableListView::_RemoveDropAnticipationRect()
{
    _SetDropAnticipationRect( BRect( 0.0, 0.0, -1.0, -1.0 ) );
    _SetDropIndex( -1 );
}


/*****************************************************************************
 * PlaylistView class
 *****************************************************************************/
PlaylistView::PlaylistView( intf_thread_t * _p_intf,
                            BRect frame, InterfaceWindow* mainWindow,
                            BMessage* selectionChangeMessage )
    : DragSortableListView( frame, "playlist listview",
                            B_MULTIPLE_SELECTION_LIST, B_FOLLOW_ALL_SIDES,
                            B_WILL_DRAW | B_NAVIGABLE | B_PULSE_NEEDED
                            | B_FRAME_EVENTS | B_FULL_UPDATE_ON_RESIZE ),
      p_intf( _p_intf ),
      fCurrentIndex( -1 ),
      fPlaying( false ),
      fDisplayMode( DISPLAY_PATH ),
      fMainWindow( mainWindow ),
      fSelectionChangeMessage( selectionChangeMessage ),
      fLastClickedItem( NULL )
{
}

PlaylistView::~PlaylistView()
{
    delete fSelectionChangeMessage;
}

/*****************************************************************************
 * PlaylistView::AttachedToWindow
 *****************************************************************************/
void
PlaylistView::AttachedToWindow()
{
    // get pulse message every two frames
    Window()->SetPulseRate( 80000 );
}

/*****************************************************************************
 * PlaylistView::MessageReceived
 *****************************************************************************/
void
PlaylistView::MessageReceived( BMessage* message)
{
    switch ( message->what )
    {
        case MSG_SOUNDPLAY:
        case B_SIMPLE_DATA:
            if ( message->HasPointer( "list" ) )
            {
                // message comes from ourself
                DragSortableListView::MessageReceived( message );
            }
            else
            {
                // message comes from another app (for example Tracker)
                message->AddInt32( "drop index", fDropIndex );
                fMainWindow->PostMessage( message, fMainWindow );
            }
            break;
        default:
            DragSortableListView::MessageReceived( message );
            break;
    }
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
                // only do something if user clicked the same item twice
                if ( fLastClickedItem == item )
                {
                    playlist_t * p_playlist;
                    p_playlist = (playlist_t *) vlc_object_find( p_intf,
                        VLC_OBJECT_PLAYLIST, FIND_ANYWHERE );
                    if( p_playlist )
                    {
                        playlist_Goto( p_playlist, i );
                        vlc_object_release( p_playlist );
                    }
                    handled = true;
                }
            }
            else
            {
                // remember last clicked item
                fLastClickedItem = item;
                if ( i == fCurrentIndex )
                {
                    r.right = r.left + TEXT_OFFSET;
                    if ( r.Contains ( where ) )
                    {
                        fMainWindow->PostMessage( PAUSE_PLAYBACK );
                        InvalidateItem( i );
                        handled = true;
                    }
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
    if ( numBytes < 1 )
        return;
        
    if ( ( bytes[0] == B_BACKSPACE ) || ( bytes[0] == B_DELETE ) )
    {
        RemoveSelected();
    }
    DragSortableListView::KeyDown( bytes, numBytes );
}

/*****************************************************************************
 * PlaylistView::Pulse
 *****************************************************************************/
void
PlaylistView::Pulse()
{
    if ( fMainWindow->IsStopped() )
        SetPlaying( false );
}

/*****************************************************************************
 * PlaylistView::SelectionChanged
 *****************************************************************************/
void
PlaylistView::SelectionChanged()
{
    BLooper* looper = Looper();
    if ( fSelectionChangeMessage && looper )
    {
        BMessage message( *fSelectionChangeMessage );
        looper->PostMessage( &message );
    }
}


/*****************************************************************************
 * PlaylistView::MoveItems
 *****************************************************************************/
void
PlaylistView::MoveItems( BList& items, int32 index )
{
#if 0
    DeselectAll();
    // we remove the items while we look at them, the insertion index is decreased
    // when the items index is lower, so that we insert at the right spot after
    // removal
    if ( fVlcWrapper->PlaylistLock() )
    {
        BList removedItems;
        BList removeItems;
        int32 count = items.CountItems();
        int32 indexOriginal = index;
        // remember currently playing item
        BListItem* playingItem = _PlayingItem();
        // collect item pointers for removal by index
        for ( int32 i = 0; i < count; i++ )
        {
            int32 removeIndex = IndexOf( (BListItem*)items.ItemAt( i ) );
            void* item = fVlcWrapper->PlaylistItemAt( removeIndex );
            if ( item && removeItems.AddItem( item ) )
            {
                if ( removeIndex < index )
                    index--;
            }
            // else ??? -> blow up
        }
        // actually remove items using pointers
        for ( int32 i = 0; i < count; i++ )
        {
            void* item = fVlcWrapper->PlaylistRemoveItem( removeItems.ItemAt( i ) );
            if ( item && !removedItems.AddItem( item ) )
                free( item );
        }
        // add items at index
        for ( int32 i = 0; void* item = removedItems.ItemAt( i ); i++ )
        {
            if ( fVlcWrapper->PlaylistAddItem( item, index ) )
                // next items will be inserted after this one
                index++;
            else
                free( item );
        }
        // update GUI
        DragSortableListView::MoveItems( items, indexOriginal );
        // restore currently playing item
        _SetPlayingIndex( playingItem );
        // update interface (in case it isn't playing,
        // there is a chance that it needs to update)
        fMainWindow->PostMessage( MSG_UPDATE );
        fVlcWrapper->PlaylistUnlock();
    }
#endif
}

/*****************************************************************************
 * PlaylistView::CopyItems
 *****************************************************************************/
void
PlaylistView::CopyItems( BList& items, int32 toIndex )
{
#if 0
    DeselectAll();
    // we remove the items while we look at them, the insertion index is decreased
    // when the items index is lower, so that we insert at the right spot after
    // removal
    if ( fVlcWrapper->PlaylistLock() )
    {
        BList clonedItems;
        int32 count = items.CountItems();
        // remember currently playing item
        BListItem* playingItem = _PlayingItem();
        // collect cloned item pointers
        for ( int32 i = 0; i < count; i++ )
        {
            int32 index = IndexOf( (BListItem*)items.ItemAt( i ) );
            void* item = fVlcWrapper->PlaylistItemAt( index );
            void* cloned = fVlcWrapper->PlaylistCloneItem( item );
            if ( cloned && !clonedItems.AddItem( cloned ) )
                free( cloned );
            
        }
        // add cloned items at index
        int32 index = toIndex;
        for ( int32 i = 0; void* item = clonedItems.ItemAt( i ); i++ )
        {
            if ( fVlcWrapper->PlaylistAddItem( item, index ) )
                // next items will be inserted after this one
                index++;
            else
                free( item );
        }
        // update GUI
        DragSortableListView::CopyItems( items, toIndex );
        // restore currently playing item
        _SetPlayingIndex( playingItem );
        // update interface (in case it isn't playing,
        // there is a chance that it needs to update)
        fMainWindow->PostMessage( MSG_UPDATE );
        fVlcWrapper->PlaylistUnlock();
    }
#endif
}

/*****************************************************************************
 * PlaylistView::RemoveItemList
 *****************************************************************************/
void
PlaylistView::RemoveItemList( BList& items )
{
#if 0
    if ( fVlcWrapper->PlaylistLock() )
    {
        // remember currently playing item
        BListItem* playingItem = _PlayingItem();
        // collect item pointers for removal
        BList removeItems;
        int32 count = items.CountItems();
        for ( int32 i = 0; i < count; i++ )
        {
            int32 index = IndexOf( (BListItem*)items.ItemAt( i ) );
            void* item = fVlcWrapper->PlaylistItemAt( index );
            if ( item && !removeItems.AddItem( item ) )
                free( item );
        }
        // remove items from playlist
        count = removeItems.CountItems();
        for ( int32 i = 0; void* item = removeItems.ItemAt( i ); i++ )
        {
            fVlcWrapper->PlaylistRemoveItem( item );
        }
        // update GUI
        DragSortableListView::RemoveItemList( items );
        // restore currently playing item
        _SetPlayingIndex( playingItem );
        // update interface (in case it isn't playing,
        // there is a chance that it needs to update)
        fMainWindow->PostMessage( MSG_UPDATE );
        fVlcWrapper->PlaylistUnlock();
    }
#endif
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
        item->Draw( owner,  frame, index % 2,
                    fDisplayMode, index == fCurrentIndex, fPlaying );
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
        int32 index;
        for ( int32 i = 0; ( index = CurrentSelection( i ) ) >= 0; i++ )
        {
            message->AddInt32( "index", index );
            // add refs to message (inter application communication)
            if ( BStringItem* item = dynamic_cast<BStringItem*>( ItemAt( index ) ) )
            {
                entry_ref ref;
                if ( get_ref_for_path( item->Text(), &ref ) == B_OK )
                    message->AddRef( "refs", &ref );
            }
        }
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

/*****************************************************************************
 * PlaylistView::SetPlaying
 *****************************************************************************/
void
PlaylistView::RebuildList()
{
    playlist_t * p_playlist;
    p_playlist = (playlist_t *) vlc_object_find( p_intf,
        VLC_OBJECT_PLAYLIST, FIND_ANYWHERE );

    if( !p_playlist )
    {
        return;
    }

    // remove all items
    BListItem * item;
    int32 count = CountItems();
    while( ( item = RemoveItem( --count ) ) )
        delete item;
    
    // rebuild listview from VLC's playlist
    vlc_mutex_lock( &p_playlist->object_lock );
    for( int i = 0; i < p_playlist->i_size; i++ )
        AddItem( new PlaylistItem( p_playlist->pp_items[i]->input.psz_name ) );
    vlc_mutex_unlock( &p_playlist->object_lock );

    vlc_object_release( p_playlist );
}


/*****************************************************************************
 * PlaylistView::SortReverse
 *****************************************************************************/
void
PlaylistView::SortReverse()
{
#if 0
    if ( int32 count = CountSelectedItems() )
    {
        int32 last  = count - 1;
        // remember currently playing item
        BListItem* playingItem = _PlayingItem();
        for ( int32 first = 0; first < count / 2; first++, last-- )
        {
            int32 index1 = CurrentSelection( first);
            int32 index2 = CurrentSelection( last);
            if ( SwapItems( index1, index2 ) )
            {
                // index2 > index1, so the list won't get messed up
                // if we remove the items in that order
                // TODO: Error checking + handling!
                void* item2 = fVlcWrapper->PlaylistRemoveItem( index2 );
                void* item1 = fVlcWrapper->PlaylistRemoveItem( index1 );
                fVlcWrapper->PlaylistAddItem( item2, index1 );
                fVlcWrapper->PlaylistAddItem( item1, index2 );
            }
        }
        // restore currently playing item
        _SetPlayingIndex( playingItem );
    }
#endif
}

/*****************************************************************************
 * PlaylistView::SortByPath
 *****************************************************************************/
void
PlaylistView::SortByPath()
{
    
}

/*****************************************************************************
 * PlaylistView::SortByName
 *****************************************************************************/
void
PlaylistView::SortByName()
{
}

/*****************************************************************************
 * PlaylistView::SetDisplayMode
 *****************************************************************************/
void
PlaylistView::SetDisplayMode( uint32 mode )
{
    if ( mode != fDisplayMode )
    {
        fDisplayMode = mode;
        Invalidate();
    }
}

/*****************************************************************************
 * PlaylistView::_PlayingItem
 *****************************************************************************/
BListItem*
PlaylistView::_PlayingItem() const
{
    playlist_t * p_playlist;
    p_playlist = (playlist_t *) vlc_object_find( p_intf,
        VLC_OBJECT_PLAYLIST, FIND_ANYWHERE );

    if( !p_playlist )
    {
        return NULL;
    }

    BListItem * item = ItemAt( p_playlist->i_index );
    vlc_object_release( p_playlist );
    return item;
}

/*****************************************************************************
 * PlaylistView::_SetPlayingIndex
 *****************************************************************************/
void
PlaylistView::_SetPlayingIndex( BListItem* playingItem )
{
    for ( int32 i = 0; BListItem* item = ItemAt( i ); i++ )
    {
        if ( item == playingItem )
        {
            playlist_t * p_playlist;
            p_playlist = (playlist_t *) vlc_object_find( p_intf,
                VLC_OBJECT_PLAYLIST, FIND_ANYWHERE );
        
            if( !p_playlist )
            {
                return;
            }

            playlist_Goto( p_playlist, i );
            SetCurrent( i );

            vlc_object_release( p_playlist );
            break;
        }
    }
}
