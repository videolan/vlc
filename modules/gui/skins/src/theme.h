/*****************************************************************************
 * theme.h: Theme class
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: theme.h,v 1.5 2003/06/22 12:46:49 asmax Exp $
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


#ifndef VLC_SKIN_THEME
#define VLC_SKIN_THEME

//--- GENERAL ---------------------------------------------------------------
#include <list>
#include <string>
using namespace std;

//---------------------------------------------------------------------------
struct intf_thread_t;
class SkinWindow;
class EventBank;
class BitmapBank;
class FontBank;
class Event;
class OffSetBank;

//---------------------------------------------------------------------------
class Theme
{
    private:

    protected:
        int  Magnet;
        intf_thread_t *p_intf;

        bool ShowInTray;
        bool ShowInTaskbar;

    public:
        // Constructors
        Theme( intf_thread_t *_p_intf );
        void StartTheme( int magnet );

        // Destructor
        virtual ~Theme();
        virtual void OnLoadTheme() = 0;

        // Initialization
        void InitTheme();
        void InitWindows();
        void InitControls();
        void ShowTheme();

        virtual void AddWindow( string name, int x, int y, bool visible,
            int fadetime, int alpha, int movealpha, bool dragdrop ) = 0;
        virtual void ChangeClientWindowName( string name ) = 0;

        SkinWindow * GetWindow( string name );

        // Banks
        BitmapBank *BmpBank;
        EventBank  *EvtBank;
        FontBank   *FntBank;
        OffSetBank *OffBank;

        // List of the windows of the skin
        list<SkinWindow *> WindowList;

        // Magetism
        void HangToAnchors( SkinWindow *wnd, int &x, int &y, bool init = false );
        bool MoveSkinMagnet( SkinWindow *wnd, int left, int top );
        void MoveSkin( SkinWindow *wnd, int left, int top );
        void CheckAnchors();

        bool ConstructPlaylist;

        // Config file treatment
        void LoadConfig();
        void SaveConfig();

        // Taskbar && system tray
        void CreateSystemMenu();
        virtual void AddSystemMenu( string name, Event *event ) = 0;
        virtual void ChangeTray() = 0;
        virtual void ChangeTaskbar() = 0;

};
//---------------------------------------------------------------------------

#endif

