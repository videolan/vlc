/*****************************************************************************
 * anchor.h: Anchor class
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: anchor.h,v 1.1 2003/03/18 02:21:47 ipkiss Exp $
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


#ifndef VLC_SKIN_ANCHOR
#define VLC_SKIN_ANCHOR

//--- GENERAL ---------------------------------------------------------------
#include <list>
using namespace std;

//---------------------------------------------------------------------------
struct intf_thread_t;
class Window;
//---------------------------------------------------------------------------
class Anchor
{
    private:
        // Position parameters
        int Left;
        int Top;

        // Ray of action
        int Len;

        // Priority
        int Priority;

        // Parent window
        Window *Parent;

        // Interface thread
        intf_thread_t *p_intf;

    public:
        // Constructor
        Anchor( intf_thread_t *_p_intf, int x, int y, int len, int priority,
                Window *parent );

        // Hang to anchor if in neighbourhood
        bool Hang( Anchor *anc, int mx, int my );
        void Add( Anchor *anc );
        void Remove( Anchor *anc );
        bool IsInList( Anchor *anc );

        // List of windows actually magnetized
        list<Anchor *> HangList;

        // Get position of anchor
        void GetPos( int &x, int &y );

        // Getters
        int GetPriority()       { return Priority; }
        Window *GetParent()     { return Parent; }
};
//---------------------------------------------------------------------------

#endif
