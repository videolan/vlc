/*****************************************************************************
 * text.h: Text control
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: text.h,v 1.2 2003/04/17 13:08:02 karibu Exp $
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


#ifndef VLC_SKIN_CONTROL_TEXT
#define VLC_SKIN_CONTROL_TEXT

//--- GENERAL ---------------------------------------------------------------
#include <string>
using namespace std;

//---------------------------------------------------------------------------
class Event;
class BitmapBank;
class Graphics;
class Window;
class Region;
class Font;

//---------------------------------------------------------------------------
class ControlText : public GenericControl
{
    private:
        // Scrolling parameters
        bool     Scroll;
        int      TextWidth;
        int      TextLeft;
        int      ScrollSpace;
        bool     Selected;
        int      SelectedX;
        int      MouseX;
        int      MouseY;
        bool     PauseScroll;

        // Initial parameters
        bool     InitScroll;
        int      InitLeft;
        int      InitWidth;

        // General parameters
        string   Text;
        int      Align;
        Font    *TextFont;
        string   FontName;
        list<string> DisplayList;
        list<string>::const_iterator Display;
        Region   *TextClipRgn;

        // Internal methods
        void SetSize();
        void SetScrolling();
        void StartScrolling();
        void StopScrolling();

    public:
        // Constructor
        ControlText( string id, bool visible, int x, int y, string text,
            string font, int align, int width, string display,
            bool scroll, int scrollspace, string help,  Window *Parent );

        // initialization
        virtual void Init();
        virtual bool ProcessEvent( Event *evt );

        // Destructor
        ~ControlText();

        // Draw control
        virtual void Draw( int x, int y, int w, int h, Graphics *dest );

        // Mouse events
        virtual bool MouseUp( int x, int y, int button );
        virtual bool MouseDown( int x, int y, int button );
        virtual bool MouseMove( int x, int y, int button );
        virtual bool MouseOver( int x, int y );
        virtual bool MouseDblClick( int x, int y, int button );

        // Move control
        virtual void MoveRelative( int xOff, int yOff );

        // Set text
        void SetText( const string newText );

        // Keep on scrolling
        void DoScroll();

        // Getters
        bool GetSelected() { return Selected; };
        bool IsScrolling() { return Scroll; };
};
//---------------------------------------------------------------------------

#endif
