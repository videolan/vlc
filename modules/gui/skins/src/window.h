/*****************************************************************************
 * window.h: Window class
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: window.h,v 1.4 2003/06/22 00:00:28 asmax Exp $
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


#ifndef VLC_SKIN_WIN
#define VLC_SKIN_WIN

//--- GENERAL ---------------------------------------------------------------
#include <list>
#include <string>
#include <vector>
using namespace std;

//---------------------------------------------------------------------------
struct intf_thread_t;
class Graphics;
class GenericControl;
class Anchor;
class Event;

//---------------------------------------------------------------------------
// Constants for scrolling
#define MOUSE_SCROLL_UP 0
#define MOUSE_SCROLL_DOWN 1

//---------------------------------------------------------------------------
class SkinWindow
{
    protected:
        // Interface thread
        intf_thread_t *p_intf;

        // Position parmaters
        int  Left;
        int  Top;
        int  Width;
        int  Height;

        // General parameters
        Graphics *Image;
        int  Transition;
        int  MoveAlpha;
        int  NormalAlpha;
        int  Alpha;
        bool WindowMoving;
        bool Hidden;
        bool Changing;

        // Fading transition;
        int StartAlpha;
        int EndAlpha;
        int StartTime;
        int EndTime;
        int Lock;

        // Tooltip
        string ToolTipText;

        // Drag & drop
        bool DragDrop;

    public:
        // Controls
        vector<GenericControl *> ControlList;

        // Constructors
        SkinWindow( intf_thread_t *_p_intf, int x, int y, bool visible,
                int transition, int normalalpha, int movealpha, bool dragdrop );

        // Destructors
        virtual ~SkinWindow();

        // Event processing
        bool ProcessEvent( Event *evt );
        virtual bool ProcessOSEvent( Event *evt ) = 0;

        // Mouse events
        void MouseUp(       int x, int y, int nutton );
        void MouseDown(     int x, int y, int button );
        void MouseMove(     int x, int y, int button );
        void MouseDblClick( int x, int y, int button );
        void SkinWindow::MouseScroll( int x, int y, int direction );

        // Window graphic aspect
        bool OnStartThemeVisible;
        void Show();
        void Hide();
        void Open();
        void Close();

        void RefreshAll();
        void Refresh( int x, int y, int w, int h );
        void RefreshImage( int x, int y, int w, int h );
        virtual void RefreshFromImage( int x, int y, int w, int h ) = 0;

        void Fade( int To, int Time = 1000, unsigned int evt = 0 );
        bool IsHidden() { return Hidden; };

        virtual void OSShow( bool show ) = 0;
        virtual void SetTransparency( int Value = -1 ) = 0;
        virtual void WindowManualMove() = 0;
        virtual void WindowManualMoveInit() = 0;

        // Window methods
        void Init();
        void ReSize();
        void GetSize( int &w, int &h );
        void GetPos(  int &x, int &y );
        virtual void Move( int left, int top ) = 0;
        virtual void Size( int width, int height ) = 0;

        // Fading transition
        bool ChangeAlpha( int time );
        void Init( int start, int end, int time );

        // Texts
        virtual void ChangeToolTipText( string text ) = 0;

        // Magnetic Anchors
        list<Anchor *> AnchorList;
        bool Moved;

        // Get the interface thread
        intf_thread_t *GetIntf()    { return p_intf; }
};
//---------------------------------------------------------------------------
class SkinWindowList
{
    private:
        static SkinWindowList *_instance;
        list<SkinWindow*> _list;
        
        SkinWindowList();
        
    public:
        static SkinWindowList *Instance();
        void Add( SkinWindow *win );
        SkinWindow *Back();
        list<SkinWindow*>::const_iterator Begin();
        list<SkinWindow*>::const_iterator End();
};
//---------------------------------------------------------------------------

#endif
