/*****************************************************************************
 * theme.cpp: Theme class
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: theme.cpp,v 1.14 2003/06/09 12:33:16 asmax Exp $
 *
 * Authors: Olivier Teulière <ipkiss@via.ecp.fr>
 *          Emmanuel Puig    <karibu@via.ecp.fr>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111,
 * USA.
 *****************************************************************************/


//--- VLC -------------------------------------------------------------------
#include <vlc/intf.h>

//--- SKIN ------------------------------------------------------------------
#include "../os_api.h"
#include "window.h"
#include "../os_window.h"
#include "banks.h"
#include "anchor.h"
#include "event.h"
#include "../os_event.h"
#include "../controls/generic.h"
#include "theme.h"
#include "vlcproc.h"
#include "skin_common.h"



//---------------------------------------------------------------------------
// THEME
//---------------------------------------------------------------------------
Theme::Theme( intf_thread_t *_p_intf )
{
    p_intf = _p_intf;
    BmpBank = new BitmapBank( p_intf );
    FntBank = new FontBank( p_intf );
    EvtBank = new EventBank( p_intf );
    OffBank = new OffSetBank( p_intf );
    ConstructPlaylist = false;

    ShowInTray    = false;
    ShowInTaskbar = false;

}
//---------------------------------------------------------------------------
Theme::~Theme()
{
    // Delete the windows
    list<SkinWindow *>::const_iterator win;
    for( win = WindowList.begin(); win != WindowList.end(); win++ )
    {
        delete (OSWindow *)(*win);
    }
    delete OffBank;
    delete EvtBank;
    delete BmpBank;
    delete FntBank;
}
//---------------------------------------------------------------------------
void Theme::ShowTheme()
{
    // Get parameters form vlcrc config file
    if( ShowInTray != (bool)config_GetInt( p_intf, "show_in_tray" ) )
        ChangeTray();

    if( ShowInTaskbar != (bool)config_GetInt( p_intf, "show_in_taskbar" ) )
        ChangeTaskbar();

    list<SkinWindow *>::const_iterator win;
    Event *evt1;
    Event *evt2;

    // Synchronize control to visible aspect
    for( win = WindowList.begin(); win != WindowList.end(); win++ )
    {
        // Synchronize windows visibility
        if( (*win)->OnStartThemeVisible )
        {
            evt1 = (Event *)new OSEvent( p_intf, (*win), WINDOW_OPEN,  1, 0 );
            evt2 = (Event *)new OSEvent( p_intf, (*win), WINDOW_CLOSE, 0, 0 );
        }
        else
        {
            evt1 = (Event *)new OSEvent( p_intf, (*win), WINDOW_OPEN,  0, 0 );
            evt2 = (Event *)new OSEvent( p_intf, (*win), WINDOW_CLOSE, 1, 0 );
        }
        evt1->PostSynchroMessage( true );
        evt2->PostSynchroMessage( true );
    }

    // Initialize magnetism
    CheckAnchors();

    // Show windows
    OSAPI_PostMessage( NULL, VLC_SHOW, 0, 0 );
}
//---------------------------------------------------------------------------
void Theme::CreateSystemMenu()
{
    AddSystemMenu( "Open file...", EvtBank->Get( "open" ) );
    AddSystemMenu( "Change skin...", EvtBank->Get( "load_skin" ) );
    AddSystemMenu( "Preferences...", NULL );
    AddSystemMenu( "SEPARATOR", 0 );
    AddSystemMenu( "Exit", EvtBank->Get( "quit" ) );
}
//---------------------------------------------------------------------------
void Theme::LoadConfig()
{
    // Get config from vlcrc file
    char *save = config_GetPsz( p_intf, "skin_config" );
    if( save == NULL )
        return;

    // Initialization
    list<SkinWindow *>::const_iterator win;
    int i = 0;
    int x, y, v, scan;

    // Get config for each window
    for( win = WindowList.begin(); win != WindowList.end(); win++ )
    {
        // Get config
        scan = sscanf( &save[i * 13], "(%4d,%4d,%1d)", &x, &y, &v );

        // If config has the correct number of arguments
        if( scan > 2 )
        {
            (*win)->Move( x, y );
            (*win)->OnStartThemeVisible = (bool)v;
        }

        // Next window
        i++;
    }
}
//---------------------------------------------------------------------------
void Theme::SaveConfig()
{
    // Initialize char where config is stored
    char *save  = new char[400];
    list<SkinWindow *>::const_iterator win;
    int i = 0;
    int x, y;

    // Save config of every window
    for( win = WindowList.begin(); win != WindowList.end(); win++ )
    {
        // Print config
        (*win)->GetPos( x, y );
        sprintf( &save[i * 13], "(%4d,%4d,%1d)", x, y,
            (*win)->OnStartThemeVisible );
        i++;
    }

    // Save config to file
    config_PutPsz( p_intf, "skin_config",     save );
    config_PutInt( p_intf, "show_in_tray",    (int)ShowInTray );
    config_PutInt( p_intf, "show_in_taskbar", (int)ShowInTaskbar );
    config_SaveConfigFile( p_intf, "skins" );

    // Free memory
    delete[] save;

}
//---------------------------------------------------------------------------
void Theme::StartTheme( int magnet )
{
    Magnet = magnet;
}
//---------------------------------------------------------------------------
void Theme::InitTheme()
{
    // Initialize the events
    EvtBank->Init();

    // Initialize the controls
    InitControls();

    // Initialize the windows
    InitWindows();
}
//---------------------------------------------------------------------------
void Theme::InitWindows()
{
    for( list<SkinWindow *>::const_iterator win = WindowList.begin();
         win != WindowList.end(); win++ )
    {
        (*win)->Init();
    }
}
//---------------------------------------------------------------------------
void Theme::InitControls()
{
    for( list<SkinWindow *>::const_iterator win = WindowList.begin();
         win != WindowList.end(); win++ )
    {
        for( unsigned int i = 0; i < (*win)->ControlList.size(); i++ )
        {
            (*win)->ControlList[i]->Init();
        }
    }
}
//---------------------------------------------------------------------------
SkinWindow * Theme::GetWindow( string name )
{
    for( list<SkinWindow *>::const_iterator win = WindowList.begin();
         win != WindowList.end(); win++ )
    {
        if( name == OSAPI_GetWindowTitle( *win ) )
        {
            return (*win);
        }
    }
    return NULL;
}
//---------------------------------------------------------------------------
void Theme::MoveSkin( SkinWindow *wnd, int left, int top )
{
    int oldx, oldy;
    SkinWindow *win;
    list<Anchor *>::const_iterator anc;
    list<Anchor *>::const_iterator hang;
    wnd->GetPos( oldx, oldy );

    // Move child windows recursively
    for( anc = wnd->AnchorList.begin(); anc != wnd->AnchorList.end(); anc++ )
    {
        for( hang = (*anc)->HangList.begin(); hang != (*anc)->HangList.end();
             hang++ )
        {
            win = (*hang)->GetParent();
            // Check that the window hasn't already moved (this avoids
            // infinite recursion with circular anchoring)
            if( !win->Moved )
            {
                win->Moved = true;
                MoveSkin( win, left, top );
            }
        }
    }

    // Move window
    wnd->Move( oldx + left, oldy + top );
}
//---------------------------------------------------------------------------
bool Theme::MoveSkinMagnet( SkinWindow *wnd, int left, int top )
{

    // If magnetism not activate
    if( !Magnet )
    {
        wnd->Move( left, top );
        return false;
    }

    // Screen bounds initialization
    int NewLeft = left;
    int NewTop  = top;
    int Sx, Sy, Wx, Wy;
    OSAPI_GetScreenSize( Sx, Sy );
    int width, height;
    wnd->GetSize( width, height );
    wnd->GetPos( Wx, Wy );

    // Magnetism with screen bounds
    if( left < Magnet && left > -Magnet)
        NewLeft = 0;
    else if( left + width > Sx - Magnet && left + width < Sx + Magnet )
        NewLeft = Sx - width;
    if( top < Magnet && top > -Magnet )
        NewTop = 0;
    else if( top + height > Sy - Magnet && top + height < Sy + Magnet )
        NewTop = Sy - height;

    // Deal with anchors
    HangToAnchors( wnd, NewLeft, NewTop );

    // All windows can be moved
    list<SkinWindow *>::const_iterator win;
    for( win = WindowList.begin(); win != WindowList.end(); win++ )
        (*win)->Moved = false;

    // Move Window
    MoveSkin( wnd, NewLeft - Wx, NewTop - Wy );

    return true;
}
//---------------------------------------------------------------------------
void Theme::HangToAnchors( SkinWindow *wnd, int &x, int &y, bool init )
{
    // Magnetism initialization
    int win_x, win_y, win_anchor_x, win_anchor_y, wnd_anchor_x, wnd_anchor_y;
    list<SkinWindow *>::const_iterator win;
    list<Anchor *>::const_iterator win_anchor, wnd_anchor;

    // Parse list of windows
    for( win = WindowList.begin(); win != WindowList.end(); win++ )
    {
        // If window is moved window
        if( (*win) == wnd )
            continue;               // Check next window

        // If window is hidden
        if( !init )
        {
            if( (*win)->IsHidden() )
                continue;           // Check next window
        }
        else
        {
            if( !(*win)->OnStartThemeVisible )
                continue;           // Check next window
        }

        // Parse anchor lists
        for( wnd_anchor  = wnd->AnchorList.begin();
             wnd_anchor != wnd->AnchorList.end(); wnd_anchor++ )
        {
            for( win_anchor  = (*win)->AnchorList.begin();
                 win_anchor != (*win)->AnchorList.end(); win_anchor++ )
            {
                if( (*wnd_anchor)->GetPriority() <
                    (*win_anchor)->GetPriority() )
                {
                    // Parent anchor is win and child is wnd !!!

                    if( !(*win_anchor)->Hang( (*wnd_anchor), x, y ) )
                    {
                        // If child is in parent list and parent doesn't hang
                        // child
                        if( (*win_anchor)->IsInList( (*wnd_anchor) ) )
                            (*win_anchor)->Remove( (*wnd_anchor) );
                    }
                    else
                    {
                        // If parent hangs child and child is not yet in list
                        if( !(*win_anchor)->IsInList( (*wnd_anchor) ) )
                        {
                            (*win_anchor)->Add( (*wnd_anchor) );
                        }

                        // Move window to stick anchor
                        (*wnd_anchor)->GetPos( wnd_anchor_x, wnd_anchor_y );
                        (*win_anchor)->GetPos( win_anchor_x, win_anchor_y );
                        (*win)->GetPos( win_x, win_y );

                        x = win_x + win_anchor_x - wnd_anchor_x;
                        y = win_y + win_anchor_y - wnd_anchor_y;

                        break;
                    }

                }
                else if( (*win_anchor)->Hang( (*wnd_anchor), x, y ) )
                {
                    if( !(*wnd_anchor)->IsInList( *win_anchor ) )
                    {
                        // Move window to stick anchor
                        (*wnd_anchor)->GetPos( wnd_anchor_x, wnd_anchor_y );
                        (*win_anchor)->GetPos( win_anchor_x, win_anchor_y );
                        (*win)->GetPos( win_x, win_y );

                        x = win_x + win_anchor_x - wnd_anchor_x;
                        y = win_y + win_anchor_y - wnd_anchor_y;
                    }

                    break;
                }
            }
        }
    }
}
//---------------------------------------------------------------------------
void Theme::CheckAnchors()
{
    list<SkinWindow *>::const_iterator win;
    int x, y;

    for( win = WindowList.begin(); win != WindowList.end(); win++ )
    {
        (*win)->GetPos( x, y );
        HangToAnchors( (*win), x, y, true );
        (*win)->Move( x, y );
    }
}
//---------------------------------------------------------------------------

