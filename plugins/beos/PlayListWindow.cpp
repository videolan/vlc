/*****************************************************************************
 * PlayListWindow.cpp: beos interface
 *****************************************************************************
 * Copyright (C) 1999, 2000, 2001 VideoLAN
 * $Id: PlayListWindow.cpp,v 1.3 2001/11/30 09:49:04 tcastley Exp $
 *
 * Authors: Jean-Marc Dressler <polux@via.ecp.fr>
 *          Samuel Hocevar <sam@zoy.org>
 *          Tony Castley <tony@castley.net>
 *          Richard Shepherd <richard@rshepherd.demon.co.uk>
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
#include "defs.h"

/* System headers */
#include <InterfaceKit.h>
#include <StorageKit.h>
#include <string.h>

/* VLC headers */
extern "C"
{
#include "config.h"
#include "common.h"
#include "intf_msg.h"
#include "threads.h"
#include "mtime.h"
#include "main.h"
#include "tests.h"
#include "stream_control.h"
#include "input_ext-intf.h"
#include "interface.h"
#include "intf_playlist.h"
}

/* BeOS interface headers */
#include "InterfaceWindow.h"
#include "PlayListWindow.h"
#include "MsgVals.h"

/*****************************************************************************
 * PlayListWindow
 *****************************************************************************/

PlayListWindow::PlayListWindow( BRect frame, const char *name,
                                  playlist_t *p_pl)
    : BWindow( frame, name, B_FLOATING_WINDOW_LOOK, B_NORMAL_WINDOW_FEEL,
                B_WILL_ACCEPT_FIRST_CLICK | B_ASYNCHRONOUS_CONTROLS )
{
    SetName( "playlist" );
    SetTitle(name);
    p_playlist = p_pl;

    /* set up the main menu */
    BMenuBar *menu_bar;
    menu_bar = new BMenuBar(BRect(0,0,0,0), "main menu");
    AddChild( menu_bar );

    BMenu *mFile;
    /* Add the file Menu */
    BMenuItem *mItem;
    menu_bar->AddItem( mFile = new BMenu( "File" ) );
    menu_bar->ResizeToPreferred();
    mFile->AddItem( mItem = new BMenuItem( "Open File" B_UTF8_ELLIPSIS,
                                           new BMessage(OPEN_FILE), 'O') );
    
    CDMenu *cd_menu = new CDMenu( "Open Disc" );
    mFile->AddItem( cd_menu );
    
    BRect rect = Bounds();
    rect.top += menu_bar->Bounds().IntegerHeight() + 1;
    BView *p_view = new BView(rect, NULL, B_FOLLOW_ALL_SIDES, B_WILL_DRAW);
    
    p_listview = new BListView(rect, "PlayList", 
                                    B_MULTIPLE_SELECTION_LIST);
    for (int i=0; i < p_playlist->i_size; i++)
    {
        p_listview->AddItem(new BStringItem(p_playlist->p_item[i].psz_name)); 
    }
    p_view->AddChild(new BScrollView("scroll_playlist", p_listview,
             B_FOLLOW_LEFT | B_FOLLOW_TOP, 0, false, true)); 
             
    AddChild(p_view);
}

PlayListWindow::~PlayListWindow()
{
}

/*****************************************************************************
 * PlayListWindow::MessageReceived
 *****************************************************************************/
void PlayListWindow::MessageReceived( BMessage * p_message )
{
    Activate();

    switch( p_message->what )
    {
    case OPEN_FILE:
        if( file_panel )
        {
            file_panel->Show();
            break;
        }
        file_panel = new BFilePanel();
        file_panel->SetTarget( this );
        file_panel->Show();
        break;

    case OPEN_DVD:
        const char *psz_device;
        char psz_source[ B_FILE_NAME_LENGTH + 4 ];
        if( p_message->FindString("device", &psz_device) != B_ERROR )
        {
            snprintf( psz_source, B_FILE_NAME_LENGTH + 4,
                      "dvd:%s", psz_device );
            psz_source[ strlen(psz_source) ] = '\0';
            intf_PlaylistAdd( p_playlist, PLAYLIST_END, (char*)psz_source );
            p_listview->AddItem(new BStringItem((char*)psz_source));
        }
    case B_REFS_RECEIVED:
    case B_SIMPLE_DATA:
        {
            entry_ref ref;
            if( p_message->FindRef( "refs", &ref ) == B_OK )
            {
                BPath path( &ref );
                intf_PlaylistAdd( p_playlist,
                                  PLAYLIST_END, (char*)path.Path() );
                p_listview->AddItem(new BStringItem((char*)path.Path()));
            }
        }
        break;
    default:
        BWindow::MessageReceived( p_message );
        break;
    }
}
