/*****************************************************************************
 * PlayListWindow.cpp: beos interface
 *****************************************************************************
 * Copyright (C) 1999, 2000, 2001 the VideoLAN team
 * $Id$
 *
 * Authors: Jean-Marc Dressler <polux@via.ecp.fr>
 *          Samuel Hocevar <sam@zoy.org>
 *          Tony Castley <tony@castley.net>
 *          Richard Shepherd <richard@rshepherd.demon.co.uk>
 *          Stephan Aßmus <stippi@yellowbites.com>
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
#if 0
/* System headers */
#include <InterfaceKit.h>
#include <StorageKit.h>
#include <string.h>

/* VLC headers */
#include <vlc/vlc.h>
#include <vlc/intf.h>

/* BeOS interface headers */
#include "InterfaceWindow.h"
#include "ListViews.h"
#include "MsgVals.h"
#include "PlayListWindow.h"

enum
{
    MSG_SELECT_ALL          = 'sall',
    MSG_SELECT_NONE         = 'none',
    MSG_RANDOMIZE           = 'rndm',
    MSG_SORT_REVERSE        = 'srtr',
    MSG_SORT_NAME           = 'srtn',
    MSG_SORT_PATH           = 'srtp',
    MSG_REMOVE              = 'rmov',
    MSG_REMOVE_ALL          = 'rmal',

    MSG_SELECTION_CHANGED   = 'slch',
    MSG_SET_DISPLAY         = 'stds',
};


/*****************************************************************************
 * PlayListWindow::PlayListWindow
 *****************************************************************************/
PlayListWindow::PlayListWindow( BRect frame, const char* name,
                                InterfaceWindow* mainWindow,
                                intf_thread_t *p_interface )
    :   BWindow( frame, name, B_FLOATING_WINDOW_LOOK, B_NORMAL_WINDOW_FEEL,
                 B_WILL_ACCEPT_FIRST_CLICK | B_ASYNCHRONOUS_CONTROLS ),
        fMainWindow( mainWindow )
{
    char psz_tmp[1024];
#define ADD_ELLIPSIS( a ) \
    memset( psz_tmp, 0, 1024 ); \
    snprintf( psz_tmp, 1024, "%s%s", a, B_UTF8_ELLIPSIS );

    p_intf = p_interface;
    
    SetName( _("playlist") );

    // set up the main menu bar
    fMenuBar = new BMenuBar( BRect(0.0, 0.0, frame.Width(), 15.0), "main menu",
                             B_FOLLOW_NONE, B_ITEMS_IN_ROW, false );

    AddChild( fMenuBar );

    // Add the File menu
    BMenu *fileMenu = new BMenu( _("File") );
    fMenuBar->AddItem( fileMenu );
    ADD_ELLIPSIS( _("Open File") );
    BMenuItem* item = new BMenuItem( psz_tmp, new BMessage( OPEN_FILE ), 'O' );
    item->SetTarget( fMainWindow );
    fileMenu->AddItem( item );

    CDMenu* cd_menu = new CDMenu( _("Open Disc") );
    fileMenu->AddItem( cd_menu );

    fileMenu->AddSeparatorItem();
    item = new BMenuItem( _("Close"),
                          new BMessage( B_QUIT_REQUESTED ), 'W' );
    fileMenu->AddItem( item );

    // Add the Edit menu
    BMenu *editMenu = new BMenu( _("Edit") );
    fMenuBar->AddItem( editMenu );
    fSelectAllMI = new BMenuItem( _("Select All"),
                                  new BMessage( MSG_SELECT_ALL ), 'A' );
    editMenu->AddItem( fSelectAllMI );
    fSelectNoneMI = new BMenuItem( _("Select None"),
                                   new BMessage( MSG_SELECT_NONE ), 'A', B_SHIFT_KEY );
    editMenu->AddItem( fSelectNoneMI );

    editMenu->AddSeparatorItem();
    fSortReverseMI = new BMenuItem( _("Sort Reverse"),
                                 new BMessage( MSG_SORT_REVERSE ), 'F' );
    editMenu->AddItem( fSortReverseMI );
    fSortNameMI = new BMenuItem( _("Sort by Name"),
                                 new BMessage( MSG_SORT_NAME ), 'N' );
fSortNameMI->SetEnabled( false );
    editMenu->AddItem( fSortNameMI );
    fSortPathMI = new BMenuItem( _("Sort by Path"),
                                 new BMessage( MSG_SORT_PATH ), 'P' );
fSortPathMI->SetEnabled( false );
    editMenu->AddItem( fSortPathMI );
    fRandomizeMI = new BMenuItem( _("Randomize"),
                                  new BMessage( MSG_RANDOMIZE ), 'R' );
fRandomizeMI->SetEnabled( false );
    editMenu->AddItem( fRandomizeMI );
    editMenu->AddSeparatorItem();
    fRemoveMI = new BMenuItem( _("Remove"),
                          new BMessage( MSG_REMOVE ) );
    editMenu->AddItem( fRemoveMI );
    fRemoveAllMI = new BMenuItem( _("Remove All"),
                                  new BMessage( MSG_REMOVE_ALL ) );
    editMenu->AddItem( fRemoveAllMI );

    // Add View menu
    fViewMenu = new BMenu( _("View") );
    fMenuBar->AddItem( fViewMenu );

    fViewMenu->SetRadioMode( true );
    BMessage* message = new BMessage( MSG_SET_DISPLAY );
    message->AddInt32( "mode", DISPLAY_PATH );
    item = new BMenuItem( _("Path"), message );
    item->SetMarked( true );
    fViewMenu->AddItem( item );

    message = new BMessage( MSG_SET_DISPLAY );
    message->AddInt32( "mode", DISPLAY_NAME );
    item = new BMenuItem( _("Name"), message );
    fViewMenu->AddItem( item );

    // make menu bar resize to correct height
    float menuWidth, menuHeight;
    fMenuBar->GetPreferredSize( &menuWidth, &menuHeight );
    // don't change next line! it's a workarround!
    fMenuBar->ResizeTo( frame.Width(), menuHeight );

    frame = Bounds();
    frame.top += fMenuBar->Bounds().IntegerHeight() + 1;
    frame.right -= B_V_SCROLL_BAR_WIDTH;

    fListView = new PlaylistView( p_intf, frame, fMainWindow,
                                  new BMessage( MSG_SELECTION_CHANGED ) );
    fBackgroundView = new BScrollView( "playlist scrollview",
                                       fListView, B_FOLLOW_ALL_SIDES,
                                       0, false, true,
                                       B_NO_BORDER );

    AddChild( fBackgroundView );

    // be up to date
    UpdatePlaylist();
    FrameResized( Bounds().Width(), Bounds().Height() );
    SetSizeLimits( menuWidth * 1.5, menuWidth * 8.0,
                   menuHeight * 5.0, menuHeight * 50.0 );

    UpdatePlaylist( true );
    // start window thread in hidden state
    Hide();
    Show();
}

/*****************************************************************************
 * PlayListWindow::~PlayListWindow
 *****************************************************************************/
PlayListWindow::~PlayListWindow()
{
}

/*****************************************************************************
 * PlayListWindow::QuitRequested
 *****************************************************************************/
bool
PlayListWindow::QuitRequested()
{
    Hide(); 
    return false;
}

/*****************************************************************************
 * PlayListWindow::MessageReceived
 *****************************************************************************/
void
PlayListWindow::MessageReceived( BMessage * p_message )
{
    switch ( p_message->what )
    {
        case OPEN_DVD:
        case B_REFS_RECEIVED:
        case B_SIMPLE_DATA:
            // forward to interface window
            fMainWindow->PostMessage( p_message );
            break;
        case MSG_SELECT_ALL:
            fListView->Select( 0, fListView->CountItems() - 1 );
            break;
        case MSG_SELECT_NONE:
            fListView->DeselectAll();
            break;
        case MSG_RANDOMIZE:
            break;
        case MSG_SORT_REVERSE:
            fListView->SortReverse();
            break;
        case MSG_SORT_NAME:
            break;
        case MSG_SORT_PATH:
            break;
        case MSG_REMOVE:
            fListView->RemoveSelected();
            break;
        case MSG_REMOVE_ALL:
            fListView->Select( 0, fListView->CountItems() - 1 );
            fListView->RemoveSelected();
            break;
        case MSG_SELECTION_CHANGED:
            _CheckItemsEnableState();
            break;
        case MSG_SET_DISPLAY:
        {
            uint32 mode;
            if ( p_message->FindInt32( "mode", (int32*)&mode ) == B_OK )
                SetDisplayMode( mode );
            break;
        }
        case B_MODIFIERS_CHANGED:
            fListView->ModifiersChanged();
            break;
        default:
            BWindow::MessageReceived( p_message );
            break;
    }
}

/*****************************************************************************
 * PlayListWindow::FrameResized
 *****************************************************************************/
void
PlayListWindow::FrameResized(float width, float height)
{
    BRect r(Bounds());
    fMenuBar->MoveTo(r.LeftTop());
    fMenuBar->ResizeTo(r.Width(), fMenuBar->Bounds().Height());
    r.top += fMenuBar->Bounds().Height() + 1.0;
    fBackgroundView->MoveTo(r.LeftTop());
    // the "+ 1.0" is to make the scrollbar
    // be partly covered by the window border
    fBackgroundView->ResizeTo(r.Width() + 1.0, r.Height() + 1.0);
}

/*****************************************************************************
 * PlayListWindow::ReallyQuit
 *****************************************************************************/
void
PlayListWindow::ReallyQuit()
{
    Lock();
    Hide();
    Quit();
}

/*****************************************************************************
 * PlayListWindow::UpdatePlaylist
 *****************************************************************************/
void
PlayListWindow::UpdatePlaylist( bool rebuild )
{
    playlist_t * p_playlist;

    if( rebuild )
        fListView->RebuildList();

    p_playlist = (playlist_t *)
        vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST, FIND_ANYWHERE );
    fListView->SetCurrent( p_playlist->i_index );
    fListView->SetPlaying( p_playlist->status.i_status == PLAYLIST_RUNNING );
    vlc_object_release( p_playlist );

    _CheckItemsEnableState();
}

/*****************************************************************************
 * PlayListWindow::SetDisplayMode
 *****************************************************************************/
void
PlayListWindow::SetDisplayMode( uint32 mode )
{
    if ( Lock() )
    {
        // propagate to list view
        fListView->SetDisplayMode( mode );
        // mark correct menu item
        for ( int32 i = 0; BMenuItem* item = fViewMenu->ItemAt( i ); i++ )
        {
            BMessage* message = item->Message();
            uint32 itemMode;
            if ( message
                 && message->FindInt32( "mode", (int32*)&itemMode ) == B_OK
                 && itemMode == mode )
            {
                item->SetMarked( true );
                break;
            }
        }
        Unlock();
    }
}

/*****************************************************************************
 * PlayListWindow::DisplayMode
 *****************************************************************************/
uint32
PlayListWindow::DisplayMode() const
{
    return fListView->DisplayMode();
}

/*****************************************************************************
 * PlayListWindow::_CheckItemsEnableState
 *****************************************************************************/
void
PlayListWindow::_CheckItemsEnableState() const
{
    // check if one item selected
    int32 test = fListView->CurrentSelection( 0 );
    bool enable1 = test >= 0;
    // check if at least two items selected
    test = fListView->CurrentSelection( 1 );
    bool enable2 = test >= 0;
    bool notEmpty = fListView->CountItems() > 0;
    _SetMenuItemEnabled( fSelectAllMI, notEmpty );
    _SetMenuItemEnabled( fSelectNoneMI, enable1 );
    _SetMenuItemEnabled( fSortReverseMI, enable2 );
//  _SetMenuItemEnabled( fSortNameMI, enable2 );
//  _SetMenuItemEnabled( fSortPathMI, enable2 );
//  _SetMenuItemEnabled( fRandomizeMI, enable2 );
    _SetMenuItemEnabled( fRemoveMI, enable1 );
    _SetMenuItemEnabled( fRemoveAllMI, notEmpty );
}

/*****************************************************************************
 * PlayListWindow::_SetMenuItemEnabled
 *****************************************************************************/
void
PlayListWindow::_SetMenuItemEnabled( BMenuItem* item, bool enabled ) const
{
    // this check should actally be done in BMenuItem::SetEnabled(), but it is not...
    if ( item->IsEnabled() != enabled )
        item->SetEnabled( enabled );
}
#endif
