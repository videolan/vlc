/*****************************************************************************
 * playlist.h: Playlist control
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: playlist.h,v 1.5 2003/04/28 12:25:34 asmax Exp $
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


#ifndef VLC_SKIN_CONTROL_PLAYLIST
#define VLC_SKIN_CONTROL_PLAYLIST

//--- GENERAL ---------------------------------------------------------------
#include <string>
using namespace std;

//---------------------------------------------------------------------------
class Event;
class Graphics;
class SkinWindow;
class SkinFont;
class Bezier;
class SkinRegion;

//---------------------------------------------------------------------------
#define MAX_PLAYLIST_SIZE 7

//---------------------------------------------------------------------------
class ControlPlayList : public GenericControl
{
    private:
        Event      *UpdateEvent;
        SkinFont       *TextFont;
        SkinFont       *PlayFont;
        string      FontName;
        string      PlayFontName;
        bool        Enabled;
        playlist_t *PlayList;

        // Scroll slider
        ControlSlider *Slider;
        int StartIndex;

        // Playlist text zone
        int Margin;
        int Column;
        int Line;
        int CaseHeight;

        int CaseWidth;
        int NumWidth;
        int FileWidth;
        int InfoWidth;
        char Num[MAX_PLAYLIST_SIZE];

        int *CaseRight;
        int *CaseLeft;
        int *CaseTextLeft;
        int TextLeft;
        int TextTop;
        int TextHeight;
        int TextWidth;
        Bezier *TextCurve;
        SkinRegion *TextClipRgn;

        int  NumOfItems;
        int  SelectColor;
        bool *Select;

        bool LongFileName;
        char * GetFileName( int i );

        // Calculate distance between two points
        double Dist( int x1, int y1, int x2, int y2 );

        // Text functions
        void DrawAllCase( Graphics *dest, int x, int y, int w, int h );
        void DrawCase( Graphics *dest, int i, int x, int y, int w, int h );
        void RefreshList();
        void RefreshAll();

    public:
        // Constructor
        ControlPlayList( string id, bool visible, int width, int infowidth,
                         string font, string playfont, int selcolor,
                         double *ptx, double *pty, int nb, bool longfilename,
                         string help, SkinWindow *Parent );

        // Destructor
        virtual ~ControlPlayList();

        // initialization
        virtual void Init();
        virtual bool ProcessEvent( Event *evt );
        void InitSliderCurve( double *ptx, double *pty, int nb,
                              string scroll_up, string scroll_down );

        // Draw control
        virtual void Draw( int x, int y, int w, int h, Graphics *dest );

        // Mouse events
        virtual bool MouseUp( int x, int y, int button );
        virtual bool MouseDown( int x, int y, int button );
        virtual bool MouseMove( int x, int y, int button );
        virtual bool MouseOver( int x, int y );
        virtual bool MouseDblClick( int x, int y, int button );
        virtual bool MouseScroll( int x, int y, int direction );
        virtual bool ToolTipTest( int x, int y );

        // Translate control
        virtual void MoveRelative( int xOff, int yOff );
};
//---------------------------------------------------------------------------

#endif
