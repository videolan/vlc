/*****************************************************************************
 * image.h: Image control
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: image.h,v 1.1 2003/03/18 02:21:47 ipkiss Exp $
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


#ifndef VLC_SKIN_CONTROL_IMAGE
#define VLC_SKIN_CONTROL_IMAGE

//--- GENERAL ---------------------------------------------------------------
#include <string>
using namespace std;

//---------------------------------------------------------------------------
class Action;
class Graphics;
class Window;

//---------------------------------------------------------------------------
class ControlImage : public GenericControl
{
    private:
        // Image ID
        string Bg;

        // Behaviour
        bool Enabled;

        // List of actions to execute when clicking
        Action  *MouseDownAction;
        string   MouseDownActionName;

    public:
        // Constructor
        ControlImage( string id, bool visible, int x, int y, string img,
            string event, string help, Window *Parent);

        // Destructor
        virtual ~ControlImage();

        // initialization
        virtual void Init();
        virtual bool ProcessEvent( Event *evt );

        //Draw image
        virtual void Draw( int x, int y, int w, int h, Graphics *dest );

        // Mouse events
        virtual bool MouseDown( int x, int y, int button );
        virtual bool MouseOver( int x, int y );

        // Enabling control
        virtual void Enable( Event *event, bool enabled );

};
//---------------------------------------------------------------------------

#endif
